/*
 * ts2k_menu.h	(C) Copyright 2002 by Dale E. Edmons.  All rights reserved.
 */

/*
 * License:	GNU (same as what hamlib currently is using)
 */

/*
 * status:	uncompiled.  Haven't finished factory defaults.
 *		anything with m_tbd is: to be developed
 */

/* Header to implement menus equivalent to those on the TS-2000. */

// sub-menu defines
#define MENU_A	1
#define MENU_B	2
#define MENU_C	3
#define MENU_D	4
#define MENU_E	5
#define MENU_F	6

// parameter value text.  may be used in any menu.
const char m_on[]="On";
const char m_off[]="Off";
const char m_to[]="TO";
const char m_co[]="CO";
const char m_h_boost[]="High Boost";
const char m_b_boost[]="Bass Boost";
const char m_f_pass[]="F Pass";
const char m_conven[]="Conven";
const char m_user[]="User";
const char m_auto[]="Auto";
const char m_norm[]="Normal";
const char m_inv[]="Inverse";
const char m_low[]="Low";
const char m_mid[]="Mid";
const char m_hi[]="High";
const char m_burst[]="Burst";
const char m_cont[]="Cont";
const char m_slow[]="Slow";
const char m_fast[]="Fast";
const char m_main[]="Main";
const char m_sub[]="Sub";
const char m_tnc_band[]="TNC Band";
const char m_main_sub[]="Main & Sub";
const char m_man[]="Manual";
const char m_morse[]="Morse";
const char m_voice[]="Voice";
const char m_neg[]="Negative";
const char m_pos[]="Positive";
const char m_locked[]="Locked";
const char m_cross[]="Cross";
const char m_client[]="Client";
const char m_commander[]="Commander";
const char m_transporter[]="Transporter";
const char m_font1[]="font1";
const char m_font2[]="font2";
const char m_num[8+2];		// storage for numeric constants
const char m_text[8+2];		// storage for text constants
//const char m_[]="";

// array valued constants (mostly) or other undefined text
const char m_tbd[]="hamlib: t.b.d.";

// PF1 key assignments
#define KEY_MENU_MAX	62
#define KEY_KEY_MAX	90
#define KEY_UNASSIGNED	99
const char key_menu[]="Menu";	// menu # 0-62
const char key_none[]="None";
const char key_key[][KEY_KEY_MAX-KEY_MENU_MAX] = {
	"Voice1", Voice2", "Voice3", "RX Moni", "DSP Moni",
	"Quick Memo MR", "Quick Memo M.IN", "Split", "TF-SET",
	"A/B", "VFO/M", "A=B", "Scan", "M>VFO", "M.IN", "CW TUNE",
	"CH1", "CH2", "CH3", "Fine", "CLR", "Call", "CTRL",
	"1MHz", "ANT1/2", "N.B.", "N.R.", "B.C.", "A.N.",
	""
};

typedef struct {
	const char menu_no[];
	const char txt[];
	const char *param_txt[];
	char cmd[30];
	const int menu[4];
	int val;		// same as P5
} ts2k_menu_t;

/*
 * Defaults for menu_t.val were obtained via minicom
 * on my rig after doing a full reset to factory defaults.
 * (unfinished)
 */

