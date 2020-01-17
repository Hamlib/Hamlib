unit hamlib_rigapi;

{ Please note: This here is still some macro's missing. FGR 2002-01-12 }

interface

Uses
    Libc;

{$MINENUMSIZE 4}
{* The $MINENUMSIZE directive controls the
 * minimum storage size of enumerated types. *}

{$UNDEF HAVE_MAGIC_NUMBERS}

{$IFDEF HAVE_MAGIC_NUMBERS}
const
    RIG_MAGIC           = $1234BABE;
    RIG_MAGIC_CAPS      = $11223344;
    RIG_MAGIC_STATE     = $11111112;
    RIG_MAGIC_CALLBACKS = $999A8889;
{$ENDIF}

const
    {*
     * Error codes that can be returned by the Hamlib functions
     *}
    RIG_OK          = 0;          {* completed sucessfully *}
    RIG_EINVAL      = 1;          {* invalid parameter *}
    RIG_ECONF       = 2;          {* invalid configuration (serial,..) *}
    RIG_ENOMEM      = 3;          {* memory shortage *}
    RIG_ENIMPL      = 4;          {* function not implemented, but will be *}
    RIG_ETIMEOUT    = 5;          {* communication timed out *}
    RIG_EIO         = 6;          {* IO error, including open failed *}
    RIG_EINTERNAL   = 7;          {* Internal Hamlib error, huh! *}
    RIG_EPROTO      = 8;          {* Protocol error *}
    RIG_ERJCTED     = 9;          {* Command rejected by the rig *}
    RIG_ETRUNC      = 10;         {* Command performed, but arg truncated *}
    RIG_ENAVAIL     = 11;         {* function not available *}
    RIG_ENTARGET    = 12;         {* VFO not targetable *}


type
    rig_model_t = integer;
    rig_ptr_t = pointer;
    float = single;
    tone_t = cardinal;

{* Forward struct references *}
//struct rig;
//struct rig_state; // how do i do this in pascal?
//typedef struct rig RIG;

const
    RIGNAMSIZ = 30;
    RIGVERSIZ = 8;
    FILPATHLEN = 512;
    FRQRANGESIZ = 30;
    MAXCHANDESC = 30;		{* describe channel eg: "WWV 5Mhz" *}
    TSLSTSIZ = 20;			{* max tuning step list size, zero ended *}
    FLTLSTSIZ = 16;		    {* max mode/filter list size, zero ended *}
    MAXDBLSTSIZ = 8;		{* max preamp/att levels supported, zero ended *}
    CHANLSTSIZ = 16;		{* max mem_list size, zero ended *}

type
    {*
     * REM: Numeric order matters for debug level
     *}
    rig_debug_level_e = (
	    RIG_DEBUG_NONE = 0,
	    RIG_DEBUG_BUG,    {* Should these values not correspond with syslog? *}
	    RIG_DEBUG_ERR,
	    RIG_DEBUG_WARN,
	    RIG_DEBUG_VERBOSE,
	    RIG_DEBUG_TRACE
    );

    {*
     * Rig capabilities
     *
     * Basic rig type, can store some useful
     * info about different radios. Each lib must
     * be able to populate this structure, so we can make
     * useful enquiries about capablilities.
     *}
    rig_port_e = (
	    RIG_PORT_NONE = 0,	{* as bizarre as it could be :) *}
	    RIG_PORT_SERIAL,
	    RIG_PORT_NETWORK,
	    RIG_PORT_DEVICE,	{* Device driver, like the WiNRADiO *}
	    RIG_PORT_PACKET,	{* e.g. SV8CS *}
	    RIG_PORT_DTMF,		{* bridge via another rig, eg. Kenwood Sky Cmd System *}
	    RIG_PORT_ULTRA,		{* IrDA Ultra protocol! *}
	    RIG_PORT_RPC 		{* RPC wrapper *}
    );

    serial_parity_e = (
	    RIG_PARITY_NONE = 0,
	    RIG_PARITY_ODD,
	    RIG_PARITY_EVEN
    );

    serial_handshake_e = (
	    RIG_HANDSHAKE_NONE = 0,
	    RIG_HANDSHAKE_XONXOFF,
	    RIG_HANDSHAKE_HARDWARE
    );

const
    RIG_FLAG_RECEIVER    = 1 shl 1;
    RIG_FLAG_TRANSMITTER = 1 shl 2;
    RIG_FLAG_SCANNER	  = 1 shl 3;

    RIG_FLAG_MOBILE      = 1 shl 4;
    RIG_FLAG_HANDHELD	  = 1 shl 5;
    RIG_FLAG_COMPUTER	  = 1 shl 6;
    RIG_FLAG_TRUNKING	  = 1 shl 7;
    RIG_FLAG_APRS		  = 1 shl 8;
    RIG_FLAG_TNC		  = 1 shl 9;
    RIG_FLAG_DXCLUSTER	  = 1 shl 10;

    RIG_FLAG_TRANSCEIVER  = (RIG_FLAG_RECEIVER or RIG_FLAG_TRANSMITTER);
    RIG_TYPE_MASK         = (RIG_FLAG_TRANSCEIVER or RIG_FLAG_SCANNER or
                             RIG_FLAG_MOBILE or RIG_FLAG_HANDHELD or
                             RIG_FLAG_COMPUTER or RIG_FLAG_TRUNKING);
    RIG_TYPE_OTHER        = 0;
    RIG_TYPE_TRANSCEIVER  = (RIG_FLAG_TRANSCEIVER);
    RIG_TYPE_HANDHELD     = (RIG_FLAG_TRANSCEIVER or RIG_FLAG_HANDHELD);
    RIG_TYPE_MOBILE       = (RIG_FLAG_TRANSCEIVER or RIG_FLAG_MOBILE);
    RIG_TYPE_RECEIVER     = (RIG_FLAG_RECEIVER);
    RIG_TYPE_PCRECEIVER   = (RIG_FLAG_COMPUTER or RIG_FLAG_RECEIVER);
    RIG_TYPE_SCANNER      = (RIG_FLAG_SCANNER or RIG_FLAG_RECEIVER);
    RIG_TYPE_TRUNKSCANNER = (RIG_TYPE_SCANNER or RIG_FLAG_TRUNKING);
    RIG_TYPE_COMPUTER     = (RIG_FLAG_TRANSCEIVER or RIG_FLAG_COMPUTER);

type
    {*
     * Development status of the backend
     *}
    rig_status_e = (
	    RIG_STATUS_ALPHA = 0,
	    RIG_STATUS_UNTESTED,	{* written from specs, rig unavailable for test, feedback most wanted! *}
	    RIG_STATUS_BETA,
	    RIG_STATUS_STABLE,
	    RIG_STATUS_BUGGY,		{* was stable, but something broke it! *}
	    RIG_STATUS_NEW
    );

    rptr_shift_e = (
	    RIG_RPT_SHIFT_NONE = 0,
	    RIG_RPT_SHIFT_MINUS,
	    RIG_RPT_SHIFT_PLUS
    );
    rptr_shift_t = rptr_shift_e;

    split_e = (
	    RIG_SPLIT_OFF = 0,
    	RIG_SPLIT_ON
    );
    split_t = split_e;

    {*
     * freq_t: frequency type in Hz, must be >32bits for SHF!
     * shortfreq_t: frequency on 31bits, suitable for offsets, shifts, etc..
     *}
    freq_t = int64;
    shortfreq_t = longint;

    { TODO: convert macros to functions }
//#define Hz(f)	((freq_t)(f))
//#define kHz(f)	((freq_t)((f)*1000))
//#define MHz(f)	((freq_t)((f)*1000000L))
//#define GHz(f)	((freq_t)((f)*1000000000LL))
//#define RIG_FREQ_NONE Hz(0)

const
    RIG_FREQ_NONE = 0;

    RIG_VFO_CURR  =  0;     {* current "tunable channel"/VFO *}
    RIG_VFO_NONE  =  0;     {* used in caps *}
    RIG_VFO_ALL	  =	-1;		{* apply to all VFO (when used as target) *}

    {*
     * Or should it be better designated
     * as a "tunable channel" (RIG_CTRL_MEM) ? --SF
     *}
    RIG_VFO_MEM	  =	-2;		{* means Memory mode, to be used with set_vfo *}
    RIG_VFO_VFO	  =	-3;		{* means (any)VFO mode, with set_vfo *}

    RIG_VFO1      = (1 shl 0);
    RIG_VFO2      = (1 shl 1);
    RIG_CTRL_MAIN = 1;
    RIG_CTRL_SUB  = 2;

    {*
     * one byte per "tunable channel":
     *
     *    MSB           LSB
     *     8             1
     *    +-+-+-+-+-+-+-+-+
     *    | |             |
     *    CTL     VFO
     *}

