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
#include "network.h"
#include "sprintflst.h"

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


int multicast_stop(RIG *rig)
{
    if (rig->state.multicast) { rig->state.multicast->runflag = 0; }

    pthread_join(rig->state.multicast->threadid, NULL);
    return RIG_OK;
}

#if 0
static int multicast_status_changed(RIG *rig)
{
    int retval;
    struct rig_cache *cachep = CACHE(rig);
#if 0
    freq_t freq, freqsave = cachep->freqMainA;

    if ((retval = rig_get_freq(rig, RIG_VFO_A, &freq)) != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: rig_get_freq:%s\n", __func__, rigerror(retval));
    }

    if (freq != freqsave) { return 1; }

#endif

    rmode_t modeA, modeAsave = cachep->modeMainA;
    rmode_t modeB, modeBsave = cachep->modeMainB;
    pbwidth_t widthA, widthAsave = cachep->widthMainA;
    pbwidth_t widthB, widthBsave = cachep->widthMainB;

#if  0

    if (rig->state.multicast->seqnumber % 2 == 0
            && (retval = rig_get_mode(rig, RIG_VFO_A, &modeA, &widthA)) != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: rig_get_modeA:%s\n", __func__, rigerror(retval));
    }

#endif

    if (modeA != modeAsave) { return 1; }

    if (widthA != widthAsave) { return 1; }

#if 0

    if (rig->state.multicast->seqnumber % 2 == 0
            && (rig->caps->targetable_vfo & RIG_TARGETABLE_MODE)
            && (retval = rig_get_mode(rig, RIG_VFO_B, &modeB, &widthB)) != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: rig_get_modeB:%s\n", __func__, rigerror(retval));
    }

#endif

    if (modeB != modeBsave) { return 1; }

    if (widthB != widthBsave) { return 1; }

    ptt_t ptt, pttsave = cachep->ptt;

#if 0

    if (rig->state.multicast->seqnumber % 2 == 0
            && (retval = rig_get_ptt(rig, RIG_VFO_CURR, &ptt)) != RIG_OK)
        if (ptt != pttsave) { return 1; }

    split_t split, splitsave = cachep->split;
    vfo_t txvfo;

    if (rig->state.multicast->seqnumber % 2 == 0
            && (retval = rig_get_split_vfo(rig, RIG_VFO_CURR, &split, &txvfo)) != RIG_OK)
        if (split != splitsave) { return 1; }

#endif

    return 0;
}
#endif

void json_add_string(char *msg, const char *key, const char *value,
                     int addComma)
{
    strcat(msg, "\"");
    strcat(msg, key);
    strcat(msg, "\": ");
    strcat(msg, "\"");
    strcat(msg, value);
    strcat(msg, "\"");

    if (addComma) { strcat(msg, ",\n"); }
}

void json_add_int(char *msg, const char *key, const int number, int addComma)
{

    strcat(msg, "\"");
    strcat(msg, key);
    strcat(msg, "\": ");
    char tmp[64];
    sprintf(tmp, "%d", number);
    strcat(msg, tmp);

    if (addComma)
    {
        strcat(msg, ",\n");
    }
}

// cppcheck-suppress unusedFunction
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

// cppcheck-suppress unusedFunction
void json_add_boolean(char *msg, const char *key, const int value, int addComma)
{

    strcat(msg, "\"");
    strcat(msg, key);
    strcat(msg, "\": ");
    char tmp[64];
    sprintf(tmp, "%s", value == 0 ? "false" : "true");
    strcat(msg, tmp);

    if (addComma)
    {
        strcat(msg, ",\n");
    }
}

void json_add_time(char *msg, int addComma)
{
    char mydate[256];
    date_strget(mydate, sizeof(mydate), 0);


    strcat(msg, "\"Time\": \"");
    strcat(msg, mydate);
    strcat(msg, "\"");

    if (addComma)
    {
        strcat(msg, ",\n");
    }
}

