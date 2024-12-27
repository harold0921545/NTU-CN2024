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
#include <map>
#include <algorithm>
#include <fstream>
#include <vector>
/*
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
*/
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <SDL2/SDL.h>

#define debug(x) cerr << #x << " = " << x << '\n'

// using namespace cv;
using namespace std;

struct User {
    string username;
    string password;
    int sockfd; // client socket
    int chat_sockfd; // chat socket
    int file_sockfd; // file socket
    int audio_sockfd; // audio socket
    SSL *ssl;
    SSL *chatting_ssl;
    SSL *file_ssl;
    SSL *audio_ssl;
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

void ssl_init();
SSL_CTX *create_context();
void configure_context(SSL_CTX *ctx);

int socket_init(char *port);
string recv_message(SSL *ssl);
void send_message(SSL *ssl, string messages);
void *handle_client(void *arg);

void thread_init();

int user_count = 0;
int server_sockfd, file_transfer_sockfd, audio_transfer_sockfd;
set<string> online_users;
SSL_CTX *ctx;
map<string, vector<string>> files, audios;

int main(int argc, char *argv[]) { // argv[1]: port number, argv[2]: chat port number, argv[3]: file port number, argv[4]: audio port number
    if (argc != 5){
        cout << "Usage: " << argv[0] << " <Port1> <Port2> <Port3> <Port4>\n";
        return 1;
    }

    // initialize SSL
    
    ssl_init();
    ctx = create_context();
    configure_context(ctx);
    // initialize socket
    cout << "[Server started]\n";
    int sockfd = socket_init(argv[1]); // server socket

    cout << "[Chat server started]\n";
    server_sockfd = socket_init(argv[2]); // chat socket
    
    cout << "[File server started]\n";
    file_transfer_sockfd = socket_init(argv[3]); // file socket

    cout << "[Audio server started]\n";
    audio_transfer_sockfd = socket_init(argv[4]); // audio socket

    if (sockfd < 0)
        return 1;
    
    // initialize thread pool
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
    }

    close(sockfd);
    pthread_exit(NULL);
    SSL_CTX_free(ctx);
    return 0;
}

void ssl_init(){
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
}