function RIG_CTRL_BAND(band,vfo: integer): integer; // macro
function RIG_VFO_A: integer; // macro
function RIG_VFO_B: integer; // macro
function RIG_VFO_C: integer; // macro
function RIG_VFO_MAIN: integer; // macro
function RIG_VFO_SUB: integer; // macro

type
    {*
     * could RIG_VFO_ALL be useful?
     * i.e. apply to all VFO, when used as target
     *}
     vfo_t = integer;

const
    RIG_TARGETABLE_NONE = $00;
    RIG_TARGETABLE_FREQ = $01;
    RIG_TARGETABLE_MODE = $02;
    RIG_TARGETABLE_ALL  = $ffffffff;


//const
//    RIG_PASSBAND_NORMAL = 0;

type
    {*
     * also see rig_passband_normal(rig,mode),
     * 	rig_passband_narrow(rig,mode) and rig_passband_wide(rig,mode)
     *}
    pbwidth_t = shortfreq_t;

    dcd_e = (
	    RIG_DCD_OFF = 0,
	    RIG_DCD_ON);
    dcd_t = dcd_e;

    dcd_type_e = (
    	RIG_DCD_NONE = 0,           {* not available *}
	    RIG_DCD_RIG,                {* i.e. has get_dcd cap *}
	    RIG_DCD_SERIAL_DSR,
	    RIG_DCD_SERIAL_CTS,
	    RIG_DCD_PARALLEL            {* DCD comes from ?? DATA1? STROBE? *}
{$ifdef TODO_MORE_DCD}
	    RIG_DCD_PTT                 {* follow ptt_type, i.e. ptt=RTS -> dcd=CTS on same line --SF *}
{$endif}
    );

    dcd_type_t = dcd_type_e;

    ptt_e = (
	    RIG_PTT_OFF = 0,
	    RIG_PTT_ON);
    ptt_t = ptt_e;

    ptt_type_e = (
        RIG_PTT_NONE = 0,			{* not available *}
        RIG_PTT_RIG,				{* legacy PTT *}
        RIG_PTT_SERIAL_DTR,
        RIG_PTT_SERIAL_RTS,
	    RIG_PTT_PARALLEL);          {* PTT accessed through DATA0 *}
    ptt_type_t = ptt_type_e;



    powerstat_e = (
		RIG_POWER_OFF = 0,
		RIG_POWER_ON,
		RIG_POWER_STANDBY
    );
    powerstat_t = powerstat_e;

    reset_e = (
		RIG_RESET_NONE = 0,
		RIG_RESET_SOFT,
		RIG_RESET_VFO,
		RIG_RESET_MCALL,		(* memory clear *)
		RIG_RESET_MASTER
    );
    reset_t = reset_e;

const
    {* VFO/MEM mode are set by set_vfo *}
    RIG_OP_NONE	    = 0;
    RIG_OP_CPY		= (1 shl 0);	{* VFO A = VFO B *}
    RIG_OP_XCHG		= (1 shl 1);	{* Exchange VFO A/B *}
    RIG_OP_FROM_VFO	= (1 shl 2);	{* VFO->MEM *}
    RIG_OP_TO_VFO	= (1 shl 3);	{* MEM->VFO *}
    RIG_OP_MCL		= (1 shl 4);	{* Memory clear *}
    RIG_OP_UP		= (1 shl 5);	{* UP *}
    RIG_OP_DOWN	    = (1 shl 6);	{* DOWN *}
    RIG_OP_BAND_UP	= (1 shl 7);	{* Band UP *}
    RIG_OP_BAND_DOWN = (1 shl 8);	{* Band DOWN *}

type
    {*
     * RIG_MVOP_DUAL_ON/RIG_MVOP_DUAL_OFF (Dual watch off/Dual watch on)
     * better be set by set_func IMHO,
     * or is it the balance (-> set_level) ? --SF
     *}
    vfo_op_t = longint;

const
    RIG_SCAN_NONE	= 0L;
    RIG_SCAN_STOP   = RIG_SCAN_NONE;
    RIG_SCAN_MEM	= (1 shl 0);   {* Scan all memory channels *}
    RIG_SCAN_SLCT	= (1 shl 1);   {* Scan all selected memory channels *}
    RIG_SCAN_PRIO	= (1 shl 2);   {* Priority watch (mem or call channel) *}
    RIG_SCAN_PROG	= (1 shl 3);   {* Programmed(edge) scan *}
    RIG_SCAN_DELTA	= (1 shl 4);   {* delta-f scan *}

type
    scan_t = longint;
    {*
     * configuration token
     *}
    token_t = longint;

const
    RIG_CONF_END = 0;

//#define RIG_TOKEN_BACKEND(t) (t)
//#define RIG_TOKEN_FRONTEND(t) ((t)|(1<<30))
//#define RIG_IS_TOKEN_FRONTEND(t) ((t)&(1<<30))

    {*
     * strongly inspired from soundmedem. Thanks Thomas!
     *}
    RIG_CONF_STRING = 0;
    RIG_CONF_COMBO = 1;
    RIG_CONF_NUMERIC = 2;
    RIG_CONF_CHECKBUTTON = 3;

    RIG_COMBO_MAX = 8;

type
    confparams = packed record
        token : token_t;
        name : PChar;       {* try to avoid spaces in the name *}
        labelcf : PChar;    // renamed to avoid clash with pascal reserved word
        tooltip : PChar;
        dflt : PChar;
        typecf : longword;  // renamed to avoid clash with pascal reserved word
        u : record
            case integer of
            0: (n: record
                    min: float;
                    max: float;
                    step: float;
                   end; );
            1: (c: record
                    combostr : array[0..RIG_COMBO_MAX-1] of PChar;
                   end; );
            end;
    end;
    TConfParms = confparams;
    PConfParms = ^TConfParms;

const
    {*
     * When optional speech synthesizer is installed
     * what about RIG_ANN_ENG and RIG_ANN_JAPAN? and RIG_ANN_CW?
     *}
    RIG_ANN_NONE   = 0;
    RIG_ANN_OFF    = RIG_ANN_NONE;
    RIG_ANN_FREQ   = (1 shl 0);
    RIG_ANN_RXMODE = (1 shl 1);
    RIG_ANN_ALL    = (1 shl 2);

type
    ann_t = longint;

    {* Antenna number *}
    ant_t = integer;

    agc_level_e = (
		RIG_AGC_OFF = 0,
		RIG_AGC_SUPERFAST,
		RIG_AGC_FAST,
		RIG_AGC_SLOW
    );

    meter_level_e = (
		RIG_METER_NONE = 0,
		RIG_METER_SWR,
		RIG_METER_COMP,
		RIG_METER_ALC,
		RIG_METER_IC,
		RIG_METER_DB
    );

    {*
     * Universal approach for use by set_level/get_level
     *}
    value_u = packed record
        case integer of
        0: (i : integer);
        1: (f : float);
    end;
    value_t = value_u;

