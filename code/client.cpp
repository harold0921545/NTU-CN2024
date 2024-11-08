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

void print_selection();
int socket_init();
void send_message(int sockfd, string messages);
string recv_message(int sockfd);
void connect_server(int sockfd, char *ip);


int main(int argc, char *argv[]) { // argv[1]: IP address of the server
    string messages, res;
    int sockfd = socket_init();

    // connect
    connect_server(sockfd, argv[1]);

    // Login or Register
    while (1){
        print_selection();
        string select;
        getline(cin, select);

        if (select.size() != 1 || select[0] < '1' || select[0] > '3') {
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
            
            msg = "$" + username + "#" + password;
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

            msg = "$$" + username + "#" + password;
            send_message(sockfd, msg);
            res = recv_message(sockfd);
            if (res == "Success"){
                cout << "[Login success]\n";
                break;
            }
            else
                cout << "[Login failed]\n";
        }
        else if (selection == 3){ // for testing, need to remove
            break;
        }
    }

    // send message
    cout << "Message: ";
    getline(cin, messages);
    if (messages.empty()) {
        cout << "Empty message\n";
        close(sockfd);
        return 1;
    }
    send_message(sockfd, messages);

    // receive respond
    res = recv_message(sockfd);
    cout << "Respond: " << res << '\n';

    close(sockfd);
    return 0;
}


void print_selection(){
    cout << "1. Register\n";
    cout << "2. Login\n";
    cout << "Selection: ";
}

void send_message(int sockfd, string messages){
    if (send(sockfd, messages.c_str(), messages.size(), 0) < 0) {
        cout << "[Sending failed]\n";
        close(sockfd);
        exit(1);
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
        res += buffer;
        if (bytes == 0)
            break;
        if (bytes < sizeof(buffer))
            break;
    }
    return res;
}

void connect_server(int sockfd, char *ip){
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = inet_addr(ip);

    if (connect(sockfd, (sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        cout << "[Connecting failed]\n";
        close(sockfd);
        exit(0);
    }
    cout << "[Connected]\n";
}

int socket_init(){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); // IPv4, TCP, default
    if (sockfd < 0) {
        cout << "[Creating socket failed]\n";
        exit(0);
    }
    cout << "[Socket created]\n";
    return sockfd;
}