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
void receiver();
void select_options(SSL *ssl, int num);
int socket_init();
void send_message(SSL *ssl, string messages);
string recv_message(SSL *ssl);
void connect_server(int sockfd, char *ip, char *port);
void login_sucess(SSL *ssl, string username);
void open_chat(SSL *ssl, string name, string username);
void chat_message(SSL *ssl, string name, string username);
void transfer_file(SSL *ssl, string name, string username);

string server_ip, chat_port, file_port;
bool Login = false;
SSL *chatting_ssl, *file_transfer_ssl;
map<string, string> chat_history;

int main(int argc, char *argv[]) { // argv[1]: IP address of the server, argv[2]: port number, argv[3]: chat port number, argv[4]: file port number
    if (argc != 5) {
        cout << "Usage: " << argv[0] << " <IP> <Port1> <Port2> <Port3>\n";
        return 1;
    }
    
    server_ip = argv[1];
    chat_port = argv[3];
    file_port = argv[4];
    
    string messages, res;
    int sockfd = socket_init();

    // connect
    ssl_init();
    SSL_CTX *ctx = create_context();
    configure_context(ctx);

    connect_server(sockfd, argv[1], argv[2]);

    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sockfd);
    if (SSL_connect(ssl) <= 0) {
        cout << "[SSL connection failed]\n";
        close(sockfd);
        return 1;
    }
    cout << "[SSL connection established]\n";
    // Login or Register
    while (1){
        select_options(ssl, 1);
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

            send_message(ssl, msg);
            res = recv_message(ssl);
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
            send_message(ssl, msg);
            res = recv_message(ssl);
            // debug(res);
            if (res == "Success"){
                cout << "[Login success]\n";
                login_sucess(ssl, username);
            }
            else
                cout << "[Login failed]\n";
        }
        else if (Selection == 3){ // Exit
            cout << "[Exit]\n";
            send_message(ssl, "@exit@");
            close(sockfd);
            return 0;
        }
    }
    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(sockfd);
    return 0;
}