void json_add_vfoA(RIG *rig, char *msg)
{
    struct rig_cache *cachep = CACHE(rig);
  
    strcat(msg, "{\n");
    json_add_string(msg, "Name", "VFOA", 1);
    json_add_int(msg, "Freq", cachep->freqMainA, 1);

    if (strlen(rig_strrmode(cachep->modeMainA)) > 0)
    {
        json_add_string(msg, "Mode", rig_strrmode(cachep->modeMainA), 1);
    }
    else
    {
        json_add_string(msg, "Mode", "None", 1);
    }

    json_add_int(msg, "Width", cachep->widthMainA, 0);

#if 0 // not working quite yet
    // what about full duplex? rx_vfo would be in rx all the time?
    rig_debug(RIG_DEBUG_ERR, "%s: rx_vfo=%s, tx_vfo=%s, split=%d\n", __func__,
              rig_strvfo(rig->state.rx_vfo), rig_strvfo(rig->state.tx_vfo),
              cachep->split);
    printf("%s: rx_vfo=%s, tx_vfo=%s, split=%d\n", __func__,
           rig_strvfo(rig->state.rx_vfo), rig_strvfo(rig->state.tx_vfo),
           cachep->split);

    if (cachep->split)
    {
        if (rig->state.tx_vfo && (RIG_VFO_B | RIG_VFO_MAIN_B))
        {
            json_add_boolean(msg, "RX", !cachep->ptt, 1);
            json_add_boolean(msg, "TX", 0, 0);
        }
        else // we must be in reverse split
        {
            json_add_boolean(msg, "RX", 0, 1);
            json_add_boolean(msg, "TX", cachep->ptt, 0);
        }
    }
    else if (rig->state.current_vfo && (RIG_VFO_A | RIG_VFO_MAIN_A))
    {
        json_add_boolean(msg, "RX", !cachep->ptt, 1);
        json_add_boolean(msg, "TX", cachep->ptt, 0);
    }
    else // VFOB must be active so never RX or TX
    {
        json_add_boolean(msg, "RX", 0, 1);
        json_add_boolean(msg, "TX", 0, 0);
    }

#endif
    strcat(msg, "\n}");
}

void json_add_vfoB(RIG *rig, char *msg)
{
    struct rig_cache *cachep = CACHE(rig);
  
    strcat(msg, ",\n{\n");
    json_add_string(msg, "Name", "VFOB", 1);
    json_add_int(msg, "Freq", cachep->freqMainB, 1);

    if (strlen(rig_strrmode(cachep->modeMainB)) > 0)
    {
        json_add_string(msg, "Mode", rig_strrmode(cachep->modeMainB), 1);
    }
    else
    {
        json_add_string(msg, "Mode", "None", 1);
    }

    json_add_int(msg, "Width", cachep->widthMainB, 0);

#if 0 // not working yet

    if (rig->state.rx_vfo != rig->state.tx_vfo && cachep->split)
    {
        if (rig->state.tx_vfo && (RIG_VFO_B | RIG_VFO_MAIN_B))
        {
            json_add_boolean(msg, "RX", 0, 1);
            json_add_boolean(msg, "TX", cachep->ptt, 0);
        }
        else // we must be in reverse split
        {
            json_add_boolean(msg, "RX", cachep->ptt, 1);
            json_add_boolean(msg, "TX", 0, 0);
        }
    }
    else if (rig->state.current_vfo && (RIG_VFO_A | RIG_VFO_MAIN_A))
    {
        json_add_boolean(msg, "RX", !cachep->ptt, 1);
        json_add_boolean(msg, "TX", cachep->ptt, 0);
    }
    else // VFOB must be active so always RX or TX
    {
        json_add_boolean(msg, "RX", 1, 1);
        json_add_boolean(msg, "TX", 1, 0);
    }

#endif

    strcat(msg, "\n}\n]\n");
}

static int multicast_send_json(RIG *rig)
{
    char msg[8192]; // could be pretty big
    char buf[4096];
    struct rig_cache *cachep = CACHE(rig);
    
//    sprintf(msg,"%s:f=%.1f", date_strget(msg, (int)sizeof(msg), 0), f);
    msg[0] = 0;
    snprintf(buf, sizeof(buf), "%s:%s", rig->caps->model_name,
             RIGPORT(rig)->pathname);
    strcat(msg, "{\n");
    json_add_string(msg, "ID", buf, 1);
    json_add_time(msg, 1);
    json_add_int(msg, "Sequence", rig->state.multicast->seqnumber++, 1);
    json_add_string(msg, "VFOCurr", rig_strvfo(rig->state.current_vfo), 1);
    json_add_int(msg, "PTT", cachep->ptt, 1);
    json_add_int(msg, "Split", cachep->split, 1);
    rig_sprintf_mode(buf, sizeof(buf), rig->state.mode_list);
    json_add_string(msg, "ModeList", buf, 1);
    strcat(msg, "\"VFOs\": [\n");
    json_add_vfoA(rig, msg);
    json_add_vfoB(rig, msg);
    strcat(msg, "}\n");

    // send the thing
    multicast_send(rig, msg, strlen(msg));
    return 0;
}

