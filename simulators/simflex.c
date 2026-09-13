#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#if defined(WIN32) || defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#define simflex_close_socket(s) closesocket(s)
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define simflex_close_socket(s) close(s)
#endif

#define PORT 4992
#define UDP_PORT 4991
#define BUFFER_SIZE 1024

/* VITA-49 constants matching smartsdr_stream.h */
#define VITA49_HEADER_BYTES     28
#define VITA49_PCC_AUDIO_F32    0x03E3
/* DAX I/Q, one class code per rate. These carry a LITTLE-endian payload of
 * counts against a full scale of 32768, where audio above is big-endian at
 * full scale 1.0. Sending I/Q the audio way would let a client that
 * byte-swaps it, or forgets to normalise it, appear to work. */
#define VITA49_PCC_IQ_24K       0x02E3
#define VITA49_PCC_IQ_48K       0x02E4
#define VITA49_PCC_IQ_96K       0x02E5
#define VITA49_PCC_IQ_192K      0x02E6
#define VITA49_DAXIQ_FULL_SCALE 32768.0f
#define VITA49_STEREO_PAIRS     128
#define VITA49_SAMPLES_PER_PKT  (VITA49_STEREO_PAIRS * 2)
#define VITA49_PAYLOAD_BYTES    (VITA49_SAMPLES_PER_PKT * (int)sizeof(float))
#define VITA49_PACKET_BYTES     (VITA49_HEADER_BYTES + VITA49_PAYLOAD_BYTES)

/* Streaming state per connection. */
struct stream_ctx
{
    int active;                         /* 1 = sending packets */
    pthread_t thread;
    struct sockaddr_in client_addr;     /* Client's UDP address */
    int udp_sock;                       /* Shared UDP socket fd */
    uint32_t stream_id;                 /* Assigned VITA-49 stream ID */
    int sample_rate;                    /* Hz (default 24000) */
    uint64_t phase;                     /* Tone generator phase */
    uint8_t packet_count;               /* 4-bit counter */
    uint32_t timestamp_int;             /* Seconds counter */
    uint64_t timestamp_frac;            /* Fractional sample counter */
};

static int client_udp_port = 0;        /* Client's registered UDP port */
static uint32_t next_stream_id = 0x40000001;
static struct stream_ctx audio_stream;  /* DAX / remote_audio_rx */
static struct stream_ctx iq_stream;      /* DAXIQ (VITA EXT_DATA) */
static int loopback_enabled = 0;       /* When 1, echo TX packets back as RX */

/* TX receive statistics (visible to command handler). */
static volatile uint64_t tx_rx_packets = 0;
static volatile uint64_t tx_rx_bytes = 0;

/* UDP RX thread: receives VITA-49 TX packets from the client.
 * In loopback mode, echoes them back with a new stream ID. */
static void *udp_rx_thread(void *arg)
{
    struct stream_ctx *ctx = (struct stream_ctx *)arg;
    uint8_t pkt[8192];

    printf("[udp_rx] Receiver thread started, sock=%d\n", ctx->udp_sock);

    while (ctx->active)
    {
        struct sockaddr_in from;
        socklen_t fromlen = sizeof(from);
        int nbytes = recvfrom(ctx->udp_sock, (char *)pkt, sizeof(pkt), 0,
                              (struct sockaddr *)&from, &fromlen);

        if (nbytes < VITA49_HEADER_BYTES)
        {
            continue;   /* Timeout or runt */
        }

        tx_rx_packets++;
        tx_rx_bytes += nbytes;

        /* In loopback mode: echo the packet back to the client
         * with a different stream ID (RX stream). */
        if (loopback_enabled && client_udp_port > 0 && ctx->stream_id != 0)
        {
            /* Overwrite the stream ID in the packet (word 1) to the
             * RX stream ID so the client's RX thread accepts it. */
            uint32_t *w = (uint32_t *)pkt;
            w[1] = htonl(ctx->stream_id);

            struct sockaddr_in dest;
            memset(&dest, 0, sizeof(dest));
            dest.sin_family = AF_INET;
            dest.sin_addr = from.sin_addr;
            dest.sin_port = htons(client_udp_port);

            sendto(ctx->udp_sock, (char *)pkt, nbytes, 0,
                   (struct sockaddr *)&dest, sizeof(dest));
        }
    }

    printf("[udp_rx] Receiver thread stopped (pkts=%llu bytes=%llu)\n",
           (unsigned long long)tx_rx_packets,
           (unsigned long long)tx_rx_bytes);
    return NULL;
}

