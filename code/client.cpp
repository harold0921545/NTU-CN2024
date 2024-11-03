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

int main(int argc, char *argv[]) { // argv[1]: IP address of the server
    string messages;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); // IPv4, TCP, default
    if (sockfd < 0) {
        cout << "Creating socket failed\n";
        return 1;
    }
    cout << "Socket created\n";

    // connect
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080); // host-to-network short
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);

    if (connect(sockfd, (sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        cout << "Connecting failed\n";
        close(sockfd);
        return 1;
    }
    cout << "Connected\n";

    // send message
    cout << "Message: ";
    getline(cin, messages);
    if (messages.empty()) {
        cout << "Empty message\n";
        close(sockfd);
        return 1;
    }
    if (send(sockfd, messages.c_str(), messages.size(), 0) < 0) {
        cout << "Sending failed\n";
        close(sockfd);
        return 1;
    }
    cout << "Sent\n";

    // receive respond
    cout << "Respond: ";
    while (1){
        char buffer[1024] = {0};
        int bytes = recv(sockfd, buffer, sizeof(buffer), 0);
        if (bytes < 0) {
            cout << "Receiving failed\n";
            close(sockfd);
            return 1;
        }
        if (bytes == 0)
            break;
        cout << buffer;
        if (bytes < sizeof(buffer))
            break;
    }
    cout << '\n';

    close(sockfd);

    return 0;
}