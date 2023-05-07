#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MULTICAST_ADDR "224.0.0.1"
#define PORT 4532
#define BUFFER_SIZE 1024

int main()
{
    // Create a UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set the SO_REUSEADDR option to allow multiple processes to use the same address
    int optval = 1;

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
    {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to any available local address and the specified port
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Construct the multicast group address
    struct ip_mreq mreq = {0};
    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_ADDR);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    // Join the multicast group
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Receive multicast packets in a loop
    while (1)
    {
        char buffer[BUFFER_SIZE];
        struct sockaddr_in src_addr = {0};
        socklen_t addrlen = sizeof(src_addr);
        ssize_t num_bytes = recvfrom(sock, buffer, BUFFER_SIZE - 1, 0,
                                     (struct sockaddr *)&src_addr, &addrlen);

        if (num_bytes < 0)
        {
            perror("recvfrom failed");
            exit(EXIT_FAILURE);
        }

        buffer[num_bytes] = '\0';
        printf("Received %zd bytes from %s:%d\n%s\n", num_bytes,
               inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port), buffer);
    }

    // Leave the multicast group
    if (setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Close the socket
    close(sock);

    return 0;
}
