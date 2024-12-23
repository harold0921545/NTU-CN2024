#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <set>
#include <algorithm>
/*
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
*/
#include <openssl/ssl.h>
#include <openssl/crypto.h>

#define debug(x) cerr << #x << " = " << x << '\n'

// using namespace cv;
using namespace std;

struct User {
    string username;
    string password;
    int sockfd; // client socket
    int chat_sockfd; // chat socket
    bool online = false;
} user[4096];

struct job {
    int sockfd;
    string ip;
};

struct thread_pool {
    pthread_t pool[1024];
    pthread_mutex_t mutex;
    pthread_attr_t client_attr;
    int thread_count = 0;
} thread_pool;

int chat_socket_init();
int socket_init(char *port);
string recv_message(int sockfd);
void send_message(int sockfd, string messages);
void *handle_client(void *arg);
void connect_client(int sockfd, char *ip, char *port);

void thread_init();

int user_count = 0;
set<string> online_users;

int main(int argc, char *argv[]) {
    if (argc != 2){
        cout << "Usage: " << argv[0] << " <Port>\n";
        return 1;
    }
    int sockfd = socket_init(argv[1]);
    if (sockfd < 0)
        return 1;
    // accept
    thread_init();

    while (1){
        // accept
        sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_sockfd = accept(sockfd, (sockaddr *) &client_addr, &client_addr_len);
        if (client_sockfd < 0) { // retry every 2 seconds
            sleep(2);
            continue;
        }

        cout << "[Client " << inet_ntoa(client_addr.sin_addr) << " is accepted]\n";
        job data = {client_sockfd, inet_ntoa(client_addr.sin_addr)};
        
        // create thread
        pthread_mutex_lock(&thread_pool.mutex);

        pthread_create(&thread_pool.pool[thread_pool.thread_count++], &thread_pool.client_attr, handle_client, (void *)&data);
        
        pthread_mutex_unlock(&thread_pool.mutex);
        
        debug(thread_pool.thread_count);
    }
    close(sockfd);
    pthread_exit(NULL);
}