// cppcheck-suppress unusedFunction
void *multicast_thread_rx(void *vrig)
{
#if 0
    char buf[256];
#endif
//    int ret = 0;
    RIG *rig = (RIG *)vrig;
    hamlib_port_t port;
    rig->state.rig_type = RIG_TYPE_TRANSCEIVER;
    rig->state.ptt_type = RIG_PTT_RIG;
    rig->state.port_type = RIG_PORT_UDP_NETWORK;
    strcpy(port.pathname, "127.0.0.1:4532");
    //rig_debug(RIG_DEBUG_TRACE, "%s: started\n", __func__);
#if  0
    network_open(&port, 4532);
#endif

    //while (rig->state.multicast->runflag && ret >= 0)
    while (rig->state.multicast->runflag)
    {
#if 0
        ret = read_string(RIGPORT(rig), (unsigned char *) buf, sizeof(buf), "\n",
                          1,
                          0, 1);
#endif

        //rig_debug(RIG_DEBUG_VERBOSE, "%s: read %s\n", __func__, buf);
        hl_usleep(10 * 1000);
    }

    return NULL;
}

#define LOOPCOUNT 50
// cppcheck-suppress unusedFunction
void *multicast_thread(void *vrig)
{
    //int retval;
    RIG *rig = (RIG *)vrig;
    //rig_debug(RIG_DEBUG_TRACE, "%s: started\n", __func__);

    // do the 1st packet all the time
    //multicast_status_changed(rig);
    //multicast_send_json(rig);
    int loopcount = LOOPCOUNT;

    freq_t freqA, freqAsave = 0;
    freq_t freqB, freqBsave = 0;
    mode_t modeA, modeAsave = 0;
    mode_t modeB, modeBsave = 0;
    ptt_t ptt, pttsave = 0;
    struct rig_cache *cachep = CACHE(rig);
    
    rig->state.multicast->runflag = 1;

    while (rig->state.multicast->runflag)
    {
#if 0

        if ((retval = rig_get_freq(rig, RIG_VFO_A, &freqA)) != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: rig_get_freqA:%s\n", __func__, rigerror(retval));
        }

        if ((rig->caps->targetable_vfo & RIG_TARGETABLE_FREQ)
                && (retval = rig_get_freq(rig, RIG_VFO_B, &freqB)) != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: rig_get_freqB:%s\n", __func__, rigerror(retval));
        }
        else
        {
            freqB = cachep->freqMainB;
        }

#else
        freqA = cachep->freqMainA;
        freqB = cachep->freqMainB;
        modeA = cachep->modeMainA;
        modeB = cachep->modeMainB;
        ptt = cachep->ptt;
#endif

        if (freqA != freqAsave
                || freqB != freqBsave
                || modeA != modeAsave
                || modeB != modeBsave
                || ptt != pttsave
                || loopcount-- <= 0)
        {
#if 0

            if (loopcount <= 0)
            {
                rig_debug(RIG_DEBUG_CACHE, "%s: sending multicast packet timeout\n", __func__);
            }
            else { rig_debug(RIG_DEBUG_ERR, "%s: sending multicast packet due to change\n", __func__); }

#endif

//            multicast_status_changed(rig);
            multicast_send_json(rig);
            freqAsave = freqA;
            freqBsave = freqB;
            modeAsave = modeA;
            modeBsave = modeB;
            pttsave = ptt;
            loopcount = LOOPCOUNT;
        }
        else
        {
            //rig_debug(RIG_DEBUG_VERBOSE, "%s: loop\n", __func__);
            hl_usleep(10 * 1000);
        }


    }

#ifdef _WIN32
    WSACleanup();
#endif


    return NULL;
}

#ifdef WIN32
static char *GetWinsockLastError(char *errorBuffer, DWORD errorBufferSize)
{
    int errorCode = WSAGetLastError();

    FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        errorBuffer,
        errorBufferSize,
        NULL
    );
    return errorBuffer;
}
#endif

int multicast_init(RIG *rig, char *addr, int port)
{
    if (rig->state.multicast && rig->state.multicast->multicast_running) { return RIG_OK; }

#ifdef _WIN32
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        char errorMessage[1024];
        fprintf(stderr, "WSAStartup failed: %s\n", GetWinsockLastError(errorMessage,
                sizeof(errorMessage)));
        return 1;
    }

