#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <map>
#include <netinet/in.h>
#include <arpa/inet.h>
/*
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
*/
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>


#define debug(x) cerr << #x << " = " << x << '\n'

//using namespace cv;
using namespace std;

void ssl_init();
SSL_CTX *create_context();
void configure_context(SSL_CTX *ctx);
void message_receiver();
void select_options(int sockfd, int num);
int socket_init();
void send_message(int sockfd, string messages);
string recv_message(int sockfd);
void connect_server(int sockfd, char *ip, char *port);
void login_sucess(int sockfd, string username);
void open_chat(int sockfd, string name, string username);
void chat_message(int sockfd, string name, string username);


string server_ip, chat_port;
bool Login = false;
int chatting_sockfd;
map<string, string> chat_history;

int main(int argc, char *argv[]) { // argv[1]: IP address of the server, argv[2]: port number, argv[3]: chat port number
    if (argc != 4) {
        cout << "Usage: " << argv[0] << " <IP> <Port1> <Port2>\n";
        return 1;
    }
    
    server_ip = argv[1];
    chat_port = argv[3];
    chatting_sockfd = -1;

    string messages, res;
    int sockfd = socket_init();

    // connect
    connect_server(sockfd, argv[1], argv[2]);
    // Login or Register
    while (1){
        select_options(sockfd, 1);
        string Select;
        getline(cin, Select);

        if (Select.size() != 1 || Select[0] < '1' || Select[0] > '3') {
            cout << "[Invalid selection]\n";
            continue;
        }
        int Selection = Select[0] - '0';
        if (Selection == 1){ // Register
            string username, password, msg;
            cout << "Username: ";
            getline(cin, username);
            cout << "Password: ";
            getline(cin, password);
            if (username.empty() || password.empty()) {
                cout << "Empty username or password\n";
                continue;
            }

            msg = "/$" + username + "/#" + password; // /$username/#password

            send_message(sockfd, msg);
            res = recv_message(sockfd);
            // debug(res);

            if (res == "Success")
                cout << "[Register success]\n";
            else
                cout << "[Register failed]\n";
        }
        else if (Selection == 2){ // Login
            string username, password, msg;
            cout << "Username: ";
            getline(cin, username);
            cout << "Password: ";
            getline(cin, password);
            if (username.empty() || password.empty()) {
                cout << "Empty username or password\n";
                continue;
            }

            msg = "/$$" + username + "/#" + password  + "#" + argv[3]; // /$$username/#password#chat_port
            send_message(sockfd, msg);
            res = recv_message(sockfd);
            // debug(res);
            if (res == "Success"){
                cout << "[Login success]\n";
                login_sucess(sockfd, username);
            }
            else
                cout << "[Login failed]\n";
        }
        else if (Selection == 3){ // Exit
            cout << "[Exit]\n";
            send_message(sockfd, "@exit@");
            close(sockfd);
            return 0;
        }
    }
    
    close(sockfd);
    return 0;
}


void select_options(int sockfd, int num){
    if (num == 1){ // login or register
        cout << "1. Register\n";
        cout << "2. Login\n";
        cout << "3. Exit\n";
        cout << "Selection: ";
    }
    else if (num == 2){ // login success
        cout << "1. Choose User\n";
        cout << "2. Logout\n";
        cout << "Selection: ";
    }
    else if (num == 3){ // send message
        // ask server for online users
        send_message(sockfd, "@listonline@");
        string res = recv_message(sockfd);
        cout << res;
        cout << "Please enter a user name: ";
    }
    else if (num == 4){ // chat
        cout << "1. Continue\n";
        cout << "2. End\n";
    }
}

void ssl_init(){
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
}

SSL_CTX *create_context(){
    const SSL_METHOD *method;
    SSL_CTX *_ctx;

    method = TLS_client_method();

    _ctx = SSL_CTX_new(method);
    if (!_ctx) {
        perror("[Unable to create SSL context]");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return _ctx;
}

void configure_context(SSL_CTX *_ctx){
    SSL_CTX_set_ecdh_auto(_ctx, 1);

    if (SSL_CTX_use_certificate_file(_ctx, "cert.pem", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(_ctx, "key.pem", SSL_FILETYPE_PEM) <= 0 ) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
}

void send_message(int sockfd, string messages){
    int attempt = 0;
    while (send(sockfd, messages.c_str(), messages.size(), 0) < 0) {
        if (attempt == 10){
            cout << "[Sending failed]\n";
            close(sockfd);
            exit(1);
        }
        attempt++;
    }
    // cout << messages << '\n';
    // cout << "[Message sent]\n";
}

string recv_message(int sockfd){
    string res = "";
    int attempt = 0;
    while (1){
        char buffer[4096] = {0};
        int bytes = recv(sockfd, buffer, sizeof(buffer), 0);
        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) // no data available
                break;
            if (attempt == 10){
                cout << "[Receiving failed]\n";
                close(sockfd);
                exit(1);
            }
            attempt++;
            sleep(1);
            continue;
        }
        if (bytes == 0)
            break;
        res.append(buffer, bytes);
        if ((unsigned long)bytes < sizeof(buffer))
            break;
    }
    return res;
}