/* UDP receiver context — shares the same UDP socket as the TX sender. */
static struct stream_ctx udp_receiver;
static pthread_t udp_rx_tid;
/*
S67319A86|slice 0 in_use=1 sample_rate=24000 RF_frequency=10.137000 client_handle=0x76AF7C73 index_letter=A rit_on=0 rit_freq=0 xit_on=0 xit_freq=0 rxant=ANT2 mode=DIGU wide=0 filter_lo=0 filter_hi=3510 step=10 step_list=1,5,10,20,100,250,500,1000 agc_mode=fast agc_threshold=65 agc_off_level=10 pan=0x40000000 txant=ANT2 loopa=0 loopb=0 qsk=0 dax=1 dax_clients=1 lock=0 tx=1 active=1 audio_level=100 audio_pan=51 audio_mute=1 record=0 play=disabled record_time=0.0 anf=0 anf_level=0 nr=0 nr_level=0 nb=0 nb_level=50 wnb=0 wnb_level=100 apf=0 apf_level=0 squelch=1 squelch_level=20 diversity=0 diversity_parent=0 diversity_child=0 diversity_index=1342177293 ant_list=ANT1,ANT2,RX_A,RX_B,XVTA,XVTB mode_list=LSB,USB,AM,CW,DIGL,DIGU,SAM,FM,NFM,DFM,RTTY fm_tone_mode=OFF fm_tone_value=67.0 fm_repeater_offset_freq=0.000000 tx_offset_freq=0.000000 repeater_offset_dir=SIMPLEX fm_tone_burst=0 fm_deviation=5000 dfm_pre_de_emphasis=0 post_demod_low=300 post_demod_high=3300 rtty_mark=2125 rtty_shift=170 digl_offset=2210 digu_offset=1500 post_demod_bypass=0 rfgain=24  tx_ant_list=ANT1,ANT2,XVTA,XVTB
*/

char *msg1 = "V1.4.0.0\n";
char *msg2 = "H2FFEDF06.M10000001|Client connected from IP 10.1.10.102\n";
char *msg3 =
    "S2FFEDF06|radio slices=0 panadapters=0 lineout_gain=54 lineout_mute=0 headphone_gain=56 headphone_mute=0 remote_on_enabled=0 pll_done=0 freq_error_ppb=0 cal_freq=15.000000 tnf_enabled=0 nickname=FaradayII callsign=WQ6Q binaural_rx=1 full_duplex_enabled=0 band_persistence_enabled=1 rtty_mark_default=2125 enforce_private_ip_connections=1 backlight=50 mute_local_audio_when_remote=1 daxiq_capacity=16 daxiq_available=16 alpha=1 low_latency_digital_modes=1 mf_enable=1.S2FFEDF06|radio filter_sharpness VOICE level=2 auto_level=1\n";
char *msg4 = "S2FFEDF06|radio filter_sharpness CW level=2 auto_level=1\n";
char *msg5 = "S2FFEDF06|radio filter_sharpness DIGITAL level=2 auto_level=1\n";
char *msg6 = "S2FFEDF06|radio static_net_params ip= gateway= netmask=\n";
char *msg7 = "S2FFEDF06|radio front_speaker_mute=1\n";
char *msg8 =
    "S2FFEDF06|radio oscillator state=gpsdo setting=auto locked=1 ext_present=0 gpsdo_present=1 tcxo_present=1\n";
char *msg9 =
    "S2FFEDF06|interlock acc_txreq_enable=0 rca_txreq_enable=0 acc_tx_enabled=0 tx1_enabled=1 tx2_enabled=0 tx3_enabled=0 tx_delay=40 acc_tx_delay=0 tx1_delay=0 tx2_delay=0 tx3_delay=0 acc_txreq_polarity=0 rca_txreq_polarity=0 time out=0\n";
char *msg10 =
    "S2FFEDF06|eq rx mode=0 63Hz=10 125Hz=10 250Hz=10 500Hz=10 1000Hz=10 2000Hz=10 4000Hz=10 8000Hz=10\n";
char *msg11 =
    "S2FFEDF06|eq rxsc mode=0 63Hz=0 125Hz=0 250Hz=0 500Hz=0 1000Hz=0 2000Hz=0 4000Hz=0 8000Hz=0\n";

/* Radios with SmartLink configured report remote_on_enabled=1 to every client,
 * including clients on the LAN. SIMFLEX_REMOTE_ON=1 models that radio so the
 * LAN registration path can be tested against it. */
static void simflex_apply_remote_on(void)
{
    const char *env = getenv("SIMFLEX_REMOTE_ON");
    char *found;

    if (env == NULL || env[0] != '1' || env[1] != '\0')
    {
        return;
    }

    found = strstr(msg3, "remote_on_enabled=0");

    if (found != NULL)
    {
        char *copy = strdup(msg3);

        if (copy != NULL)
        {
            found = strstr(copy, "remote_on_enabled=0");
            found[sizeof("remote_on_enabled=") - 1] = '1';
            msg3 = copy;
        }
    }
}

