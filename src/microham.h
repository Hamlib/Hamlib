//
// Support for microHam
//
// Store the fd's of the sockets here. Then we can tell later on
// whether we are working on a "real" serial interface or on a socket
//

int uh_ptt_fd   = -1;   // PUBLIC! must be visible in iofunc.c in WIN32 case
int uh_radio_fd = -1;   // PUBLIC! must be visible in iofunc.c in WIN32 case

extern int  uh_open_radio(int baud, int databits, int stopbits, int rtscts);
extern int  uh_open_ptt();
extern void uh_set_ptt(int ptt);
extern int  uh_get_ptt();
extern void uh_close_radio();
extern void uh_close_ptt();