void connect_server(int sockfd, char *ip, char *port){
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

int socket_init(){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); // IPv4, TCP, default
    if (sockfd < 0) {
        cout << "[Creating socket failed]\n";
        exit(1);
    }
    cout << "[Socket created]\n";
    return sockfd;
}

void login_sucess(int sockfd, string username){
    chatting_sockfd = socket_init();
    connect_server(chatting_sockfd, (char *)server_ip.c_str(), (char *)chat_port.c_str());

    // non-blocking mode
    int flags = fcntl(chatting_sockfd, F_GETFL, 0);
    fcntl(chatting_sockfd, F_SETFL, flags | O_NONBLOCK);


    Login = true;

    while (1){
        message_receiver();

        select_options(sockfd, 2);
        string select, messages;
        getline(cin, select);
        
        message_receiver();

        if (select.size() != 1 || select[0] < '1' || select[0] > '2') {
            cout << "[Invalid selection]\n";
            continue;
        }
        int selection = select[0] - '0';

        if (selection == 1){ // send message
            message_receiver();

            select_options(sockfd, 3);
            string name, msg;
            getline(cin, name);
            message_receiver();

            if (name.empty()) {
                cout << "Empty name\n";
                continue;
            }

            msg = "@verify@" + name;
            send_message(sockfd, msg);

            message_receiver();

            string res = recv_message(sockfd);

            message_receiver();
            // what if user logout now?
            if (res == "Failed"){
                cout << "User not found\n";
                continue;
            }

            message_receiver();
            open_chat(sockfd, name, username);
            message_receiver();
        }
        else if (selection == 2){ // logout
            message_receiver();

            send_message(sockfd, "@logout@");
            cout << "[Logout]\n";
            break;
        }
    }
    close(chatting_sockfd);
}

void message_receiver(){ // receving chat message
    if (Login){
        string res = recv_message(chatting_sockfd);
        for (int i = 0; i < (int)res.size(); ++i){
            if (res[i] == '#'){
                string name = "", username = "", msg = "";
                int j;
                for (j = i + 1; res[j] != '#'; ++j)
                    name += res[j];
                for (j = name.size() + i + 2; res[j] != ':'; ++j)
                    username += res[j];
                cout << "[New message from " << name << "] ";
                for (j = name.size() + username.size() + i + 3; res[j] != '\n' && j < (int)res.size(); ++j){
                    cout << res[j];
                    msg += res[j];
                }
                cout << '\n';
                msg = name + ": " + msg + "\n";
                chat_history[name] += msg;
                i = j;
            }
        }
    }
}


void open_chat(int sockfd, string name, string username){
    cout << "-----------------------------------\n";
    cout << "Chatting with " << name << '\n';
    cout << chat_history[name];
    cout << "-----------------------------------\n";
    while (1){
        select_options(sockfd, 4);
        string select;
        getline(cin, select);
        if (select.size() != 1 || select[0] < '1' || select[0] > '2') {
            cout << "[Invalid selection]\n";
            continue;
        }
        int selection = select[0] - '0';
        if (selection == 2){
            cout << "[End chat]\n";
            break;
        }
        chat_message(sockfd, name, username);
    }
}

void chat_message(int sockfd, string name, string username){
    message_receiver();

    string messages;
    cout << "Message: ";
    getline(cin, messages);

    message_receiver();
    
    if (messages.empty()) {
        cout << "Empty message\n";
        return ;
    }
    chat_history[name] += ": " + messages + "\n";
    messages = "#" + username + "#" + name + ":" + messages; // #from#to:message
    send_message(sockfd, messages);
    string res = recv_message(sockfd);
    if (res == "Failed")
        cout << "[Chat message failed]\n";
    else
        cout << "[Chat message sent]\n";

    message_receiver();
}