SSL_CTX *create_context(){
    const SSL_METHOD *method;
    SSL_CTX *_ctx;

    method = TLS_server_method();

    _ctx = SSL_CTX_new(method);
    if (!_ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return _ctx;
}

void configure_context(SSL_CTX *_ctx){
    if (SSL_CTX_use_certificate_file(_ctx, "cert.pem", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(_ctx, "key.pem", SSL_FILETYPE_PEM) <= 0 ) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
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

void send_message(SSL *ssl, string messages){
    int attempt = 0;
    while (SSL_write(ssl, messages.c_str(), messages.size()) < 0) { 
        attempt++;
        if (attempt == 10){
            cout << "[Sending failed]\n";
            SSL_shutdown(ssl);
            SSL_free(ssl);
            exit(1);
        }
    }
}

void *handle_client(void *arg){
    job data = *(job *)arg;
    int client_sockfd = data.sockfd;
    string ip = data.ip;
    string User = "";

    // SSL connection
    pthread_mutex_lock(&thread_pool.mutex);
    
    SSL *ssl = SSL_new(ctx);

    pthread_mutex_unlock(&thread_pool.mutex);
    
    SSL_set_fd(ssl, client_sockfd);
    if (SSL_accept(ssl) <= 0) { // if failed, close the connection
        cout << "[SSL accept failed]\n";
        close(client_sockfd);
        pthread_exit(NULL);
    }
    cout << "[SSL connection established]\n";

    SSL *file_ssl, *audio_ssl;
    while (1){
        // recieve
        string buffer = recv_message(ssl);
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
                        User = username;
                        send_message(ssl, "Success");
                        cout << "[User: " << username <<  " login success]\n";
                        success = true;

                        pthread_mutex_lock(&thread_pool.mutex);
                        user[i].sockfd = client_sockfd;
                        
                        user[i].ssl = ssl; // maybe login from another ip
                        user[i].online = true;
                        online_users.insert(username);

                        // chat connection
                        sockaddr_in chat_client_addr;
                        socklen_t chat_client_addr_len = sizeof(chat_client_addr);
                        int chat_sockfd;
                        while (1){    
                            chat_sockfd = accept(server_sockfd, (sockaddr *) &chat_client_addr, &chat_client_addr_len);
                            if (chat_sockfd > 0) { // retry every 2 seconds
                                break;
                            }
                            sleep(2);
                        }
                        user[i].chat_sockfd = chat_sockfd;
                        user[i].chatting_ssl = SSL_new(ctx);
                        SSL_set_fd(user[i].chatting_ssl, chat_sockfd);
                        if (SSL_accept(user[i].chatting_ssl) <= 0) { // if failed, close the connection
                            cout << "[SSL accept failed]\n";
                            close(chat_sockfd);
                            pthread_exit(NULL);
                        }
                        
                        cout << "[Client " << inet_ntoa(chat_client_addr.sin_addr) << "'s chat connection is accepted]\n";

                        // file connection
                        sockaddr_in file_client_addr;
                        socklen_t file_client_addr_len = sizeof(file_client_addr);
                        int file_sockfd;
                        while (1){    
                            file_sockfd = accept(file_transfer_sockfd, (sockaddr *) &file_client_addr, &file_client_addr_len);
                            if (file_sockfd > 0) { // retry every 2 seconds
                                break;
                            }
                        }
                        user[i].file_sockfd = file_sockfd;
                        user[i].file_ssl = SSL_new(ctx);
                        file_ssl = user[i].file_ssl;
                        SSL_set_fd(user[i].file_ssl, file_sockfd);
                        if (SSL_accept(user[i].file_ssl) <= 0) { // if failed, close the connection
                            cout << "[SSL accept failed]\n";
                            close(file_sockfd);
                            pthread_exit(NULL);
                        }

                        cout << "[Client " << inet_ntoa(chat_client_addr.sin_addr) << "'s file transfer connection is accepted]\n";
                        
                        // audio connection
                        sockaddr_in audio_client_addr;
                        socklen_t audio_client_addr_len = sizeof(audio_client_addr);
                        int audio_sockfd;
                        while (1){    
                            audio_sockfd = accept(audio_transfer_sockfd, (sockaddr *) &audio_client_addr, &audio_client_addr_len);
                            if (audio_sockfd > 0) { // retry every 2 seconds
                                break;
                            }
                        }
                        user[i].audio_sockfd = audio_sockfd;
                        user[i].audio_ssl = SSL_new(ctx);
                        audio_ssl = user[i].audio_ssl;
                        SSL_set_fd(user[i].audio_ssl, audio_sockfd);
                        if (SSL_accept(user[i].audio_ssl) <= 0) { // if failed, close the connection
                            cout << "[SSL accept failed]\n";
                            close(audio_sockfd);
                            pthread_exit(NULL);
                        }

                        cout << "[Client " << inet_ntoa(chat_client_addr.sin_addr) << "'s audio transfer connection is accepted]\n";
                        pthread_mutex_unlock(&thread_pool.mutex);
                        break;
                    }
                }
                if (!success){
                    send_message(ssl, "Failed");
                    cout << "[User: " << username <<  " login failed]\n";
                }
            }
            else{ // register
                if (user_count > 4096){ // too many users
                    send_message(ssl, "Failed");
                    cout << "[User full]\n";
                    continue;
                }
                bool success = true;
                for (int i = 0; i < user_count; ++i){
                    if (user[i].username == username){
                        send_message(ssl, "Failed");
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
                user[user_count].ssl = ssl;
                user[user_count].online = false; // hasn't login
                user_count++;
                pthread_mutex_unlock(&thread_pool.mutex);
                
                send_message(ssl, "Success");
                cout << "[User: " << username <<  " register success]\n";
            }
        }
        else if (buffer == "@logout@"){
            string name;
            for (int i = 0; i < user_count; ++i){
                if (user[i].sockfd == client_sockfd){
                    pthread_mutex_lock(&thread_pool.mutex);
                    user[i].online = false;
                    name = user[i].username;
                    online_users.erase(name);
                    
                    SSL_shutdown(user[i].chatting_ssl);
                    SSL_free(user[i].chatting_ssl);
                    close(user[i].chat_sockfd);

                    SSL_shutdown(user[i].file_ssl);
                    SSL_free(user[i].file_ssl);
                    close(user[i].file_sockfd);
                    
                    SSL_shutdown(user[i].audio_ssl);
                    SSL_free(user[i].audio_ssl);
                    close(user[i].audio_sockfd);
                    
                    pthread_mutex_unlock(&thread_pool.mutex);
                    break;
                }
            }
            User = "";
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
            send_message(ssl, res);
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
                send_message(ssl, "Success");
                cout << "[" << name << " found]\n";
            }
            else{
                send_message(ssl, "Failed");
                cout << "[" << name << " not found]\n";
            }
        }
        else if (buffer[0] == '#'){
            string name = "", username = "";
            for (int i = 1; buffer[i] != '#'; ++i)
                username += buffer[i];
            for (int i = username.size() + 2; buffer[i] != ':'; ++i)
                name += buffer[i];
            
            SSL *ssl_to_send;
            bool success = false;
            for (int i = 0; i < user_count; ++i){
                if (user[i].username == name){
                    ssl_to_send = user[i].chatting_ssl;
                    success = true;
                    break;
                }
            }
            if (success == false){
                send_message(ssl, "Failed");
                cout << "[User not found]\n";
            }
            else{
                send_message(ssl, "Success");

                pthread_mutex_lock(&thread_pool.mutex);
                send_message(ssl_to_send, buffer);
                pthread_mutex_unlock(&thread_pool.mutex);
                
                cout << "[Message sent]\n";
            }
        }
        else if (buffer[0] == '%'){ // file transfer
            // %from#to$file_name@file_size
            string name = "", username = "", file_name = "", _file_size = "";
            long file_size = 0;
            for (int i = 1; buffer[i] != '#'; ++i)
                username += buffer[i];
            for (int i = username.size() + 2; buffer[i] != '$'; ++i)
                name += buffer[i];
            for (int i = name.size() + username.size() + 3; buffer[i] != '@'; ++i)
                file_name += buffer[i];
            for (int i = file_name.size() + name.size() + username.size() + 4; i < bytes; ++i)
                _file_size += buffer[i];
            file_size = stol(_file_size);
            
            bool success = false;
            for (int i = 0; i < user_count; ++i){
                if (user[i].username == name){
                    success = true;
                    break;
                }
            }

            if (success == false){
                send_message(ssl, "Failed");
                cout << "[User not found]\n";
            }
            else{
                send_message(ssl, "Success"); // tell client ready to receive

                // receive file and store
                FILE *file = fopen(file_name.c_str(), "wb");
                if (file == NULL){
                    cout << "[File open failed]\n";
                    continue;
                }

                while (file_size > 0){
                    string _buffer = recv_message(ssl);
                    fwrite(_buffer.c_str(), 1, _buffer.size(), file);
                    send_message(ssl, "Success");
                    file_size -= _buffer.size();
                }
                
                fclose(file);
                pthread_mutex_lock(&thread_pool.mutex);
                files[name].push_back(buffer);
                pthread_mutex_unlock(&thread_pool.mutex);
            }
        }
        else if (buffer[0] == '&'){ // &from#to$audio_name@audio_size
            string name = "", username = "", audio_name = "", _audio_size = "";
            long audio_size = 0;
            for (int i = 1; buffer[i] != '#'; ++i)
                username += buffer[i];
            for (int i = username.size() + 2; buffer[i] != '$'; ++i)
                name += buffer[i];
            for (int i = name.size() + username.size() + 3; buffer[i] != '@'; ++i)
                audio_name += buffer[i];
            for (int i = audio_name.size() + name.size() + username.size() + 4; i < bytes; ++i)
                _audio_size += buffer[i];
            audio_size = stol(_audio_size);

            bool success = false;
            for (int i = 0; i < user_count; ++i){
                if (user[i].username == name){
                    success = true;
                    break;
                }
            }
            
            if (success == false){
                send_message(ssl, "Failed");
                cout << "[User not found]\n";
            }
            else{
                send_message(ssl, "Success"); // tell client ready to receive

                // receive audio and store
                FILE *file = fopen(audio_name.c_str(), "wb");
                if (file == NULL){
                    cout << "[Audio open failed]\n";
                    continue;
                }

                while (audio_size > 0){
                    string _buffer = recv_message(ssl);
                    fwrite(_buffer.c_str(), 1, _buffer.size(), file);
                    send_message(ssl, "Success");
                    audio_size -= _buffer.size();
                }
                
                fclose(file);
                pthread_mutex_lock(&thread_pool.mutex);
                audios[name].push_back(buffer); // &from#to$audio_name@audio_size
                pthread_mutex_unlock(&thread_pool.mutex);
            }
        }
        else if (buffer == "@file@"){ // send file to receiver
            while (!files[User].empty()){
                string _file = files[User].back();
                files[User].pop_back();
                send_message(file_ssl, _file);
                string res = recv_message(file_ssl);
                if (res != "Success"){
                    cout << "[File transfer failed]\n";
                    break;
                }

                string name = "", username = "", file_name = "", _file_size = "";
                long file_size = 0;
                for (int i = 1; _file[i] != '#'; ++i)
                    name += _file[i];
                for (int i = name.size() + 2; _file[i] != '$'; ++i)
                    username += _file[i];
                for (int i = name.size() + username.size() + 3; _file[i] != '@'; ++i)
                    file_name += _file[i];
                for (int i = file_name.size() + name.size() + username.size() + 4; i < (int)_file.size(); ++i)
                    _file_size += _file[i];
                file_size = stol(_file_size);

                FILE *file = fopen(file_name.c_str(), "rb");
                if (file == NULL){
                    cout << "[File not found]\n";
                    continue;
                }
                char _buffer[2048] = {0};

                while (file_size > 0){
                    int _bytes = fread(_buffer, 1, sizeof(_buffer), file);
                    int attempt = 0;
                    while (1){
                        send_message(file_ssl, string(_buffer, _bytes));
                        res = recv_message(file_ssl);
                        if (res == "Success"){
                            memset(_buffer, 0, sizeof(_buffer));
                            break;
                        }
                        else{
                            attempt++;
                            if (attempt == 10){
                                cout << "[Transfer file failed]\n";
                                pthread_exit(NULL);
                            }
                        }
                    }
                    file_size -= _bytes;
                }
            }
            send_message(file_ssl, "No more files");
        }
        else if (buffer == "@audio@"){ // send audio to receiver
            while (!audios[User].empty()){
                string _audio = audios[User].back();
                audios[User].pop_back();
                send_message(audio_ssl, _audio);
                string res = recv_message(audio_ssl);
                if (res != "Success"){
                    cout << "[Audio transfer failed]\n";
                    break;
                }

                string name = "", username = "", audio_name = "";
                long audio_size = 0;
                for (int i = 1; _audio[i] != '#'; ++i)
                    name += _audio[i];
                for (int i = name.size() + 2; _audio[i] != '$'; ++i)
                    username += _audio[i];
                for (int i = name.size() + username.size() + 3; _audio[i] != '@'; ++i)
                    audio_name += _audio[i];

                FILE *file = fopen(audio_name.c_str(), "rb");
                if (file == NULL){
                    cout << "[Audio file not found]\n";
                    continue;
                }

                SDL_AudioSpec wav_spec;
                Uint32 wav_length;
                Uint8 *wav_buffer;

                if (SDL_LoadWAV(audio_name.c_str(), &wav_spec, &wav_buffer, &wav_length) == NULL){
                    cout << "[Audio file not found]\n";
                    continue;
                }
                
                while (audio_size < wav_length){
                    int _bytes = min(2048, (int)(wav_length - audio_size));
                    int attempt = 0;
                    while (1){
                        send_message(audio_ssl, string((char *)(wav_buffer + audio_size) , _bytes));
                        res = recv_message(audio_ssl);
                        if (res == "Success")
                            break;
                        else{
                            attempt++;
                            if (attempt == 10){
                                cout << "[Transfer audio failed]\n";
                                pthread_exit(NULL);
                            }
                        }
                    }
                    audio_size += _bytes;
                }
                send_message(audio_ssl, "Finished");

                SDL_FreeWAV(wav_buffer);
            }
            send_message(audio_ssl, "Nomoreaudios");
        }
    }
    
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_sockfd);
    pthread_exit(NULL);
}

void thread_init(){
    pthread_mutex_init(&thread_pool.mutex, NULL);
    thread_pool.thread_count = 0;
    pthread_attr_init(&thread_pool.client_attr);
    pthread_attr_setdetachstate(&thread_pool.client_attr, PTHREAD_CREATE_DETACHED);
}