/* pan=0 until slice create / prep binds a pan id (Hamlib smartsdr_stream). */
static double simflex_rf_mhz = 10.137;
static uint32_t simflex_slice_pan = 0;
#define SIMFLEX_PAN_ID       0x40000000U
#define SIMFLEX_WATERFALL_ID  0x42000001U


static void simflex_send_display_pan_line(int sock, uint32_t pan, uint32_t wf)
{
    char line[256];

    snprintf(line, sizeof(line),
             "S67319A86|display pan 0x%x xmin=0 xmax=1024 waterfall=0x%x\n",
             pan, wf);
    send(sock, line, strlen(line), 0);
}


static void simflex_send_slice_line(int sock)
{
    char line[8192];
    char panfield[24];

    if (simflex_slice_pan == 0U)
    {
        snprintf(panfield, sizeof(panfield), "0");
    }
    else
    {
        snprintf(panfield, sizeof(panfield), "0x%x", simflex_slice_pan);
    }

    snprintf(line, sizeof(line),
             "S67319A86|slice 0 in_use=1 sample_rate=24000 RF_frequency=%.6f "
             "client_handle=0x76AF7C73 index_letter=A rit_on=0 rit_freq=0 "
             "xit_on=0 xit_freq=0 rxant=ANT2 mode=DIGU wide=0 filter_lo=0 "
             "filter_hi=3510 step=10 step_list=1,5,10,20,100,250,500,1000 "
             "agc_mode=fast agc_threshold=65 agc_off_level=10 pan=%s "
             "txant=ANT2 loopa=0 loopb=0 qsk=0 dax=1 dax_clients=1 lock=0 "
             "tx=1 active=1 audio_level=100 audio_pan=51 audio_mute=1 "
             "record=0 play=disabled record_time=0.0 anf=0 anf_level=0 "
             "nr=0 nr_level=0 nb=0 nb_level=50 wnb=0 wnb_level=100 apf=0 "
             "apf_level=0 squelch=1 squelch_level=20 diversity=0 "
             "diversity_parent=0 diversity_child=0 diversity_index=1342177293 "
             "ant_list=ANT1,ANT2,RX_A,RX_B,XVTA,XVTB mode_list=LSB,USB,AM,CW,"
             "DIGL,DIGU,SAM,FM,NFM,DFM,RTTY fm_tone_mode=OFF "
             "fm_tone_value=67.0 fm_repeater_offset_freq=0.000000 "
             "tx_offset_freq=0.000000 repeater_offset_dir=SIMPLEX "
             "fm_tone_burst=0 fm_deviation=5000 dfm_pre_de_emphasis=0 "
             "post_demod_low=300 post_demod_high=3300 rtty_mark=2125 "
             "rtty_shift=170 digl_offset=2210 digu_offset=1500 "
             "post_demod_bypass=0 rfgain=24  tx_ant_list=ANT1,ANT2,XVTA,XVTB",
             simflex_rf_mhz, panfield);
    {
        size_t n = strlen(line);

        if (n + 1 < sizeof(line))
        {
            line[n] = '\n';
            line[n + 1] = '\0';
            n++;
        }

        send(sock, line, n, 0);
    }
}


/* Build a VITA-49 audio packet with PCC 0x03E3 (float32 stereo).
 * Fills buf with VITA49_PACKET_BYTES of data. */
static void build_vita49_audio_packet(uint8_t *buf, struct stream_ctx *ctx)
{
    uint32_t *w = (uint32_t *)buf;
    int packet_words = VITA49_PACKET_BYTES / 4;

    /* Word 0: packet_type=1, C=1, T=0, TSI=01, TSF=01, count, size */
    uint32_t word0 = (0x1 << 28)     /* IF Data with Stream ID */
                     | (1 << 27)      /* Class ID present */
                     | (0 << 26)      /* No trailer */
                     | (1 << 22)      /* TSI = UTC */
                     | (1 << 20)      /* TSF = sample count */
                     | ((ctx->packet_count & 0xF) << 16)
                     | (packet_words & 0xFFFF);
    w[0] = htonl(word0);

    /* Word 1: Stream ID */
    w[1] = htonl(ctx->stream_id);

    /* Words 2-3: Class ID (OUI + PCC) — FlexRadio OUI 0x00001C2D */
    uint64_t class_id = ((uint64_t)0x00001C2D << 32)
                        | (uint64_t)VITA49_PCC_AUDIO_F32;
    w[2] = htonl((uint32_t)(class_id >> 32));
    w[3] = htonl((uint32_t)(class_id & 0xFFFFFFFF));

    /* Word 4: Integer timestamp (seconds) */
    w[4] = htonl(ctx->timestamp_int);

    /* Words 5-6: Fractional timestamp (64-bit sample count) */
    w[5] = htonl((uint32_t)(ctx->timestamp_frac >> 32));
    w[6] = htonl((uint32_t)(ctx->timestamp_frac & 0xFFFFFFFF));

    /* Payload: 128 stereo pairs of big-endian float32 (1kHz sine) */
    uint32_t *payload = &w[7];
    double freq = 1000.0;

    for (int i = 0; i < VITA49_STEREO_PAIRS; i++)
    {
        float val = 0.5f * sinf(2.0f * M_PI * freq
                                * ctx->phase / ctx->sample_rate);
        uint32_t raw;
        memcpy(&raw, &val, sizeof(float));
        payload[i * 2] = htonl(raw);       /* Left */
        payload[i * 2 + 1] = htonl(raw);   /* Right */
        ctx->phase++;
    }

    ctx->timestamp_frac += VITA49_STEREO_PAIRS;
    ctx->packet_count++;
}


