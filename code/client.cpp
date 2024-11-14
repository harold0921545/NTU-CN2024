#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define debug(x) cerr << #x << " = " << x << '\n'

using namespace std;

int socket_init();
void send_message(int sockfd, string messages);
string recv_message(int sockfd);
void connect_server(int sockfd, char *ip, char *port);


int main(int argc, char *argv[]) { // argv[1]: IP address of the server, argv[2]: port number
    if (argc != 3) {
        cout << "Usage: " << argv[0] << " <IP> <Port>\n";
        return 1;
    }

    string messages, res;
    int sockfd = socket_init();

    // connect
    connect_server(sockfd, argv[1], argv[2]);

    // Login or Register
    while (1){
        cout << "1. Register\n";
        cout << "2. Login\n";
        cout << "Selection: ";
        string select;
        getline(cin, select);

        if (select.size() != 1 || select[0] < '1' || select[0] > '2') {
            cout << "[Invalid selection]\n";
            continue;
        }
        int selection = select[0] - '0';
        if (selection == 1){
            string username, password, msg;
            cout << "Username: ";
            getline(cin, username);
            cout << "Password: ";
            getline(cin, password);
            if (username.empty() || password.empty()) {
                cout << "Empty username or password\n";
                continue;
            }

            msg = "/$" + username + "/#" + password;
            send_message(sockfd, msg);
            res = recv_message(sockfd);
            if (res == "Success")
                cout << "[Register success]\n";
            else
                cout << "[Register failed]\n";
        }
        else if (selection == 2){
            string username, password, msg;
            cout << "Username: ";
            getline(cin, username);
            cout << "Password: ";
            getline(cin, password);
            if (username.empty() || password.empty()) {
                cout << "Empty username or password\n";
                continue;
            }

            msg = "/$$" + username + "/#" + password;
            send_message(sockfd, msg);
            res = recv_message(sockfd);
            if (res == "Success"){
                cout << "[Login success]\n";
                break;
            }
            else
                cout << "[Login failed]\n";
        }
    }
    // Login success
    while (1){
        cout << "1. Send message\n";
        cout << "2. Logout\n";
        cout << "Selection: ";
        string select;
        getline(cin, select);

        if (select.size() != 1 || select[0] < '1' || select[0] > '2') {
            cout << "[Invalid selection]\n";
            continue;
        }
        int selection = select[0] - '0';

        if (selection == 1){
            // send message
            cout << "Message: ";
            getline(cin, messages);
            if (messages.empty()) {
                cout << "Empty message\n";
                continue;
            }
            send_message(sockfd, messages);

            // receive respond
            res = recv_message(sockfd);
            cout << "Respond: " << res << '\n';
        }
        else if (selection == 2){
            // logout
            send_message(sockfd, "@logout@");
            cout << "[Logout]\n";
            break;
        }
    }
    close(sockfd);
    return 0;
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
    cout << "Sent\n";
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

void connect_server(int sockfd, char *ip, char *port){
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(port));
    server_addr.sin_addr.s_addr = inet_addr(ip);

    if (connect(sockfd, (sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        cout << "[Connecting failed]\n";
        close(sockfd);
        exit(1);
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