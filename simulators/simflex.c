#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if defined(WIN32) || defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

#define PORT 4992
#define BUFFER_SIZE 1024
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

char *slicetxt =
    "S67319A86|slice 0 in_use=1 sample_rate=24000 RF_frequency=10.137000 client_handle=0x76AF7C73 index_letter=A rit_on=0 rit_freq=0 xit_on=0 xit_freq=0 rxant=ANT2 mode=DIGU wide=0 filter_lo=0 filter_hi=3510 step=10 step_list=1,5,10,20,100,250,500,1000 agc_mode=fast agc_threshold=65 agc_off_level=10 pan=0x40000000 txant=ANT2 loopa=0 loopb=0 qsk=0 dax=1 dax_clients=1 lock=0 tx=1 active=1 audio_level=100 audio_pan=51 audio_mute=1 record=0 play=disabled record_time=0.0 anf=0 anf_level=0 nr=0 nr_level=0 nb=0 nb_level=50 wnb=0 wnb_level=100 apf=0 apf_level=0 squelch=1 squelch_level=20 diversity=0 diversity_parent=0 diversity_child=0 diversity_index=1342177293 ant_list=ANT1,ANT2,RX_A,RX_B,XVTA,XVTB mode_list=LSB,USB,AM,CW,DIGL,DIGU,SAM,FM,NFM,DFM,RTTY fm_tone_mode=OFF fm_tone_value=67.0 fm_repeater_offset_freq=0.000000 tx_offset_freq=0.000000 repeater_offset_dir=SIMPLEX fm_tone_burst=0 fm_deviation=5000 dfm_pre_de_emphasis=0 post_demod_low=300 post_demod_high=3300 rtty_mark=2125 rtty_shift=170 digl_offset=2210 digu_offset=1500 post_demod_bypass=0 rfgain=24  tx_ant_list=ANT1,ANT2,XVTA,XVTB";

int main()
{
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
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
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
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

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt)))
    {
#endif
        perror("setsockopt");
        close(server_fd);
#if defined(WIN32) || defined(_WIN32)
        WSACleanup();
#endif
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket to the network address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        close(server_fd);
#if defined(WIN32) || defined(_WIN32)
        WSACleanup();
#endif
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        close(server_fd);
#if defined(WIN32) || defined(_WIN32)
        WSACleanup();
#endif
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d\n", PORT);

    while (1)
    {
        // Accept incoming connection
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                                 (socklen_t *)&addrlen)) < 0)
        {
            perror("accept");
            close(server_fd);
#if defined(WIN32) || defined(_WIN32)
            WSACleanup();
#endif
            exit(EXIT_FAILURE);
        }

        printf("Connection accepted\n");
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
            char buffer[BUFFER_SIZE] = {0};
            char reply[BUFFER_SIZE] = {0};
            int seqnum = -1;
            int valread = read(new_socket, buffer, BUFFER_SIZE);

            if (valread < 0)
            {
                perror("read");
                break;
            }

            if (valread == 0)
            {
                // Connection closed
                printf("Connection closed by client\n");
                break;
            }

            buffer[valread] = '\0';
            printf("Received: %s", buffer);

            sscanf(buffer, "C%d", &seqnum);
            sprintf(reply, "R%d|0%c", seqnum, 0x0a);

            if (strstr(buffer, "sub slice"))
            {
                char buf[8192];
                sprintf(buf, "%s%c", slicetxt, 0x0a);

                if (send(new_socket, buf, strlen(buf), 0) != strlen(buf))
                {
                    perror("send");
                    break;
                }
            }

            // Echo back the received message
            if (send(new_socket, reply, valread, 0) != valread)
            {
                perror("send");
                break;
            }
        }

        close(new_socket);
    }

    close(server_fd);
#if defined(WIN32) || defined(_WIN32)
    WSACleanup();
#endif
    return 0;
}