/* VITA-49 EXT_DATA (type 0x3) IQ: interleaved I,Q float32 BE, same frame size. */
/* Store a 32-bit word little-endian regardless of host order. */
static void put_le32(uint32_t *dst, uint32_t v)
{
    uint8_t *p = (uint8_t *)dst;
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}


static uint16_t iq_pcc_for_rate(int sample_rate)
{
    switch (sample_rate)
    {
    case 24000:  return VITA49_PCC_IQ_24K;

    case 96000:  return VITA49_PCC_IQ_96K;

    case 192000: return VITA49_PCC_IQ_192K;

    default:     return VITA49_PCC_IQ_48K;
    }
}


static void build_vita49_iq_packet(uint8_t *buf, struct stream_ctx *ctx)
{
    uint32_t *w = (uint32_t *)buf;
    int packet_words = VITA49_PACKET_BYTES / 4;
    uint32_t word0 = (0x3 << 28)     /* Extension Data with Stream ID */
                     | (1 << 27)
                     | (0 << 26)
                     | (1 << 22)
                     | (1 << 20)
                     | ((ctx->packet_count & 0xF) << 16)
                     | (packet_words & 0xFFFF);
    w[0] = htonl(word0);

    w[1] = htonl(ctx->stream_id);

    {
        uint64_t class_id = ((uint64_t)0x00001C2D << 32)
                            | (uint64_t)iq_pcc_for_rate(ctx->sample_rate);
        w[2] = htonl((uint32_t)(class_id >> 32));
        w[3] = htonl((uint32_t)(class_id & 0xFFFFFFFF));
    }

    w[4] = htonl(ctx->timestamp_int);
    w[5] = htonl((uint32_t)(ctx->timestamp_frac >> 32));
    w[6] = htonl((uint32_t)(ctx->timestamp_frac & 0xFFFFFFFF));

    uint32_t *payload = &w[7];
    double tone_hz = 1000.0;

    for (int i = 0; i < VITA49_STEREO_PAIRS; i++)
    {
        /* Half scale, expressed the way the radio does: counts, not 0.5. */
        float i_s = 0.5f * VITA49_DAXIQ_FULL_SCALE
                    * sinf(2.0f * M_PI * (float)tone_hz
                           * (float)ctx->phase / (float)ctx->sample_rate);
        float q_s = 0.5f * VITA49_DAXIQ_FULL_SCALE
                    * cosf(2.0f * M_PI * (float)tone_hz
                           * (float)ctx->phase / (float)ctx->sample_rate);
        uint32_t raw_i, raw_q;
        memcpy(&raw_i, &i_s, sizeof(float));
        memcpy(&raw_q, &q_s, sizeof(float));
        put_le32(&payload[i * 2], raw_i);
        put_le32(&payload[i * 2 + 1], raw_q);
        ctx->phase++;
    }

    ctx->timestamp_frac += (uint64_t)VITA49_SAMPLES_PER_PKT;
    ctx->packet_count++;
}


/* Streaming thread: sends VITA-49 audio packets at real-time rate. */
static void *stream_tx_thread(void *arg)
{
    struct stream_ctx *ctx = (struct stream_ctx *)arg;
    uint8_t pkt[VITA49_PACKET_BYTES];
    int frame_us = (VITA49_STEREO_PAIRS * 1000000) / ctx->sample_rate;

    printf("[stream] TX thread started: stream_id=0x%08X rate=%d "
           "-> %s:%d\n",
           ctx->stream_id, ctx->sample_rate,
           inet_ntoa(ctx->client_addr.sin_addr),
           ntohs(ctx->client_addr.sin_port));

    while (ctx->active)
    {
        build_vita49_audio_packet(pkt, ctx);
        sendto(ctx->udp_sock, (char *)pkt, VITA49_PACKET_BYTES, 0,
               (struct sockaddr *)&ctx->client_addr,
               sizeof(ctx->client_addr));
        usleep(frame_us);
    }

    printf("[stream] TX thread stopped: stream_id=0x%08X\n", ctx->stream_id);
    return NULL;
}


