/*
 *  Hamlib TenTenc backend - TT-565 headers
 *  Copyright (c) 2004-2011 by Stephane Fillod
 *  Copyright (c) 2004-2011 by Martin Ewing
 *
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/**
 * \addtogroup tentec_orion
 * @{ */

/**
 * \file orion.h
 * \brief Backend for Tentec Orion 565 / 566
 *
 * This backend supports the Ten-Tec Orion (565) and Orion II (566) transceivers.
 */


#define BACKEND_VER "20220614"

#define TRUE	1
#define FALSE	0
#define TT565_BUFSIZE 32

/**
 * \brief Memory capability
 *
 * Orion's own  memory channel holds a freq, mode, and bandwidth.
 * May be captured from VFO A or B and applied to VFO A or B.
 * It cannot directly be read or written from the computer!
 */
#define TT565_MEM_CAP {        \
	.freq = 1,      \
	.mode = 1,      \
	.width = 1,     \
}

static int tt565_init(RIG *rig);
static int tt565_open(RIG *rig);
static int tt565_cleanup(RIG *rig);
static int tt565_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int tt565_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int tt565_set_vfo(RIG *rig, vfo_t vfo);
static int tt565_get_vfo(RIG *rig, vfo_t *vfo);
static int tt565_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int tt565_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int tt565_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo);
static int tt565_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo);
static int tt565_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int tt565_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
static int tt565_reset(RIG *rig, reset_t reset);
static int tt565_set_mem(RIG * rig, vfo_t vfo, int ch);
static int tt565_get_mem(RIG * rig, vfo_t vfo, int *ch);
static int tt565_vfo_op(RIG * rig, vfo_t vfo, vfo_op_t op);
static int tt565_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts);
static int tt565_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts);
static int tt565_set_rit(RIG * rig, vfo_t vfo, shortfreq_t rit);
static int tt565_get_rit(RIG * rig, vfo_t vfo, shortfreq_t *rit);
static int tt565_set_xit(RIG * rig, vfo_t vfo, shortfreq_t xit);
static int tt565_get_xit(RIG * rig, vfo_t vfo, shortfreq_t *xit);
static int tt565_set_level(RIG * rig, vfo_t vfo, setting_t level, value_t val);
static int tt565_get_level(RIG * rig, vfo_t vfo, setting_t level, value_t *val);
static const char* tt565_get_info(RIG *rig);
static int tt565_send_morse(RIG *rig, vfo_t vfo, const char *msg);
static int tt565_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
static int tt565_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
static int tt565_set_ant(RIG * rig, vfo_t vfo, ant_t ant, value_t option);
static int tt565_get_ant(RIG *rig, vfo_t vfo, ant_t dummy, value_t *option, ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx);

/** \brief Orion private data */
struct tt565_priv_data {
	int ch;		/*!< memory channel */
	vfo_t vfo_curr; /*!< Currently selected VFO */
};

/** \brief Orion Supported Modes */
#define TT565_MODES (RIG_MODE_FM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|\
			RIG_MODE_RTTY|RIG_MODE_AM)
/** \brief Orion Receiver Modes */
#define TT565_RXMODES (TT565_MODES)

/** \brief Orion Supported Functions */
#define TT565_FUNCS (RIG_FUNC_LOCK|RIG_FUNC_TUNER|RIG_FUNC_VOX|RIG_FUNC_NB)

/** \brief Orion Supported Levels */
#define TT565_LEVELS (RIG_LEVEL_RAWSTR| \
				RIG_LEVEL_CWPITCH| \
				RIG_LEVEL_SQL|RIG_LEVEL_IF| \
				RIG_LEVEL_RFPOWER|RIG_LEVEL_KEYSPD| \
				RIG_LEVEL_RF|RIG_LEVEL_NR| \
				RIG_LEVEL_MICGAIN| \
				RIG_LEVEL_AF|RIG_LEVEL_AGC| \
				RIG_LEVEL_VOXGAIN|RIG_LEVEL_VOXDELAY|RIG_LEVEL_ANTIVOX| \
				RIG_LEVEL_COMP|RIG_LEVEL_PREAMP| \
				RIG_LEVEL_SWR|RIG_LEVEL_ATT)

/** \brief Orion Tx/Rx Antennas*/
#define TT565_ANTS (RIG_ANT_1|RIG_ANT_2)
/** \brief Orion Rx Antennas*/
#define TT565_RXANTS (TT565_ANTS|RIG_ANT_3)