const ts2k_menu_t ts2k_menus[] = {
	{ "00", "Display Brightness", {m_off, m_num, NULL}, "", {00,0,0,0}, 3},
	{ "01", "Key Illumination", {m_off, m_on, NULL}, "", {01,0,0,0}, 1},
	{ "02", "Tuning Control Change Per Revolution", {m_num, NULL}, "", {02,0,0,0}, 1},
	{ "03", "Tune using MULTI/CH Frequency Step", {m_off, m_on, NULL}, "", {03,0,0,0}, 1},
	{ "04", "Round off VFO using MULTI/CH Control", {m_off, m_on, NULL}, "", {04,0,0,0}, 1},
	{ "05", "9kHz step size for MULTI/CH in AM Broadcast Band", {m_off, m_on, NULL}, "", {05,0,0,0}, 0},
	{ "06A", "Mem: Memory-VFO Split", {m_off, m_on, NULL}, "", {06,1,0,0}, 0},
	{ "06B", "Mem: Temporary Frequency change after Memory Recall", {m_off, m_on, NULL}, "", {06,2,0,0}, 0},
	{ "07", "Program scan partially slowed", {m_off, m_on, NULL}, "", {07,0,0,0}, 0},
	{ "08", "Program Slow-Scan Range", {m_num, NULL}, "", {08,0,0,0}, 0},
	{ "09", "Program scan hold", {m_off, m_on, NULL}, "", {09,0,0,0}, 0},
	{ "10", "Scan Resume Method", {m_to, m_co, NULL}, "", {10,0,0,0}, 0},
	{ "11", "Visual Scan Range", {m_tbd, NULL}, "", {11,0,0,0}, 0},
	{ "12", "Beep Volume", {m_off, m_num, NULL}, "", {12,0,0,0}, 0},
	{ "13", "Sidetone Volume", {m_off, m_num, NULL}, "", {13,0,0,0}, 0},
	{ "14", "Message Playback Volume", {m_off, m_num, NULL}, "", {14,0,0,0}, 0},
	{ "15", "Voice Volume", {m_off, m_num, NULL}, "", {15,0,0,0}, 0},
	{ "16", "Output Configuration for sp2 or headphones", {m_tbd, NULL}, "", {16,0,0,0}, 0},
	{ "17", "Reversed output configuration for sp2 or headphones", {m_off, m_on, NULL}, "", {17,0,0,0}, 0},
	{ "18", "RX-Dedicated antenna", {m_off, m_on, NULL}, "", {18,0,0,0}, 0},
	{ "19A", "S-Meter: SQ", {m_off, m_on, NULL}, "", {19,1,0,0}, 0},
	{ "19B", "S-Meter: Hang Time", {m_off, m_tbd, NULL}, "", {19,2,0,0}, 0},
	{ "20", "RX Equalizer", {m_off, m_h_boost, m_b_boost, m_conven, m_user, NULL}, "", {20,0,0,0}, 0},
	{ "21", "TX Equalizer", {m_off, m_h_boost, m_b_boost, m_conven, m_user, NULL}, "", {21,0,0,0}, 0},
	{ "22", "TX Filter Bandwidth for SSB or AM", {m_tbd, NULL}, "", {22,0,0,0}, 0},
	{ "23", "Fine Transmit power change step", {m_off, m_on, NULL}, "", {23,0,0,0}, 0},
	{ "24", "Time-out Timer", {m_off, m_tbd, NULL}, "", {24,0,0,0}, 0},
	{ "25", "Transverter Frequency Display", {m_off, m_on, NULL}, "", {25,0,0,0}, 0},
	{ "26", "TX Hold; Antenna tuner", {m_off, m_on, NULL}, "", {26,0,0,0}, 0},
	{ "27", "Antenna tuner while receiving", {m_off, m_on, NULL}, "", {27,0,0,0}, 0},
	{ "28A", "Linear Amp Control Relay: HF", {m_off, m_num, NULL}, "", {28,1,0,0}, 0},
	{ "28B", "Linear Amp Control Relay: 50MHz", {m_off, m_num, NULL}, "", {28,2,0,0}, 0},
	{ "28C", "Linear Amp Control Relay: 144MHz", {m_off, m_num, NULL}, "", {28,3,0,0}, 0},
	{ "28D", "Linear Amp Control Relay: 430MHz", {m_off, m_num, NULL}, "", {28,4,0,0}, 0},
	{ "28E", "Linear Amp Control Relay: 1.2GHz", {m_off, m_num, NULL}, "", {28,5,0,0}, 0},
	{ "29A", "CW Message: Playback Repeat", {m_off, m_on, NULL}, "", {29,1,0,0}, 0},
	{ "29B", "CW Message: Playback Interval", {m_num, NULL}, "", {29,2,0,0}, 0},
	{ "30", "Keying Priority over playback", {m_off, m_on, NULL}, "", {30,0,0,0}, 0},
	{ "31", "CW RX Pitch/TX sidetone frequency", {m_tbd, NULL}, "", {31,0,0,0}, 0},
	{ "32", "CW rise time", {m_tbd, NULL}, "", {32,0,0,0}, 0},
	{ "33", "CW weighting", {m_auto, m_tbd, NULL}, "", {33,0,0,0}, 0},
	{ "34", "Reversed CW weighting", {m_off, m_on, NULL}, "", {34,0,0,0}, 0},
	{ "35", "Bug key function", {m_off, m_on, NULL}, "", {35,0,0,0}, 0},
	{ "36", "Auto CW TX when keying in SSB", {m_off, m_on, NULL}, "", {36,0,0,0}, 0},
	{ "37", "Frequency correction for SSB-to-CW change", {m_off, m_on, NULL}, "", {37,0,0,0}, 0},
	{ "38", "FSK shift", {m_tbd, NULL}, "", {38,0,0,0}, 0},
	{ "39", "FSK key-down polarity", {m_norm, m_inv, NULL}, "", {39,0,0,0}, 0},
	{ "40", "FSK tone frequency", {m_tbd, NULL}, "", {40,0,0,0}, 0},
	{ "41", "FM mic gain", {m_low, m_mid, m_high, NULL}, "", {41,0,0,0}, 0},
	{ "42", "FM sub-tone type", {m_burst, m_cont, NULL}, "", {42,0,0,0}, 0},
	{ "43", "Auto repeater offset", {m_off, m_on, NULL}, "", {43,0,0,0}, 0},
	{ "44", "TX hold; 1750Hz tone", {m_off, m_on, NULL}, "", {44,0,0,0}, 0},
	{ "45A", "DTMF: Memory", {m_tbd, NULL}, "", {45,1,0,0}, 0},
	{ "45B", "DTMF: TX speed", {m_fast, m_slow, NULL}, "", {45,2,0,0}, 0},
	{ "45C", "DTMF: Pause duration", {m_tbd, NULL}, "", {45,3,0,0}, 0},
	{ "45D", "DTMF: Mic control", {m_off, m_on, NULL}, "", {45,4,0,0}, 0},
	{ "46", "TNC: Main/Sub", {m_main, m_sub, NULL}, "", {46,0,0,0}, 0},
	{ "47", "Data transfer rate; Built-in TNC", {m_tbd, NULL}, "", {47,0,0,0}, 0},
	{ "48", "DCD sense", {m_tnc, m_main_sub, NULL}, "", {48,0,0,0}, 0},
	{ "49A", "Packet Cluster: Tune", {m_auto, m_man, NULL}, "", {49,1,0,0}, 0},
	{ "49B", "Packet Cluster: RX confirmation tone", {m_off, m_morse, m_voice, NULL}, "", {49,2,0,0}, 0},
	{ "50A", "TNC: filter bandwidth", {m_off, m_on, NULL}, "", {50,1,0,0}, 0},
	{ "50B", "TNC: AF input level", {m_tbd, NULL}, "", {50,2,0,0}, 0},
	{ "50C", "TNC: Main band AF output level", {m_tbd, NULL}, "", {50,3,0,0}, 0},
	{ "50D", "TNC: Sub band AF output level", {m_tbd, NULL}, "", {50,4,0,0}, 0},
	{ "50E", "TNC: External", {m_main, m_sub, NULL}, "", {50,5,0,0}, 0},
	{ "50F", "TNC: Ext. Data transfer rate", {m_tbd, NULL}, "", {50,6,0,0}, 0},
	{ "51A", "Front panel PF key program", {key_menu, key_key, NULL}, "", {51,1,0,0}, 0},
	{ "51B", "Mic key program: PF1", {key_menu, key_key, NULL}, "", {51,2,0,0}, 0},
	{ "51C", "Mic key program: PF2", {key_menu, key_key, NULL}, "", {51,3,0,0}, 0},
	{ "51D", "Mic key program: PF3", {key_menu, key_key, NULL}, "", {51,4,0,0}, 0},
	{ "51E", "Mic key program: PF4", {key_menu, key_key, NULL}, "", {51,5,0,0}, 0},
	{ "52", "Settings copy to another transceiver", {m_off, m_on, NULL}, "", {52,0,0,0}, 0},
	{ "53", "Settings Copy to VFO", {m_off, m_on, NULL}, "", {53,0,0,0}, 0},
	{ "54", "TX inhibit", {m_off, m_on, NULL}, "", {54,0,0,0}, 0},
	{ "55", "Packet operation", {m_off, m_on, NULL}, "", {0550,0,0}, 0},
	{ "56", "COM connector parameters", {m_tbd, NULL}, "", {56,0,0,0}, 0},
	{ "57", "Auto power off", {m_off, m_on, NULL}, "", {57,0,0,0}, 0},
	{ "58", "Detachable-panel Display font in easy operation mode", {m_font1, m_font1, NULL}, "", {58,0,0,0}, 0},
	{ "59", "Panel display contrast", {m_tbd, NULL}, "", {59,0,0,0}, 0},
	{ "60", "Detachable-panel display reversal", {m_tbd, NULL}, "", {60,0,0,0}, 0},
	{ "61A", "Repeater mode select", {m_off, m_locked, m_cross, NULL}, "", {61,1,0,0}, 0},
	{ "61B", "TX hold; repeater", {m_off, m_on, NULL}, "", {61,2,0,0}, 0},
	{ "61C", "Remote Control ID code", {m_num, NULL}, "", {61,3,0,0}, 0},
	{ "61D", "Remote control acknowledge", {m_off, m_on, NULL}, "", {61,4,0,0}, 0},
	{ "61E", "Remote control", {m_off, m_on, NULL}, "", {61,5,0,0}, 0},
	{ "62A", "Commander callsign", {m_text, NULL}, "", {62,1,0,0}, 0},
	{ "62B", "Transporter callsign", {m_text, NULL}, "", {62,2,0,0}, 0},
	{ "62C", "Sky Command tone frequency", {m_tbd, NULL}, "", {62,3,0,0}, 0},
	{ "62D", "Sky command communication speed", {m_tbd, NULL}, "", {62,4,0,0}, 0},
	{ "62E", "Transceiver define", {m_off, m_client, m_command, m_transporter, NULL}, "", {62,5,0,0}, 0},
	{ NULL, NULL, NULL, {00,0,0,0}, 0}
};