static void *stream_iq_thread(void *arg)
{
    struct stream_ctx *ctx = (struct stream_ctx *)arg;
    uint8_t pkt[VITA49_PACKET_BYTES];
    int frame_us = (VITA49_STEREO_PAIRS * 1000000) / ctx->sample_rate;

    printf("[stream] IQ thread started: stream_id=0x%08X rate=%d "
           "-> %s:%d\n",
           ctx->stream_id, ctx->sample_rate,
           inet_ntoa(ctx->client_addr.sin_addr),
           ntohs(ctx->client_addr.sin_port));

    while (ctx->active)
    {
        build_vita49_iq_packet(pkt, ctx);
        sendto(ctx->udp_sock, (char *)pkt, VITA49_PACKET_BYTES, 0,
               (struct sockaddr *)&ctx->client_addr,
               sizeof(ctx->client_addr));
        usleep(frame_us);
    }

    printf("[stream] IQ thread stopped: stream_id=0x%08X\n", ctx->stream_id);
    return NULL;
}


/* Start streaming VITA-49 audio to the client. */
static void start_audio_stream(struct stream_ctx *ctx, int udp_sock,
                                struct sockaddr_in *client_addr,
                                uint32_t stream_id, int sample_rate)
{
    if (ctx->active)
    {
        printf("[stream] Already streaming, ignoring\n");
        return;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->udp_sock = udp_sock;
    ctx->client_addr = *client_addr;
    ctx->stream_id = stream_id;
    ctx->sample_rate = sample_rate > 0 ? sample_rate : 24000;
    ctx->active = 1;

    pthread_create(&ctx->thread, NULL, stream_tx_thread, ctx);
}


static void start_iq_stream(struct stream_ctx *ctx, int udp_sock,
                            struct sockaddr_in *client_addr,
                            uint32_t stream_id, int sample_rate)
{
    if (ctx->active)
    {
        printf("[stream] IQ already streaming, ignoring\n");
        return;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->udp_sock = udp_sock;
    ctx->client_addr = *client_addr;
    ctx->stream_id = stream_id;
    ctx->sample_rate = sample_rate > 0 ? sample_rate : 48000;
    ctx->active = 1;
    pthread_create(&ctx->thread, NULL, stream_iq_thread, ctx);
}


/* Stop an active audio stream. */
static void stop_audio_stream(struct stream_ctx *ctx)
{
    if (!ctx->active)
    {
        return;
    }

    ctx->active = 0;
    pthread_join(ctx->thread, NULL);
    printf("[stream] Stream 0x%08X stopped\n", ctx->stream_id);
}


static void stop_iq_stream(struct stream_ctx *ctx)
{
    if (!ctx->active)
    {
        return;
    }

    ctx->active = 0;
    pthread_join(ctx->thread, NULL);
    printf("[stream] IQ stream 0x%08X stopped\n", ctx->stream_id);
}


/* Create the shared UDP socket for VITA-49 data (port 4991). */
static int create_udp_socket(void)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock < 0)
    {
        perror("UDP socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(UDP_PORT);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("UDP bind");
        simflex_close_socket(sock);
        return -1;
    }

    /* Set receive timeout so we can check for TX packets periodically.
     * Winsock takes a DWORD of milliseconds here rather than a struct timeval,
     * and reads the zero tv_sec of one as "wait forever", which would strand
     * the receive thread and hang the join that stops it. */
#ifdef _WIN32
    DWORD tv = 100;
#else
    struct timeval tv = { .tv_sec = 0, .tv_usec = 100000 };
#endif
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));

    printf("UDP socket bound to port %d\n", UDP_PORT);

    /* Start the UDP receiver thread to handle incoming TX packets. */
    memset(&udp_receiver, 0, sizeof(udp_receiver));
    udp_receiver.udp_sock = sock;
    udp_receiver.active = 1;
    pthread_create(&udp_rx_tid, NULL, udp_rx_thread, &udp_receiver);

    return sock;
}


int main(int argc, char *argv[])
{
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    int tcp_port = PORT;

    /* Clients may connect and close immediately (a liveness probe does), and
     * the banner below is written the moment a connection is accepted. Without
     * this the resulting SIGPIPE terminates the simulator. */
#ifdef SIGPIPE
    signal(SIGPIPE, SIG_IGN);
#endif

    simflex_apply_remote_on();

    if (argc > 1)
    {
        tcp_port = atoi(argv[1]);

        if (tcp_port <= 0 || tcp_port > 65535)
        {
            tcp_port = PORT;
        }
    }

    /* Disable output buffering for reliable debug output */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

#if defined(WIN32) || defined(_WIN32)
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (iResult != 0)
    {
        printf("WSAStartup failed: %d\n", iResult);
        exit(EXIT_FAILURE);
    }

#endif


    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket failed");
#if defined(WIN32) || defined(_WIN32)
        WSACleanup();
#endif
        exit(EXIT_FAILURE);
    }

    // Set socket options