int socket_init(char *port){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); // IPv4, TCP, default
    if (sockfd < 0) {
        cout << "[Creating socket failed]\n";
        return -1;
    }
    cout << "[Socket created]\n";
    // bind
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(port)); // host-to-network short
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // accept any IP address

    if (bind(sockfd, (sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        cout << "[Binding socket failed]\n";
        close(sockfd);
        return -1;
    }
    cout << "[Socket bound]\n";

    // listen
    if (listen(sockfd, 10) < 0) { // 10 is the maximum number of connections
        cout << "[Listening failed]\n";
        close(sockfd);
        return -1;
    }
    cout << "[Listening]\n";

    // set non-blocking
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    return sockfd;
}

string recv_message(int sockfd){
    string res = "";
    int attempt = 0;
    while (1){
        char buffer[4096] = {0};
        int bytes = recv(sockfd, buffer, sizeof(buffer), 0);
        if (bytes < 0) {
            if (attempt < 10 && (errno == EAGAIN || errno == EWOULDBLOCK)){
                attempt++;
                continue;
            }
            cout << "[Receiving failed]\n";
            close(sockfd);
            pthread_exit(NULL);
        }
        if (bytes == 0)
            break;
        res.append(buffer, bytes);
        if ((unsigned long)bytes < sizeof(buffer))
            break;
    }
    return res;
}

void send_message(int sockfd, string messages){
    int attempt = 0;
    while (send(sockfd, messages.c_str(), messages.size(), 0) < 0) { 
        attempt++;
        if (attempt == 10){
            cout << "[Sending failed]\n";
            close(sockfd);
            exit(1);
        }
    }
}

void *handle_client(void *arg){
    job data = *(job *)arg;
    int client_sockfd = data.sockfd;
    string ip = data.ip;

    while (1){
        // recieve
        string buffer = recv_message(client_sockfd);
        int bytes = buffer.size();
        cout << "Recieved: " << buffer << '\n';
        bool special = false;
        if (buffer[0] == '/' && buffer[1] == '$'){
            for (int i = 2; i < bytes - 1; ++i){
                if (buffer[i] == '/' && buffer[i + 1] == '#'){
                    special = true;
                    break;
                }
            }
        }

        if (special){ // register or login
            if (user_count >= 4096){ // should not happen
                cout << "User full\n";
                continue;
            }
            int n = buffer.size();
            string username, password;
            bool login = (buffer[2] == '$');
            while (buffer[n] != '#' && login)
                n--;
            for (int i = 2 + login; (buffer[i] != '/' || buffer[i + 1] != '#'); ++i)
                username += buffer[i];
            for (int i = username.size() + 4 + login; i < n; ++i)
                password += buffer[i];
            debug(password);
            if (login){
                // chat port
                string chat_port = "";
                int I = buffer.size() - 1;
                while (buffer[I] != '#')
                    chat_port += buffer[I--];
                reverse(chat_port.begin(), chat_port.end());

                bool success = false;
                for (int i = 0; i < user_count; ++i){
                    if (user[i].username == username && user[i].password == password){
                        send_message(client_sockfd, "Success");
                        cout << "[User: " << username <<  " login success]\n";
                        success = true;

                        pthread_mutex_lock(&thread_pool.mutex);

                        user[i].sockfd = client_sockfd; // maybe login from another ip
                        user[i].online = true;
                        online_users.insert(username);

                        // chat connection
                        user[i].chat_sockfd = chat_socket_init();
                        connect_client(user[i].chat_sockfd, (char *)ip.c_str(), (char *)chat_port.c_str());

                        pthread_mutex_unlock(&thread_pool.mutex);
                        break;
                    }
                }
                if (!success){
                    send_message(client_sockfd, "Failed");
                    cout << "[User: " << username <<  " login failed]\n";
                }
            }
            else{
                if (user_count > 4096){ // too many users
                    send_message(client_sockfd, "Failed");
                    cout << "[User full]\n";
                    continue;
                }
                bool success = true;
                for (int i = 0; i < user_count; ++i){
                    if (user[i].username == username){
                        send_message(client_sockfd, "Failed");
                        cout << "[User: " << username <<  " register failed: user name exist]\n";
                        success = false;
                        break;
                    }
                }
                if (!success)
                    continue;
                // modify global data, lock
                pthread_mutex_lock(&thread_pool.mutex);
                user[user_count].username = username;
                user[user_count].password = password;
                user[user_count].sockfd = client_sockfd;
                user[user_count].online = false; // hasn't login
                user_count++;
                pthread_mutex_unlock(&thread_pool.mutex);
                
                send_message(client_sockfd, "Success");
                cout << "[User: " << username <<  " register success]\n";
            }
        }
        else if (buffer == "@logout"){
            string name;
            for (int i = 0; i < user_count; ++i){
                if (user[i].sockfd == client_sockfd){
                    pthread_mutex_lock(&thread_pool.mutex);
                    user[i].online = false;
                    name = user[i].username;
                    online_users.erase(name);
                    close(user[i].chat_sockfd);
                    user[i].chat_sockfd = -1;
                    pthread_mutex_unlock(&thread_pool.mutex);
                    break;
                }
            }
            cout << "[User " << name << " logout]\n";
        }
        else if (buffer == "@exit@"){
            cout << "[Client " << ip << " disconnected]\n";
            break;
        }
        else if (buffer == "@listonline@"){
            string res = "";
            int count = 0;
            for (auto it = online_users.begin(); it != online_users.end(); ++it)
                res += (to_string(++count) + ". " + *it + "\n");
            send_message(client_sockfd, res);
            cout << "[Online users sent]\n";
        }
        else if (buffer.substr(0, 8) == "@verify@"){
            string name = "";
            bool success = false;
            for (int i = 8; i < bytes; ++i)
                name += buffer[i];
            for (int i = 0; i < user_count; ++i){
                if (online_users.find(name) != online_users.end()){
                    success = true;
                    break;
                }
            }
            if (success){
                send_message(client_sockfd, "Success");
                cout << "[" << name << " found]\n";
            }
            else{
                send_message(client_sockfd, "Failed");
                cout << "[" << name << " not found]\n";
            }
        }
        else if (buffer[0] == '#'){
            string name = "", username = "";
            for (int i = 1; buffer[i] != '#'; ++i)
                username += buffer[i];
            for (int i = username.size() + 2; buffer[i] != ':'; ++i)
                name += buffer[i];
            
            int sockfd_to_send = -1;
            for (int i = 0; i < user_count; ++i){
                if (user[i].username == name){
                    sockfd_to_send = user[i].chat_sockfd;
                    break;
                }
            }
            if (sockfd_to_send == -1){
                send_message(client_sockfd, "Failed");
                cout << "[User not found]\n";
            }
            else{
                send_message(client_sockfd, "Success");
                send_message(sockfd_to_send, buffer);
                cout << "[Message sent]\n";
            }
        }
    }

    close(client_sockfd);
    pthread_exit(NULL);
}

void thread_init(){
    pthread_mutex_init(&thread_pool.mutex, NULL);
    thread_pool.thread_count = 0;
    pthread_attr_init(&thread_pool.client_attr);
    pthread_attr_setdetachstate(&thread_pool.client_attr, PTHREAD_CREATE_DETACHED);
}

int chat_socket_init(){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); // IPv4, TCP, default
    if (sockfd < 0) {
        cout << "[Creating socket failed]\n";
        exit(1);
    }
    cout << "[Socket created]\n";
    return sockfd;
}

void connect_client(int sockfd, char *ip, char *port){
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(port));
    server_addr.sin_addr.s_addr = inet_addr(ip);

    int attempt = 0;
    while (connect(sockfd, (sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        if (attempt == 10){
            cout << "[Connecting failed]\n";
            close(sockfd);
            exit(1);
        }
        attempt++;
        sleep(1);
    }
    cout << "[Connected]\n";
}