/** \brief Orion Parameters */
#define TT565_PARMS (RIG_PARM_NONE)

/**
 * \brief Orion VFOs - A and B
 */
#define TT565_VFO (RIG_VFO_A|RIG_VFO_B)
/**
 * \brief Orion VFO Operations
 *
 * Allowed operations
 */
#define TT565_VFO_OPS (RIG_OP_UP|RIG_OP_DOWN|\
		RIG_OP_TO_VFO|RIG_OP_FROM_VFO| \
		RIG_OP_TUNE)

/**
 * \brief S-Meter Calibration list
 *
 * List format: { hardware units, dB relative to S9}
 *
 * These alternate tables must be of equal size, because they may be
 * switched depending on firmware version detection.
 *
 * Note high end of scale is severely compressed in v1
 * Table corrected against v 1.372, 11/2007
 */
#define TT565_STR_CAL_V1 { 14,  { \
                {   1, -47 }, /* padding to match lengths with v2 */ \
		        {  10, -47 }, \
                {  13, -42 }, \
                {  18, -37 }, \
                {  22, -32 }, \
                {  27, -27 }, \
                {  32, -18 }, \
                {  37, -11 }, \
                {  42,  -4 }, \
                {  47,  -1 }, \
                {  52,  10 }, \
                {  57,  20 }, \
                {  65,  30 }, \
                {  74,  40 }, \
        } }
/**
 * Calibration for Version 2.062a firmware, from Rigserve project.
 * Again, this is approximate based on one measurement.
 */
#define TT565_STR_CAL_V2 { 14, { \
                { 10., -48. }, /* S1 = min. indication */ \
                { 24., -42. }, \
                { 38., -36. }, \
                { 47., -30. }, \
                { 61., -24. }, \
                { 70., -18. }, \
                { 79., -12. }, \
                { 84.,  -6. }, \
                { 94.,   0. }, /* S9 */ \
                { 103., 10. }, \
                { 118., 20. }, \
                { 134., 30. }, \
                { 147., 40. }, \
                { 161., 50. }, \
        } }

#undef TT565_TIME		/* Define to enable time checks */
#define TT565_ASCII_FREQ    /* select ascii mode for vfo commands */
	/* Note:  Binary mode seems buggy at certain freqs like
	7015679 < freq < 7015936, etc.  Use ascii mode. */

/**
 * \brief tt565 transceiver capabilities.
 *
 * All of the Orion's personality is defined here!
 *
 * Protocol is documented at Tentec's firmware site
 *		http://www.rfsquared.com/
 */