#if defined(WIN32) || defined(_WIN32)

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)))
    {
#else

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
#endif
        perror("setsockopt SO_REUSEADDR");
        simflex_close_socket(server_fd);
#if defined(WIN32) || defined(_WIN32)
        WSACleanup();
#endif
        exit(EXIT_FAILURE);
    }

#if !defined(WIN32) && !defined(_WIN32) && defined(SO_REUSEPORT)
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons((uint16_t)tcp_port);

    // Bind the socket to the network address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        simflex_close_socket(server_fd);
#if defined(WIN32) || defined(_WIN32)
        WSACleanup();
#endif
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        simflex_close_socket(server_fd);
#if defined(WIN32) || defined(_WIN32)
        WSACleanup();
#endif
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d\n", tcp_port);

    int udp_sock = create_udp_socket();

    if (udp_sock < 0)
    {
        simflex_close_socket(server_fd);
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        // Accept incoming connection
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                                 (socklen_t *)&addrlen)) < 0)
        {
            perror("accept");
            simflex_close_socket(server_fd);
#if defined(WIN32) || defined(_WIN32)
            WSACleanup();
#endif
            exit(EXIT_FAILURE);
        }

        printf("Connection accepted from %s\n", inet_ntoa(address.sin_addr));
        struct sockaddr_in client_ip = address;  /* Save client's IP */
        client_udp_port = 0;
        simflex_rf_mhz = 10.137;
        simflex_slice_pan = 0;

        send(new_socket, msg1, strlen(msg1), 0);
        send(new_socket, msg2, strlen(msg2), 0);
        send(new_socket, msg3, strlen(msg3), 0);
        send(new_socket, msg4, strlen(msg4), 0);
        send(new_socket, msg5, strlen(msg5), 0);
        send(new_socket, msg6, strlen(msg6), 0);
        send(new_socket, msg7, strlen(msg7), 0);
        send(new_socket, msg8, strlen(msg8), 0);
        send(new_socket, msg9, strlen(msg9), 0);
        send(new_socket, msg10, strlen(msg10), 0);
        send(new_socket, msg11, strlen(msg11), 0);


        while (1)
        {
            char readbuf[BUFFER_SIZE * 4] = {0};
            int valread = recv(new_socket, readbuf, sizeof(readbuf) - 1, 0);

            if (valread < 0)
            {
                perror("read");
                break;
            }

            if (valread == 0)
            {
                printf("Connection closed by client\n");
                break;
            }

            readbuf[valread] = '\0';

            /* Parse line-by-line — TCP may deliver multiple commands
             * in a single read. */
            char *saveptr = NULL;
            char *line = strtok_r(readbuf, "\n", &saveptr);

            while (line != NULL)
            {
            char buffer[BUFFER_SIZE] = {0};
            char reply[4096] = {0};
            int seqnum = -1;

            strncpy(buffer, line, sizeof(buffer) - 1);
            printf("Received: %s\n", buffer);

            sscanf(buffer, "C%d", &seqnum);

            /* Default reply: success with no extra data */
            sprintf(reply, "R%d|0%c", seqnum, 0x0a);

            if (strstr(buffer, "sub slice"))
            {
                simflex_send_slice_line(new_socket);
            }
            else if (strstr(buffer, "client udpport"))
            {
                int port = 0;
                sscanf(strstr(buffer, "client udpport") + 14, " %d", &port);
                client_udp_port = port;
                printf("[cmd] Client UDP port set to %d\n", port);
            }
            else if (strstr(buffer, "client udp_register"))
            {
                printf("[cmd] client udp_register (WAN path)\n");
            }
            else if (strstr(buffer, "client program"))
            {
                printf("[cmd] Client program registered\n");
            }
            else if (strstr(buffer, "client set"))
            {
                printf("[cmd] Client settings applied\n");
            }
            else if (strstr(buffer, "audio_stream create"))
            {
                /* Parse dax= parameter */
                int dax = 1;
                char *dax_p = strstr(buffer, "dax=");

                if (dax_p)
                {
                    sscanf(dax_p + 4, "%d", &dax);
                }

                uint32_t sid = next_stream_id++;
                printf("[cmd] audio_stream create dax=%d -> stream_id=0x%08X\n",
                       dax, sid);

                /* Reply with stream ID */
                sprintf(reply, "R%d|0|0x%08X%c", seqnum, sid, 0x0a);

                /* Start streaming to client */
                if (client_udp_port > 0)
                {
                    struct sockaddr_in dest;
                    memset(&dest, 0, sizeof(dest));
                    dest.sin_family = AF_INET;
                    dest.sin_addr = client_ip.sin_addr;
                    dest.sin_port = htons(client_udp_port);
                    start_audio_stream(&audio_stream, udp_sock, &dest,
                                       sid, 24000);
                }
            }
            else if (strstr(buffer, "audio_stream set"))
            {
                printf("[cmd] audio_stream set (tx enable or other)\n");
            }
            else if (strstr(buffer, "dax audio set"))
            {
                /* Handle tx=1 — enable loopback so TX packets are
                 * echoed back through the RX stream. */
                if (strstr(buffer, "tx=1"))
                {
                    loopback_enabled = 1;
                    /* Tell the UDP receiver the RX stream ID to stamp
                     * onto echoed packets. */
                    udp_receiver.stream_id = audio_stream.stream_id;
                    printf("[cmd] dax audio TX enabled, loopback on "
                           "(rx_sid=0x%08X)\n", udp_receiver.stream_id);
                }
                else
                {
                    loopback_enabled = 0;
                    printf("[cmd] dax audio TX disabled\n");
                }
            }
            else if (strstr(buffer, "audio_stream remove"))
            {
                printf("[cmd] audio_stream remove\n");
                stop_audio_stream(&audio_stream);
            }
            else if (strstr(buffer, "stream create") != NULL
                     && strstr(buffer, "remote_audio_rx") != NULL)
            {
                /* Demod audio to the client. Unlike the DAXIQ form this
                 * command carries no ip=/port=, so the destination is only
                 * known if the client registered it with "client udpport". */
                uint32_t sid = next_stream_id++;
                printf("[cmd] stream create type=remote_audio_rx "
                       "-> stream_id=0x%08X\n", sid);

                sprintf(reply, "R%d|0|0x%08X%c", seqnum, sid, 0x0a);

                if (client_udp_port > 0)
                {
                    struct sockaddr_in dest;
                    memset(&dest, 0, sizeof(dest));
                    dest.sin_family = AF_INET;
                    dest.sin_addr = client_ip.sin_addr;
                    dest.sin_port = htons(client_udp_port);
                    start_audio_stream(&audio_stream, udp_sock, &dest,
                                       sid, 24000);
                }
                else
                {
                    printf("[cmd] no client udpport registered — "
                           "cannot send audio\n");
                }
            }
            else if (strstr(buffer, "stream create") != NULL
                     && strstr(buffer, "daxiq_channel=") != NULL)
            {
                int daxiq_ch = 1;
                int rate = 48000;
                char *chp = strstr(buffer, "daxiq_channel=");
                char *portp = strstr(buffer, "port=");

                if (portp != NULL)
                {
                    int p = 0;

                    if (sscanf(portp + 5, "%d", &p) == 1 && p > 0)
                    {
                        client_udp_port = p;
                    }
                }

                if (chp != NULL)
                {
                    sscanf(chp + 14, "%d", &daxiq_ch);
                }

                uint32_t sid = next_stream_id++;
                printf("[cmd] stream create type=dax_iq daxiq_channel=%d "
                       "-> stream_id=0x%08X\n",
                       daxiq_ch, sid);
                sprintf(reply, "R%d|0|0x%08X%c", seqnum, sid, 0x0a);

                if (client_udp_port > 0)
                {
                    struct sockaddr_in dest;
                    memset(&dest, 0, sizeof(dest));
                    dest.sin_family = AF_INET;
                    dest.sin_addr = client_ip.sin_addr;
                    dest.sin_port = htons(client_udp_port);
                    start_iq_stream(&iq_stream, udp_sock, &dest, sid, rate);
                }
            }
            else if (strstr(buffer, "stream create") != NULL
                     && strstr(buffer, "daxiq=") != NULL)
            {
                int daxiq_ch = 1;
                int rate = 48000;
                char *chp = strstr(buffer, "daxiq=");
                char *rate_p = strstr(buffer, "rate=");
                char *portp = strstr(buffer, "port=");

                if (portp != NULL)
                {
                    int p = 0;

                    if (sscanf(portp + 5, "%d", &p) == 1 && p > 0)
                    {
                        client_udp_port = p;
                    }
                }

                if (chp != NULL)
                {
                    sscanf(chp + 6, "%d", &daxiq_ch);
                }

                if (rate_p != NULL)
                {
                    sscanf(rate_p + 5, "%d", &rate);
                }

                uint32_t sid = next_stream_id++;
                printf("[cmd] stream create daxiq=%d rate=%d "
                       "-> stream_id=0x%08X\n",
                       daxiq_ch, rate, sid);
                sprintf(reply, "R%d|0|0x%08X%c", seqnum, sid, 0x0a);

                if (client_udp_port > 0)
                {
                    struct sockaddr_in dest;
                    memset(&dest, 0, sizeof(dest));
                    dest.sin_family = AF_INET;
                    dest.sin_addr = client_ip.sin_addr;
                    dest.sin_port = htons(client_udp_port);
                    start_iq_stream(&iq_stream, udp_sock, &dest, sid, rate);
                }
            }
            else if (strstr(buffer, "stream set") != NULL
                     && strstr(buffer, "daxiq_rate=") != NULL)
            {
                printf("[cmd] stream set daxiq_rate (simflex)\n");
            }
            else if (strstr(buffer, "stream remove"))
            {
                unsigned long rid = 0;
                char *rp = strstr(buffer, "stream remove");

                if (rp != NULL)
                {
                    rp += (int)strlen("stream remove");

                    while (*rp == ' ')
                    {
                        rp++;
                    }

                    rid = strtoul(rp, NULL, 0);
                }

                printf("[cmd] stream remove 0x%lX\n", rid);

                if (rid != 0 && (uint32_t)rid == iq_stream.stream_id)
                {
                    stop_iq_stream(&iq_stream);
                }

                if (rid != 0 && (uint32_t)rid == audio_stream.stream_id)
                {
                    stop_audio_stream(&audio_stream);
                }
            }
            else if (strstr(buffer, "filt "))
            {
                printf("[cmd] filt (passband)\n");
            }
            else if (strstr(buffer, "slice set"))
            {
                printf("[cmd] slice set\n");
            }
            else if (strstr(buffer, "slice create"))
            {
                char *pp = strstr(buffer, "pan=");
                char *fp = strstr(buffer, "freq=");

                if (fp != NULL)
                {
                    sscanf(fp + 5, "%lf", &simflex_rf_mhz);
                }

                if (pp != NULL)
                {
                    simflex_slice_pan = (uint32_t)strtoul(pp + 4, NULL, 0);
                }

                printf("[cmd] slice create pan=0x%x RF=%.6f MHz\n",
                       simflex_slice_pan, simflex_rf_mhz);
            }
            else if (strstr(buffer, "display panafall create"))
            {
                sprintf(reply, "R%d|0|0x%x,0x%x%c", seqnum, SIMFLEX_PAN_ID,
                        SIMFLEX_WATERFALL_ID, 0x0a);
                printf("[cmd] display panafall create\n");
            }
            else if (strstr(buffer, "display panafall c "))
            {
                sprintf(reply, "R%d|0|0x%x,0x%x%c", seqnum, SIMFLEX_PAN_ID,
                        SIMFLEX_WATERFALL_ID, 0x0a);
                printf("[cmd] display panafall c\n");
            }
            else if (strstr(buffer, "display pan create"))
            {
                sprintf(reply, "R%d|0|0x%x,0x%x%c", seqnum, SIMFLEX_PAN_ID,
                        SIMFLEX_WATERFALL_ID, 0x0a);
                printf("[cmd] display pan create\n");
            }
            else if (strstr(buffer, "display pan c "))
            {
                sprintf(reply, "R%d|0|0x%x,0x%x%c", seqnum, SIMFLEX_PAN_ID,
                        SIMFLEX_WATERFALL_ID, 0x0a);
                printf("[cmd] display pan c\n");
            }
            else if (strstr(buffer, "display pan remove"))
            {
                printf("[cmd] display pan remove\n");
            }
            else if (strstr(buffer, "display panafall set"))
            {
                printf("[cmd] display panafall set (daxiq_channel bind)\n");
            }
            else if (strstr(buffer, "display pan set"))
            {
                printf("[cmd] display pan set\n");
            }

            /* Send the reply */
            int reply_len = strlen(reply);

            if (send(new_socket, reply, reply_len, 0) != reply_len)
            {
                perror("send");
                break;
            }

            if (strstr(buffer, "display pan set"))
            {
                unsigned long pan_arg = 0UL;
                char *hp = strstr(buffer, "0x");

                if (hp != NULL)
                {
                    pan_arg = strtoul(hp, NULL, 0);
                }

                if (pan_arg == 0UL)
                {
                    pan_arg = SIMFLEX_PAN_ID;
                }

                simflex_send_display_pan_line(new_socket, (uint32_t)pan_arg,
                                              SIMFLEX_WATERFALL_ID);
            }

            line = strtok_r(NULL, "\n", &saveptr);
            }  /* while (line) */
        }  /* while (1) read loop */

        /* Client disconnected — stop any active stream */
        stop_audio_stream(&audio_stream);
        stop_iq_stream(&iq_stream);
        loopback_enabled = 0;
        tx_rx_packets = 0;
        tx_rx_bytes = 0;
        simflex_close_socket(new_socket);
    }

    /* Stop the UDP receiver thread */
    udp_receiver.active = 0;
    pthread_join(udp_rx_tid, NULL);

    simflex_close_socket(udp_sock);
    simflex_close_socket(server_fd);
#if defined(WIN32) || defined(_WIN32)
    WSACleanup();
#endif
    return 0;
}