/*
 * Items related to menu structure
 */

// Programmable memories
typedef struct {
	int	curr;	// PM now in use
	int	orig;	// what PM to restore on exit
	int	orig_menu;	// orignal menu in effect

	int	menu;	// menuA or menuB of current PM

	// the following set which PM's are private or public
	// they are set on init and enforced until exit
	unsigned int	pub;
	unsigned int	priv;
#ifdef TS2K_ENFORCE_PM
# define TS2K_PM_PUB	( 	   	  (1<<2) | (1<<3)		 )
# define TS2K_PM_PRIV	( (1<<0) | (1<<1)	 |	 (1<<4) | (1<<5) )
#else
# define TS2K_PM_PUB	( (1<<0) | (1<<1) | (1<<2) | (1<<3) | (1<<4) | (1<<5) )
# define TS2K_PM_PRIV	(0)
#endif

} ts2k_pm_t;

// Implemented/coded functions
int ts2k_menu_init(RIG *rig, ts2k_menu_t *menu[]);
int ts2k_menu_close(RIG *rig, ts2k_menu_t *menu[]);
int ts2k_get_menu_no(RIG *rig, ts2k_menu_t *menu, int *main, int *sub);
int ts2k_set_menu_no(RIG *rig, ts2k_menu_t *menu, int main, int sub);

// Unimplemented/uncoded functions
char * ts2k_list_menu_no(RIG *rig, ts2k_menu_t *menu, int main, int sub);
int ts2k_get_menu(RIG *rig, ts2k_menu_t *menu);
int ts2k_set_menu(RIG *rig, ts2k_menu_t *menu);
int ts2k_menu_parse(RIG *rig, ts2k_menu_t *menu);

/*
 * Related functions specific to this rig
 */
// Programmable memories[0, ..., 5] + menu[A, ..., B]
int ts2k_get_menu_mode(RIG *rig, ts2k_menu_t *menu, ts2k_pm_t *pm);
int ts2k_set_menu_mode(RIG *rig, ts2k_menu_t *menu, ts2k_pm_t *pm);

int ts2k_pm_init(RIG *rig, ts2k_pm_t *pm); 
int ts2k_pm_close(RIG *rig, ts2k_pm_t *pm);

// End ts2k_menu.c
