#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define debug(x) cerr << #x << " = " << x << '\n'

using namespace std;

struct User {
    string username;
    string password;
} user[4096];

int socket_init();
string recv_message(int sockfd);
void send_message(int sockfd, string messages);

int main(int argc, char *argv[]) {
    int sockfd = socket_init();
    int user_count = 0;
    if (sockfd < 0)
        return 1;
    // accept
    while (1){
        // accept
        sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_sockfd = accept(sockfd, (sockaddr *) &client_addr, &client_addr_len);
        if (client_sockfd < 0) {
            cout << "[Accepting failed]\n";
            close(sockfd);
            return 1;
        }
        cout << "[Client " << inet_ntoa(client_addr.sin_addr) << " is accepted]\n";

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
                string username, password;
                bool login = (buffer[2] == '$');
                for (int i = 2 + login; (buffer[i] != '/' || buffer[i + 1] != '#'); ++i)
                    username += buffer[i];
                for (int i = username.size() + 4 + login; i < bytes; ++i)
                    password += buffer[i];
                debug(username);
                debug(password);
                if (login){
                    bool success = false;
                    for (int i = 0; i < user_count; ++i){
                        if (user[i].username == username && user[i].password == password){
                            send_message(client_sockfd, "Success");
                            cout << "[User: " << username <<  " login success]\n";
                            success = true;
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
                    user[user_count].username = username;
                    user[user_count].password = password;
                    user_count++;
                    send_message(client_sockfd, "Success");
                    cout << "[User: " << username <<  " register success]\n";
                }
            }
            else if (buffer == "@logout@"){
                cout << "[Client " << inet_ntoa(client_addr.sin_addr) << " disconnected]\n";
                break;
            }
            else{ // receive message
                // flip the case
                for (int i = 0; i < bytes; i++){
                    if (islower(buffer[i]))
                        buffer[i] = toupper(buffer[i]);
                    else if (isupper(buffer[i]))
                        buffer[i] = tolower(buffer[i]);
                }
                send_message(client_sockfd, buffer); // if client not received, try again
                cout << "Sent: " << buffer << '\n';
            }
        }

        // TODO: pthread
        close(client_sockfd);
    }
    close(sockfd);
    return 0;
}



int socket_init(){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); // IPv4, TCP, default
    if (sockfd < 0) {
        cout << "[Creating socket failed]\n";
        return -1;
    }
    cout << "[Socket created]\n";
    // bind
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080); // host-to-network short
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

    return sockfd;
}

string recv_message(int sockfd){
    string res = "";
    while (1){
        char buffer[4096] = {0};
        int bytes = recv(sockfd, buffer, sizeof(buffer), 0);
        if (bytes < 0) {
            cout << "[Receiving failed]\n";
            close(sockfd);
            exit(1);
        }
        if (bytes == 0)
            break;
        res.append(buffer, bytes);
        if (bytes < sizeof(buffer))
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