void select_options(SSL *ssl, int num){
    if (num == 1){ // login or register
        cout << "1. Register\n";
        cout << "2. Login\n";
        cout << "3. Exit\n";
        cout << "Selection: ";
    }
    else if (num == 2){ // login success
        cout << "1. Texting\n";
        cout << "2. Transfer file\n";
        cout << "3. Logout\n";
        cout << "Selection: ";
    }
    else if (num == 3){ // send message
        // ask server for online users
        send_message(ssl, "@listonline@");
        string res = recv_message(ssl);
        cout << res;
        cout << "Please enter an user name: ";
    }
    else if (num == 4){ // chat
        cout << "1. Text\n";
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
    if (!_ctx){
        cout << "[Unable to create SSL context]\n";
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return _ctx;
}

void configure_context(SSL_CTX *_ctx){
    if (SSL_CTX_load_verify_locations(_ctx, "cert.pem", NULL) != 1){
        cout << "[Unable to load verify locations]\n";
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    SSL_CTX_set_verify(_ctx, SSL_VERIFY_PEER, NULL);
}

void send_message(SSL *ssl, string messages){
    int attempt = 0;
    while (SSL_write(ssl, messages.c_str(), messages.size()) < 0) {
        if (attempt == 10){
            cout << "[Sending failed]\n";
            SSL_shutdown(ssl);
            SSL_free(ssl);
            exit(1);
        }
        attempt++;
    }
    // cout << messages << '\n';
    // cout << "[Message sent]\n";
}

string recv_message(SSL *ssl){
    string res = "";
    int attempt = 0;
    while (1){
        char buffer[4096] = {0};
        int bytes = SSL_read(ssl, buffer, sizeof(buffer));
        if (bytes < 0) {
            int err = SSL_get_error(ssl, bytes);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
                break;
            attempt++;
            if (attempt < 10){
                sleep(1);
                continue;
            }
            cout << "[Receiving failed]\n";
            SSL_shutdown(ssl);
            SSL_free(ssl);
            exit(1);
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

void login_sucess(SSL *ssl, string username){
    // text receving socket
    int chatting_sockfd = socket_init();
    connect_server(chatting_sockfd, (char *)server_ip.c_str(), (char *)chat_port.c_str());
    
    // non-blocking mode
    int flags = fcntl(chatting_sockfd, F_GETFL, 0);
    fcntl(chatting_sockfd, F_SETFL, flags | O_NONBLOCK);

    SSL_CTX *ctx = create_context();
    configure_context(ctx);
    chatting_ssl = SSL_new(ctx);
    SSL_set_fd(chatting_ssl, chatting_sockfd);
    int attempt = 0;
    while (SSL_connect(chatting_ssl) <= 0) {
        if (attempt == 10){
            cout << "[SSL connection failed]\n";
            close(chatting_sockfd);
            return ;
        }
        attempt++;
        sleep(1);
    }
    cout << "[Chatting SSL connection established]\n";

    // file receving socket
    int file_transfer_sockfd = socket_init();
    connect_server(file_transfer_sockfd, (char *)server_ip.c_str(), (char *)file_port.c_str());

    // non-blocking mode
    flags = fcntl(file_transfer_sockfd, F_GETFL, 0);
    fcntl(file_transfer_sockfd, F_SETFL, flags | O_NONBLOCK);

    file_transfer_ssl = SSL_new(ctx);
    SSL_set_fd(file_transfer_ssl, file_transfer_sockfd);
    attempt = 0;
    while (SSL_connect(file_transfer_ssl) <= 0) {
        if (attempt == 10){
            cout << "[SSL connection failed]\n";
            close(file_transfer_sockfd);
            return ;
        }
        attempt++;
        sleep(1);
    }
    cout << "[File transfer SSL connection established]\n";

    Login = true;

    while (1){
        receiver();

        select_options(ssl, 2);
        string select, messages;
        getline(cin, select);
        
        receiver();

        if (select.size() != 1 || select[0] < '1' || select[0] > '3') {
            cout << "[Invalid selection]\n";
            continue;
        }
        int selection = select[0] - '0';

        if (selection == 1 || selection == 2){ // send message, file transfer
            receiver();

            select_options(ssl, 3);
            string name, msg;
            getline(cin, name);
            receiver();

            if (name.empty()) {
                cout << "Empty name\n";
                continue;
            }

            msg = "@verify@" + name;
            send_message(ssl, msg);

            receiver();

            string res = recv_message(ssl);

            receiver();
            // what if user logout now?
            if (res == "Failed"){
                cout << "User not found\n";
                continue;
            }

            receiver();
            if (selection == 1)
                open_chat(ssl, name, username);
            else if (selection == 2)
                transfer_file(ssl, name, username);
            receiver();
        }
        else if (selection == 3){ // logout
            receiver();

            send_message(ssl, "@logout@");
            cout << "[Logout]\n";
            break;
        }
    }
    close(chatting_sockfd);
    SSL_shutdown(chatting_ssl);
    SSL_free(chatting_ssl);
    SSL_CTX_free(ctx);

    Login = false;
    return ;
}

void receiver(){ // receving chat message
    if (Login){
        string res = recv_message(chatting_ssl);
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
        string res2 = recv_message(file_transfer_ssl);
        for (int i = 0; i < (int)res2.size(); ++i){
            if (res2[i] == '%'){
                string name = "", username = "", file_name = "", _file_size = "";
                long file_size = 0;
                int j;
                for (j = i + 1; res2[j] != '#'; ++j)
                    name += res2[j];
                for (j = name.size() + i + 2; res2[j] != '$'; ++j)
                    username += res2[j];
                for (j = name.size() + username.size() + i + 3; res2[j] != '@'; ++j)
                    file_name += res2[j];
                for (j = file_name.size() + name.size() + username.size() + i + 4; j < (int)res2.size(); ++j)
                    _file_size += res2[j];
                file_size = stol(_file_size);
                cout << "[New file from " << name << "] " << file_name << " " << file_size << " bytes\n";
                i = j;
                send_message(file_transfer_ssl, "Success"); // tell server ready to receive

                FILE *file = fopen(file_name.c_str(), "wb");
                if (file == NULL){
                    cout << "File not found\n";
                    return ;
                }
                while (file_size > 0){
                    string buffer = recv_message(file_transfer_ssl);
                    fwrite(buffer.c_str(), 1, buffer.size(), file);
                    file_size -= buffer.size();
                    send_message(file_transfer_ssl, "Success");
                }
                fclose(file);
                cout << "[File transfer success]\n";
            }
        }
    }
}


void open_chat(SSL *ssl, string name, string username){
    cout << "-----------------------------------\n";
    cout << "Chatting with " << name << '\n';
    cout << chat_history[name];
    cout << "-----------------------------------\n";
    while (1){
        select_options(ssl, 4);
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
        chat_message(ssl, name, username);
    }
}

void chat_message(SSL *ssl, string name, string username){
    receiver();

    string messages;
    cout << "Message: ";
    getline(cin, messages);

    receiver();
    
    if (messages.empty()) {
        cout << "Empty message\n";
        return ;
    }
    chat_history[name] += ": " + messages + "\n";
    messages = "#" + username + "#" + name + ":" + messages; // #from#to:message
    send_message(ssl, messages);
    string res = recv_message(ssl);
    if (res == "Failed")
        cout << "[Chat message failed]\n";
    else
        cout << "[Chat message sent]\n";

    receiver();
}

void transfer_file(SSL *ssl, string name, string username){
    receiver();

    string file_name;
    cout << "Please enter file name: ";
    getline(cin, file_name);

    receiver();
    if (file_name.empty()) {
        cout << "Empty file name\n";
        return ;
    }
    FILE *file = fopen(file_name.c_str(), "rb");
    if (file == NULL){
        cout << "File not found\n";
        return ;
    }
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);
    char buffer[2048] = {0};

    string msg = "%" + username + "#" + name + "$" + file_name + "@" + to_string(file_size); // %from#to$file_name@file_size
    send_message(ssl, msg);
    string res = recv_message(ssl);

    
    if (res == "Failed"){
        cout << "[Transfer file failed]\n";
        return ;
    }

    while (file_size > 0){
        int bytes = fread(buffer, 1, sizeof(buffer), file);
        int attempt = 0;
        while (1){
            send_message(ssl, string(buffer, bytes));
            memset(buffer, 0, sizeof(buffer));
            res = recv_message(ssl);
            if (res == "Success")
                break;
            else{
                attempt++;
                if (attempt == 10){
                    cout << "[Transfer file failed]\n";
                    return ;
                }
            }
        }
        file_size -= bytes;
    }

    fclose(file);
    cout << "[Transfer file success]\n";

    receiver();
}