#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <hamlib/rig.h>

typedef RIG*  Hamlib__Rig;

static int
not_here(char *s)
{
    croak("%s not implemented on this architecture", s);
    return -1;
}

static double
constant_F(char *name, int len, int arg)
{
    switch (name[1 + 0]) {
    case 'I':
	if (strEQ(name + 1, "ILPATHLEN")) {	/* F removed */
#ifdef FILPATHLEN
	    return FILPATHLEN;
#else
	    goto not_there;
#endif
	}
    case 'L':
	if (strEQ(name + 1, "LTLSTSIZ")) {	/* F removed */
#ifdef FLTLSTSIZ
	    return FLTLSTSIZ;
#else
	    goto not_there;
#endif
	}
    case 'R':
	if (strEQ(name + 1, "RQRANGESIZ")) {	/* F removed */
#ifdef FRQRANGESIZ
	    return FRQRANGESIZ;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant__(char *name, int len, int arg)
{
    if (1 + 1 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[1 + 1]) {
    case 'B':
	if (strEQ(name + 1, "_BEGIN_DECLS")) {	/* _ removed */
#ifdef __BEGIN_DECLS
	    return __BEGIN_DECLS;
#else
	    goto not_there;
#endif
	}
    case 'E':
	if (strEQ(name + 1, "_END_DECLS")) {	/* _ removed */
#ifdef __END_DECLS
	    return __END_DECLS;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_OP_B(char *name, int len, int arg)
{
    if (8 + 4 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[8 + 4]) {
    case 'D':
	if (strEQ(name + 8, "AND_DOWN")) {	/* RIG_OP_B removed */
#ifdef RIG_OP_BAND_DOWN
	    return RIG_OP_BAND_DOWN;
#else
	    goto not_there;
#endif
	}
    case 'U':
	if (strEQ(name + 8, "AND_UP")) {	/* RIG_OP_B removed */
#ifdef RIG_OP_BAND_UP
	    return RIG_OP_BAND_UP;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_OP(char *name, int len, int arg)
{
    if (6 + 1 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[6 + 1]) {
    case 'B':
	if (!strnEQ(name + 6,"_", 1))
	    break;
	return constant_RIG_OP_B(name, len, arg);
    case 'C':
	if (strEQ(name + 6, "_CPY")) {	/* RIG_OP removed */
#ifdef RIG_OP_CPY
	    return RIG_OP_CPY;
#else
	    goto not_there;
#endif
	}
    case 'D':
	if (strEQ(name + 6, "_DOWN")) {	/* RIG_OP removed */
#ifdef RIG_OP_DOWN
	    return RIG_OP_DOWN;
#else
	    goto not_there;
#endif
	}
    case 'F':
	if (strEQ(name + 6, "_FROM_VFO")) {	/* RIG_OP removed */
#ifdef RIG_OP_FROM_VFO
	    return RIG_OP_FROM_VFO;
#else
	    goto not_there;
#endif
	}
    case 'L':
	if (strEQ(name + 6, "_LEFT")) {	/* RIG_OP removed */
#ifdef RIG_OP_LEFT
	    return RIG_OP_LEFT;
#else
	    goto not_there;
#endif
	}
    case 'M':
	if (strEQ(name + 6, "_MCL")) {	/* RIG_OP removed */
#ifdef RIG_OP_MCL
	    return RIG_OP_MCL;
#else
	    goto not_there;
#endif
	}
    case 'N':
	if (strEQ(name + 6, "_NONE")) {	/* RIG_OP removed */
#ifdef RIG_OP_NONE
	    return RIG_OP_NONE;
#else
	    goto not_there;
#endif
	}
    case 'R':
	if (strEQ(name + 6, "_RIGHT")) {	/* RIG_OP removed */
#ifdef RIG_OP_RIGHT
	    return RIG_OP_RIGHT;
#else
	    goto not_there;
#endif
	}
    case 'T':
	if (strEQ(name + 6, "_TO_VFO")) {	/* RIG_OP removed */
#ifdef RIG_OP_TO_VFO
	    return RIG_OP_TO_VFO;
#else
	    goto not_there;
#endif
	}
    case 'U':
	if (strEQ(name + 6, "_UP")) {	/* RIG_OP removed */
#ifdef RIG_OP_UP
	    return RIG_OP_UP;
#else
	    goto not_there;
#endif
	}
    case 'X':
	if (strEQ(name + 6, "_XCHG")) {	/* RIG_OP removed */
#ifdef RIG_OP_XCHG
	    return RIG_OP_XCHG;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_O(char *name, int len, int arg)
{
    switch (name[5 + 0]) {
    case 'K':
	if (strEQ(name + 5, "K")) {	/* RIG_O removed */
#ifdef RIG_OK
	    return RIG_OK;
#else
	    goto not_there;
#endif
	}
    case 'P':
	return constant_RIG_OP(name, len, arg);
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_PARM_A(char *name, int len, int arg)
{
    switch (name[10 + 0]) {
    case 'N':
	if (strEQ(name + 10, "NN")) {	/* RIG_PARM_A removed */
#ifdef RIG_PARM_ANN
	    return RIG_PARM_ANN;
#else
	    goto not_there;
#endif
	}
    case 'P':
	if (strEQ(name + 10, "PO")) {	/* RIG_PARM_A removed */
#ifdef RIG_PARM_APO
	    return RIG_PARM_APO;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_PARM_BA(char *name, int len, int arg)
{
    switch (name[11 + 0]) {
    case 'C':
	if (strEQ(name + 11, "CKLIGHT")) {	/* RIG_PARM_BA removed */
#ifdef RIG_PARM_BACKLIGHT
	    return RIG_PARM_BACKLIGHT;
#else
	    goto not_there;
#endif
	}
    case 'T':
	if (strEQ(name + 11, "T")) {	/* RIG_PARM_BA removed */
#ifdef RIG_PARM_BAT
	    return RIG_PARM_BAT;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_PARM_B(char *name, int len, int arg)
{
    switch (name[10 + 0]) {
    case 'A':
	return constant_RIG_PARM_BA(name, len, arg);
    case 'E':
	if (strEQ(name + 10, "EEP")) {	/* RIG_PARM_B removed */
#ifdef RIG_PARM_BEEP
	    return RIG_PARM_BEEP;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_PAR(char *name, int len, int arg)
{
    if (7 + 2 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[7 + 2]) {
    case 'A':
	if (!strnEQ(name + 7,"M_", 2))
	    break;
	return constant_RIG_PARM_A(name, len, arg);
    case 'B':
	if (!strnEQ(name + 7,"M_", 2))
	    break;
	return constant_RIG_PARM_B(name, len, arg);
    case 'F':
	if (strEQ(name + 7, "M_FLOAT_LIST")) {	/* RIG_PAR removed */
#ifdef RIG_PARM_FLOAT_LIST
	    return RIG_PARM_FLOAT_LIST;
#else
	    goto not_there;
#endif
	}
    case 'N':
	if (strEQ(name + 7, "M_NONE")) {	/* RIG_PAR removed */
#ifdef RIG_PARM_NONE
	    return RIG_PARM_NONE;
#else
	    goto not_there;
#endif
	}
    case 'R':
	if (strEQ(name + 7, "M_READONLY_LIST")) {	/* RIG_PAR removed */
#ifdef RIG_PARM_READONLY_LIST
	    return RIG_PARM_READONLY_LIST;
#else
	    goto not_there;
#endif
	}
    case 'T':
	if (strEQ(name + 7, "M_TIME")) {	/* RIG_PAR removed */
#ifdef RIG_PARM_TIME
	    return RIG_PARM_TIME;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_P(char *name, int len, int arg)
{
    if (5 + 1 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[5 + 1]) {
    case 'R':
	if (!strnEQ(name + 5,"A", 1))
	    break;
	return constant_RIG_PAR(name, len, arg);
    case 'S':
	if (strEQ(name + 5, "ASSBAND_NORMAL")) {	/* RIG_P removed */
#ifdef RIG_PASSBAND_NORMAL
	    return RIG_PASSBAND_NORMAL;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_A(char *name, int len, int arg)
{
    if (5 + 3 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[5 + 3]) {
    case 'A':
	if (strEQ(name + 5, "NN_ALL")) {	/* RIG_A removed */
#ifdef RIG_ANN_ALL
	    return RIG_ANN_ALL;
#else
	    goto not_there;
#endif
	}
    case 'F':
	if (strEQ(name + 5, "NN_FREQ")) {	/* RIG_A removed */
#ifdef RIG_ANN_FREQ
	    return RIG_ANN_FREQ;
#else
	    goto not_there;
#endif
	}
    case 'N':
	if (strEQ(name + 5, "NN_NONE")) {	/* RIG_A removed */
#ifdef RIG_ANN_NONE
	    return RIG_ANN_NONE;
#else
	    goto not_there;
#endif
	}
    case 'O':
	if (strEQ(name + 5, "NN_OFF")) {	/* RIG_A removed */
#ifdef RIG_ANN_OFF
	    return RIG_ANN_OFF;
#else
	    goto not_there;
#endif
	}
    case 'R':
	if (strEQ(name + 5, "NN_RXMODE")) {	/* RIG_A removed */
#ifdef RIG_ANN_RXMODE
	    return RIG_ANN_RXMODE;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_SCAN_P(char *name, int len, int arg)
{
    if (10 + 1 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[10 + 1]) {
    case 'I':
	if (strEQ(name + 10, "RIO")) {	/* RIG_SCAN_P removed */
#ifdef RIG_SCAN_PRIO
	    return RIG_SCAN_PRIO;
#else
	    goto not_there;
#endif
	}
    case 'O':
	if (strEQ(name + 10, "ROG")) {	/* RIG_SCAN_P removed */
#ifdef RIG_SCAN_PROG
	    return RIG_SCAN_PROG;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_SCAN_S(char *name, int len, int arg)
{
    switch (name[10 + 0]) {
    case 'L':
	if (strEQ(name + 10, "LCT")) {	/* RIG_SCAN_S removed */
#ifdef RIG_SCAN_SLCT
	    return RIG_SCAN_SLCT;
#else
	    goto not_there;
#endif
	}
    case 'T':
	if (strEQ(name + 10, "TOP")) {	/* RIG_SCAN_S removed */
#ifdef RIG_SCAN_STOP
	    return RIG_SCAN_STOP;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_SC(char *name, int len, int arg)
{
    if (6 + 3 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[6 + 3]) {
    case 'D':
	if (strEQ(name + 6, "AN_DELTA")) {	/* RIG_SC removed */
#ifdef RIG_SCAN_DELTA
	    return RIG_SCAN_DELTA;
#else
	    goto not_there;
#endif
	}
    case 'M':
	if (strEQ(name + 6, "AN_MEM")) {	/* RIG_SC removed */
#ifdef RIG_SCAN_MEM
	    return RIG_SCAN_MEM;
#else
	    goto not_there;
#endif
	}
    case 'N':
	if (strEQ(name + 6, "AN_NONE")) {	/* RIG_SC removed */
#ifdef RIG_SCAN_NONE
	    return RIG_SCAN_NONE;
#else
	    goto not_there;
#endif
	}
    case 'P':
	if (!strnEQ(name + 6,"AN_", 3))
	    break;
	return constant_RIG_SCAN_P(name, len, arg);
    case 'S':
	if (!strnEQ(name + 6,"AN_", 3))
	    break;
	return constant_RIG_SCAN_S(name, len, arg);
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_S(char *name, int len, int arg)
{
    switch (name[5 + 0]) {
    case 'C':
	return constant_RIG_SC(name, len, arg);
    case 'E':
	if (strEQ(name + 5, "ETTING_MAX")) {	/* RIG_S removed */
#ifdef RIG_SETTING_MAX
	    return RIG_SETTING_MAX;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_CONF_C(char *name, int len, int arg)
{
    switch (name[10 + 0]) {
    case 'H':
	if (strEQ(name + 10, "HECKBUTTON")) {	/* RIG_CONF_C removed */
#ifdef RIG_CONF_CHECKBUTTON
	    return RIG_CONF_CHECKBUTTON;
#else
	    goto not_there;
#endif
	}
    case 'O':
	if (strEQ(name + 10, "OMBO")) {	/* RIG_CONF_C removed */
#ifdef RIG_CONF_COMBO
	    return RIG_CONF_COMBO;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_CON(char *name, int len, int arg)
{
    if (7 + 2 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[7 + 2]) {
    case 'C':
	if (!strnEQ(name + 7,"F_", 2))
	    break;
	return constant_RIG_CONF_C(name, len, arg);
    case 'N':
	if (strEQ(name + 7, "F_NUMERIC")) {	/* RIG_CON removed */
#ifdef RIG_CONF_NUMERIC
	    return RIG_CONF_NUMERIC;
#else
	    goto not_there;
#endif
	}
    case 'S':
	if (strEQ(name + 7, "F_STRING")) {	/* RIG_CON removed */
#ifdef RIG_CONF_STRING
	    return RIG_CONF_STRING;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_CO(char *name, int len, int arg)
{
    switch (name[6 + 0]) {
    case 'M':
	if (strEQ(name + 6, "MBO_MAX")) {	/* RIG_CO removed */
#ifdef RIG_COMBO_MAX
	    return RIG_COMBO_MAX;
#else
	    goto not_there;
#endif
	}
    case 'N':
	return constant_RIG_CON(name, len, arg);
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_CT(char *name, int len, int arg)
{
    if (6 + 3 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[6 + 3]) {
    case 'M':
	if (strEQ(name + 6, "RL_MAIN")) {	/* RIG_CT removed */
#ifdef RIG_CTRL_MAIN
	    return RIG_CTRL_MAIN;
#else
	    goto not_there;
#endif
	}
    case 'S':
	if (strEQ(name + 6, "RL_SUB")) {	/* RIG_CT removed */
#ifdef RIG_CTRL_SUB
	    return RIG_CTRL_SUB;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_C(char *name, int len, int arg)
{
    switch (name[5 + 0]) {
    case 'O':
	return constant_RIG_CO(name, len, arg);
    case 'T':
	return constant_RIG_CT(name, len, arg);
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_TYPE_T(char *name, int len, int arg)
{
    if (10 + 1 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[10 + 1]) {
    case 'A':
	if (strEQ(name + 10, "RANSCEIVER")) {	/* RIG_TYPE_T removed */
#ifdef RIG_TYPE_TRANSCEIVER
	    return RIG_TYPE_TRANSCEIVER;
#else
	    goto not_there;
#endif
	}
    case 'U':
	if (strEQ(name + 10, "RUNKSCANNER")) {	/* RIG_TYPE_T removed */
#ifdef RIG_TYPE_TRUNKSCANNER
	    return RIG_TYPE_TRUNKSCANNER;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_TYPE_M(char *name, int len, int arg)
{
    switch (name[10 + 0]) {
    case 'A':
	if (strEQ(name + 10, "ASK")) {	/* RIG_TYPE_M removed */
#ifdef RIG_TYPE_MASK
	    return RIG_TYPE_MASK;
#else
	    goto not_there;
#endif
	}
    case 'O':
	if (strEQ(name + 10, "OBILE")) {	/* RIG_TYPE_M removed */
#ifdef RIG_TYPE_MOBILE
	    return RIG_TYPE_MOBILE;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_TY(char *name, int len, int arg)
{
    if (6 + 3 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[6 + 3]) {
    case 'C':
	if (strEQ(name + 6, "PE_COMPUTER")) {	/* RIG_TY removed */
#ifdef RIG_TYPE_COMPUTER
	    return RIG_TYPE_COMPUTER;
#else
	    goto not_there;
#endif
	}
    case 'H':
	if (strEQ(name + 6, "PE_HANDHELD")) {	/* RIG_TY removed */
#ifdef RIG_TYPE_HANDHELD
	    return RIG_TYPE_HANDHELD;
#else
	    goto not_there;
#endif
	}
    case 'M':
	if (!strnEQ(name + 6,"PE_", 3))
	    break;
	return constant_RIG_TYPE_M(name, len, arg);
    case 'O':
	if (strEQ(name + 6, "PE_OTHER")) {	/* RIG_TY removed */
#ifdef RIG_TYPE_OTHER
	    return RIG_TYPE_OTHER;
#else
	    goto not_there;
#endif
	}
    case 'P':
	if (strEQ(name + 6, "PE_PCRECEIVER")) {	/* RIG_TY removed */
#ifdef RIG_TYPE_PCRECEIVER
	    return RIG_TYPE_PCRECEIVER;
#else
	    goto not_there;
#endif
	}
    case 'R':
	if (strEQ(name + 6, "PE_RECEIVER")) {	/* RIG_TY removed */
#ifdef RIG_TYPE_RECEIVER
	    return RIG_TYPE_RECEIVER;
#else
	    goto not_there;
#endif
	}
    case 'S':
	if (strEQ(name + 6, "PE_SCANNER")) {	/* RIG_TY removed */
#ifdef RIG_TYPE_SCANNER
	    return RIG_TYPE_SCANNER;
#else
	    goto not_there;
#endif
	}
    case 'T':
	if (!strnEQ(name + 6,"PE_", 3))
	    break;
	return constant_RIG_TYPE_T(name, len, arg);
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_TA(char *name, int len, int arg)
{
    if (6 + 9 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[6 + 9]) {
    case 'A':
	if (strEQ(name + 6, "RGETABLE_ALL")) {	/* RIG_TA removed */
#ifdef RIG_TARGETABLE_ALL
	    return RIG_TARGETABLE_ALL;
#else
	    goto not_there;
#endif
	}
    case 'F':
	if (strEQ(name + 6, "RGETABLE_FREQ")) {	/* RIG_TA removed */
#ifdef RIG_TARGETABLE_FREQ
	    return RIG_TARGETABLE_FREQ;
#else
	    goto not_there;
#endif
	}
    case 'M':
	if (strEQ(name + 6, "RGETABLE_MODE")) {	/* RIG_TA removed */
#ifdef RIG_TARGETABLE_MODE
	    return RIG_TARGETABLE_MODE;
#else
	    goto not_there;
#endif
	}
    case 'N':
	if (strEQ(name + 6, "RGETABLE_NONE")) {	/* RIG_TA removed */
#ifdef RIG_TARGETABLE_NONE
	    return RIG_TARGETABLE_NONE;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_TR(char *name, int len, int arg)
{
    if (6 + 2 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[6 + 2]) {
    case 'O':
	if (strEQ(name + 6, "N_OFF")) {	/* RIG_TR removed */
#ifdef RIG_TRN_OFF
	    return RIG_TRN_OFF;
#else
	    goto not_there;
#endif
	}
    case 'P':
	if (strEQ(name + 6, "N_POLL")) {	/* RIG_TR removed */
#ifdef RIG_TRN_POLL
	    return RIG_TRN_POLL;
#else
	    goto not_there;
#endif
	}
    case 'R':
	if (strEQ(name + 6, "N_RIG")) {	/* RIG_TR removed */
#ifdef RIG_TRN_RIG
	    return RIG_TRN_RIG;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_T(char *name, int len, int arg)
{
    switch (name[5 + 0]) {
    case 'A':
	return constant_RIG_TA(name, len, arg);
    case 'R':
	return constant_RIG_TR(name, len, arg);
    case 'Y':
	return constant_RIG_TY(name, len, arg);
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_EN(char *name, int len, int arg)
{
    switch (name[6 + 0]) {
    case 'A':
	if (strEQ(name + 6, "AVAIL")) {	/* RIG_EN removed */
#ifdef RIG_ENAVAIL
	    return RIG_ENAVAIL;
#else
	    goto not_there;
#endif
	}
    case 'I':
	if (strEQ(name + 6, "IMPL")) {	/* RIG_EN removed */
#ifdef RIG_ENIMPL
	    return RIG_ENIMPL;
#else
	    goto not_there;
#endif
	}
    case 'O':
	if (strEQ(name + 6, "OMEM")) {	/* RIG_EN removed */
#ifdef RIG_ENOMEM
	    return RIG_ENOMEM;
#else
	    goto not_there;
#endif
	}
    case 'T':
	if (strEQ(name + 6, "TARGET")) {	/* RIG_EN removed */
#ifdef RIG_ENTARGET
	    return RIG_ENTARGET;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_EIN(char *name, int len, int arg)
{
    switch (name[7 + 0]) {
    case 'T':
	if (strEQ(name + 7, "TERNAL")) {	/* RIG_EIN removed */
#ifdef RIG_EINTERNAL
	    return RIG_EINTERNAL;
#else
	    goto not_there;
#endif
	}
    case 'V':
	if (strEQ(name + 7, "VAL")) {	/* RIG_EIN removed */
#ifdef RIG_EINVAL
	    return RIG_EINVAL;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_EI(char *name, int len, int arg)
{
    switch (name[6 + 0]) {
    case 'N':
	return constant_RIG_EIN(name, len, arg);
    case 'O':
	if (strEQ(name + 6, "O")) {	/* RIG_EI removed */
#ifdef RIG_EIO
	    return RIG_EIO;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_ET(char *name, int len, int arg)
{
    switch (name[6 + 0]) {
    case 'I':
	if (strEQ(name + 6, "IMEOUT")) {	/* RIG_ET removed */
#ifdef RIG_ETIMEOUT
	    return RIG_ETIMEOUT;
#else
	    goto not_there;
#endif
	}
    case 'R':
	if (strEQ(name + 6, "RUNC")) {	/* RIG_ET removed */
#ifdef RIG_ETRUNC
	    return RIG_ETRUNC;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_E(char *name, int len, int arg)
{
    switch (name[5 + 0]) {
    case 'C':
	if (strEQ(name + 5, "CONF")) {	/* RIG_E removed */
#ifdef RIG_ECONF
	    return RIG_ECONF;
#else
	    goto not_there;
#endif
	}
    case 'I':
	return constant_RIG_EI(name, len, arg);
    case 'N':
	return constant_RIG_EN(name, len, arg);
    case 'P':
	if (strEQ(name + 5, "PROTO")) {	/* RIG_E removed */
#ifdef RIG_EPROTO
	    return RIG_EPROTO;
#else
	    goto not_there;
#endif
	}
    case 'R':
	if (strEQ(name + 5, "RJCTED")) {	/* RIG_E removed */
#ifdef RIG_ERJCTED
	    return RIG_ERJCTED;
#else
	    goto not_there;
#endif
	}
    case 'T':
	return constant_RIG_ET(name, len, arg);
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_VFO_A(char *name, int len, int arg)
{
    switch (name[9 + 0]) {
    case '\0':
	if (strEQ(name + 9, "")) {	/* RIG_VFO_A removed */
#ifdef RIG_VFO_A
	    return RIG_VFO_A;
#else
	    goto not_there;
#endif
	}
    case 'L':
	if (strEQ(name + 9, "LL")) {	/* RIG_VFO_A removed */
#ifdef RIG_VFO_ALL
	    return RIG_VFO_ALL;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_VFO_C(char *name, int len, int arg)
{
    switch (name[9 + 0]) {
    case '\0':
	if (strEQ(name + 9, "")) {	/* RIG_VFO_C removed */
#ifdef RIG_VFO_C
	    return RIG_VFO_C;
#else
	    goto not_there;
#endif
	}
    case 'U':
	if (strEQ(name + 9, "URR")) {	/* RIG_VFO_C removed */
#ifdef RIG_VFO_CURR
	    return RIG_VFO_CURR;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_VFO_M(char *name, int len, int arg)
{
    switch (name[9 + 0]) {
    case 'A':
	if (strEQ(name + 9, "AIN")) {	/* RIG_VFO_M removed */
#ifdef RIG_VFO_MAIN
	    return RIG_VFO_MAIN;
#else
	    goto not_there;
#endif
	}
    case 'E':
	if (strEQ(name + 9, "EM")) {	/* RIG_VFO_M removed */
#ifdef RIG_VFO_MEM
	    return RIG_VFO_MEM;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_VFO_(char *name, int len, int arg)
{
    switch (name[8 + 0]) {
    case 'A':
	return constant_RIG_VFO_A(name, len, arg);
    case 'B':
	if (strEQ(name + 8, "B")) {	/* RIG_VFO_ removed */
#ifdef RIG_VFO_B
	    return RIG_VFO_B;
#else
	    goto not_there;
#endif
	}
    case 'C':
	return constant_RIG_VFO_C(name, len, arg);
    case 'M':
	return constant_RIG_VFO_M(name, len, arg);
    case 'N':
	if (strEQ(name + 8, "NONE")) {	/* RIG_VFO_ removed */
#ifdef RIG_VFO_NONE
	    return RIG_VFO_NONE;
#else
	    goto not_there;
#endif
	}
    case 'S':
	if (strEQ(name + 8, "SUB")) {	/* RIG_VFO_ removed */
#ifdef RIG_VFO_SUB
	    return RIG_VFO_SUB;
#else
	    goto not_there;
#endif
	}
    case 'V':
	if (strEQ(name + 8, "VFO")) {	/* RIG_VFO_ removed */
#ifdef RIG_VFO_VFO
	    return RIG_VFO_VFO;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_V(char *name, int len, int arg)
{
    if (5 + 2 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[5 + 2]) {
    case '1':
	if (strEQ(name + 5, "FO1")) {	/* RIG_V removed */
#ifdef RIG_VFO1
	    return RIG_VFO1;
#else
	    goto not_there;
#endif
	}
    case '2':
	if (strEQ(name + 5, "FO2")) {	/* RIG_V removed */
#ifdef RIG_VFO2
	    return RIG_VFO2;
#else
	    goto not_there;
#endif
	}
    case '_':
	if (!strnEQ(name + 5,"FO", 2))
	    break;
	return constant_RIG_VFO_(name, len, arg);
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_FLAG_TRA(char *name, int len, int arg)
{
    if (12 + 2 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[12 + 2]) {
    case 'C':
	if (strEQ(name + 12, "NSCEIVER")) {	/* RIG_FLAG_TRA removed */
#ifdef RIG_FLAG_TRANSCEIVER
	    return RIG_FLAG_TRANSCEIVER;
#else
	    goto not_there;
#endif
	}
    case 'M':
	if (strEQ(name + 12, "NSMITTER")) {	/* RIG_FLAG_TRA removed */
#ifdef RIG_FLAG_TRANSMITTER
	    return RIG_FLAG_TRANSMITTER;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_FLAG_TR(char *name, int len, int arg)
{
    switch (name[11 + 0]) {
    case 'A':
	return constant_RIG_FLAG_TRA(name, len, arg);
    case 'U':
	if (strEQ(name + 11, "UNKING")) {	/* RIG_FLAG_TR removed */
#ifdef RIG_FLAG_TRUNKING
	    return RIG_FLAG_TRUNKING;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_FLAG_T(char *name, int len, int arg)
{
    switch (name[10 + 0]) {
    case 'N':
	if (strEQ(name + 10, "NC")) {	/* RIG_FLAG_T removed */
#ifdef RIG_FLAG_TNC
	    return RIG_FLAG_TNC;
#else
	    goto not_there;
#endif
	}
    case 'R':
	return constant_RIG_FLAG_TR(name, len, arg);
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_FL(char *name, int len, int arg)
{
    if (6 + 3 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[6 + 3]) {
    case 'A':
	if (strEQ(name + 6, "AG_APRS")) {	/* RIG_FL removed */
#ifdef RIG_FLAG_APRS
	    return RIG_FLAG_APRS;
#else
	    goto not_there;
#endif
	}
    case 'C':
	if (strEQ(name + 6, "AG_COMPUTER")) {	/* RIG_FL removed */
#ifdef RIG_FLAG_COMPUTER
	    return RIG_FLAG_COMPUTER;
#else
	    goto not_there;
#endif
	}
    case 'D':
	if (strEQ(name + 6, "AG_DXCLUSTER")) {	/* RIG_FL removed */
#ifdef RIG_FLAG_DXCLUSTER
	    return RIG_FLAG_DXCLUSTER;
#else
	    goto not_there;
#endif
	}
    case 'H':
	if (strEQ(name + 6, "AG_HANDHELD")) {	/* RIG_FL removed */
#ifdef RIG_FLAG_HANDHELD
	    return RIG_FLAG_HANDHELD;
#else
	    goto not_there;
#endif
	}
    case 'M':
	if (strEQ(name + 6, "AG_MOBILE")) {	/* RIG_FL removed */
#ifdef RIG_FLAG_MOBILE
	    return RIG_FLAG_MOBILE;
#else
	    goto not_there;
#endif
	}
    case 'R':
	if (strEQ(name + 6, "AG_RECEIVER")) {	/* RIG_FL removed */
#ifdef RIG_FLAG_RECEIVER
	    return RIG_FLAG_RECEIVER;
#else
	    goto not_there;
#endif
	}
    case 'S':
	if (strEQ(name + 6, "AG_SCANNER")) {	/* RIG_FL removed */
#ifdef RIG_FLAG_SCANNER
	    return RIG_FLAG_SCANNER;
#else
	    goto not_there;
#endif
	}
    case 'T':
	if (!strnEQ(name + 6,"AG_", 3))
	    break;
	return constant_RIG_FLAG_T(name, len, arg);
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_FUNC_N(char *name, int len, int arg)
{
    switch (name[10 + 0]) {
    case 'B':
	if (strEQ(name + 10, "B")) {	/* RIG_FUNC_N removed */
#ifdef RIG_FUNC_NB
	    return RIG_FUNC_NB;
#else
	    goto not_there;
#endif
	}
    case 'O':
	if (strEQ(name + 10, "ONE")) {	/* RIG_FUNC_N removed */
#ifdef RIG_FUNC_NONE
	    return RIG_FUNC_NONE;
#else
	    goto not_there;
#endif
	}
    case 'R':
	if (strEQ(name + 10, "R")) {	/* RIG_FUNC_N removed */
#ifdef RIG_FUNC_NR
	    return RIG_FUNC_NR;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_FUNC_A(char *name, int len, int arg)
{
    switch (name[10 + 0]) {
    case 'B':
	if (strEQ(name + 10, "BM")) {	/* RIG_FUNC_A removed */
#ifdef RIG_FUNC_ABM
	    return RIG_FUNC_ABM;
#else
	    goto not_there;
#endif
	}
    case 'I':
	if (strEQ(name + 10, "IP")) {	/* RIG_FUNC_A removed */
#ifdef RIG_FUNC_AIP
	    return RIG_FUNC_AIP;
#else
	    goto not_there;
#endif
	}
    case 'N':
	if (strEQ(name + 10, "NF")) {	/* RIG_FUNC_A removed */
#ifdef RIG_FUNC_ANF
	    return RIG_FUNC_ANF;
#else
	    goto not_there;
#endif
	}
    case 'P':
	if (strEQ(name + 10, "PF")) {	/* RIG_FUNC_A removed */
#ifdef RIG_FUNC_APF
	    return RIG_FUNC_APF;
#else
	    goto not_there;
#endif
	}
    case 'R':
	if (strEQ(name + 10, "RO")) {	/* RIG_FUNC_A removed */
#ifdef RIG_FUNC_ARO
	    return RIG_FUNC_ARO;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_FUNC_R(char *name, int len, int arg)
{
    switch (name[10 + 0]) {
    case 'E':
	if (strEQ(name + 10, "EV")) {	/* RIG_FUNC_R removed */
#ifdef RIG_FUNC_REV
	    return RIG_FUNC_REV;
#else
	    goto not_there;
#endif
	}
    case 'N':
	if (strEQ(name + 10, "NF")) {	/* RIG_FUNC_R removed */
#ifdef RIG_FUNC_RNF
	    return RIG_FUNC_RNF;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_FUNC_S(char *name, int len, int arg)
{
    switch (name[10 + 0]) {
    case 'B':
	if (strEQ(name + 10, "BKIN")) {	/* RIG_FUNC_S removed */
#ifdef RIG_FUNC_SBKIN
	    return RIG_FUNC_SBKIN;
#else
	    goto not_there;
#endif
	}
    case 'Q':
	if (strEQ(name + 10, "QL")) {	/* RIG_FUNC_S removed */
#ifdef RIG_FUNC_SQL
	    return RIG_FUNC_SQL;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_FUNC_T(char *name, int len, int arg)
{
    switch (name[10 + 0]) {
    case 'O':
	if (strEQ(name + 10, "ONE")) {	/* RIG_FUNC_T removed */
#ifdef RIG_FUNC_TONE
	    return RIG_FUNC_TONE;
#else
	    goto not_there;
#endif
	}
    case 'S':
	if (strEQ(name + 10, "SQL")) {	/* RIG_FUNC_T removed */
#ifdef RIG_FUNC_TSQL
	    return RIG_FUNC_TSQL;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_FUNC_V(char *name, int len, int arg)
{
    switch (name[10 + 0]) {
    case 'O':
	if (strEQ(name + 10, "OX")) {	/* RIG_FUNC_V removed */
#ifdef RIG_FUNC_VOX
	    return RIG_FUNC_VOX;
#else
	    goto not_there;
#endif
	}
    case 'S':
	if (strEQ(name + 10, "SC")) {	/* RIG_FUNC_V removed */
#ifdef RIG_FUNC_VSC
	    return RIG_FUNC_VSC;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_FUNC_F(char *name, int len, int arg)
{
    switch (name[10 + 0]) {
    case 'A':
	if (strEQ(name + 10, "AGC")) {	/* RIG_FUNC_F removed */
#ifdef RIG_FUNC_FAGC
	    return RIG_FUNC_FAGC;
#else
	    goto not_there;
#endif
	}
    case 'B':
	if (strEQ(name + 10, "BKIN")) {	/* RIG_FUNC_F removed */
#ifdef RIG_FUNC_FBKIN
	    return RIG_FUNC_FBKIN;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_FUNC_L(char *name, int len, int arg)
{
    switch (name[10 + 0]) {
    case 'M':
	if (strEQ(name + 10, "MP")) {	/* RIG_FUNC_L removed */
#ifdef RIG_FUNC_LMP
	    return RIG_FUNC_LMP;
#else
	    goto not_there;
#endif
	}
    case 'O':
	if (strEQ(name + 10, "OCK")) {	/* RIG_FUNC_L removed */
#ifdef RIG_FUNC_LOCK
	    return RIG_FUNC_LOCK;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_FUNC_M(char *name, int len, int arg)
{
    switch (name[10 + 0]) {
    case 'B':
	if (strEQ(name + 10, "BC")) {	/* RIG_FUNC_M removed */
#ifdef RIG_FUNC_MBC
	    return RIG_FUNC_MBC;
#else
	    goto not_there;
#endif
	}
    case 'N':
	if (strEQ(name + 10, "N")) {	/* RIG_FUNC_M removed */
#ifdef RIG_FUNC_MN
	    return RIG_FUNC_MN;
#else
	    goto not_there;
#endif
	}
    case 'O':
	if (strEQ(name + 10, "ON")) {	/* RIG_FUNC_M removed */
#ifdef RIG_FUNC_MON
	    return RIG_FUNC_MON;
#else
	    goto not_there;
#endif
	}
    case 'U':
	if (strEQ(name + 10, "UTE")) {	/* RIG_FUNC_M removed */
#ifdef RIG_FUNC_MUTE
	    return RIG_FUNC_MUTE;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_FU(char *name, int len, int arg)
{
    if (6 + 3 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[6 + 3]) {
    case 'A':
	if (!strnEQ(name + 6,"NC_", 3))
	    break;
	return constant_RIG_FUNC_A(name, len, arg);
    case 'B':
	if (strEQ(name + 6, "NC_BC")) {	/* RIG_FU removed */
#ifdef RIG_FUNC_BC
	    return RIG_FUNC_BC;
#else
	    goto not_there;
#endif
	}
    case 'C':
	if (strEQ(name + 6, "NC_COMP")) {	/* RIG_FU removed */
#ifdef RIG_FUNC_COMP
	    return RIG_FUNC_COMP;
#else
	    goto not_there;
#endif
	}
    case 'F':
	if (!strnEQ(name + 6,"NC_", 3))
	    break;
	return constant_RIG_FUNC_F(name, len, arg);
    case 'L':
	if (!strnEQ(name + 6,"NC_", 3))
	    break;
	return constant_RIG_FUNC_L(name, len, arg);
    case 'M':
	if (!strnEQ(name + 6,"NC_", 3))
	    break;
	return constant_RIG_FUNC_M(name, len, arg);
    case 'N':
	if (!strnEQ(name + 6,"NC_", 3))
	    break;
	return constant_RIG_FUNC_N(name, len, arg);
    case 'R':
	if (!strnEQ(name + 6,"NC_", 3))
	    break;
	return constant_RIG_FUNC_R(name, len, arg);
    case 'S':
	if (!strnEQ(name + 6,"NC_", 3))
	    break;
	return constant_RIG_FUNC_S(name, len, arg);
    case 'T':
	if (!strnEQ(name + 6,"NC_", 3))
	    break;
	return constant_RIG_FUNC_T(name, len, arg);
    case 'V':
	if (!strnEQ(name + 6,"NC_", 3))
	    break;
	return constant_RIG_FUNC_V(name, len, arg);
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_F(char *name, int len, int arg)
{
    switch (name[5 + 0]) {
    case 'L':
	return constant_RIG_FL(name, len, arg);
    case 'R':
	if (strEQ(name + 5, "REQ_NONE")) {	/* RIG_F removed */
#ifdef RIG_FREQ_NONE
	    return RIG_FREQ_NONE;
#else
	    goto not_there;
#endif
	}
    case 'U':
	return constant_RIG_FU(name, len, arg);
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_I(char *name, int len, int arg)
{
    if (5 + 9 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[5 + 9]) {
    case '1':
	if (strEQ(name + 5, "TU_REGION1")) {	/* RIG_I removed */
#ifdef RIG_ITU_REGION1
	    return RIG_ITU_REGION1;
#else
	    goto not_there;
#endif
	}
    case '2':
	if (strEQ(name + 5, "TU_REGION2")) {	/* RIG_I removed */
#ifdef RIG_ITU_REGION2
	    return RIG_ITU_REGION2;
#else
	    goto not_there;
#endif
	}
    case '3':
	if (strEQ(name + 5, "TU_REGION3")) {	/* RIG_I removed */
#ifdef RIG_ITU_REGION3
	    return RIG_ITU_REGION3;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_LEVEL_NO(char *name, int len, int arg)
{
    switch (name[12 + 0]) {
    case 'N':
	if (strEQ(name + 12, "NE")) {	/* RIG_LEVEL_NO removed */
#ifdef RIG_LEVEL_NONE
	    return RIG_LEVEL_NONE;
#else
	    goto not_there;
#endif
	}
    case 'T':
	if (strEQ(name + 12, "TCHF")) {	/* RIG_LEVEL_NO removed */
#ifdef RIG_LEVEL_NOTCHF
	    return RIG_LEVEL_NOTCHF;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_LEVEL_N(char *name, int len, int arg)
{
    switch (name[11 + 0]) {
    case 'O':
	return constant_RIG_LEVEL_NO(name, len, arg);
    case 'R':
	if (strEQ(name + 11, "R")) {	/* RIG_LEVEL_N removed */
#ifdef RIG_LEVEL_NR
	    return RIG_LEVEL_NR;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_LEVEL_PB(char *name, int len, int arg)
{
    if (12 + 2 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[12 + 2]) {
    case 'I':
	if (strEQ(name + 12, "T_IN")) {	/* RIG_LEVEL_PB removed */
#ifdef RIG_LEVEL_PBT_IN
	    return RIG_LEVEL_PBT_IN;
#else
	    goto not_there;
#endif
	}
    case 'O':
	if (strEQ(name + 12, "T_OUT")) {	/* RIG_LEVEL_PB removed */
#ifdef RIG_LEVEL_PBT_OUT
	    return RIG_LEVEL_PBT_OUT;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_LEVEL_P(char *name, int len, int arg)
{
    switch (name[11 + 0]) {
    case 'B':
	return constant_RIG_LEVEL_PB(name, len, arg);
    case 'R':
	if (strEQ(name + 11, "REAMP")) {	/* RIG_LEVEL_P removed */
#ifdef RIG_LEVEL_PREAMP
	    return RIG_LEVEL_PREAMP;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_LEVEL_A(char *name, int len, int arg)
{
    switch (name[11 + 0]) {
    case 'F':
	if (strEQ(name + 11, "F")) {	/* RIG_LEVEL_A removed */
#ifdef RIG_LEVEL_AF
	    return RIG_LEVEL_AF;
#else
	    goto not_there;
#endif
	}
    case 'G':
	if (strEQ(name + 11, "GC")) {	/* RIG_LEVEL_A removed */
#ifdef RIG_LEVEL_AGC
	    return RIG_LEVEL_AGC;
#else
	    goto not_there;
#endif
	}
    case 'L':
	if (strEQ(name + 11, "LC")) {	/* RIG_LEVEL_A removed */
#ifdef RIG_LEVEL_ALC
	    return RIG_LEVEL_ALC;
#else
	    goto not_there;
#endif
	}
    case 'P':
	if (strEQ(name + 11, "PF")) {	/* RIG_LEVEL_A removed */
#ifdef RIG_LEVEL_APF
	    return RIG_LEVEL_APF;
#else
	    goto not_there;
#endif
	}
    case 'T':
	if (strEQ(name + 11, "TT")) {	/* RIG_LEVEL_A removed */
#ifdef RIG_LEVEL_ATT
	    return RIG_LEVEL_ATT;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_LEVEL_RF(char *name, int len, int arg)
{
    switch (name[12 + 0]) {
    case '\0':
	if (strEQ(name + 12, "")) {	/* RIG_LEVEL_RF removed */
#ifdef RIG_LEVEL_RF
	    return RIG_LEVEL_RF;
#else
	    goto not_there;
#endif
	}
    case 'P':
	if (strEQ(name + 12, "POWER")) {	/* RIG_LEVEL_RF removed */
#ifdef RIG_LEVEL_RFPOWER
	    return RIG_LEVEL_RFPOWER;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_LEVEL_R(char *name, int len, int arg)
{
    switch (name[11 + 0]) {
    case 'E':
	if (strEQ(name + 11, "EADONLY_LIST")) {	/* RIG_LEVEL_R removed */
#ifdef RIG_LEVEL_READONLY_LIST
	    return RIG_LEVEL_READONLY_LIST;
#else
	    goto not_there;
#endif
	}
    case 'F':
	return constant_RIG_LEVEL_RF(name, len, arg);
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_LEVEL_B(char *name, int len, int arg)
{
    switch (name[11 + 0]) {
    case 'A':
	if (strEQ(name + 11, "ALANCE")) {	/* RIG_LEVEL_B removed */
#ifdef RIG_LEVEL_BALANCE
	    return RIG_LEVEL_BALANCE;
#else
	    goto not_there;
#endif
	}
    case 'K':
	if (strEQ(name + 11, "KINDL")) {	/* RIG_LEVEL_B removed */
#ifdef RIG_LEVEL_BKINDL
	    return RIG_LEVEL_BKINDL;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_LEVEL_SQ(char *name, int len, int arg)
{
    if (12 + 1 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[12 + 1]) {
    case '\0':
	if (strEQ(name + 12, "L")) {	/* RIG_LEVEL_SQ removed */
#ifdef RIG_LEVEL_SQL
	    return RIG_LEVEL_SQL;
#else
	    goto not_there;
#endif
	}
    case 'S':
	if (strEQ(name + 12, "LSTAT")) {	/* RIG_LEVEL_SQ removed */
#ifdef RIG_LEVEL_SQLSTAT
	    return RIG_LEVEL_SQLSTAT;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_LEVEL_S(char *name, int len, int arg)
{
    switch (name[11 + 0]) {
    case 'Q':
	return constant_RIG_LEVEL_SQ(name, len, arg);
    case 'T':
	if (strEQ(name + 11, "TRENGTH")) {	/* RIG_LEVEL_S removed */
#ifdef RIG_LEVEL_STRENGTH
	    return RIG_LEVEL_STRENGTH;
#else
	    goto not_there;
#endif
	}
    case 'W':
	if (strEQ(name + 11, "WR")) {	/* RIG_LEVEL_S removed */
#ifdef RIG_LEVEL_SWR
	    return RIG_LEVEL_SWR;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_LEVEL_C(char *name, int len, int arg)
{
    switch (name[11 + 0]) {
    case 'O':
	if (strEQ(name + 11, "OMP")) {	/* RIG_LEVEL_C removed */
#ifdef RIG_LEVEL_COMP
	    return RIG_LEVEL_COMP;
#else
	    goto not_there;
#endif
	}
    case 'W':
	if (strEQ(name + 11, "WPITCH")) {	/* RIG_LEVEL_C removed */
#ifdef RIG_LEVEL_CWPITCH
	    return RIG_LEVEL_CWPITCH;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_LEVEL_M(char *name, int len, int arg)
{
    switch (name[11 + 0]) {
    case 'E':
	if (strEQ(name + 11, "ETER")) {	/* RIG_LEVEL_M removed */
#ifdef RIG_LEVEL_METER
	    return RIG_LEVEL_METER;
#else
	    goto not_there;
#endif
	}
    case 'I':
	if (strEQ(name + 11, "ICGAIN")) {	/* RIG_LEVEL_M removed */
#ifdef RIG_LEVEL_MICGAIN
	    return RIG_LEVEL_MICGAIN;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_L(char *name, int len, int arg)
{
    if (5 + 5 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[5 + 5]) {
    case 'A':
	if (!strnEQ(name + 5,"EVEL_", 5))
	    break;
	return constant_RIG_LEVEL_A(name, len, arg);
    case 'B':
	if (!strnEQ(name + 5,"EVEL_", 5))
	    break;
	return constant_RIG_LEVEL_B(name, len, arg);
    case 'C':
	if (!strnEQ(name + 5,"EVEL_", 5))
	    break;
	return constant_RIG_LEVEL_C(name, len, arg);
    case 'F':
	if (strEQ(name + 5, "EVEL_FLOAT_LIST")) {	/* RIG_L removed */
#ifdef RIG_LEVEL_FLOAT_LIST
	    return RIG_LEVEL_FLOAT_LIST;
#else
	    goto not_there;
#endif
	}
    case 'I':
	if (strEQ(name + 5, "EVEL_IF")) {	/* RIG_L removed */
#ifdef RIG_LEVEL_IF
	    return RIG_LEVEL_IF;
#else
	    goto not_there;
#endif
	}
    case 'K':
	if (strEQ(name + 5, "EVEL_KEYSPD")) {	/* RIG_L removed */
#ifdef RIG_LEVEL_KEYSPD
	    return RIG_LEVEL_KEYSPD;
#else
	    goto not_there;
#endif
	}
    case 'M':
	if (!strnEQ(name + 5,"EVEL_", 5))
	    break;
	return constant_RIG_LEVEL_M(name, len, arg);
    case 'N':
	if (!strnEQ(name + 5,"EVEL_", 5))
	    break;
	return constant_RIG_LEVEL_N(name, len, arg);
    case 'P':
	if (!strnEQ(name + 5,"EVEL_", 5))
	    break;
	return constant_RIG_LEVEL_P(name, len, arg);
    case 'R':
	if (!strnEQ(name + 5,"EVEL_", 5))
	    break;
	return constant_RIG_LEVEL_R(name, len, arg);
    case 'S':
	if (!strnEQ(name + 5,"EVEL_", 5))
	    break;
	return constant_RIG_LEVEL_S(name, len, arg);
    case 'V':
	if (strEQ(name + 5, "EVEL_VOX")) {	/* RIG_L removed */
#ifdef RIG_LEVEL_VOX
	    return RIG_LEVEL_VOX;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_M(char *name, int len, int arg)
{
    if (5 + 4 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[5 + 4]) {
    case 'A':
	if (strEQ(name + 5, "ODE_AM")) {	/* RIG_M removed */
#ifdef RIG_MODE_AM
	    return RIG_MODE_AM;
#else
	    goto not_there;
#endif
	}
    case 'C':
	if (strEQ(name + 5, "ODE_CW")) {	/* RIG_M removed */
#ifdef RIG_MODE_CW
	    return RIG_MODE_CW;
#else
	    goto not_there;
#endif
	}
    case 'F':
	if (strEQ(name + 5, "ODE_FM")) {	/* RIG_M removed */
#ifdef RIG_MODE_FM
	    return RIG_MODE_FM;
#else
	    goto not_there;
#endif
	}
    case 'L':
	if (strEQ(name + 5, "ODE_LSB")) {	/* RIG_M removed */
#ifdef RIG_MODE_LSB
	    return RIG_MODE_LSB;
#else
	    goto not_there;
#endif
	}
    case 'N':
	if (strEQ(name + 5, "ODE_NONE")) {	/* RIG_M removed */
#ifdef RIG_MODE_NONE
	    return RIG_MODE_NONE;
#else
	    goto not_there;
#endif
	}
    case 'R':
	if (strEQ(name + 5, "ODE_RTTY")) {	/* RIG_M removed */
#ifdef RIG_MODE_RTTY
	    return RIG_MODE_RTTY;
#else
	    goto not_there;
#endif
	}
    case 'S':
	if (strEQ(name + 5, "ODE_SSB")) {	/* RIG_M removed */
#ifdef RIG_MODE_SSB
	    return RIG_MODE_SSB;
#else
	    goto not_there;
#endif
	}
    case 'U':
	if (strEQ(name + 5, "ODE_USB")) {	/* RIG_M removed */
#ifdef RIG_MODE_USB
	    return RIG_MODE_USB;
#else
	    goto not_there;
#endif
	}
    case 'W':
	if (strEQ(name + 5, "ODE_WFM")) {	/* RIG_M removed */
#ifdef RIG_MODE_WFM
	    return RIG_MODE_WFM;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_RIG_(char *name, int len, int arg)
{
    switch (name[4 + 0]) {
    case 'A':
	return constant_RIG_A(name, len, arg);
    case 'C':
	return constant_RIG_C(name, len, arg);
    case 'E':
	return constant_RIG_E(name, len, arg);
    case 'F':
	return constant_RIG_F(name, len, arg);
    case 'I':
	return constant_RIG_I(name, len, arg);
    case 'L':
	return constant_RIG_L(name, len, arg);
    case 'M':
	return constant_RIG_M(name, len, arg);
    case 'O':
	return constant_RIG_O(name, len, arg);
    case 'P':
	return constant_RIG_P(name, len, arg);
    case 'S':
	return constant_RIG_S(name, len, arg);
    case 'T':
	return constant_RIG_T(name, len, arg);
    case 'V':
	return constant_RIG_V(name, len, arg);
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_R(char *name, int len, int arg)
{
    if (1 + 2 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[1 + 2]) {
    case 'N':
	if (strEQ(name + 1, "IGNAMSIZ")) {	/* R removed */
#ifdef RIGNAMSIZ
	    return RIGNAMSIZ;
#else
	    goto not_there;
#endif
	}
    case 'V':
	if (strEQ(name + 1, "IGVERSIZ")) {	/* R removed */
#ifdef RIGVERSIZ
	    return RIGVERSIZ;
#else
	    goto not_there;
#endif
	}
    case '_':
	if (!strnEQ(name + 1,"IG", 2))
	    break;
	return constant_RIG_(name, len, arg);
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant_M(char *name, int len, int arg)
{
    if (1 + 2 >= len ) {
	errno = EINVAL;
	return 0;
    }
    switch (name[1 + 2]) {
    case 'C':
	if (strEQ(name + 1, "AXCHANDESC")) {	/* M removed */
#ifdef MAXCHANDESC
	    return MAXCHANDESC;
#else
	    goto not_there;
#endif
	}
    case 'D':
	if (strEQ(name + 1, "AXDBLSTSIZ")) {	/* M removed */
#ifdef MAXDBLSTSIZ
	    return MAXDBLSTSIZ;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

static double
constant(char *name, int len, int arg)
{
    errno = 0;
    switch (name[0 + 0]) {
    case 'C':
	if (strEQ(name + 0, "CHANLSTSIZ")) {	/*  removed */
#ifdef CHANLSTSIZ
	    return CHANLSTSIZ;
#else
	    goto not_there;
#endif
	}
    case 'F':
	return constant_F(name, len, arg);
    case 'M':
	return constant_M(name, len, arg);
    case 'R':
	return constant_R(name, len, arg);
    case 'T':
	if (strEQ(name + 0, "TSLSTSIZ")) {	/*  removed */
#ifdef TSLSTSIZ
	    return TSLSTSIZ;
#else
	    goto not_there;
#endif
	}
    case '_':
	return constant__(name, len, arg);
    case 'p':
	if (strEQ(name + 0, "ptr_t")) {	/*  removed */
#ifdef ptr_t
	    return ptr_t;
#else
	    goto not_there;
#endif
	}
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

MODULE = Hamlib		PACKAGE = Hamlib

double
constant(sv,arg)
    PROTOTYPE: $$
    PREINIT:
    STRLEN      len;
    INPUT:
    SV *        sv
    char *      s = SvPV(sv, len);
    int     arg
    CODE:
    RETVAL = constant(s,len,arg);
    OUTPUT:
    RETVAL

MODULE = Hamlib		PACKAGE = Hamlib::Rig		PREFIX = rig_

double
constant(sv,arg)
    PROTOTYPE: $$
    PREINIT:
    STRLEN      len;
    INPUT:
    SV *        sv
    char *      s = SvPV(sv, len);
    int     arg
    CODE:
    RETVAL = constant(s,len,arg);
    OUTPUT:
    RETVAL

Hamlib::Rig
rig_init(rig_model)
    rig_model_t rig_model
        PROTOTYPE: $
    CODE:
    {
        RIG * theRig;
        theRig = rig_init(rig_model);
        RETVAL = theRig;
    }
	POST_CALL:
		if (RETVAL == NULL)
			croak("rig_init error");
    OUTPUT:
        RETVAL

void
rig_DESTROY(rig)
    Hamlib::Rig   rig
    PROTOTYPE: $
    CODE:
    {
        rig_cleanup(rig);
    }

int
rig_open(rig)
	Hamlib::Rig	rig
    PROTOTYPE: $
	CODE:
		{
        RETVAL = rig_open(rig);
		}
	POST_CALL:
		if (RETVAL != RIG_OK)
			croak("rig_open error: '%s'", rigerror(RETVAL));
    OUTPUT:
        RETVAL

int
rig_close(rig)
	Hamlib::Rig	rig
        PROTOTYPE: $
    CODE:
    {
        RETVAL = rig_close(rig);
    }
    OUTPUT:
        RETVAL

int
rig_cleanup(rig)
	Hamlib::Rig	rig

int
rig_set_vfo(rig, vfo)
	Hamlib::Rig	rig
	vfo_t	vfo

int
rig_get_vfo(rig)
	Hamlib::Rig	rig
    CODE:
    {
	    vfo_t vfo;
        rig_get_vfo(rig, &vfo);
        RETVAL = vfo;
    }
    OUTPUT:
        RETVAL


int
rig_set_freq(rig, freq, vfo = RIG_VFO_CURR)
	  Hamlib::Rig	rig
	  freq_t	freq
	  vfo_t	vfo
	C_ARGS:
	  rig, vfo, freq

freq_t
rig_get_freq(rig, vfo = RIG_VFO_CURR)
	  Hamlib::Rig	rig
	  vfo_t	vfo
    CODE:
    {
	    freq_t freq;
        rig_get_freq(rig, vfo, &freq);
        RETVAL = freq;
    }
    OUTPUT:
        RETVAL

int
rig_set_mode(rig, mode, width = RIG_PASSBAND_NORMAL, vfo = RIG_VFO_CURR)
	Hamlib::Rig	rig
	rmode_t	mode
	pbwidth_t	width
	vfo_t	vfo
	C_ARGS:
	  rig, vfo, mode, width

void
rig_get_mode(rig, vfo = RIG_VFO_CURR)
	Hamlib::Rig	rig
	vfo_t	vfo
    PPCODE:
    {
		rmode_t	mode;
		pbwidth_t	width;
        rig_get_mode(rig, vfo, &mode, &width);
		EXTEND(sp,2);
		PUSHs(sv_2mortal(newSVuv(mode)));
		PUSHs(sv_2mortal(newSViv(width)));
    }

int
rig_set_split_mode(rig, mode, width = RIG_PASSBAND_NORMAL, vfo = RIG_VFO_CURR)
	Hamlib::Rig	rig
	rmode_t	mode
	pbwidth_t	width
	vfo_t	vfo
	C_ARGS:
	  rig, vfo, mode, width

int
rig_get_split_mode(rig, vfo = RIG_VFO_CURR)
	Hamlib::Rig	rig
	vfo_t	vfo
    CODE:
    {
		rmode_t	mode;
		pbwidth_t	width;
        rig_get_split_mode(rig, vfo, &mode, &width);
        RETVAL = mode;
    }

void
rig_debug(debug_level, fmt, ...)
	enum rig_debug_level_e	debug_level
	const char *	fmt

ant_t
rig_get_ant(rig, vfo = RIG_VFO_CURR)
	  Hamlib::Rig	rig
	  vfo_t	vfo
    CODE:
    {
	    ant_t ant;
        rig_get_ant(rig, vfo, &ant);
        RETVAL = ant;
    }
    OUTPUT:
        RETVAL

const struct rig_caps *
rig_get_caps(rig_model)
	rig_model_t	rig_model

int
rig_get_channel(rig, chan)
	RIG *	rig
	channel_t *	chan

int
rig_get_ctcss_sql(rig, vfo = RIG_VFO_CURR)
	  Hamlib::Rig	rig
	  vfo_t	vfo
    CODE:
    {
	    tone_t tone;
        rig_get_ctcss_sql(rig, vfo, &tone);
        RETVAL = tone;
    }
    OUTPUT:
        RETVAL

int
rig_get_ctcss_tone(rig, vfo = RIG_VFO_CURR)
	  Hamlib::Rig	rig
	  vfo_t	vfo
    CODE:
    {
	    tone_t tone;
        rig_get_ctcss_tone(rig, vfo, &tone);
        RETVAL = tone;
    }
    OUTPUT:
        RETVAL

int
rig_get_dcd(rig, vfo = RIG_VFO_CURR)
	  Hamlib::Rig	rig
	  vfo_t	vfo
    CODE:
    {
	    dcd_t dcd;
        rig_get_dcd(rig, vfo, &dcd);
        RETVAL = dcd;
    }
    OUTPUT:
        RETVAL

int
rig_get_dcs_code(rig, vfo = RIG_VFO_CURR)
	  Hamlib::Rig	rig
	  vfo_t	vfo
    CODE:
    {
	    tone_t code;
        rig_get_dcs_code(rig, vfo, &code);
        RETVAL = code;
    }
    OUTPUT:
        RETVAL

int
rig_get_dcs_sql(rig, vfo = RIG_VFO_CURR)
	  Hamlib::Rig	rig
	  vfo_t	vfo
    CODE:
    {
	    tone_t code;
        rig_get_dcs_sql(rig, vfo, &code);
        RETVAL = code;
    }
    OUTPUT:
        RETVAL

int
rig_get_func(rig, func, vfo = RIG_VFO_CURR)
	  Hamlib::Rig	rig
	  setting_t	func
	  vfo_t	vfo
    CODE:
    {
	    int status;
        rig_get_func(rig, vfo, func, &status);
        RETVAL = status;
    }
    OUTPUT:
        RETVAL


const char *
rig_get_info(rig)
	Hamlib::Rig	rig

value_t
rig_get_level(rig, level, vfo = RIG_VFO_CURR)
	  Hamlib::Rig	rig
	  setting_t	level
	  vfo_t	vfo
    CODE:
    {
	value_t	val;
        rig_get_level(rig, vfo, level, &val);
        RETVAL = val;
    }
    OUTPUT:
        RETVAL


int
rig_get_mem(rig, vfo = RIG_VFO_CURR)
	  Hamlib::Rig	rig
	  vfo_t	vfo
    CODE:
    {
	int ch;
        rig_get_mem(rig, vfo, &ch);
        RETVAL = ch;
    }
    OUTPUT:
        RETVAL


value_t
rig_get_parm(rig, parm)
	  Hamlib::Rig	rig
	  setting_t	parm
    CODE:
    {
	value_t	val;
        rig_get_parm(rig, parm, &val);
        RETVAL = val;
    }
    OUTPUT:
        RETVAL


int
rig_get_powerstat(rig)
	  Hamlib::Rig	rig
    CODE:
    {
	    powerstat_t status;
        rig_get_powerstat(rig, &status);
        RETVAL = status;
    }
    OUTPUT:
        RETVAL

int
rig_get_ptt(rig, vfo = RIG_VFO_CURR)
	  Hamlib::Rig	rig
	  vfo_t	vfo
    CODE:
    {
	    ptt_t ptt;
        rig_get_ptt(rig, vfo, &ptt);
        RETVAL = ptt;
    }
    OUTPUT:
        RETVAL

const freq_range_t *
rig_get_range(range_list, freq, mode)
	const freq_range_t *	range_list
	freq_t	freq
	rmode_t	mode

shortfreq_t
rig_get_resolution(rig, mode)
	Hamlib::Rig	rig
	rmode_t	mode

int
rig_get_rit(rig, vfo = RIG_VFO_CURR)
	  Hamlib::Rig	rig
	  vfo_t	vfo
    CODE:
    {
	    shortfreq_t rit;
        rig_get_rit(rig, vfo, &rit);
        RETVAL = rit;
    }
    OUTPUT:
        RETVAL

int
rig_get_rptr_offs(rig, vfo = RIG_VFO_CURR)
	  Hamlib::Rig	rig
	  vfo_t	vfo
    CODE:
    {
	    shortfreq_t rptr_offs;
        rig_get_rptr_offs(rig, vfo, &rptr_offs);
        RETVAL = rptr_offs;
    }
    OUTPUT:
        RETVAL

int
rig_get_rptr_shift(rig, vfo = RIG_VFO_CURR)
	  Hamlib::Rig	rig
	  vfo_t	vfo
    CODE:
    {
	    rptr_shift_t rptr_shift;
        rig_get_rptr_shift(rig, vfo, &rptr_shift);
        RETVAL = rptr_shift;
    }
    OUTPUT:
        RETVAL

split_t
rig_get_split(rig, vfo = RIG_VFO_CURR)
	  Hamlib::Rig	rig
	  vfo_t	vfo
    CODE:
    {
	    split_t split;
        rig_get_split(rig, vfo, &split);
        RETVAL = split;
    }
    OUTPUT:
        RETVAL


int
rig_set_split_freq(rig, tx_freq, vfo = RIG_VFO_CURR)
	  Hamlib::Rig	rig
	  freq_t	tx_freq
	  vfo_t	vfo
	C_ARGS:
	  rig, vfo, tx_freq

freq_t
rig_get_split_freq(rig, vfo = RIG_VFO_CURR)
	  Hamlib::Rig	rig
	  vfo_t	vfo
    CODE:
    {
	    freq_t tx_freq;
        rig_get_freq(rig, vfo, &tx_freq);
        RETVAL = tx_freq;
    }
    OUTPUT:
        RETVAL

int
rig_set_ext_level(rig, name, value, vfo = RIG_VFO_CURR)
	Hamlib::Rig	rig
	const char *	name
	const char *	value
	vfo_t		vfo
    CODE:
    {
		value_t val;
		const struct confparams *cfp;

		cfp = rig_ext_lookup(rig, name);
		if (!cfp)
			return;     /* no such parameter */

		/* FIXME: only RIG_CONF_STRING supported so far */
		val.s = value;
        RETVAL = rig_set_ext_level(rig, vfo, cfp->token, val);
    }
	OUTPUT:
		RETVAL

SV*
rig_get_ext_level(rig, name, vfo = RIG_VFO_CURR)
	Hamlib::Rig	rig
	const char *	name
	vfo_t		vfo
    CODE:
    {
		value_t val;
		char s[256];
		const struct confparams *cfp;

		cfp = rig_ext_lookup(rig, name);
		if (!cfp)
			return;     /* no such parameter */

		/* FIXME: only RIG_CONF_STRING supported so far */
		val.s = s;
        rig_get_ext_level(rig, vfo, cfp->token, &val);
        RETVAL = newSVpv(val.s, 0);
    }
	OUTPUT:
		RETVAL

int
rig_set_ext_parm(rig, name, value)
	Hamlib::Rig	rig
	const char *	name
	const char *	value
    CODE:
    {
		value_t val;
		const struct confparams *cfp;

		cfp = rig_ext_lookup(rig, name);
		if (!cfp)
			return;     /* no such parameter */

		/* FIXME: only RIG_CONF_STRING supported so far */
		val.s = value;
        RETVAL = rig_set_ext_parm(rig, cfp->token, val);
    }
	OUTPUT:
		RETVAL

SV*
rig_get_ext_parm(rig, name)
	Hamlib::Rig	rig
	const char *	name
    CODE:
    {
		value_t val;
		char s[256];
		const struct confparams *cfp;

		cfp = rig_ext_lookup(rig, name);
		if (!cfp)
			return;     /* no such parameter */

		/* FIXME: only RIG_CONF_STRING supported so far */
		val.s = s;
        rig_get_ext_parm(rig, cfp->token, &val);
        RETVAL = newSVpv(val.s, 0);
    }
	OUTPUT:
		RETVAL


int
rig_set_conf(rig, name, val)
	Hamlib::Rig	rig
	const char *	name
	const char *	val
    CODE:
    {
		token_t	token;
		token = rig_token_lookup(rig, name);
        RETVAL = rig_set_conf(rig, token, val);
    }
	OUTPUT:
		RETVAL

SV*
rig_get_conf(rig, name)
	Hamlib::Rig	rig
	const char *	name
    CODE:
    {
		token_t	token;
		char val[256];

		token = rig_token_lookup(rig, name);
        rig_get_conf(rig, token, val);
        RETVAL = newSVpv(val, 0);
    }
	OUTPUT:
		RETVAL

int
rig_get_trn(rig, trn)
	  Hamlib::Rig	rig
    CODE:
    {
	    int trn;
        rig_get_trn(rig, &trn);
        RETVAL = trn;
    }
    OUTPUT:
        RETVAL

int
rig_get_ts(rig, vfo = RIG_VFO_CURR)
	  Hamlib::Rig	rig
	  vfo_t	vfo
    CODE:
    {
	    shortfreq_t ts;
	    freq_t freq;
        rig_get_ts(rig, vfo, &ts);
        RETVAL = ts;
    }
    OUTPUT:
        RETVAL

int
rig_get_xit(rig, vfo = RIG_VFO_CURR)
	  Hamlib::Rig	rig
	  vfo_t	vfo
    CODE:
    {
	    shortfreq_t xit;
        rig_get_xit(rig, vfo, &xit);
        RETVAL = xit;
    }
    OUTPUT:
        RETVAL

setting_t
rig_has_get_func(rig, func)
	Hamlib::Rig	rig
	setting_t	func

setting_t
rig_has_get_level(rig, level)
	Hamlib::Rig	rig
	setting_t	level

setting_t
rig_has_get_parm(rig, parm)
	Hamlib::Rig	rig
	setting_t	parm

scan_t
rig_has_scan(rig, scan)
	Hamlib::Rig	rig
	scan_t	scan

setting_t
rig_has_set_func(rig, func)
	Hamlib::Rig	rig
	setting_t	func

setting_t
rig_has_set_level(rig, level)
	Hamlib::Rig	rig
	setting_t	level

setting_t
rig_has_set_parm(rig, parm)
	Hamlib::Rig	rig
	setting_t	parm

vfo_op_t
rig_has_vfo_op(rig, op)
	Hamlib::Rig	rig
	vfo_op_t	op

int
rig_load_all_backends()

int
rig_load_backend(be_name)
	const char *	be_name

int
rig_mW2power(rig, power, mwpower, freq, mode)
	RIG *	rig
	float *	power
	unsigned int	mwpower
	freq_t	freq
	rmode_t	mode

int
rig_need_debug(debug_level)
	enum rig_debug_level_e	debug_level

pbwidth_t
rig_passband_narrow(rig, mode)
	Hamlib::Rig	rig
	rmode_t	mode

pbwidth_t
rig_passband_normal(rig, mode)
	Hamlib::Rig	rig
	rmode_t	mode

pbwidth_t
rig_passband_wide(rig, mode)
	Hamlib::Rig	rig
	rmode_t	mode

int
rig_power2mW(rig, mwpower, power, freq, mode)
	RIG *	rig
	unsigned int *	mwpower
	float	power
	freq_t	freq
	rmode_t	mode

rig_model_t
rig_probe(p)
	port_t *	p

rig_model_t
rig_probe_all(p)
	port_t *	p

int
rig_recv_dtmf(rig, vfo, digits, length)
	RIG *	rig
	vfo_t	vfo
	char *	digits
	int *	length

int
rig_register(caps)
	const struct rig_caps *	caps

int
rig_reset(rig, reset)
	Hamlib::Rig	rig
	reset_t	reset

int
rig_scan(rig, scan, ch, vfo = RIG_VFO_CURR)
	Hamlib::Rig	rig
	scan_t	scan
	int	ch
	vfo_t	vfo
	C_ARGS:
	  rig, vfo, scan, ch

int
rig_send_dtmf(rig, vfo, digits, vfo = RIG_VFO_CURR)
	Hamlib::Rig	rig
	const char *	digits
	vfo_t	vfo
	C_ARGS:
	  rig, vfo, digits

int
rig_send_morse(rig, vfo, msg, vfo = RIG_VFO_CURR)
	Hamlib::Rig	rig
	const char *	msg
	vfo_t	vfo
	C_ARGS:
	  rig, vfo, msg

int
rig_set_ant(rig, ant, vfo = RIG_VFO_CURR)
	  Hamlib::Rig	rig
	  ant_t ant
	  vfo_t	vfo
	C_ARGS:
	  rig, vfo, ant

int
rig_set_bank(rig, bank, vfo = RIG_VFO_CURR)
	Hamlib::Rig	rig
	int	bank
	vfo_t	vfo
	C_ARGS:
	  rig, vfo, bank

int
rig_set_channel(rig, chan)
	RIG *	rig
	const channel_t *	chan

int
rig_set_ctcss_sql(rig, tone, vfo = RIG_VFO_CURR)
	Hamlib::Rig	rig
	tone_t	tone
	vfo_t	vfo
	C_ARGS:
	  rig, vfo, tone

int
rig_set_ctcss_tone(rig, tone, vfo = RIG_VFO_CURR)
	Hamlib::Rig	rig
	tone_t	tone
	vfo_t	vfo
	C_ARGS:
	  rig, vfo, tone

int
rig_set_dcs_code(rig, code, vfo = RIG_VFO_CURR)
	Hamlib::Rig	rig
	tone_t	code
	vfo_t	vfo
	C_ARGS:
	  rig, vfo, code

int
rig_set_dcs_sql(rig, code, vfo = RIG_VFO_CURR)
	Hamlib::Rig	rig
	tone_t	code
	vfo_t	vfo
	C_ARGS:
	  rig, vfo, code

void
rig_set_debug(debug_level)
	enum rig_debug_level_e	debug_level

int
rig_set_func(rig, func, status, vfo = RIG_VFO_CURR)
	  Hamlib::Rig	rig
	  setting_t	func
	  int	status
	  vfo_t	vfo
	C_ARGS:
	  rig, vfo, func, status

int
rig_set_level(rig, level, val, vfo = RIG_VFO_CURR)
	  Hamlib::Rig	rig
	  setting_t	level
	  value_t	val
	  vfo_t	vfo
	C_ARGS:
	  rig, vfo, level, val

int
rig_set_mem(rig, ch, vfo = RIG_VFO_CURR)
	  Hamlib::Rig	rig
	  int	ch
	  vfo_t	vfo
	C_ARGS:
	  rig, vfo, ch

int
rig_set_parm(rig, parm, val)
	  Hamlib::Rig	rig
	  setting_t	parm
	  value_t	val

int
rig_set_powerstat(rig, status)
	Hamlib::Rig	rig
	powerstat_t	status

int
rig_set_ptt(rig, ptt, vfo = RIG_VFO_CURR)
	Hamlib::Rig	rig
	ptt_t	ptt
	vfo_t	vfo
	C_ARGS:
	  rig, vfo, ptt

int
rig_set_rit(rig, rit, vfo = RIG_VFO_CURR)
	Hamlib::Rig	rig
	shortfreq_t	rit
	vfo_t	vfo
	C_ARGS:
	  rig, vfo, rit

int
rig_set_rptr_offs(rig, rptr_offs, vfo = RIG_VFO_CURR)
	Hamlib::Rig	rig
	shortfreq_t	rptr_offs
	vfo_t	vfo
	C_ARGS:
	  rig, vfo, rptr_offs

int
rig_set_rptr_shift(rig, rptr_shift, vfo = RIG_VFO_CURR)
	Hamlib::Rig	rig
	rptr_shift_t	rptr_shift
	vfo_t	vfo
	C_ARGS:
	  rig, vfo, rptr_shift

int
rig_set_split(rig, split, vfo = RIG_VFO_CURR)
	  Hamlib::Rig	rig
	  split_t	split
	  vfo_t	vfo
	C_ARGS:
	  rig, vfo, split

int
rig_set_trn(rig, trn)
	Hamlib::Rig	rig
	int	trn

int
rig_set_ts(rig, ts, vfo = RIG_VFO_CURR)
	Hamlib::Rig	rig
	shortfreq_t	ts
	vfo_t	vfo
	C_ARGS:
	  rig, vfo, ts

int
rig_set_xit(rig, xit, vfo = RIG_VFO_CURR)
	Hamlib::Rig	rig
	shortfreq_t	xit
	vfo_t	vfo
	C_ARGS:
	  rig, vfo, xit

int
rig_setting2idx(s)
	setting_t	s

int
rig_unregister(rig_model)
	rig_model_t	rig_model

int
rig_vfo_op(rig, op, vfo = RIG_VFO_CURR)
	Hamlib::Rig	rig
	vfo_op_t	op
	vfo_t	vfo
	C_ARGS:
	  rig, vfo, op

const char *
rigerror(errnum)
	int	errnum
