//include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
//#include <errno.h>
//#include <unistd.h>
#include <hamlib/rig.h>
//#include <sys/socket.h>
#include <netinet/in.h>

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifndef MULTICAST_H
#define MULTICAST_H
#if 0 // moved to rig.h
struct multicast_s
{
    int multicast_running;
    int sock;
    int seqnumber;
    int runflag; // = 0;
    pthread_t threadid;
#ifdef HAVE_ARPA_INET_H
    struct ip_mreq mreq; // = {0};
    struct sockaddr_in dest_addr; // = {0};
#endif
};
#endif

struct multicast_vfo
{
    char *name;
    double freq;
    char *mode;
    int width;
    int widthLower;
    int widthUpper;
    unsigned char rx; // true if in rx mode
    unsigned char tx; // true in in tx mode
};

struct multicast_broadcast
{
    char *ID;
    struct multicast_vfo **vfo;
};

// returns # of bytes sent
int multicast_init(RIG *rig, char *addr, int port);
int multicast_send(RIG *rig, unsigned char *msg, int msglen);
#endif //MULTICAST_H
