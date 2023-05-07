#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include "hamlib/rig.h"
#include "misc.h"
#include "multicast.h"

#define RIG_MULTICAST_ADDR "224.0.0.1"
#define RIG_MULTICAST_PORT 4532

#if 0
static int multicast_running;
static int sock;
static int seqnumber;
static int runflag = 0;
static struct ip_mreq mreq = {0};
static pthread_t threadid;
static struct sockaddr_in dest_addr = {0};
#endif


static int multicast_status_changed(RIG *rig)
{
    int retval;
#if 0
    freq_t freq, freqsave = rig->state.cache.freqMainA;

    if ((retval = rig_get_freq(rig, RIG_VFO_A, &freq)) != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: rig_get_freq:%s\n", __func__, rigerror(retval));
    }

    if (freq != freqsave) { return 1; }

#endif

    rmode_t mode, modesave = rig->state.cache.modeMainA;
    pbwidth_t width, widthsave = rig->state.cache.widthMainA;

    if (rig->state.multicast->seqnumber % 2 == 0
            && (retval = rig_get_mode(rig, RIG_VFO_A, &mode, &width)) != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: rig_get_freq:%s\n", __func__, rigerror(retval));
    }

    if (mode != modesave) { return 1; }

    if (width != widthsave) { return 1; }

    ptt_t ptt, pttsave = rig->state.cache.ptt;

    if (rig->state.multicast->seqnumber % 2 == 0
            && (retval = rig_get_ptt(rig, RIG_VFO_A, &ptt)) != RIG_OK)
        if (ptt != pttsave) { return 1; }

    split_t split, splitsave = rig->state.cache.split;
    vfo_t txvfo;

    if (rig->state.multicast->seqnumber % 2 == 0
            && (retval = rig_get_split_vfo(rig, RIG_VFO_A, &split, &txvfo)) != RIG_OK)
        if (split != splitsave) { return 1; }

    return 0;
}

void json_add_string(char *msg, const char *key, const char *value)
{
    if (strlen(msg) != 0)
    {
        strcat(msg, ",\n");
    }

    strcat(msg, "\"");
    strcat(msg, key);
    strcat(msg, "\": ");
    strcat(msg, "\"");
    strcat(msg, value);
    strcat(msg, "\"");
}

void json_add_int(char *msg, const char *key, const int number)
{
    if (strlen(msg) != 0)
    {
        strcat(msg, ",\n");
    }

    strcat(msg, "\"");
    strcat(msg, key);
    strcat(msg, "\": ");
    char tmp[64];
    sprintf(tmp, "%d", number);
    strcat(msg, tmp);
}

void json_add_double(char *msg, const char *key, const double value)
{
    if (strlen(msg) != 0)
    {
        strcat(msg, ",\n");
    }

    strcat(msg, "\"");
    strcat(msg, key);
    strcat(msg, "\": ");
    char tmp[64];
    sprintf(tmp, "%f", value);
    strcat(msg, tmp);
}

void json_add_boolean(char *msg, const char *key, const int value)
{
    if (strlen(msg) != 0)
    {
        strcat(msg, ",\n");
    }

    strcat(msg, "\"");
    strcat(msg, key);
    strcat(msg, "\": ");
    char tmp[64];
    sprintf(tmp, "%s", value == 0 ? "False" : "True");
    strcat(msg, tmp);
}

void json_add_time(char *msg)
{
    char mydate[256];
    date_strget(mydate, sizeof(mydate), 0);

    if (strlen(msg) != 0)
    {
        strcat(msg, ",\n");
    }

    strcat(msg, "\"Time\": ");
    strcat(msg, mydate);
}

