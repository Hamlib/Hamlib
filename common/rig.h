/*
 * Basic rig type, can store some useful
 * info about different radios. Each lib must
 * be able to populate this structure, so we can make
 * useful enquiries about capablilities.
 */


#define RIG_PARITY_ODD       0
#define RIG_PARITY_EVEN      1
#define RIG_PARITY_NONE      2

struct rig_caps {
  char rig_name[30];		/* eg ft847 */
  unsigned short int serial_rate_min;		/* eg 4800 */
  unsigned short int serial_rate_max;		/* eg 9600 */
  unsigned char serial_data_bits;	/* eg 8 */
  unsigned char serial_stop_bits;	/* eg 2 */
  unsigned char serial_parity;	/*  */

};