const
    RIG_LEVEL_NONE	  =	0ULL;
    RIG_LEVEL_PREAMP  =	(1 shl 0);	{* Preamp, arg int (dB) *}
    RIG_LEVEL_ATT	  =	(1 shl 1);	{* Attenuator, arg int (dB) *}
    RIG_LEVEL_VOX	  =	(1 shl 2);	{* VOX delay, arg int (tenth of seconds) *}
    RIG_LEVEL_AF	  =	(1 shl 3);	{* Volume, arg float [0.0..1.0] *}
    RIG_LEVEL_RF	  =	(1 shl 4);	{* RF gain (not TX power), arg float [0.0..1.0] or in dB ?? -20..20 ? *}
    RIG_LEVEL_SQL	  =	(1 shl 5);	{* Squelch, arg float [0.0 .. 1.0] *}
    RIG_LEVEL_IF	  =	(1 shl 6);	{* IF, arg int (Hz) *}
    RIG_LEVEL_APF	  =	(1 shl 7);	{* APF, arg float [0.0 .. 1.0] *}
    RIG_LEVEL_NR	  =	(1 shl 8);	{* Noise Reduction, arg float [0.0 .. 1.0] *}
    RIG_LEVEL_PBT_IN  =	(1 shl 9);	{* Twin PBT (inside), arg float [0.0 .. 1.0] *}
    RIG_LEVEL_PBT_OUT =	(1 shl 10);	{* Twin PBT (outside), arg float [0.0 .. 1.0] *}
    RIG_LEVEL_CWPITCH =	(1 shl 11);	{* CW pitch, arg int (Hz) *}
    RIG_LEVEL_RFPOWER =	(1 shl 12);	{* RF Power, arg float [0.0 .. 1.0] *}
    RIG_LEVEL_MICGAIN =	(1 shl 13);	{* MIC Gain, arg float [0.0 .. 1.0] *}
    RIG_LEVEL_KEYSPD  =	(1 shl 14);	{* Key Speed, arg int (WPM) *}
    RIG_LEVEL_NOTCHF  =	(1 shl 15);	{* Notch Freq., arg int (Hz) *}
    RIG_LEVEL_COMP	  =	(1 shl 16);	{* Compressor, arg float [0.0 .. 1.0] *}
    RIG_LEVEL_AGC	  =	(1 shl 17);	{* AGC, arg int (see enum agc_level_e) *}
    RIG_LEVEL_BKINDL  =	(1 shl 18);	{* BKin Delay, arg int (tenth of dots) *}
    RIG_LEVEL_BALANCE =	(1 shl 19);	{* Balance (Dual Watch), arg float [0.0 .. 1.0] *}
    RIG_LEVEL_METER	  =	(1 shl 20);	{* Display meter, arg int (see enum meter_level_e) *}

	{* These ones are not settable *}
    RIG_LEVEL_SQLSTAT =	(1 shl 27);	{* SQL status, arg int (open=1/closed=0). Deprecated, use get_dcd instead *}
    RIG_LEVEL_SWR	  =	(1 shl 28);	{* SWR, arg float *}
    RIG_LEVEL_ALC	  =	(1 shl 29);	{* ALC, arg float *}
    RIG_LEVEL_STRENGTH=	(1 shl 30);	{* Signal strength, arg int (dB) *}

    RIG_LEVEL_FLOAT_LIST = (RIG_LEVEL_AF or RIG_LEVEL_RF or
                            RIG_LEVEL_SQL or RIG_LEVEL_APF or
                            RIG_LEVEL_NR or RIG_LEVEL_PBT_IN or
                            RIG_LEVEL_PBT_OUT or RIG_LEVEL_RFPOWER or
                            RIG_LEVEL_MICGAIN or RIG_LEVEL_COMP or
                            RIG_LEVEL_BALANCE or RIG_LEVEL_SWR or
                            RIG_LEVEL_ALC);

    RIG_LEVEL_READONLY_LIST = (RIG_LEVEL_SQLSTAT or RIG_LEVEL_SWR or
                               RIG_LEVEL_ALC or RIG_LEVEL_STRENGTH);

//    RIG_LEVEL_IS_FLOAT(l) ((l)&RIG_LEVEL_FLOAT_LIST)  // macro
//    RIG_LEVEL_SET(l) ((l)&~RIG_LEVEL_READONLY_LIST)   // macro


    {*
     * Parameters are settings that are not VFO specific
     *}
    RIG_PARM_NONE	   = 0;
    RIG_PARM_ANN	   = (1 shl 0);	{* "Announce" level, see ann_t *}
    RIG_PARM_APO	   = (1 shl 1);	{* Auto power off, int in minute *}
    RIG_PARM_BACKLIGHT = (1 shl 2);	{* LCD light, float [0.0..1.0] *}
    RIG_PARM_BEEP	   = (1 shl 4);	{* Beep on keypressed, int (0,1) *}
    RIG_PARM_TIME	   = (1 shl 5);	{* hh:mm:ss, int in seconds from 00:00:00 *}
    RIG_PARM_BAT	   = (1 shl 6);	{* battery level, float [0.0..1.0] *}

    RIG_PARM_FLOAT_LIST = (RIG_PARM_BACKLIGHT or RIG_PARM_BAT);
    RIG_PARM_READONLY_LIST = (RIG_PARM_BAT);

//    RIG_PARM_IS_FLOAT(l) ((l)&RIG_PARM_FLOAT_LIST)    // macro
//    RIG_PARM_SET(l) ((l)&~RIG_PARM_READONLY_LIST)     // macro

    RIG_SETTING_MAX = 64;

type
    setting_t = int64;          {* hope 64 bits will be enough.. *}


const
    {*
     * tranceive mode, ie. the rig notify the host of any event,
     * like freq changed, mode changed, etc.
     *}
    RIG_TRN_OFF  = 0;
    RIG_TRN_RIG  = 1;
    RIG_TRN_POLL = 2;

    {*
     * These are activated functions.
     *}
    RIG_FUNC_NONE     =	0;
    RIG_FUNC_FAGC     =	(1 shl 0);		{* Fast AGC *}
    RIG_FUNC_NB	      =	(1 shl 1);		{* Noise Blanker *}
    RIG_FUNC_COMP     =	(1 shl 2);		{* Compression *}
    RIG_FUNC_VOX      =	(1 shl 3);		{* VOX *}
    RIG_FUNC_TONE     =	(1 shl 4);		{* Tone *}
    RIG_FUNC_TSQL     =	(1 shl 5);		{* may require a tone field *}
    RIG_FUNC_SBKIN    =	(1 shl 6);		{* Semi Break-in *}
    RIG_FUNC_FBKIN    =	(1 shl 7);		{* Full Break-in, for CW mode *}
    RIG_FUNC_ANF      =	(1 shl 8);		{* Automatic Notch Filter (DSP); *}
    RIG_FUNC_NR       =	(1 shl 9);		{* Noise Reduction (DSP); *}
    RIG_FUNC_AIP      =	(1 shl 10);		{* AIP (Kenwood); *}
    RIG_FUNC_APF      =	(1 shl 11);		{* Auto Passband Filter *}
    RIG_FUNC_MON      =	(1 shl 12);		{* Monitor transmitted signal, != rev *}
    RIG_FUNC_MN       =	(1 shl 13);		{* Manual Notch (Icom); *}
    RIG_FUNC_RF       =	(1 shl 14);		{* RTTY Filter (Icom);  TNX AD7AI -- N0NB *}
    RIG_FUNC_ARO      =	(1 shl 15);		{* Auto Repeater Offset *}
    RIG_FUNC_LOCK     =	(1 shl 16);		{* Lock *}
    RIG_FUNC_MUTE     =	(1 shl 17);		{* Mute, could be emulated by LEVEL_AF*}
    RIG_FUNC_VSC      =	(1 shl 18);		{* Voice Scan Control *}
    RIG_FUNC_REV      =	(1 shl 19);		{* Reverse tx and rx freqs *}
    RIG_FUNC_SQL      =	(1 shl 20);		{* Turn Squelch Monitor on/off*}
    RIG_FUNC_ABM      =	(1 shl 21);		{* Auto Band Mode *}
    RIG_FUNC_BC       =	(1 shl 22);		{* Beat Canceller *}
    RIG_FUNC_MBC      =	(1 shl 23);		{* Manual Beat Canceller *}
    RIG_FUNC_LMP      = (1 shl 24);     {* LCD lamp ON/OFF *}

    {*
     * power unit macros, converts to mW
     * This is limited to 2MW on 32 bits systems.
     *}
//#define mW(p)	 ((int)(p))             // macro
//#define Watts(p) ((int)((p)*1000))    // macro
//#define W(p)	 Watts(p)               // macro
//#define kW(p)	 ((int)((p)*1000000L))  // macro

type
     rmode_t = cardinal;	{* radio mode  *}

const
    {*
     * Do not use an enum since this will be used w/ rig_mode_t bit fields.
     * Also, how should CW reverse sideband and RTTY reverse
     * sideband be handled?
     *}
    RIG_MODE_NONE =	0;
    RIG_MODE_AM   =	(1 shl 0);
    RIG_MODE_CW   =	(1 shl 1);
    RIG_MODE_USB  =	(1 shl 2);	 {* select somewhere else the filters ? *}
    RIG_MODE_LSB  =	(1 shl 3);
    RIG_MODE_RTTY =	(1 shl 4);
    RIG_MODE_FM   =	(1 shl 5);
    RIG_MODE_WFM  =	(1 shl 6);	{* after all, Wide FM is a mode on its own *}

{* macro for backends, no to be used by rig_set_mode et al. *}
    RIG_MODE_SSB  = (RIG_MODE_USB or RIG_MODE_LSB);

//#define RIG_DBLST_END  0		{* end marker in a preamp/att level list *}
//#define RIG_IS_DBLST_END(d) ((d)==0)  // macro