const struct rig_caps tt565_caps = {
RIG_MODEL(RIG_MODEL_TT565),
.model_name = "TT-565 Orion",
.mfg_name =  "Ten-Tec",
.version =  BACKEND_VER ".0",
.copyright =  "LGPL",
.status =  RIG_STATUS_STABLE,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_NONE,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  57600,
.serial_rate_max =  57600,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_HARDWARE,
.write_delay =  0,          /* no delay between characters written */
.post_write_delay =  0,		  /* ms delay between writes DEBUGGING HERE */
.timeout =  2000,						/* ms */
.retry =  4,

.has_get_func =  TT565_FUNCS,
.has_set_func =  TT565_FUNCS,
.has_get_level =  TT565_LEVELS,
.has_set_level =  RIG_LEVEL_SET(TT565_LEVELS),
.has_get_parm =  TT565_PARMS,
.has_set_parm =  TT565_PARMS,

.level_gran = {},
.parm_gran =  {},
.ctcss_list =  NULL,
.dcs_list =  NULL,
.preamp =   { 20, RIG_DBLST_END },
.attenuator =   { 6, 12, 18, RIG_DBLST_END },
.max_rit =  kHz(8),
.max_xit =  kHz(8),
.max_ifshift =  kHz(8),
.vfo_ops = TT565_VFO_OPS,
.targetable_vfo =  RIG_TARGETABLE_ALL,
.transceive =  RIG_TRN_OFF,
.bank_qty =   0,
.chan_desc_sz =  0,

.chan_list =  {
		{   0, 199, RIG_MTYPE_MEM, TT565_MEM_CAP },
		},

/* Note Orion's ranges correspond to the hardware capability - same in
 * regions 1 and 2.  Band edges (VFOA) are wider than legal bands.
 * VFOB is used for general coverage receive.
 */
.rx_range_list1 =  {
/*	FRQ_RNG_HF(1,TT565_RXMODES, -1,-1,RIG_VFO_N(0),TT565_RXANTS), */
	{kHz(1790),kHz(2010),TT565_RXMODES,-1,-1,RIG_VFO_N(0),TT565_RXANTS},
	{kHz(3490),kHz(4075),TT565_RXMODES,-1,-1,RIG_VFO_N(0),TT565_RXANTS},
	{kHz(5100),kHz(5450),TT565_RXMODES,-1,-1,RIG_VFO_N(0),TT565_RXANTS},
	{kHz(6890),kHz(7430),TT565_RXMODES,-1,-1,RIG_VFO_N(0),TT565_RXANTS},
	{kHz(10090),kHz(10160),TT565_RXMODES,-1,-1,RIG_VFO_N(0),TT565_RXANTS},
	{kHz(13990),kHz(15010),TT565_RXMODES,-1,-1,RIG_VFO_N(0),TT565_RXANTS},
	{kHz(18058),kHz(18178),TT565_RXMODES,-1,-1,RIG_VFO_N(0),TT565_RXANTS},
	{kHz(20990),kHz(21460),TT565_RXMODES,-1,-1,RIG_VFO_N(0),TT565_RXANTS},
	{kHz(24880),kHz(25000),TT565_RXMODES,-1,-1,RIG_VFO_N(0),TT565_RXANTS},
	{kHz(27990),kHz(29710),TT565_RXMODES,-1,-1,RIG_VFO_N(0),TT565_RXANTS},
	{kHz(100),MHz(30),TT565_RXMODES,-1,-1,RIG_VFO_N(1),TT565_RXANTS},
	RIG_FRNG_END,
  },
.tx_range_list1 =  {
/*	FRQ_RNG_HF(1,TT565_MODES, W(5),W(100),RIG_VFO_N(0),TT565_ANTS), */
	{kHz(1790),kHz(2010),TT565_MODES, W(5),W(100),RIG_VFO_N(0),TT565_ANTS},
	{kHz(3490),kHz(4075),TT565_MODES, W(5),W(100),RIG_VFO_N(0),TT565_ANTS},
	{kHz(5100),kHz(5450),TT565_MODES, W(5),W(100),RIG_VFO_N(0),TT565_ANTS},
	{kHz(6890),kHz(7430),TT565_MODES, W(5),W(100),RIG_VFO_N(0),TT565_ANTS},
	{kHz(10090),kHz(10160),TT565_MODES, W(5),W(100),RIG_VFO_N(0),TT565_ANTS},
	{kHz(13990),kHz(15010),TT565_MODES, W(5),W(100),RIG_VFO_N(0),TT565_ANTS},
	{kHz(18058),kHz(18178),TT565_MODES, W(5),W(100),RIG_VFO_N(0),TT565_ANTS},
	{kHz(20990),kHz(21460),TT565_MODES, W(5),W(100),RIG_VFO_N(0),TT565_ANTS},
	{kHz(24880),kHz(25000),TT565_MODES, W(5),W(100),RIG_VFO_N(0),TT565_ANTS},
	{kHz(27990),kHz(29710),TT565_MODES, W(5),W(100),RIG_VFO_N(0),TT565_ANTS},
	RIG_FRNG_END,
  },

.rx_range_list2 =  {
/*	FRQ_RNG_HF(2,TT565_RXMODES, -1,-1,RIG_VFO_N(0),TT565_RXANTS),
	{MHz(5.25),MHz(5.40),TT565_RXMODES,-1,-1,RIG_VFO_N(0),TT565_RXANTS}, */
	{kHz(1790),kHz(2010),TT565_RXMODES,-1,-1,RIG_VFO_N(0),TT565_RXANTS},
	{kHz(3490),kHz(4075),TT565_RXMODES,-1,-1,RIG_VFO_N(0),TT565_RXANTS},
	{kHz(5100),kHz(5450),TT565_RXMODES,-1,-1,RIG_VFO_N(0),TT565_RXANTS},
	{kHz(6890),kHz(7430),TT565_RXMODES,-1,-1,RIG_VFO_N(0),TT565_RXANTS},
	{kHz(10090),kHz(10160),TT565_RXMODES,-1,-1,RIG_VFO_N(0),TT565_RXANTS},
	{kHz(13990),kHz(15010),TT565_RXMODES,-1,-1,RIG_VFO_N(0),TT565_RXANTS},
	{kHz(18058),kHz(18178),TT565_RXMODES,-1,-1,RIG_VFO_N(0),TT565_RXANTS},
	{kHz(20990),kHz(21460),TT565_RXMODES,-1,-1,RIG_VFO_N(0),TT565_RXANTS},
	{kHz(24880),kHz(25000),TT565_RXMODES,-1,-1,RIG_VFO_N(0),TT565_RXANTS},
	{kHz(27990),kHz(29710),TT565_RXMODES,-1,-1,RIG_VFO_N(0),TT565_RXANTS},
	{kHz(100),MHz(30),TT565_RXMODES,-1,-1,RIG_VFO_N(1),TT565_RXANTS},
	RIG_FRNG_END,
  },
.tx_range_list2 =  {
/*	FRQ_RNG_HF(2,TT565_MODES, W(5),W(100),RIG_VFO_N(0),TT565_ANTS),
	{MHz(5.25),MHz(5.40),TT565_MODES,W(5),W(100),RIG_VFO_N(0),TT565_ANTS}, */
	{kHz(1790),kHz(2010),TT565_MODES, W(5),W(100),RIG_VFO_N(0),TT565_ANTS},
	{kHz(3490),kHz(4075),TT565_MODES, W(5),W(100),RIG_VFO_N(0),TT565_ANTS},
	{kHz(5100),kHz(5450),TT565_MODES, W(5),W(100),RIG_VFO_N(0),TT565_ANTS},
	{kHz(6890),kHz(7430),TT565_MODES, W(5),W(100),RIG_VFO_N(0),TT565_ANTS},
	{kHz(10090),kHz(10160),TT565_MODES, W(5),W(100),RIG_VFO_N(0),TT565_ANTS},
	{kHz(13990),kHz(15010),TT565_MODES, W(5),W(100),RIG_VFO_N(0),TT565_ANTS},
	{kHz(18058),kHz(18178),TT565_MODES, W(5),W(100),RIG_VFO_N(0),TT565_ANTS},
	{kHz(20990),kHz(21460),TT565_MODES, W(5),W(100),RIG_VFO_N(0),TT565_ANTS},
	{kHz(24880),kHz(25000),TT565_MODES, W(5),W(100),RIG_VFO_N(0),TT565_ANTS},
	{kHz(27990),kHz(29710),TT565_MODES, W(5),W(100),RIG_VFO_N(0),TT565_ANTS},
	RIG_FRNG_END,
  },

.tuning_steps =  {
	 {TT565_RXMODES,1},
	 {TT565_RXMODES,10},
	 {TT565_RXMODES,100},
	 {TT565_RXMODES,kHz(1)},
	 {TT565_RXMODES,kHz(10)},
	 {TT565_RXMODES,kHz(100)},
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
	/* 9MHz IF filters: 15kHz, 6kHz, 2.4kHz, 1.0kHz */
	/* opt: 1.8kHz, 500Hz, 250Hz */
		{RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY, kHz(2.4)},
		{RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY, 100},
		{RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY, kHz(6)},
		{RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY, 0}, /* 590 filters */
		{RIG_MODE_AM, kHz(6)},
		{RIG_MODE_AM, kHz(4)},
		{RIG_MODE_FM, kHz(15)},
		RIG_FLT_END,
	},
.priv =  (void*)NULL,

.rig_init =  tt565_init,
.rig_cleanup =  tt565_cleanup,
.rig_open = tt565_open,

.set_freq =  tt565_set_freq,
.get_freq =  tt565_get_freq,
.set_vfo =  tt565_set_vfo,
.get_vfo =  tt565_get_vfo,
.set_mode =  tt565_set_mode,
.get_mode =  tt565_get_mode,
.set_split_vfo =  tt565_set_split_vfo,
.get_split_vfo =  tt565_get_split_vfo,
.set_level =  tt565_set_level,
.get_level =  tt565_get_level,
.set_mem =  tt565_set_mem,
.get_mem =  tt565_get_mem,
.set_ptt =  tt565_set_ptt,
.get_ptt =  tt565_get_ptt,
.vfo_op =  tt565_vfo_op,
.set_ts =  tt565_set_ts,
.get_ts =  tt565_get_ts,
.set_rit =  tt565_set_rit,
.get_rit =  tt565_get_rit,
.set_xit =  tt565_set_xit,
.get_xit =  tt565_get_xit,
.reset =  tt565_reset,
.get_info =  tt565_get_info,
.send_morse = tt565_send_morse,
.wait_morse =  rig_wait_morse,
.get_func = tt565_get_func,
.set_func = tt565_set_func,
.get_ant = tt565_get_ant,
.set_ant = tt565_set_ant,

/* V2 is default. S-Meter cal table may be changed if V1 firmware detected. */
.str_cal = TT565_STR_CAL_V2,
};