void json_add_vfoA(RIG *rig, char *msg)
{
    strcat(msg, "{\n");
    json_add_string(msg, "Name", "VFOA");
    json_add_int(msg, "Freq", rig->state.cache.freqMainA);

    if (strlen(rig_strrmode(rig->state.cache.modeMainA)) > 0)
    {
        json_add_string(msg, "Mode", rig_strrmode(rig->state.cache.modeMainA));
    }

    if (rig->state.cache.widthMainA > 0)
    {
        json_add_int(msg, "Width", rig->state.cache.widthMainA);
    }

    json_add_boolean(msg, "RX", !rig->state.cache.ptt);
    json_add_boolean(msg, "TX", rig->state.cache.ptt);
    strcat(msg, "\n}\n");
}

void json_add_vfoB(RIG *rig, char *msg)
{
    strcat(msg, "{\n");
    json_add_string(msg, "Name", "VFOB");
    json_add_int(msg, "Freq", rig->state.cache.freqMainB);

    if (strlen(rig_strrmode(rig->state.cache.modeMainB)) > 0)
    {
        json_add_string(msg, "Mode", rig_strrmode(rig->state.cache.modeMainB));
    }

    if (rig->state.cache.widthMainB > 0)
    {
        json_add_int(msg, "Width", rig->state.cache.widthMainB);
    }

    json_add_boolean(msg, "RX", !rig->state.cache.ptt);
    json_add_boolean(msg, "TX", rig->state.cache.ptt);
    strcat(msg, "\n},\n");
}

static int multicast_send_json(RIG *rig)
{
    char msg[8192]; // could be pretty big
    char buf[4096];
//    sprintf(msg,"%s:f=%.1f", date_strget(msg, (int)sizeof(msg), 0), f);
    msg[0] = 0;
    snprintf(buf, sizeof(buf), "%s:%s", rig->caps->model_name,
             rig->state.rigport.pathname);
    json_add_string(msg, "ID", buf);
    json_add_time(msg);
    json_add_int(msg, "Sequence", rig->state.multicast->seqnumber++);
    json_add_string(msg, "VFOCurr", rig_strvfo(rig->state.current_vfo));
    json_add_int(msg, "Split", rig->state.cache.split);
    strcat(msg, ",\n\"VFOs\": [\n{\n");
    json_add_vfoA(rig, msg);

    // send the thing
    multicast_send(rig, msg, strlen(msg));
    return 0;
}

void *multicast_thread(void *vrig)
{
    int retval;
    RIG *rig = (RIG *)vrig;
    rig_debug(RIG_DEBUG_TRACE, "%s: multicast_thread started\n", __func__);

    // do the 1st packet all the time
    multicast_status_changed(rig);
    multicast_send_json(rig);
    int loopcount = 4;

    while (rig->state.multicast->runflag)
    {
        hl_usleep(100 * 1000);
        freq_t freq, freqsave = 0;

        if ((retval = rig_get_freq(rig, RIG_VFO_A, &freq)) != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: rig_get_freq:%s\n", __func__, rigerror(retval));
        }

        if (freq != freqsave || loopcount-- == 0)
        {
            multicast_status_changed(rig);
            multicast_send_json(rig);
            loopcount = 4;
        }

    }

    return NULL;
}