type
    {*
     * Put together a bunch of this struct in an array to define
     * what your rig have access to
     *}
    freq_range_list = packed record
        start: freq_t;
        endfq: freq_t;  // Renamed to avoid clash with pascal reserved name
        modes: rmode_t;	     {* bitwise OR'ed RIG_MODE_* *}
        low_power : integer; {* in mW, -1 for no power (ie. rx list) *}
        high_power: integer; {* in mW, -1 for no power (ie. rx list) *}
        vfo : vfo_t;         {* VFOs that can access this range *}
        ant : ant_t;
    end;
    freq_range_t = freq_range_list;

//#define RIG_FRNG_END     {Hz(0),Hz(0),RIG_MODE_NONE,0,0,RIG_VFO_NONE}
//#define RIG_IS_FRNG_END(r) ((r).start == Hz(0) && (r).end == Hz(0))

const
    RIG_ITU_REGION1 = 1;
    RIG_ITU_REGION2 = 2;
    RIG_ITU_REGION3 = 3;

type
    {*
     * Lists the tuning steps available for each mode
     *}
    tuning_step_list = packed record
        modes : rmode_t;	    {* bitwise OR'ed RIG_MODE_* *}
        ts : shortfreq_t;		{* tuning step in Hz *}
    end;

//#define RIG_TS_END     {RIG_MODE_NONE,0}
//#define RIG_IS_TS_END(t)	((t).modes == RIG_MODE_NONE && (t).ts == 0) // macro

    {*
     * Lists the filters available for each mode
     * If more than one filter is available for a given mode,
     * the first entry in the array will be the default
     * filter to use for this mode (cf rig_set_mode).
     *}
    filter_list = packed record
        modes : rmode_t;	    {* bitwise OR'ed RIG_MODE_* *}
        width : pbwidth_t;		{* passband width in Hz *}
    end;

//#define RIG_FLT_END     {RIG_MODE_NONE,0}
//#define RIG_IS_FLT_END(f)	((f).modes == RIG_MODE_NONE)




    {*
     * Convenience struct, describes a freq/vfo/mode combo
     * Also useful for memory handling -- FS
     *
     * TODO: skip flag, split, shift, etc.
     *}
    channel = packed record
        channel_num : integer;
        freq : freq_t;
        mode : rmode_t;
        width : pbwidth_t;
        tx_freq : freq_t;
        tx_mode : rmode_t;
        tx_width : pbwidth_t;
        split : split_t;
        rptr_shift : rptr_shift_t;
        rptr_offs : shortfreq_t;
        vfo : vfo_t;

        ant : integer;  {* antenna number *}
        tuning_step : shortfreq_t;
        rit : shortfreq_t;
        xit : shortfreq_t;
        funcs : setting_t;
        levels : array[0..RIG_SETTING_MAX-1] of value_t;
        ctcss_tone : tone_t;
        ctcss_sql : tone_t;
        dcs_code : tone_t;
        dcs_sql : tone_t;
        channel_desc : array[0..MAXCHANDESC-1] of char;
    end;
    channel_t = channel;

    (*
     * chan_t is used to describe what memory your rig is equipped with
     * cf. chan_list field in caps
     * Example for the Ic706MkIIG (99 memory channels, 2 scan edges, 2 call chans):
     * 	chan_t chan_list[] = {
     * 		{ 1, 99, RIG_MTYPE_MEM, 0 },
     * 		{ 100, 103, RIG_MTYPE_EDGE, 0 },
     * 		{ 104, 105, RIG_MTYPE_CALL, 0 },
     * 		RIG_CHAN_END
     * 	}
     *)
    chan_type_e = (
		RIG_MTYPE_NONE=0,
		RIG_MTYPE_MEM,			{* regular *}
		RIG_MTYPE_EDGE,			{* scan edge *}
		RIG_MTYPE_CALL,			{* call channel *}
		RIG_MTYPE_MEMOPAD,		{* inaccessible on Icom, what about others? *}
		RIG_MTYPE_SAT			{* satellite *}
    );

    chan_list = packed record
		start : integer;        {* rig memory channel _number_ *}
		endch : integer;        // Note: two members renamed due to name conflict with pascal reserved word.
		typech : chan_type_e;   {* among EDGE, MEM, CALL, .. *}
        reserved : integer;     {* don't know yet, maybe something like flags *}
    end;
    chan_t = chan_list;

//#define RIG_CHAN_END     {0,0,RIG_MTYPE_NONE,0}
//#define RIG_IS_CHAN_END(c)	((c).type == RIG_MTYPE_NONE)


{* Basic rig type, can store some useful
 * info about different radios. Each lib must
 * be able to populate this structure, so we can make
 * useful enquiries about capablilities.
 *}

{*
 * The main idea of this struct is that it will be defined by the backend
 * rig driver, and will remain readonly for the application.
 * Fields that need to be modifiable by the application are
 * copied into the struct rig_state, which is a kind of private
 * of the RIG instance.
 * This way, you can have several rigs running within the same application,
 * sharing the struct rig_caps of the backend, while keeping their own
 * customized data.
 * NB: don't move fields around, as backend depends on it when initializing
 * 		their caps.
 *}


type
    PRig = ^TRig;   // forward declaration

    rig_caps = packed record
        rig_model : rig_model_t;

        model_name : PChar;
        mfg_name : PChar;
        version : PChar;
        copyright : PChar;
        status : rig_status_e;

        rig_type : integer;
        ptt_type : ptt_type_e;
        dcd_type : dcd_type_e;
        port_type : rig_port_e;


        serial_rate_min : integer;
        serial_rate_max : integer;
        serial_data_bits : integer;
        serial_stop_bits : integer;
        serial_parity : serial_parity_e;
        serial_handshake : serial_handshake_e;

        write_delay : integer;
        post_write_delay : integer;
        timeout : integer;
        retry : integer;

        has_get_func : setting_t;
        has_set_func : setting_t;
        has_get_level : setting_t;
        has_set_level : setting_t;
        has_get_parm : setting_t;
        has_set_parm : setting_t;

        level_gran : array[0..RIG_SETTING_MAX-1] of integer;
        parm_gran : array[0..RIG_SETTING_MAX-1] of integer;

        ctcss_list : ^tone_t;   // ?? How do I make it a const ?
        dcs_list : ^tone_t;

        preamp : array[0..MAXDBLSTSIZ-1] of integer;
        attenuator : array[0..MAXDBLSTSIZ-1] of integer;

        max_rit : shortfreq_t;
        max_xit : shortfreq_t;
        max_ifshift : shortfreq_t;

        announces : ann_t;
        vfo_ops : vfo_op_t;
        scan_ops : scan_t;
        targetable_vfo : integer;
        transceive : integer;

        bank_qty : integer;
        chan_desc_sz : integer;

        chan_list : array[0..CHANLSTSIZ-1] of chan_t;
        rx_range_list1 : array[0..FRQRANGESIZ-1] of freq_range_t;  {* ITU region 1 *}
        tx_range_list1 : array[0..FRQRANGESIZ-1] of freq_range_t;
        rx_range_list2 : array[0..FRQRANGESIZ-1] of freq_range_t;  {* ITU region 2 *}
        tx_range_list2 : array[0..FRQRANGESIZ-1] of freq_range_t;

        tuning_steps : array[0..TSLSTSIZ-1] of tuning_step_list;

        filters : array[0..FLTLSTSIZ-1] of filter_list;	{* mode/filter table, at -6dB *}

        cfgparams : PConfparms;
        priv : rig_ptr_t;

        {*
         * Rig Admin API
         *
         *}
        rig_init : function(rig: PRig): integer; cdecl;
        rig_open : function(rig: PRig): integer; cdecl;
        rig_close : function(rig: PRig): integer; cdecl;
        rig_cleanup : function(rig: PRig): integer; cdecl;


        {*
         *  General API commands, from most primitive to least.. :()
         *  List Set/Get functions pairs
         *}
        set_freq : function(rig: PRig; vfo: vfo_t; freq: freq_t): integer; cdecl;
        get_freq : function(rig: PRig; vfo: vfo_t; var freq: freq_t): integer; cdecl;

        set_mode : function(rig: PRig; vfo: vfo_t; mode: rmode_t; width: pbwidth_t): integer; cdecl;
        get_mode : function(rig: PRig; vfo: vfo_t; var mode: rmode_t; var width: pbwidth_t): integer; cdecl;

        set_vfo : function(rig: PRig; vfo: vfo_t): integer; cdecl;
        get_vfo : function(rig: PRig; var vfo: vfo_t): integer; cdecl;

        set_ptt : function(rig: PRig; vfo: vfo_t; ptt: ptt_t): integer; cdecl;
        get_ptt : function(rig: PRig; vfo: vfo_t; var ptt: ptt_t): integer; cdecl;
        get_dcd : function(rig: PRig; vfo: vfo_t; var dcd: dcd_t): integer; cdecl;

        set_rptr_shift : function(rig: PRig; vfo: vfo_t; rptr_shift: rptr_shift_t): integer; cdecl;
        get_rptr_shift : function(rig: PRig; vfo: vfo_t; var rptr_shift: rptr_shift_t): integer; cdecl;

        set_rptr_offs : function(rig: PRig; vfo: vfo_t; offs: shortfreq_t): integer; cdecl;
        get_rptr_offs : function(rig: PRig; vfo: vfo_t; var offs: shortfreq_t): integer; cdecl;

        set_split_freq : function(rig: PRig; vfo: vfo_t; tx_freq: freq_t): integer; cdecl;
        get_split_freq : function(rig: PRig; vfo: vfo_t; var tx_freqs: freq_t): integer; cdecl;
        set_split_mode : function(rig: PRig; vfo: vfo_t; tx_mode: rmode_t; tx_width : pbwidth_t): integer; cdecl;
        get_split_mode : function(rig: PRig; vfo: vfo_t; var tx_mode: rmode_t; var tx_width : pbwidth_t): integer; cdecl;
        set_split : function(rig: PRig; vfo: vfo_t; split : split_t): integer; cdecl;
        get_split : function(rig: PRig; vfo: vfo_t; var split: split_t): integer; cdecl;

        set_rit : function(rig: PRig; vfo: vfo_t; rit : shortfreq_t): integer; cdecl;
        get_rit : function(rig: PRig; vfo: vfo_t; var rit: shortfreq_t): integer; cdecl;
        set_xit : function(rig: PRig; vfo: vfo_t; xit : shortfreq_t): integer; cdecl;
        get_xit : function(rig: PRig; vfo: vfo_t; var xit: shortfreq_t): integer; cdecl;

        set_ts : function(rig: PRig; vfo: vfo_t; ts : shortfreq_t): integer; cdecl;
        get_ts : function(rig: PRig; vfo: vfo_t; var ts: shortfreq_t): integer; cdecl;

        set_ctcss_tone : function(rig: PRig; vfo: vfo_t; tone : tone_t): integer; cdecl;
        get_ctcss_tone : function(rig: PRig; vfo: vfo_t; var tone: tone_t): integer; cdecl;
        set_dcs_code : function(rig: PRig; vfo: vfo_t; code : tone_t): integer; cdecl;
        get_dcs_code : function(rig: PRig; vfo: vfo_t; var code: tone_t): integer; cdecl;

        set_dcs_sql : function(rig: PRig; vfo: vfo_t; code : tone_t): integer; cdecl;
        get_dcs_sql : function(rig: PRig; vfo: vfo_t; var code: tone_t): integer; cdecl;
        set_ctcss_sql : function(rig: PRig; vfo: vfo_t; tone : tone_t): integer; cdecl;
        get_ctcss_sql : function(rig: PRig; vfo: vfo_t; var tone: tone_t): integer; cdecl;

        {*
         * It'd be nice to have a power2mW and mW2power functions
         * that could tell at what power (watts) the rig is running.
         * Unfortunately, on most rigs, the formula is not the same
         * on all bands/modes. Have to work this out.. --SF
         *}
        power2mW : function(rig: PRig; var mwpower: cardinal; power : float; freq : freq_t; mode: rmode_t): integer; cdecl;
        mW2power : function(rig: PRig; var power : float; mwpower: cardinal; freq : freq_t; mode: rmode_t): integer; cdecl;

        set_powerstat : function(rig: PRig; status : powerstat_t): integer; cdecl;
        get_powerstat : function(rig: PRig; var status : powerstat_t): integer; cdecl;
        reset : function(rig: PRig; reset : reset_t): integer; cdecl;

        set_ant : function(rig: PRig; vfo: vfo_t; ant : ant_t): integer; cdecl;
        get_ant : function(rig: PRig; vfo: vfo_t; var ant : ant_t): integer; cdecl;

        set_level : function(rig: PRig; vfo: vfo_t; level : setting_t; val : value_t): integer; cdecl;
        get_level : function(rig: PRig; vfo: vfo_t; level : setting_t; var val : value_t): integer; cdecl;

        set_func : function(rig: PRig; vfo: vfo_t; func : setting_t; status : integer): integer; cdecl;
        get_func : function(rig: PRig; vfo: vfo_t; func : setting_t; var status : integer): integer; cdecl;

        set_parm : function(rig: PRig; parm : setting_t; val : value_t): integer; cdecl;
        get_parm : function(rig: PRig; parm : setting_t; var val : value_t): integer; cdecl;

        set_conf : function(rig: PRig; token : token_t; val : PChar): integer; cdecl;
        get_conf : function(rig: PRig; token : token_t; val : PChar): integer; cdecl;

        send_dtmf : function(rig: PRig; vfo: vfo_t; digits : PChar): integer; cdecl;
        recv_dtmf : function(rig: PRig; vfo: vfo_t; digits : PChar; var length : integer): integer; cdecl;
        send_morse : function(rig: PRig; vfo: vfo_t; msg : PChar): integer; cdecl;

        set_bank : function(rig: PRig; vfo: vfo_t; bank : integer): integer; cdecl;
        set_mem : function(rig: PRig; vfo: vfo_t; ch : integer): integer; cdecl;
        get_mem : function(rig: PRig; vfo: vfo_t; var ch : integer): integer; cdecl;
        vfo_op : function(rig: PRig; vfo: vfo_t; op : vfo_op_t): integer; cdecl;
        scan : function(rig: PRig; vfo: vfo_t; scan: scan_t; ch : integer): integer; cdecl;

        set_trn : function(rig: PRig; trn: integer): integer; cdecl;
        get_trn : function(rig: PRig; var trn: integer): integer; cdecl;

        decode_event : function(rig: PRig): integer; cdecl;

        {*
         * Convenience Functions
         *}
        set_channel : function(rig: PRig; const chan: channel_t): integer; cdecl;
        get_channel : function(rig: PRig; var chan: channel_t): integer; cdecl;

        {* get firmware info, etc. *}
        get_info : function(rig: PRig): PChar; cdecl;
{$IFDEF HAVE_MAGIC_NUMBERS}
        magic: cardinal;
{$ENDIF}        
    end;
    TRigCaps = rig_caps;
    PRigCaps = ^TRigCaps;


    port_t = packed record
        ptype : record
           case integer of
             0: ( rig: rig_port_e; );
             1: ( ptt: ptt_type_e; );
             2: ( dcd: dcd_type_e; );
        end;

        fd : integer;
        stream: pointer;
{$ifdef WINDOWS}
        handle: HANDLE;
{$endif}
        write_delay: integer;
        post_write_delay: integer;
        post_write_date: timeval;
        timeout: integer;
        retry: integer;

        pathname: array[0..FILPATHLEN-1] of char;

        parm : record
            case integer of
            0: ( serial : record
                   rate: integer;
                   data_bits: integer;
                   stop_bits: integer;
                   parity: serial_parity_e;
                   handshake: serial_parity_e;
                 end; );
            1: ( parallel: record
                   pin: integer;
                 end; );
            2: ( device: record
                   { place holder }
                 end; );
{$ifdef NET}
            3: ( network: record
                   saddr: struct_socketaddr; // look in libc.pas for correct.
                 end; );
{$endif}
        end;
    end;
    TPort = port_t;
    PPort = ^TPort;


    {*
     * Rig state
     *
     * This struct contains live data, as well as a copy of capability fields
     * that may be updated (ie. customized)
     *
     * It is fine to move fields around, as this kind of struct should
     * not be initialized like caps are.
     *}
    TRigState = packed record
     	{*
	     * overridable fields
	     *}
        rigport : port_t;
        pttport : port_t;
        dcdport : port_t;

        vfo_comp : double;
        itu_region : integer;

        rx_range_list : array [0..FRQRANGESIZ-1] of freq_range_t;
        tx_range_list : array[0..FRQRANGESIZ-1] of freq_range_t;


        tuning_steps : array[0..TSLSTSIZ-1] of tuning_step_list;

        filters : array[0..FLTLSTSIZ-1] of filter_list;

        chan_list : array[0..CHANLSTSIZ-1] of chan_t;

        max_rit : shortfreq_t;
        max_xit : shortfreq_t;
        max_ifshift : shortfreq_t;

        announces : ann_t;

        preamp : array[0..MAXDBLSTSIZ-1] of integer;
        attenuator : array[0..MAXDBLSTSIZ-1] of integer;

        has_get_func : setting_t;
        has_set_func : setting_t;
        has_get_level : setting_t;
        has_set_level : setting_t;
        has_get_parm : setting_t;
        has_set_parm : setting_t;

        level_gran : array[0..RIG_SETTING_MAX-1] of integer;

        {*
         * non overridable fields, internal use
         *}
        hold_decode : integer;
        current_vfo : vfo_t;
        transceive : integer;
        vfo_list : integer;
        comm_state : integer;
        {*
         * Pointer to private data
         *}
        priv : pointer;
        {*
         * internal use by hamlib++ & Kylix for event handling
         *}
        obj : pointer;   // Pointer to the Hamlib Component of this rig.
{$IFDEF HAVE_MAGIC_NUMBERS}
        magic : cardinal;
{$ENDIF}
    end;

    TRigCallbacks = packed record
        freq_event: function(rig: PRig; vfo: vfo_t; freq: freq_t): integer; cdecl;
        mode_event: function(rig: PRig; vfo: vfo_t; mode: rmode_t; width: pbwidth_t): integer; cdecl;
        vfo_event : function(rig: PRig; vfo: vfo_t): integer; cdecl;
        ptt_event : function(rig: PRig; mode: ptt_t): integer; cdecl;
        dcd_event : function(rig: PRig; mode: dcd_t): integer; cdecl;
{$IFDEF HAVE_MAGIC_NUMBERS}
        magic : cardinal;
{$ENDIF}
    end;

    rig = packed record
        caps: PRigCaps;
        state: TRigState;
        callbacks: TRigCallbacks;
{$IFDEF HAVE_MAGIC_NUMBERS}
        magic: cardinal;
{$ENDIF}
    end;
    TRig = rig;
    //PRig = ^TRig  // declared previously

{* --------------- API function prototypes -----------------*}
function rig_init (rig_model : rig_model_t): PRig; cdecl;
function rig_open (rig: PRig): integer; cdecl;
function rig_close (rig: PRig): integer; cdecl;
function rig_cleanup (rig: PRig): integer; cdecl;
function rig_probe (p: PPort): rig_model_t; cdecl;

{*
 *  General API commands, from most primitive to least.. :()
 *  List Set/Get functions pairs
 *}
function rig_set_freq (rig: PRig; vfo: vfo_t; freq: freq_t): integer; cdecl;
function rig_get_freq (rig: PRig; vfo: vfo_t; var freq: freq_t): integer; cdecl;

function rig_set_mode (rig: PRig; vfo: vfo_t; mode: rmode_t; width: pbwidth_t): integer; cdecl;
function rig_get_mode (rig: PRig; vfo: vfo_t; var mode: rmode_t; var width: pbwidth_t): integer; cdecl;

function rig_set_vfo (rig: PRig; vfo: vfo_t): integer; cdecl;
function rig_get_vfo (rig: PRig; var vfo: vfo_t): integer; cdecl;

function rig_set_ptt (rig: PRig; vfo: vfo_t; ptt: ptt_t): integer; cdecl;
function rig_get_ptt (rig: PRig; vfo: vfo_t; var ptt: ptt_t): integer; cdecl;
function rig_get_dcd (rig: PRig; vfo: vfo_t; var dcd: dcd_t): integer; cdecl;

function rig_set_rptr_shift (rig: PRig; vfo: vfo_t; rptr_shift: rptr_shift_t): integer; cdecl;
function rig_get_rptr_shift (rig: PRig; vfo: vfo_t; var rptr_shift: rptr_shift_t): integer; cdecl;
function rig_set_rptr_offs (rig: PRig; vfo: vfo_t; offs: shortfreq_t): integer; cdecl;
function rig_get_rptr_offs (rig: PRig; vfo: vfo_t; var offs: shortfreq_t): integer; cdecl;

function rig_set_ctcss_tode (rig: PRig; vfo: vfo_t; tone : tone_t): integer; cdecl;
function rig_get_ctcss_tode (rig: PRig; vfo: vfo_t; var tone: tone_t): integer; cdecl;
function rig_set_dcs_code (rig: PRig; vfo: vfo_t; code : tone_t): integer; cdecl;
function rig_get_dcs_code (rig: PRig; vfo: vfo_t; var code: tone_t): integer; cdecl;

function rig_set_ctcss_sql (rig: PRig; vfo: vfo_t; tone : tone_t): integer; cdecl;
function rig_get_ctcss_sql (rig: PRig; vfo: vfo_t; var tone: tone_t): integer; cdecl;
function rig_set_dcs_sql (rig: PRig; vfo: vfo_t; code : tone_t): integer; cdecl;
function rig_get_dcs_sql (rig: PRig; vfo: vfo_t; var code: tone_t): integer; cdecl;

function rig_set_split_freq (rig: PRig; vfo: vfo_t; tx_freq: freq_t): integer; cdecl;
function rig_get_split_freq (rig: PRig; vfo: vfo_t; var tx_freqs: freq_t): integer; cdecl;
function rig_set_split_mode (rig: PRig; vfo: vfo_t; tx_mode: rmode_t; tx_width : pbwidth_t): integer; cdecl;
function rig_get_split_mode (rig: PRig; vfo: vfo_t; var tx_mode: rmode_t; var tx_width : pbwidth_t): integer; cdecl;
function rig_set_split (rig: PRig; vfo: vfo_t; split : split_t): integer; cdecl;
function rig_get_split (rig: PRig; vfo: vfo_t; var split: split_t): integer; cdecl;

function rig_set_rit (rig: PRig; vfo: vfo_t; rit : shortfreq_t): integer; cdecl;
function rig_get_rit (rig: PRig; vfo: vfo_t; var rit: shortfreq_t): integer; cdecl;
function rig_set_xit (rig: PRig; vfo: vfo_t; xit : shortfreq_t): integer; cdecl;
function rig_get_xit (rig: PRig; vfo: vfo_t; var xit: shortfreq_t): integer; cdecl;

function rig_set_ts (rig: PRig; vfo: vfo_t; ts : shortfreq_t): integer; cdecl;
function rig_get_ts (rig: PRig; vfo: vfo_t; var ts: shortfreq_t): integer; cdecl;

function rig_power2mW (rig: PRig; var mwpower: cardinal; power : float; freq : freq_t; mode: rmode_t): integer; cdecl;
function rig_mW2power (rig: PRig; var power : float; mwpower: cardinal; freq : freq_t; mode: rmode_t): integer; cdecl;

function rig_get_resolution (rig: PRig; mode: rmode_t): shortfreq_t; cdecl;

//#define rig_get_strength(r,v,s) rig_get_level((r),(v),RIG_LEVEL_STRENGTH, (value_t * )(s))  // macro

function rig_set_func (rig: PRig; vfo: vfo_t; func : setting_t; status : integer): integer; cdecl;
function rig_get_func (rig: PRig; vfo: vfo_t; func : setting_t; var status : integer): integer; cdecl;

function rig_set_level (rig: PRig; vfo: vfo_t; level : setting_t; val : value_t): integer; cdecl;
function rig_get_level (rig: PRig; vfo: vfo_t; level : setting_t; var val : value_t): integer; cdecl;

function rig_set_parm (rig: PRig; parm : setting_t; val : value_t): integer; cdecl;
function rig_get_parm (rig: PRig; parm : setting_t; var val : value_t): integer; cdecl;

function rig_set_conf (rig: PRig; token : token_t; val : PChar): integer; cdecl;
function rig_get_conf (rig: PRig; token : token_t; val : PChar): integer; cdecl;

function rig_set_powerstat (rig: PRig; status : powerstat_t): integer; cdecl;
function rig_get_powerstat (rig: PRig; var status : powerstat_t): integer; cdecl;

function rig_set_ant (rig: PRig; vfo: vfo_t; ant : ant_t): integer; cdecl;
function rig_get_ant (rig: PRig; vfo: vfo_t; var ant : ant_t): integer; cdecl;

function rig_reset (rig: PRig; reset : reset_t): integer; cdecl;

type __cfunc_3 = function(const _conf : TConfParms; _data: pointer): integer; cdecl;
function rig_token_foreach (rig: PRig; cfunc: __cfunc_3; data: pointer): integer; cdecl;
function rig_confparam_lookup (rig: PRig; name : PChar): PConfParms; cdecl;
function rig_token_lookup (rig: PRig; name : PChar): token_t; cdecl;

function rig_has_get_func (rig: PRig; func : setting_t): setting_t; cdecl;
function rig_has_set_func (rig: PRig; func : setting_t): setting_t; cdecl;
function rig_has_get_level (rig: PRig; level : setting_t): setting_t; cdecl;
function rig_has_set_level (rig: PRig; level : setting_t): setting_t; cdecl;
function rig_has_get_parm (rig: PRig; parm : setting_t): setting_t; cdecl;
function rig_has_set_parm (rig: PRig; parm : setting_t): setting_t; cdecl;

function rig_send_dtmf (rig: PRig; vfo: vfo_t; digits : PChar): integer; cdecl;
function rig_recv_dtmf (rig: PRig; vfo: vfo_t; digits : PChar; var length : integer): integer; cdecl;
function rig_send_morse (rig: PRig; vfo: vfo_t; msg : PChar): integer; cdecl;

function rig_set_bank (rig: PRig; vfo: vfo_t; bank : integer): integer; cdecl;
function rig_set_mem (rig: PRig; vfo: vfo_t; ch : integer): integer; cdecl;
function rig_get_mem (rig: PRig; vfo: vfo_t; var ch : integer): integer; cdecl;

function rig_vfo_op (rig: PRig; vfo: vfo_t; op : vfo_op_t): integer; cdecl;
function rig_scan (rig: PRig; vfo: vfo_t; scan: scan_t; ch : integer): integer; cdecl;

function rig_has_vfo_op (rig: PRig; op : vfo_op_t): vfo_op_t; cdecl;
function rig_has_scan (rig: PRig; scan : scan_t): scan_t; cdecl;

function rig_set_trn (rig: PRig; trn: integer): integer; cdecl;
function rig_get_trn (rig: PRig; var trn: integer): integer; cdecl;

function rig_decode_event (rig: PRig): integer; cdecl;
function rig_get_info (rig: PRig): PChar; cdecl;

function rig_set_channel (rig: PRig; const chan: channel_t): integer; cdecl; 	{* mem *}
function rig_get_channel (rig: PRig; var chan: channel_t): integer; cdecl;
function rig_restore_channel (rig: PRig; const chan: channel_t): integer; cdecl; {* curr VFO *}
function rig_save_channel (rig: PRig; var chan: channel_t): integer; cdecl;

function rig_get_caps (rig_model: rig_model_t): PRigCaps; cdecl;
//function rig_get_range (const range_list: array of freq_range_t; freq: freq_t; rmode_t mode): PFreqRange; cdecl;

function rig_passband_normal (rig: PRig; mode: rmode_t): pbwidth_t; cdecl;
function rig_passband_narrow (rig: PRig; mode: rmode_t): pbwidth_t; cdecl;
function rig_passband_wide (rig: PRig; mode: rmode_t): pbwidth_t; cdecl;


function rig_setting2idx (s: setting_t): integer; cdecl;
// #define rig_idx2setting(i) (1<<(i))  // macro

{*
 * Even if these functions are prefixed with "rig_", they are not rig specific
 * Maybe "hamlib_" would have been better. Let me know. --SF
 *}
function rigerror (errnum: integer): PChar; cdecl;
procedure rig_set_debug (debug_level: rig_debug_level_e); cdecl;
function rig_need_debug (debug_level: rig_debug_level_e): integer; cdecl;
procedure rig_debug (debug_level: rig_debug_level_e; fmt: PChar); cdecl; varargs;

function rig_register (caps : PRigCaps): integer; cdecl;
function rig_unregister (rig_model : rig_model_t): integer; cdecl;

type __cfunc_4 = function(const _rig_caps: TRigCaps; _data: pointer): integer; cdecl;
function rig_list_foreach (cfunc: __cfunc_4; data: pointer): integer; cdecl;

function rig_load_backend (be_name: PChar): integer; cdecl;
function rig_check_backend (rig_model : rig_model_t): integer; cdecl;
function rig_load_all_backends : integer; cdecl;
function rig_probe_all (port : PPort) : rig_model_t; cdecl;

{$IFDEF HAVE_MAGIC_NUMBERS}
function rig_check_magic (rig: PRig): integer; cdecl;
function rigapi_check_magic(rig: PRig): cardinal; cdecl;
{$ENDIF}

function rig_check_types : integer; cdecl;
procedure rigapi_check_types;

const
    hamlib_modulename = 'libhamlib.so';

implementation

uses
    SysUtils;

function rig_init;              external hamlib_modulename name 'rig_init';
function rig_open;              external hamlib_modulename name 'rig_open';
function rig_close;             external hamlib_modulename name 'rig_close';
function rig_cleanup;           external hamlib_modulename name 'rig_cleanup';
function rig_probe;             external hamlib_modulename name 'rig_probe';
function rig_set_freq;          external hamlib_modulename name 'rig_set_freq';
function rig_get_freq;          external hamlib_modulename name 'rig_get_freq';
function rig_set_mode;          external hamlib_modulename name 'rig_set_mode';
function rig_get_mode;          external hamlib_modulename name 'rig_get_mode';
function rig_set_vfo;           external hamlib_modulename name 'rig_set_vfo';
function rig_get_vfo;           external hamlib_modulename name 'rig_get_vfo';
function rig_set_ptt;           external hamlib_modulename name 'rig_set_ptt';
function rig_get_ptt;           external hamlib_modulename name 'rig_get_ptt';
function rig_get_dcd;           external hamlib_modulename name 'rig_get_dcd';
function rig_set_rptr_shift;    external hamlib_modulename name 'rig_set_rptr_shift';
function rig_get_rptr_shift;    external hamlib_modulename name 'rig_get_rptr_shift';
function rig_set_rptr_offs;     external hamlib_modulename name 'rig_set_rptr_offs';
function rig_get_rptr_offs;     external hamlib_modulename name 'rig_get_rptr_offs';
function rig_set_ctcss_tode;    external hamlib_modulename name 'rig_set_ctcss_tode';
function rig_get_ctcss_tode;    external hamlib_modulename name 'rig_get_ctcss_tode';
function rig_set_dcs_code;      external hamlib_modulename name 'rig_set_dcs_code';
function rig_get_dcs_code;      external hamlib_modulename name 'rig_get_dcs_code';
function rig_set_ctcss_sql;     external hamlib_modulename name 'rig_set_ctcss_sql';
function rig_get_ctcss_sql;     external hamlib_modulename name 'rig_get_ctcss_sql';
function rig_set_dcs_sql;       external hamlib_modulename name 'rig_set_dcs_sql';
function rig_get_dcs_sql;       external hamlib_modulename name 'rig_get_dcs_sql';
function rig_set_split_freq;    external hamlib_modulename name 'rig_set_split_freq';
function rig_get_split_freq;    external hamlib_modulename name 'rig_get_split_freq';
function rig_set_split_mode;    external hamlib_modulename name 'rig_set_split_mode';
function rig_get_split_mode;    external hamlib_modulename name 'rig_get_split_mode';
function rig_set_split;         external hamlib_modulename name 'rig_set_split';
function rig_get_split;         external hamlib_modulename name 'rig_get_split';
function rig_set_rit;           external hamlib_modulename name 'rig_set_rit';
function rig_get_rit;           external hamlib_modulename name 'rig_get_rit';
function rig_set_xit;           external hamlib_modulename name 'rig_set_xit';
function rig_get_xit;           external hamlib_modulename name 'rig_get_xit';
function rig_set_ts;            external hamlib_modulename name 'rig_set_ts';
function rig_get_ts;            external hamlib_modulename name 'rig_get_ts';
function rig_power2mW;          external hamlib_modulename name 'rig_power2mW';
function rig_mW2power;          external hamlib_modulename name 'rig_mW2power';
function rig_get_resolution;    external hamlib_modulename name 'rig_get_resolution';
function rig_set_func;          external hamlib_modulename name 'rig_set_func';
function rig_get_func;          external hamlib_modulename name 'rig_get_func';
function rig_set_level;         external hamlib_modulename name 'rig_set_level';
function rig_get_level;         external hamlib_modulename name 'rig_get_level';
function rig_set_parm;          external hamlib_modulename name 'rig_set_parm';
function rig_get_parm;          external hamlib_modulename name 'rig_get_parm';
function rig_set_conf;          external hamlib_modulename name 'rig_set_conf';
function rig_get_conf;          external hamlib_modulename name 'rig_get_conf';
function rig_set_powerstat;     external hamlib_modulename name 'rig_set_powerstat';
function rig_get_powerstat;     external hamlib_modulename name 'rig_get_powerstat';
function rig_set_ant;           external hamlib_modulename name 'rig_set_ant';
function rig_get_ant;           external hamlib_modulename name 'rig_get_ant';
function rig_reset;             external hamlib_modulename name 'rig_reset';
function rig_token_foreach;     external hamlib_modulename name 'rig_token_foreach';
function rig_confparam_lookup;  external hamlib_modulename name 'rig_confparam_lookup';
function rig_token_lookup;      external hamlib_modulename name 'rig_token_lookup';
function rig_has_get_func;      external hamlib_modulename name 'rig_has_get_func';
function rig_has_set_func;      external hamlib_modulename name 'rig_has_set_func';
function rig_has_get_level;     external hamlib_modulename name 'rig_has_get_level';
function rig_has_set_level;     external hamlib_modulename name 'rig_has_set_level';
function rig_has_get_parm;      external hamlib_modulename name 'rig_has_get_parm';
function rig_has_set_parm;      external hamlib_modulename name 'rig_has_set_parm';
function rig_send_dtmf;         external hamlib_modulename name 'rig_send_dtmf';
function rig_recv_dtmf;         external hamlib_modulename name 'rig_recv_dtmf';
function rig_send_morse;        external hamlib_modulename name 'rig_send_morse';
function rig_set_bank;          external hamlib_modulename name 'rig_set_bank';
function rig_set_mem;           external hamlib_modulename name 'rig_set_mem';
function rig_get_mem;           external hamlib_modulename name 'rig_get_mem';
function rig_vfo_op;            external hamlib_modulename name 'rig_vfo_op';
function rig_scan;              external hamlib_modulename name 'rig_scan';
function rig_has_vfo_op;        external hamlib_modulename name 'rig_has_vfo_op';
function rig_has_scan;          external hamlib_modulename name 'rig_has_scan';
function rig_set_trn;           external hamlib_modulename name 'rig_set_trn';
function rig_get_trn;           external hamlib_modulename name 'rig_get_trn';
function rig_decode_event;      external hamlib_modulename name 'rig_decode_event';
function rig_get_info;          external hamlib_modulename name 'rig_get_info';
function rig_set_channel;       external hamlib_modulename name 'rig_set_channel';
function rig_get_channel;       external hamlib_modulename name 'rig_get_channel';
function rig_restore_channel;   external hamlib_modulename name 'rig_restore_channel';
function rig_save_channel;      external hamlib_modulename name 'rig_save_channel';
function rig_get_caps;          external hamlib_modulename name 'rig_get_caps';
//function rig_get_range;         external hamlib_modulename name 'rig_get_range';
function rig_passband_normal;   external hamlib_modulename name 'rig_passband_normal';
function rig_passband_narrow;   external hamlib_modulename name 'rig_passband_narrow';
function rig_passband_wide;     external hamlib_modulename name 'rig_passband_wide';
function rig_setting2idx;       external hamlib_modulename name 'rig_setting2idx';
function rigerror;              external hamlib_modulename name 'rigerror';
procedure rig_set_debug;        external hamlib_modulename name 'rig_set_debug';
function rig_need_debug;        external hamlib_modulename name 'rig_need_debug';
procedure rig_debug;            external hamlib_modulename name 'rig_debug';
function rig_register;          external hamlib_modulename name 'rig_register';
function rig_unregister;        external hamlib_modulename name 'rig_unregister';
function rig_list_foreach;      external hamlib_modulename name 'rig_list_foreach';
function rig_load_backend;      external hamlib_modulename name 'rig_load_backend';
function rig_check_backend;     external hamlib_modulename name 'rig_check_backend';
function rig_load_all_backends; external hamlib_modulename name 'rig_load_all_backends';
function rig_probe_all;         external hamlib_modulename name 'rig_probe_all';

{$IFDEF HAVE_MAGIC_NUMBERS}
function rig_check_magic;       external hamlib_modulename name 'rig_check_magic';
function rigapi_check_magic(rig: PRig): cardinal;
begin
    if (rig = nil)
    then result := $FFFF
    else begin
         result := 0;

         if (rig^.magic <> RIG_MAGIC)
         then result := result or (1 shl 1);

         if (rig^.caps = nil)
         then result := result or (1 shl 2)
         else if (rig^.caps^.magic <> RIG_MAGIC_CAPS)
              then result := result or (1 shl 3);

         if (rig^.state.magic <> RIG_MAGIC_STATE)
         then result := result or (1 shl 4);

         if (rig^.callbacks.magic <> RIG_MAGIC_CALLBACKS)
         then result := result or (1 shl 5);
         end;
end;
{$ENDIF}

{* Macro implementations *}

function RIG_CTRL_BAND(band, vfo: integer): integer;
begin
RIG_CTRL_BAND := (($80 shl (8*((band)-1))) or (vfo shl (8*((band)-1))) );
end;
function RIG_VFO_A : integer;
begin
RIG_VFO_A := (RIG_CTRL_BAND(RIG_CTRL_MAIN, RIG_VFO1));
end;
function RIG_VFO_B : integer;
begin
RIG_VFO_B := (RIG_CTRL_BAND(RIG_CTRL_MAIN, RIG_VFO2));
end;
function RIG_VFO_C : integer;
begin
RIG_VFO_C := (RIG_CTRL_BAND(RIG_CTRL_SUB, RIG_VFO1));
end;
function RIG_VFO_MAIN : integer;
begin
RIG_VFO_MAIN := (RIG_CTRL_BAND(RIG_CTRL_MAIN, RIG_VFO_CURR));
end;
function RIG_VFO_SUB : integer;
begin
RIG_VFO_SUB := (RIG_CTRL_BAND(RIG_CTRL_SUB, RIG_VFO_CURR));
end;


function rig_check_types;       external hamlib_modulename name 'rig_check_types';
procedure rigapi_check_types;
begin
    rig_debug(RIG_DEBUG_TRACE, 'Type Checking for Pascal -----------------------------'+chr($a));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(TRig) = '+ IntToStr(sizeof(TRig))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(TRigCaps) = '+ IntToStr(sizeof(TRigCaps))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(TRigState) = '+ IntToStr(sizeof(TRigState))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(TRigCallbacks) = '+ IntToStr(sizeof(TRigCallbacks))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(TPort) = '+ IntToStr(sizeof(TPort))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(chan_list) = '+ IntToStr(sizeof(chan_list))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(chan_type_e) = '+ IntToStr(sizeof(chan_type_e))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(filter_list) = '+ IntToStr(sizeof(filter_list))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(tuning_step_list) = '+ IntToStr(sizeof(tuning_step_list))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(freq_range_list) = '+ IntToStr(sizeof(freq_range_list))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(setting_t) = '+ IntToStr(sizeof(setting_t))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(value_t) = '+ IntToStr(sizeof(value_t))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(agc_level_e) = '+ IntToStr(sizeof(agc_level_e))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(meter_level_e) = '+ IntToStr(sizeof(meter_level_e))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(confparams) = '+ IntToStr(sizeof(confparams))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(powerstat_e) = '+ IntToStr(sizeof(powerstat_e))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(reset_e) = '+ IntToStr(sizeof(reset_e))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(ptt_type_e) = '+ IntToStr(sizeof(ptt_type_e))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(ptt_e) = '+ IntToStr(sizeof(ptt_e))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(dcd_e) = '+ IntToStr(sizeof(dcd_e))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(dcd_type_e) = '+ IntToStr(sizeof(dcd_type_e))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(pbwidth_t) = '+ IntToStr(sizeof(pbwidth_t))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(freq_t) = '+ IntToStr(sizeof(freq_t))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(shortfreq) = '+ IntToStr(sizeof(shortfreq_t))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(split_e) = '+ IntToStr(sizeof(split_e))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(rptr_shift_e) = '+ IntToStr(sizeof(rptr_shift_e))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(rig_status_e) = '+ IntToStr(sizeof(rig_status_e))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(serial_parity_e) = '+ IntToStr(sizeof(serial_parity_e))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(serial_handshake_e) = '+ IntToStr(sizeof(serial_handshake_e))+chr($a) ));
    rig_debug(RIG_DEBUG_TRACE, PChar('sizeof(rig_debug_level_e) = '+ IntToStr(sizeof(rig_debug_level_e))+chr($a) ));
end;


end.