/*
 * Eagle TT-599 share the same ability as Orion's
 */
#define TT599_MODES (RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_CWR|\
            /* optional */ RIG_MODE_AM|RIG_MODE_FM)
#define TT599_RXMODES TT599_MODES
#define TT599_FUNCS (RIG_FUNC_ANF)
#define TT599_LEVELS (RIG_LEVEL_RAWSTR| \
				RIG_LEVEL_SWR|RIG_LEVEL_RFPOWER| \
				RIG_LEVEL_NR|RIG_LEVEL_AGC| \
				RIG_LEVEL_PREAMP|RIG_LEVEL_ATT)
#define TT599_PARMS (RIG_PARM_NONE)
#define TT599_MEM_CAP  { \
	.freq = 1,      \
	.mode = 1,      \
	.width = 1,     \
	.split = 1,     \
	.tx_freq = 1,   \
}
#define TT599_ANTS (RIG_ANT_1)
#define TT599_RXANTS TT599_ANTS
#define TT599_VFO (RIG_VFO_A|RIG_VFO_B)
#define TT599_VFO_OPS (RIG_OP_TO_VFO|RIG_OP_FROM_VFO)

/*
 * Random guess, to be measured. See FAQ at http://hamlib.org
 */
#define TT599_STR_CAL { 3, { \
                { 10., -48. }, /* S1 = min. indication */ \
                { 94.,   0. }, /* S9 */ \
                { 161., 50. }, \
        } }

