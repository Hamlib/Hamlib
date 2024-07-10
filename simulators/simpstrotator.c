#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_PORT 12001
#define REPLY_PORT 12002
#define BUFFER_SIZE 1024

void handle_receive(int recv_sockfd, struct sockaddr_in *client_addr, socklen_t addr_len, char *buffer) {
    ssize_t recv_len;
    recv_len = recvfrom(recv_sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)client_addr, &addr_len);
    if (recv_len < 0) {
        perror("recvfrom");
        close(recv_sockfd);
        exit(EXIT_FAILURE);
    }

    buffer[recv_len] = '\0'; // Null-terminate the received string
    printf("Received packet from %s:%d\n", inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
    printf("Data: %s\n", buffer);
}

void handle_send(int send_sockfd, struct sockaddr_in *client_addr, socklen_t addr_len, const char *message) {
    ssize_t sent_len;
    sent_len = sendto(send_sockfd, message, strlen(message), 0, (struct sockaddr *)client_addr, addr_len);
    if (sent_len < 0) {
        perror("sendto");
        close(send_sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Sent reply to %s:%d from port %d\n", inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port), REPLY_PORT);
}

int main() {
    int recv_sockfd, reply_sockfd;
    struct sockaddr_in server_addr, reply_addr, client_addr;
    char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(client_addr);

    // Create a UDP socket for receiving on SERVER_PORT
    if ((recv_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Configure the server address for receiving
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    // Bind the receiving socket to the server address
    if (bind(recv_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(recv_sockfd);
        exit(EXIT_FAILURE);
    }

    // Create a UDP socket for replying on REPLY_PORT
    if ((reply_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        close(recv_sockfd);
        exit(EXIT_FAILURE);
    }

    // Configure the reply address for sending
    memset(&reply_addr, 0, sizeof(reply_addr));
    reply_addr.sin_family = AF_INET;
    reply_addr.sin_addr.s_addr = INADDR_ANY;
    reply_addr.sin_port = htons(REPLY_PORT);

    // Bind the reply socket to the reply address
    if (bind(reply_sockfd, (struct sockaddr *)&reply_addr, sizeof(reply_addr)) < 0) {
        perror("bind");
        close(recv_sockfd);
        close(reply_sockfd);
        exit(EXIT_FAILURE);
    }

    printf("UDP server is listening on port %d for incoming packets and on port %d for replies\n", SERVER_PORT, REPLY_PORT);

    while (1) {
        // Handle incoming packets on SERVER_PORT
        handle_receive(recv_sockfd, &client_addr, addr_len, buffer);

        // Send "OK" reply to the client from REPLY_PORT
        handle_send(reply_sockfd, &client_addr, addr_len, "OK");
    }

    // Close the sockets
    close(recv_sockfd);
    close(reply_sockfd);

    return 0;
}

