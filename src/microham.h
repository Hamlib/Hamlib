//
// declaration of functions implemented in microham.c
//
//

extern int  uh_open_radio(int baud, int databits, int stopbits, int rtscts);
extern int  uh_open_ptt();
extern void uh_set_ptt(int ptt);
extern int  uh_get_ptt();
extern void uh_close_radio();
extern void uh_close_ptt();

