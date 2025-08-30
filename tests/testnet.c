/* Test network address variations in hamlib */
#include <stdlib.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>

#include <hamlib/config.h>

#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif

#ifdef HAVE_NETDB_H
#  include <netdb.h>
#endif

#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif

#include "../src/misc.h"

static int rig_network_addr(char *hoststr, char *portstr)
{

    struct in6_addr serveraddr;
    struct addrinfo hints, *res;
    int status;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;


    status = getaddrinfo(hoststr, portstr, &hints, &res);

    if (status == 0 && res->ai_family == AF_INET6)
    {
        printf("Using IPV6 for %s:%s\n", hoststr, portstr);
    }
    else if (status == 0)
    {
        printf("Using IPV4 for %s:%s\n", hoststr, portstr);
    }

    if (status != 0)
    {
        printf("%s: cannot get host \"%s\": %s\n",
               __func__,
               hoststr,
               gai_strerror(errno));
        return 1;
    }

    status = inet_pton(AF_INET, hoststr, &serveraddr);

    if (status != 1) /* not valid IPv4 address, maybe IPV6? */
    {
        status = inet_pton(AF_INET6, hoststr, &serveraddr);

        if (status != 1) /* nope */
        {
            return 1;
        }
    }

    return 0;
}

static int test_host(char *hoststr, char *host, char *port)
{
    int status;
    char host2[256], port2[6];
    status = parse_hoststr(hoststr, strlen(hoststr), host2, port2);

    printf("---------------------\n");

    if (status == 0)
    {
        if (strcmp(host, host2) || strcmp(port, port2))
        {
            printf("%s: Mismatch, expected host=%s, port=%s, got host=%s, port=%s\n",
                   __func__,
                   host, port, host2, port2);
            return -1;
        }
        else
        {
            rig_network_addr(host, port);
        }

        printf("%s: %s, host=%s, port=%s\n", __func__, hoststr, host, port);
        return 0;
    }
    else
    {
        printf("%s: ERROR!! %s, host=%s, port=%s\n", __func__, hoststr, host, port);
        return -1;
    }
}

int
main(int argc, const char *argv[])
{
    // IPV4
    test_host("127.0.0.1", "127.0.0.1", "");
    test_host("127.0.0.1:4532", "127.0.0.1", "4532");
    test_host("192.168.1.1", "192.168.1.1", "");
    test_host("192.168.1.1:4532", "192.168.1.1", "4532");
    test_host("mdblack-VirtualBox", "mdblack-VirtualBox", "");
    test_host("mdblack-VirtualBox:4532", "mdblack-VirtualBox", "4532");
    test_host("localhost", "localhost", "");
    test_host("localhost:4532", "localhost", "4532");
    // IPV6 with brackets
    test_host("[fe80::e034:55ef:ce2a:dc83]", "fe80::e034:55ef:ce2a:dc83", "");
    test_host("[fe80::e034:55ef:ce2a:dc83]:4532", "fe80::e034:55ef:ce2a:dc83",
              "4532");

    test_host("fe80::e034:55ef:ce2a:dc83", "fe80::e034:55ef:ce2a:dc83", "");
    test_host("fe80::e034:55ef:ce2a:dc83:4532", "fe80::e034:55ef:ce2a:dc83",
              "4532");
    test_host("fe80:e034:55ef:ce2a:dc83:1234:5678:9abc",
              "fe80:e034:55ef:ce2a:dc83:1234:5678:9abc", "");
    test_host("fe80:e034:55ef:ce2a:dc83:1234:5678:9abc:4532",
              "fe80:e034:55ef:ce2a:dc83:1234:5678:9abc",
              "4532");
    test_host("::1", "::1", "");
    test_host("::1:4532", "::1", "4532");

#if 1 // server side addresses with  IPV6
    test_host("fe80::e034:55ef:ce2a:dc83%eth0", "fe80::e034:55ef:ce2a:dc83%eth0",
              "");
    test_host("[fe80::e034:55ef:ce2a:dc83%1]", "fe80::e034:55ef:ce2a:dc83%1", "");
    test_host("[fe80::e034:55ef:ce2a:dc83%1]:4532", "fe80::e034:55ef:ce2a:dc83%1",
              "4532");
    test_host("fe80::e034:55ef:ce2a:dc83%eth0:4532",
              "fe80::e034:55ef:ce2a:dc83%eth0", "4532");
    test_host("fe80::e034:55ef:ce2a:dc83%1", "fe80::e034:55ef:ce2a:dc83%1", "");
    test_host("fe80::e034:55ef:ce2a:dc83%1:4532", "fe80::e034:55ef:ce2a:dc83%1",
              "4532");
    test_host("[fe80::e034:55ef:ce2a:dc83%eth0]",
              "fe80::e034:55ef:ce2a:dc83%eth0", "");
    test_host("[fe80::e034:55ef:ce2a:dc83%eth0]:4532",
              "fe80::e034:55ef:ce2a:dc83%eth0", "4532");
#endif

    return 0;
}
