#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
//#pragma does not work on mingw
//#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

#define MCAST_PORT 4532
#define MCAST_ADDR "224.0.0.1"
#define BUFFER_SIZE 4096

int main()
{
    int sock;
    struct sockaddr_in mcast_addr;
    char buffer[BUFFER_SIZE];
    int bytes_received;

#ifdef _WIN32
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        fprintf(stderr, "WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

#endif

    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }

    // Set the SO_REUSEADDR option to allow multiple processes to use the same address
    int optval = 1;

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&optval,
                   sizeof(optval)) < 0)
    {
        //rig_debug(RIG_DEBUG_ERR, "%s: setsockopt: %s\n", __func__, strerror(errno));
        //return -RIG_EIO;
        return 1;
    }


    memset(&mcast_addr, 0, sizeof(mcast_addr));
    mcast_addr.sin_family = AF_INET;
    mcast_addr.sin_port = htons(MCAST_PORT);
    mcast_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&mcast_addr, sizeof(mcast_addr)) < 0)
    {
        perror("bind() failed");
        exit(EXIT_FAILURE);
    }

    struct ip_mreq mreq;

    mreq.imr_multiaddr.s_addr = inet_addr(MCAST_ADDR);

    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq,
                   sizeof(mreq)) < 0)
    {
        perror("setsockopt() failed");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        bytes_received = recvfrom(sock, buffer, BUFFER_SIZE, 0, NULL, 0);

        if (bytes_received < 0)
        {
            perror("recvfrom() failed");
            break;
        }

        buffer[bytes_received] = '\0';
        printf("%s\n", buffer);
    }

    // Drop membership before closing the socket
    if (setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *)&mreq,
                   sizeof(mreq)) < 0)
    {
        perror("setsockopt() failed");
    }

    close(sock);
#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}