#endif

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
#ifdef _WIN32
        int err = WSAGetLastError();
        rig_debug(RIG_DEBUG_ERR, "%s: socket: WSAGetLastError=%d\n", __func__, err);
#else
        rig_debug(RIG_DEBUG_ERR, "%s: socket: %s\n", __func__, strerror(errno));
#endif
        return -RIG_EIO;
    }

#if 0
    // Set the SO_REUSEADDR option to allow multiple processes to use the same address
    int optval = 1;

    if (setsockopt(rig->state.multicast->sock, SOL_SOCKET, SO_REUSEADDR,
                   (char *)&optval,
                   sizeof(optval)) < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: setsockopt: %s\n", __func__, strerror(errno));
        return -RIG_EIO;
    }

#endif

    // Bind the socket to any available local address and the specified port
    //struct sockaddr_in saddr = {0};
    //saddr.sin_family = AF_INET;
    //saddr.sin_addr.s_addr = inet_addr(addr);
    //saddr.sin_port = htons(port);

#if 0

    if (bind(rig->state.multicast->sock, (struct sockaddr *)&saddr,
             sizeof(saddr)) < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: bind: %s\n", __func__, strerror(errno));
        return -RIG_EIO;
    }

#endif

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

#if 0

// look like we need to implement the client in a separate thread?
    // Join the multicast group
    if (setsockopt(rig->state.multicast->sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   (char *)&rig->state.multicast->mreq, sizeof(rig->state.multicast->mreq)) < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: setsockopt: %s\n", __func__, strerror(errno));
        //return -RIG_EIO;
    }

#endif

    // prime the dest_addr for the send routine
    memset(&rig->state.multicast->dest_addr, 0,
           sizeof(rig->state.multicast->dest_addr));
    rig->state.multicast->dest_addr.sin_family = AF_INET;
    rig->state.multicast->dest_addr.sin_addr.s_addr = inet_addr(addr);
    rig->state.multicast->dest_addr.sin_port = htons(port);

#if 0
    rig->state.multicast->runflag = 1;
    pthread_create(&rig->state.multicast->threadid, NULL, multicast_thread,
                   (void *)rig);
    //printf("threadid=%ld\n", rig->state.multicast->threadid);
    rig->state.multicast->multicast_running = 1;
    pthread_create(&rig->state.multicast->threadid, NULL, multicast_thread_rx,
                   (void *)rig);
#endif
    return RIG_OK;
}

// cppcheck-suppress unusedFunction
void multicast_close(RIG *rig)
{
    // Leave the multicast group
    if (setsockopt(rig->state.multicast->sock, IPPROTO_IP,
                   IP_DROP_MEMBERSHIP, (char *)&rig->state.multicast->mreq,
                   sizeof(rig->state.multicast->mreq)) < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: setsockopt: %s\n", __func__, strerror(errno));
        return;
    }

    // Close the socket
    if (close(rig->state.multicast->sock))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: close: %s\n", __func__, strerror(errno));
    }
}

// if msglen=0 msg is assumed to be a string
int multicast_send(RIG *rig, const char *msg, int msglen)
{
    // Construct the message to send
    if (msglen == 0) { msglen = strlen((char *)msg); }

    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;

    addr.sin_addr.s_addr = inet_addr("224.0.0.1");

    addr.sin_port = htons(4532);


    // Send the message to the multicast group
    ssize_t num_bytes = sendto(rig->state.multicast->sock, msg, msglen, 0,
                               (struct sockaddr *)&addr,
                               sizeof(addr));

    if (num_bytes < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: sendto: %s\n", __func__, strerror(errno));
        return -RIG_EIO;
    }

    return num_bytes;
}

//#define TEST
#ifdef TEST
int main(int argc, const char *argv[])
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
        fprintf(stderr, "rig==NULL?\n");
        return 1;
    }

    strncpy(RIGPORT(rig)->pathname, "/dev/ttyUSB0", HAMLIB_FILPATHLEN - 1);
    RIGPORT(rig)->parm.serial.rate = 38400;
    rig_open(rig);
    multicast_init(rig, "224.0.0.1", 4532);
    pthread_join(rig->state.multicast->threadid, NULL);
    pthread_exit(NULL);
    return 0;
}
#endif