/**
 * \brief tt599 transceiver capabilities.
 *
 * All of the Eagle's personality is defined here!
 *
 * Protocol is documented in Programmers Reference Manual V1.001 at
 *		http://www.tentec.com/index.php?id=360#down
 *
 */
const struct rig_caps tt599_caps = {
RIG_MODEL(RIG_MODEL_TT599),
.model_name = "TT-599 Eagle",
.mfg_name =  "Ten-Tec",
.version =  BACKEND_VER ".0",
.copyright =  "LGPL",
.status =  RIG_STATUS_STABLE,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_NONE,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  57600,
.serial_rate_max =  57600,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_HARDWARE,
.write_delay =  0,          /* no delay between characters written */
.post_write_delay =  0,		  /* ms delay between writes DEBUGGING HERE */
.timeout =  2000,						/* ms */
.retry =  4,

.has_get_func =  TT599_FUNCS,
.has_set_func =  TT599_FUNCS,
.has_get_level =  TT599_LEVELS|RIG_LEVEL_RF,
.has_set_level =  RIG_LEVEL_SET(TT599_LEVELS),
.has_get_parm =  TT599_PARMS,
.has_set_parm =  TT599_PARMS,

.level_gran = {},
.parm_gran =  {},
.ctcss_list =  NULL,
.dcs_list =  NULL,
.preamp =   { 10, RIG_DBLST_END },
.attenuator =   { 12, RIG_DBLST_END },
.max_rit =  kHz(0),
.max_xit =  kHz(0),
.max_ifshift =  kHz(0),
.vfo_ops = TT599_VFO_OPS,
.targetable_vfo =  RIG_TARGETABLE_FREQ,
.transceive =  RIG_TRN_OFF,
.bank_qty =   0,
.chan_desc_sz =  0,

.chan_list =  {
		{   1, 100, RIG_MTYPE_MEM, TT599_MEM_CAP },
		},

.rx_range_list1 =  {
	FRQ_RNG_HF(1,TT599_RXMODES, -1,-1,RIG_VFO_N(0),TT599_RXANTS),
	FRQ_RNG_6m(1,TT599_RXMODES, -1,-1,RIG_VFO_N(0),TT599_RXANTS),
	{kHz(500),MHz(30),TT599_RXMODES,-1,-1,RIG_VFO_N(1),TT599_RXANTS},
	RIG_FRNG_END,
  },
.tx_range_list1 =  {
	FRQ_RNG_HF(1,TT599_MODES, W(5),W(100),RIG_VFO_N(0),TT599_ANTS),
	FRQ_RNG_6m(1,TT599_MODES, W(5),W(100),RIG_VFO_N(0),TT599_ANTS),
	RIG_FRNG_END,
  },

.rx_range_list2 =  {
	FRQ_RNG_HF(2,TT599_RXMODES, -1,-1,RIG_VFO_N(0),TT599_RXANTS),
	FRQ_RNG_6m(2,TT599_RXMODES, -1,-1,RIG_VFO_N(0),TT599_RXANTS),
	{MHz(5.25),MHz(5.40),TT599_RXMODES,-1,-1,RIG_VFO_N(0),TT599_RXANTS},
	{kHz(500),MHz(30),TT599_RXMODES,-1,-1,RIG_VFO_N(1),TT599_RXANTS},
	RIG_FRNG_END,
  },
.tx_range_list2 =  {
	FRQ_RNG_HF(2,TT599_MODES, W(5),W(100),RIG_VFO_N(0),TT599_ANTS),
	FRQ_RNG_6m(2,TT599_MODES, W(5),W(100),RIG_VFO_N(0),TT599_ANTS),
	{MHz(5.25),MHz(5.40),TT599_MODES,W(5),W(100),RIG_VFO_N(0),TT599_ANTS},
	RIG_FRNG_END,
  },

.tuning_steps =  {
	 {TT599_RXMODES,1},
	 {TT599_RXMODES,10},
	 {TT599_RXMODES,100},
	 {TT599_RXMODES,kHz(1)},
	 {TT599_RXMODES,kHz(10)},
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
	/*  15kHz, 6kHz, 2.4kHz, 1.0kHz */
    /* 9MHz IF filters: 2.4K standard */
    /* optional = 300, 600, 1.8k, 6k, 15k */
		{RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB, kHz(2.4)},
		{RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB, 600},
		{RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB, 300},
		{RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB, kHz(1.8)},
		{RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB, kHz(6)},
		{RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB, 0}, /* 127 filters */
		{RIG_MODE_AM, kHz(6)},
		{RIG_MODE_FM, kHz(15)},
		RIG_FLT_END,
	},
.priv =  (void*)NULL,

.rig_init =  tt565_init,
.rig_cleanup =  tt565_cleanup,
.rig_open = tt565_open,

.set_freq =  tt565_set_freq,
.get_freq =  tt565_get_freq,
.set_vfo =  tt565_set_vfo,
.get_vfo =  tt565_get_vfo,
.set_mode =  tt565_set_mode,
.get_mode =  tt565_get_mode,
.set_split_vfo =  tt565_set_split_vfo,
.get_split_vfo =  tt565_get_split_vfo,
.set_level =  tt565_set_level,
.get_level =  tt565_get_level,
.set_mem =  tt565_set_mem,
.get_mem =  tt565_get_mem,
.set_ptt =  tt565_set_ptt,
.get_ptt =  tt565_get_ptt,
.vfo_op =  tt565_vfo_op,
.get_info =  tt565_get_info,
.get_func = tt565_get_func,
.set_func = tt565_set_func,

.str_cal = TT599_STR_CAL,
};


/*
 * Function definitions below
 */

/** \brief End of command marker */
#define EOM "\015"	/* CR */
/** \brief USB Mode */
#define TT565_USB '0'
/** \brief LSB Mode */
#define TT565_LSB '1'
/** \brief CW normal Mode */
#define TT565_CW  '2'
/** \brief CW reverse Mode */
#define TT565_CWR '3'
/** \brief AM Mode */
#define TT565_AM  '4'
/** \brief FM Mode */
#define TT565_FM  '5'
/** \brief RTTY Mode */
#define TT565_RTTY '6'

/** \brief minimum sidetone freq., Hz */
#define TT565_TONE_MIN 300
/** \brief maximum sidetone freq., Hz */
#define TT565_TONE_MAX 1200

/** \brief minimum CW keyer rate, wpm */
#define TT565_CW_MIN 10
/** \brief maximum CW keyer rate, wpm */
#define TT565_CW_MAX 60

/** @} */