int multicast_init(RIG *rig, char *addr, int port)
{
    if (rig->state.multicast == NULL)
    {
        rig->state.multicast = calloc(1, sizeof(struct multicast_s));
    }
    else if (rig->state.multicast->multicast_running) { return RIG_OK; } // we only need one port

    //rig->state.multicast->mreq = {0};

    if (addr == NULL) { addr = RIG_MULTICAST_ADDR; }

    if (port == 0) { port = RIG_MULTICAST_PORT; }

    // Create a UDP socket
    rig->state.multicast->sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (rig->state.multicast->sock < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: socket: %s\n", __func__, strerror(errno));
        return -RIG_EIO;
    }

    // Set the SO_REUSEADDR option to allow multiple processes to use the same address
    int optval = 1;

    if (setsockopt(rig->state.multicast->sock, SOL_SOCKET, SO_REUSEADDR, (char*)&optval,
                   sizeof(optval)) < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: setsockopt: %s\n", __func__, strerror(errno));
        return -RIG_EIO;
    }

    // Bind the socket to any available local address and the specified port
    struct sockaddr_in saddr = {0};
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(port);

    if (bind(rig->state.multicast->sock, (struct sockaddr *)&saddr,
             sizeof(saddr)) < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: bind: %s\n", __func__, strerror(errno));
        return -RIG_EIO;
    }

    // Construct the multicast group address
    // struct ip_mreq mreq = {0};
    rig->state.multicast->mreq.imr_multiaddr.s_addr = inet_addr(addr);
    rig->state.multicast->mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    // Set the multicast TTL (time-to-live) to limit the scope of the packets
    char ttl = 1;

    if (setsockopt(rig->state.multicast->sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl,
                   sizeof(ttl)) < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: setsockopt: %s\n", __func__, strerror(errno));
        return -RIG_EIO;
    }

    // Join the multicast group
    if (setsockopt(rig->state.multicast->sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   (char*)&rig->state.multicast->mreq, sizeof(rig->state.multicast->mreq)) < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: setsockopt: %s\n", __func__, strerror(errno));
        return -RIG_EIO;
    }

    // prime the dest_addr for the send routine
    rig->state.multicast->dest_addr.sin_family = AF_INET;
    rig->state.multicast->dest_addr.sin_addr.s_addr = inet_addr(addr);
    rig->state.multicast->dest_addr.sin_port = htons(port);

    printf("starting thread\n");

    rig->state.multicast->runflag = 1;
    pthread_create(&rig->state.multicast->threadid, NULL, multicast_thread,
                   (void *)rig);
    //printf("threadid=%ld\n", rig->state.multicast->threadid);
    rig->state.multicast->multicast_running = 1;
    return RIG_OK;
}

void multicast_close(RIG *rig)
{
    int retval;

    // Leave the multicast group
    if ((retval = setsockopt(rig->state.multicast->sock, IPPROTO_IP,
                             IP_DROP_MEMBERSHIP, (char*)&rig->state.multicast->mreq,
                             sizeof(rig->state.multicast->mreq))) < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: setsockopt: %s\n", __func__, strerror(errno));
        return;
    }

    // Close the socket
    if ((retval = close(rig->state.multicast->sock)))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: close: %s\n", __func__, strerror(errno));
    }
}

// if msglen=0 msg is assumed to be a string
int multicast_send(RIG *rig, const char *msg, int msglen)
{
    // Construct the message to send
    if (msglen == 0) { msglen = strlen((char *)msg); }

    // Send the message to the multicast group
    ssize_t num_bytes = sendto(rig->state.multicast->sock, msg, msglen, 0,
                               (struct sockaddr *)&rig->state.multicast->dest_addr,
                               sizeof(rig->state.multicast->dest_addr));

    if (num_bytes < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: sendto: %s\n", __func__, strerror(errno));
        return -RIG_EIO;
    }
    else
    {
//        printf("Sent %zd bytes to multicast group %s:%d\n", num_bytes, MULTICAST_ADDR,
//               PORT);
    }

    return num_bytes;
}

//#define TEST
#ifdef TEST
int main(int argc, char *argv[])
{
    RIG *rig;
    rig_model_t myrig_model;
    rig_set_debug_level(RIG_DEBUG_NONE);

    if (argc > 1) { myrig_model = atoi(argv[1]); }
    else
    {
        myrig_model = 1035;
    }

    rig = rig_init(myrig_model);

    if (rig == NULL)
    {

    }

    strncpy(rig->state.rigport.pathname, "/dev/ttyUSB0", HAMLIB_FILPATHLEN - 1);
    rig->state.rigport.parm.serial.rate = 38400;
    rig_open(rig);
    multicast_init(rig, "224.0.0.1", 4532);
    pthread_join(rig->state.multicast->threadid, NULL);
    pthread_exit(NULL);
    return 0;
}
#endif
