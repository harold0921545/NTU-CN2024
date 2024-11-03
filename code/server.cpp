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

int main(int argc, char *argv[]) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); // IPv4, TCP, default
    if (sockfd < 0) {
        cout << "Creating socket failed\n";
        return 1;
    }
    cout << "Socket created\n";
    // bind
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080); // host-to-network short
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // accept any IP address

    if (bind(sockfd, (sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        cout << "Binding socket failed\n";
        close(sockfd);
        return 1;
    }
    cout << "Socket bound\n";

    // listen
    if (listen(sockfd, 10) < 0) { // 10 is the maximum number of connections
        cout << "Listening failed\n";
        close(sockfd);
        return 1;
    }
    cout << "Listening\n";
    // accept
    while (1){
        sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_sockfd = accept(sockfd, (sockaddr *) &client_addr, &client_addr_len);
        if (client_sockfd < 0) {
            cout << "Accepting failed\n";
            close(sockfd);
            return 1;
        }
        cout << "Client " << inet_ntoa(client_addr.sin_addr) << "is accepted\n";

        // recieve
        while (1){
            char buffer[1024];
            int bytes = read(client_sockfd, buffer, sizeof(buffer));
            if (bytes < 0) {
                cout << "Reading failed\n";
                close(client_sockfd);
                close(sockfd);
                return 1;
            }
            if (bytes == 0){
                cout << "Disconnected\n";
                break;
            }
            cout << "Received: " << buffer << '\n';

            // respond
            // flip the case
            for (int i = 0; i < bytes; i++){
                if (islower(buffer[i]))
                    buffer[i] = toupper(buffer[i]);
                else if (isupper(buffer[i]))
                    buffer[i] = tolower(buffer[i]);
            }
            while (send(client_sockfd, buffer, bytes, 0) < 0); // if client not received, try again
            cout << "Sent: " << buffer << '\n';
        }

        // TODO: pthread
        close(client_sockfd);
    }
    close(sockfd);
    return 0;
}