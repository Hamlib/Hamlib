/*
 * hamlib - (C) Frank Singleton 2000,2001 (vk3fcs@ix.netcom.com)
 *          (C) Stephane Fillod 2000-2009
 *
 * ft817.c - (C) Chris Karpinsky 2001 (aa1vl@arrl.net)
 * This shared library provides an API for communicating
 * via serial interface to an FT-817 using the "CAT" interface.
 * The starting point for this code was Frank's ft847 implementation.
 *
 * Then, Tommi OH2BNS improved the code a lot in the framework of the
 * FT-857 backend. These improvements have now (August 2005) been
 * copied back and adopted for the FT-817.
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

/*
 * Unimplemented features supported by the FT-817:
 *
 *   - RIT ON/OFF without touching the RIT offset. This would
 *     need frontend support (eg. a new RIG_FUNC_xxx)
 *
 *   - RX status command returns info that is not used:
 *      - discriminator centered (yes/no flag)
 *      - received ctcss/dcs matched (yes/no flag)                     TBC
 *
 *   - TX status command returns info that is not used:
 *      - split on/off flag; actually this could have been used
 *        for get_split_vfo, but the flag is valid only when
 *        PTT is ON.
 *      - high swr flag
 *
 * Todo / tocheck list (oz9aec):
 * - test get_dcd; rigctl does not support it?
 * - squelch
 * - the many "fixme" stuff around
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>  	/* String function definitions */
#include <unistd.h>  	/* UNIX standard function definitions */

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include "hamlib/rig.h"
#include "serial.h"
#include "yaesu.h"
#include "ft817.h"
#include "misc.h"
#include "tones.h"
#include "bandplan.h"


/* Native ft817 cmd set prototypes. These are READ ONLY as each */
/* rig instance will copy from these and modify if required . */
/* Complete sequences (1) can be read and used directly as a cmd sequence . */
/* Incomplete sequences (0) must be completed with extra parameters */
/* eg: mem number, or freq etc.. */
static const yaesu_cmd_set_t ncmd[] = {
	{ 1, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* lock on */
	{ 1, { 0x00, 0x00, 0x00, 0x00, 0x80 } }, /* lock off */
	{ 1, { 0x00, 0x00, 0x00, 0x00, 0x08 } }, /* ptt on */
	{ 1, { 0x00, 0x00, 0x00, 0x00, 0x88 } }, /* ptt off */
	{ 0, { 0x00, 0x00, 0x00, 0x00, 0x01 } }, /* set freq */
	{ 1, { 0x00, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main LSB */
	{ 1, { 0x01, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main USB */
	{ 1, { 0x02, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main CW */
	{ 1, { 0x03, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main CWR */
	{ 1, { 0x04, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main AM */
	{ 1, { 0x08, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main FM */
	{ 1, { 0x88, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main FM-N */
	{ 1, { 0x0a, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main DIG */
	{ 1, { 0x0c, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main PKT */
	{ 1, { 0x00, 0x00, 0x00, 0x00, 0x05 } }, /* clar on */
	{ 1, { 0x00, 0x00, 0x00, 0x00, 0x85 } }, /* clar off */
	{ 0, { 0x00, 0x00, 0x00, 0x00, 0xf5 } }, /* set clar freq */
	{ 1, { 0x00, 0x00, 0x00, 0x00, 0x81 } }, /* toggle vfo a/b */
	{ 1, { 0x00, 0x00, 0x00, 0x00, 0x02 } }, /* split on */
	{ 1, { 0x00, 0x00, 0x00, 0x00, 0x82 } }, /* split off */
	{ 1, { 0x09, 0x00, 0x00, 0x00, 0x09 } }, /* set RPT shift MINUS */
	{ 1, { 0x49, 0x00, 0x00, 0x00, 0x09 } }, /* set RPT shift PLUS */
	{ 1, { 0x89, 0x00, 0x00, 0x00, 0x09 } }, /* set RPT shift SIMPLEX */
	{ 0, { 0x00, 0x00, 0x00, 0x00, 0xf9 } }, /* set RPT offset freq */
	{ 1, { 0x0a, 0x00, 0x00, 0x00, 0x0a } }, /* set DCS on */
	{ 1, { 0x2a, 0x00, 0x00, 0x00, 0x0a } }, /* set CTCSS on */
	{ 1, { 0x4a, 0x00, 0x00, 0x00, 0x0a } }, /* set CTCSS encoder on */
	{ 1, { 0x8a, 0x00, 0x00, 0x00, 0x0a } }, /* set CTCSS/DCS off */
	{ 0, { 0x00, 0x00, 0x00, 0x00, 0x0b } }, /* set CTCSS tone */
	{ 0, { 0x00, 0x00, 0x00, 0x00, 0x0c } }, /* set DCS code */
	{ 1, { 0x00, 0x00, 0x00, 0x00, 0xe7 } }, /* get RX status  */
	{ 1, { 0x00, 0x00, 0x00, 0x00, 0xf7 } }, /* get TX status  */
	{ 1, { 0x00, 0x00, 0x00, 0x00, 0x03 } }, /* get FREQ and MODE status */
	{ 1, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* pwr wakeup sequence */
	{ 1, { 0x00, 0x00, 0x00, 0x00, 0x0f } }, /* pwr on */
	{ 1, { 0x00, 0x00, 0x00, 0x00, 0x8f } }, /* pwr off */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0xbb } }, /* eeprom read */
};

enum ft817_digi {
  FT817_DIGI_RTTY = 0,
  FT817_DIGI_PSK_L,
  FT817_DIGI_PSK_U,
  FT817_DIGI_USER_L,
  FT817_DIGI_USER_U,
};

#define FT817_ALL_RX_MODES      (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_PKTFM|\
                                 RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM|RIG_MODE_PKTUSB)
#define FT817_SSB_CW_RX_MODES   (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY)
#define FT817_CWN_RX_MODES      (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_RTTY)
#define FT817_AM_FM_RX_MODES    (RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_PKTFM)

#define FT817_OTHER_TX_MODES    (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_USB|\
                                 RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM|RIG_MODE_PKTUSB)
#define FT817_AM_TX_MODES       (RIG_MODE_AM)

#define FT817_VFO_ALL           (RIG_VFO_A|RIG_VFO_B)
#define FT817_ANTS              0

#define FT817_STR_CAL { 16, \
                { \
                    { 0x00, -54 }, /* S0 */ \
                    { 0x01, -48 }, \
                    { 0x02, -42 }, \
                    { 0x03, -36 }, \
                    { 0x04, -30 }, \
                    { 0x05, -24 }, \
                    { 0x06, -18 }, \
                    { 0x07, -12 }, \
                    { 0x08, -6 }, \
                    { 0x09,  0 },  /* S9 */ \
                    { 0x0A,  10 }, /* +10 */ \
                    { 0x0B,  20 }, /* +20 */ \
                    { 0x0C,  30 }, /* +30 */ \
                    { 0x0D,  40 }, /* +40 */ \
                    { 0x0E,  50 }, /* +50 */ \
                    { 0x0F,  60 }  /* +60 */ \
                } }

const struct rig_caps ft817_caps = {
	.rig_model =           RIG_MODEL_FT817,
	.model_name =          "FT-817",
	.mfg_name =            "Yaesu",
	.version =             "0.5.1",
	.copyright =           "LGPL",
	.status =              RIG_STATUS_BETA,
	.rig_type =            RIG_TYPE_TRANSCEIVER,
	.ptt_type =            RIG_PTT_RIG,
	.dcd_type =            RIG_DCD_RIG,
	.port_type =           RIG_PORT_SERIAL,
	.serial_rate_min =     4800,
	.serial_rate_max =     38400,
	.serial_data_bits =    8,
	.serial_stop_bits =    2,
	.serial_parity =       RIG_PARITY_NONE,
	.serial_handshake =    RIG_HANDSHAKE_NONE,
	.write_delay =         FT817_WRITE_DELAY,
	.post_write_delay =    FT817_POST_WRITE_DELAY,
	.timeout =             FT817_TIMEOUT,
	.retry =               0,
	.has_get_func =        RIG_FUNC_NONE,
	.has_set_func =        RIG_FUNC_LOCK | RIG_FUNC_TONE | RIG_FUNC_TSQL,
	.has_get_level =       RIG_LEVEL_STRENGTH | RIG_LEVEL_RAWSTR | RIG_LEVEL_RFPOWER,
	.has_set_level =       RIG_LEVEL_NONE,
	.has_get_parm =        RIG_PARM_NONE,
	.has_set_parm =        RIG_PARM_NONE,
	.level_gran =          {},                     /* granularity */
	.parm_gran =           {},
	.ctcss_list =          common_ctcss_list,
	.dcs_list =            common_dcs_list,   /* only 104 out of 106 supported */
	.preamp =              { RIG_DBLST_END, },
	.attenuator =          { RIG_DBLST_END, },
	.max_rit =             Hz(9990),
	.max_xit =             Hz(0),
	.max_ifshift =         Hz(0),
	.vfo_ops =             RIG_OP_TOGGLE,
	.targetable_vfo =      0,
	.transceive =          RIG_TRN_OFF,
	.bank_qty =            0,
	.chan_desc_sz =        0,
	.chan_list =           { RIG_CHAN_END, },

	.rx_range_list1 =  {
		{kHz(100),MHz(56), FT817_ALL_RX_MODES,-1,-1},
		{MHz(76), MHz(108),RIG_MODE_WFM,      -1,-1},
		{MHz(118),MHz(164),FT817_ALL_RX_MODES,-1,-1},
		{MHz(420),MHz(470),FT817_ALL_RX_MODES,-1,-1},
		RIG_FRNG_END,
	},
	.tx_range_list1 =  {
		FRQ_RNG_HF(1, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
		FRQ_RNG_HF(1, FT817_AM_TX_MODES, W(0.5),W(1.5), FT817_VFO_ALL, FT817_ANTS),

		FRQ_RNG_6m(1, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
		FRQ_RNG_6m(1, FT817_AM_TX_MODES, W(0.5),W(1.5), FT817_VFO_ALL, FT817_ANTS),

		FRQ_RNG_2m(1, FT817_OTHER_TX_MODES, W(0.5),W(5),FT817_VFO_ALL,FT817_ANTS),
		FRQ_RNG_2m(1, FT817_AM_TX_MODES, W(0.5),W(1.5),FT817_VFO_ALL,FT817_ANTS),

		FRQ_RNG_70cm(1, FT817_OTHER_TX_MODES, W(0.5),W(5),FT817_VFO_ALL,FT817_ANTS),
		FRQ_RNG_70cm(1, FT817_AM_TX_MODES, W(0.5),W(1.5),FT817_VFO_ALL,FT817_ANTS),

		RIG_FRNG_END,
	},


	.rx_range_list2 =  {
		{kHz(100),MHz(56), FT817_ALL_RX_MODES,-1,-1},
		{MHz(76), MHz(108),RIG_MODE_WFM,      -1,-1},
		{MHz(118),MHz(164),FT817_ALL_RX_MODES,-1,-1},
		{MHz(420),MHz(470),FT817_ALL_RX_MODES,-1,-1},
		RIG_FRNG_END,
	},

	.tx_range_list2 =  {
		FRQ_RNG_HF(2, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
		FRQ_RNG_HF(2, FT817_AM_TX_MODES, W(0.5),W(1.5), FT817_VFO_ALL, FT817_ANTS),
	/* FIXME: 60 meters in US version */

		FRQ_RNG_6m(2, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
		FRQ_RNG_6m(2, FT817_AM_TX_MODES, W(0.5),W(1.5), FT817_VFO_ALL, FT817_ANTS),

		FRQ_RNG_2m(2, FT817_OTHER_TX_MODES, W(0.5),W(5),FT817_VFO_ALL,FT817_ANTS),
		FRQ_RNG_2m(2, FT817_AM_TX_MODES, W(0.5),W(1.5),FT817_VFO_ALL,FT817_ANTS),

		FRQ_RNG_70cm(2, FT817_OTHER_TX_MODES, W(0.5),W(5),FT817_VFO_ALL,FT817_ANTS),
		FRQ_RNG_70cm(2, FT817_AM_TX_MODES, W(0.5),W(1.5),FT817_VFO_ALL,FT817_ANTS),

		RIG_FRNG_END,
	},

	.tuning_steps =  {
		{FT817_SSB_CW_RX_MODES,Hz(10)},
		{FT817_AM_FM_RX_MODES|RIG_MODE_WFM,Hz(100)},
		RIG_TS_END,
	},

	.filters = {
		{FT817_SSB_CW_RX_MODES, kHz(2.2)},  /* normal passband */
		{FT817_CWN_RX_MODES, 500},          /* CW and RTTY narrow */
		{RIG_MODE_AM, kHz(6)},              /* AM normal */
		{RIG_MODE_FM|RIG_MODE_PKTFM, kHz(9)},
		{RIG_MODE_WFM, kHz(15)},
		RIG_FLT_END,
	},

    .str_cal = FT817_STR_CAL,

	.rig_init = 		ft817_init,
	.rig_cleanup =          ft817_cleanup,
	.rig_open = 		ft817_open,
	.rig_close = 		ft817_close,
	.set_freq = 		ft817_set_freq,
	.get_freq = 		ft817_get_freq,
	.set_mode = 		ft817_set_mode,
	.get_mode = 		ft817_get_mode,
	.set_ptt = 		ft817_set_ptt,
	.get_ptt = 		ft817_get_ptt,
	.get_dcd = 		ft817_get_dcd,
	.set_rptr_shift = 	ft817_set_rptr_shift,
	.set_rptr_offs = 	ft817_set_rptr_offs,
	.set_split_vfo = 	ft817_set_split_vfo,
	.get_split_vfo =	ft817_get_split_vfo,
	.set_rit = 		ft817_set_rit,
	.set_dcs_code = 	ft817_set_dcs_code,
	.set_ctcss_tone = 	ft817_set_ctcss_tone,
	.set_dcs_sql = 	        ft817_set_dcs_sql,
	.set_ctcss_sql = 	ft817_set_ctcss_sql,
    .power2mW =             ft817_power2mW,
    .mW2power =             ft817_mW2power,
	.set_powerstat = 	ft817_set_powerstat,
	.get_level = 		ft817_get_level,
	.set_func = 		ft817_set_func,
	.vfo_op =               ft817_vfo_op,
};

/* ---------------------------------------------------------------------- */

int ft817_init (RIG *rig)
{
	struct ft817_priv_data *p;

	rig_debug (RIG_DEBUG_VERBOSE,"ft817: ft817_init called \n");

	if ((p = calloc(1, sizeof(struct ft817_priv_data))) == NULL)
		return -RIG_ENOMEM;

	/* Copy complete native cmd set to private cmd storage area */
	memcpy(p->pcs, ncmd, sizeof(ncmd));

	rig->state.priv = (void*) p;

	return RIG_OK;
}

int ft817_cleanup (RIG *rig)
{
	rig_debug (RIG_DEBUG_VERBOSE,"ft817: ft817_cleanup called \n");

	if (rig->state.priv)
		free(rig->state.priv);
	rig->state.priv = NULL;

	return RIG_OK;
}

int ft817_open (RIG *rig)
{
	rig_debug (RIG_DEBUG_VERBOSE,"ft817: ft817_open called \n");

	return RIG_OK;
}

int ft817_close (RIG *rig)
{
	rig_debug (RIG_DEBUG_VERBOSE,"ft817: ft817_close called \n");

	return RIG_OK;
}

/* ---------------------------------------------------------------------- */

static inline long timediff(struct timeval *tv1, struct timeval *tv2)
{
	struct timeval tv;

	tv.tv_usec = tv1->tv_usec - tv2->tv_usec;
	tv.tv_sec  = tv1->tv_sec  - tv2->tv_sec;

	return ((tv.tv_sec * 1000L) + (tv.tv_usec / 1000L));
}

static int check_cache_timeout(struct timeval *tv)
{
	struct timeval curr;
	long t;

	if (tv->tv_sec == 0 && tv->tv_usec == 0) {
		rig_debug(RIG_DEBUG_VERBOSE, "ft817: cache invalid\n");
		return 1;
	}

	gettimeofday(&curr, NULL);

	if ((t = timediff(&curr, tv)) < FT817_CACHE_TIMEOUT) {
		rig_debug(RIG_DEBUG_VERBOSE, "ft817: using cache (%ld ms)\n", t);
		return 0;
	} else {
		rig_debug(RIG_DEBUG_VERBOSE, "ft817: cache timed out (%ld ms)\n", t);
		return 1;
	}
}

static int ft817_read_eeprom(RIG *rig, unsigned short addr, unsigned char *out)
{
  struct ft817_priv_data *p = (struct ft817_priv_data *) rig->state.priv;
  unsigned char data[YAESU_CMD_LENGTH];
  int n;

  memcpy(data, (char *)p->pcs[FT817_NATIVE_CAT_EEPROM_READ].nseq, YAESU_CMD_LENGTH);

  data[0] = addr >> 8;
  data[1] = addr & 0xfe;

  write_block(&rig->state.rigport, (char *) data, YAESU_CMD_LENGTH);

  if ((n = read_block(&rig->state.rigport, (char *) data, 2)) < 0)
    return n;

  if (n != 2)
    return -RIG_EIO;

  *out = data[addr % 2];

  return RIG_OK;
}

static int ft817_get_status(RIG *rig, int status)
{
	struct ft817_priv_data *p = (struct ft817_priv_data *) rig->state.priv;
	struct timeval *tv;
	unsigned char *data;
	int len;
	int n;

	switch (status) {
	case FT817_NATIVE_CAT_GET_FREQ_MODE_STATUS:
		data = p->fm_status;
		len  = YAESU_CMD_LENGTH;
		tv   = &p->fm_status_tv;
		break;
	case FT817_NATIVE_CAT_GET_RX_STATUS:
		data = &p->rx_status;
		len  = 1;
		tv   = &p->rx_status_tv;
		break;
	case FT817_NATIVE_CAT_GET_TX_STATUS:
		data = &p->tx_status;
		len  = 1;
		tv   = &p->tx_status_tv;
		break;
	default:
		rig_debug(RIG_DEBUG_ERR, "ft817_get_status: Internal error!\n");
		return -RIG_EINTERNAL;
	}

	serial_flush(&rig->state.rigport);

	write_block(&rig->state.rigport, (char *) p->pcs[status].nseq, YAESU_CMD_LENGTH);

	if ((n = read_block(&rig->state.rigport, (char *) data, len)) < 0)
		return n;

	if (n != len)
		return -RIG_EIO;

  if (status == FT817_NATIVE_CAT_GET_FREQ_MODE_STATUS) {
    if ((n = ft817_read_eeprom(rig, 0x0065, &p->fm_status[5])) < 0)
      return n;

    p->fm_status[5] >>= 5;
  }

	gettimeofday(tv, NULL);

	return RIG_OK;
}

/* ---------------------------------------------------------------------- */

int ft817_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
	struct ft817_priv_data *p = (struct ft817_priv_data *) rig->state.priv;
	int n;

	if (vfo != RIG_VFO_CURR)
		return -RIG_ENTARGET;

	if (check_cache_timeout(&p->fm_status_tv))
		if ((n = ft817_get_status(rig, FT817_NATIVE_CAT_GET_FREQ_MODE_STATUS)) < 0)
			return n;

	*freq = from_bcd_be(p->fm_status, 8) * 10;

	return RIG_OK;
}

int ft817_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
	struct ft817_priv_data *p = (struct ft817_priv_data *) rig->state.priv;
	int n;

	if (vfo != RIG_VFO_CURR)
		return -RIG_ENTARGET;

	if (check_cache_timeout(&p->fm_status_tv))
		if ((n = ft817_get_status(rig, FT817_NATIVE_CAT_GET_FREQ_MODE_STATUS)) < 0)
			return n;

	switch (p->fm_status[4] & 0x7f) {
	case 0x00:
		*mode = RIG_MODE_LSB;
		break;
	case 0x01:
		*mode = RIG_MODE_USB;
		break;
	case 0x02:
		*mode = RIG_MODE_CW;
		break;
	case 0x03:
		*mode = RIG_MODE_CWR;
		break;
	case 0x04:
		*mode = RIG_MODE_AM;
		break;
	case 0x06:
		*mode = RIG_MODE_WFM;
		break;
	case 0x08:
		*mode = RIG_MODE_FM;
		break;
  case 0x0a:
    switch (p->fm_status[5])
      {
      case FT817_DIGI_RTTY: *mode = RIG_MODE_RTTYR; break;
      case FT817_DIGI_PSK_L: *mode = RIG_MODE_PKTLSB; break;
      case FT817_DIGI_PSK_U: *mode = RIG_MODE_PKTUSB; break;
      case FT817_DIGI_USER_L: *mode = RIG_MODE_PKTLSB; break;
      case FT817_DIGI_USER_U: *mode = RIG_MODE_PKTUSB; break;
      }
    break;
	case 0x0C:
		*mode = RIG_MODE_PKTFM;
		break;
	default:
		*mode = RIG_MODE_NONE;
	}

	if (p->fm_status[4] & 0x80)		/* narrow */
		{
			*width = rig_passband_narrow (rig, *mode);
		}
	else
		{
			*width = RIG_PASSBAND_NORMAL;
		}

	return RIG_OK;
}

int ft817_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
  struct ft817_priv_data *p = (struct ft817_priv_data *) rig->state.priv;
  int n;

  if (vfo != RIG_VFO_CURR)
    return -RIG_ENTARGET;

  if (check_cache_timeout(&p->tx_status_tv))
    if ((n = ft817_get_status(rig, FT817_NATIVE_CAT_GET_TX_STATUS)) < 0)
      return n;

  if (p->tx_status & 0x80)
    {
      // TX status not valid when in RX
      unsigned char c;
      if ((n = ft817_read_eeprom(rig, 0x007a, &c)) < 0) /* get split status */
        return n;
      *split = (c & 0x80) ? RIG_SPLIT_ON : RIG_SPLIT_OFF;
    }
  else
    {
      *split = (p->tx_status & 0x20) ? RIG_SPLIT_ON : RIG_SPLIT_OFF;
    }

  return RIG_OK;
}

int ft817_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
	struct ft817_priv_data *p = (struct ft817_priv_data *) rig->state.priv;
	int n;

	if (vfo != RIG_VFO_CURR)
		return -RIG_ENTARGET;

	if (check_cache_timeout(&p->tx_status_tv))
		if ((n = ft817_get_status(rig, FT817_NATIVE_CAT_GET_TX_STATUS)) < 0)
			return n;

	*ptt = ((p->tx_status & 0x80) == 0);

	return RIG_OK;
}

static int ft817_get_pometer_level(RIG *rig, value_t *val)
{
	struct ft817_priv_data *p = (struct ft817_priv_data *) rig->state.priv;
	int n;

	if (check_cache_timeout(&p->tx_status_tv))
		if ((n = ft817_get_status(rig, FT817_NATIVE_CAT_GET_TX_STATUS)) < 0)
			return n;

	/* Valid only if PTT is on.
       FT-817 returns the number of bars in the lowest 4 bits
    */
	if ((p->tx_status & 0x80) == 0) {
        /* the rig has 10 bars on its display */
        val->f = (p->tx_status & 0x0F) / 10.0;

    }
	else {
		val->f = 0.0;
    }

	return RIG_OK;
}


/* frontend will always use RAWSTR+cal_table */
static int ft817_get_smeter_level(RIG *rig, value_t *val)
{
	struct ft817_priv_data *p = (struct ft817_priv_data *) rig->state.priv;
	int n;

	if (check_cache_timeout(&p->rx_status_tv))
		if ((n = ft817_get_status(rig, FT817_NATIVE_CAT_GET_RX_STATUS)) < 0)
			return n;

	//n = (p->rx_status & 0x0F) - 9;

	//val->i = n * ((n > 0) ? 10 : 6);

    /* S-meter value is returned in the lower 4 bits.
       0x00 = S0 (-54dB)
       0x01 = S1
       0x02 = S2
       ...
       0x09 = S9 (0dB)
       0x0A = S9+10 (10dB)
       0x0B = S9+20 and so on
    */
    n = (p->rx_status & 0x0F);

    if (n < 0x0A) {
        val->i = (6*n)-54;
    }
    else {
        val->i = 10*(n-9);
    }

	return RIG_OK;
}


static int ft817_get_raw_smeter_level(RIG *rig, value_t *val)
{
	struct ft817_priv_data *p = (struct ft817_priv_data *) rig->state.priv;
	int n;

	if (check_cache_timeout(&p->rx_status_tv))
		if ((n = ft817_get_status(rig, FT817_NATIVE_CAT_GET_RX_STATUS)) < 0)
			return n;

	val->i = p->rx_status & 0x0F;

	return RIG_OK;
}


int ft817_get_level (RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
	if (vfo != RIG_VFO_CURR)
		return -RIG_ENTARGET;

	switch (level) {

	case RIG_LEVEL_STRENGTH:
        /* The front-end will always call for RAWSTR and use the cal_table */
		return ft817_get_smeter_level(rig, val);
        break;

	case RIG_LEVEL_RAWSTR:
		return ft817_get_raw_smeter_level(rig, val);
        break;

	case RIG_LEVEL_RFPOWER:
		return ft817_get_pometer_level(rig, val);
        break;

	default:
		return -RIG_EINVAL;
	}

	return RIG_OK;
}

int ft817_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
	struct ft817_priv_data *p = (struct ft817_priv_data *) rig->state.priv;
	int n;

	if (vfo != RIG_VFO_CURR)
		return -RIG_ENTARGET;

	if (check_cache_timeout(&p->rx_status_tv))
		if ((n = ft817_get_status(rig, FT817_NATIVE_CAT_GET_RX_STATUS)) < 0)
			return n;

	/* TODO: consider bit 6 too ??? (CTCSS/DCS code match) */
	if (p->rx_status & 0x80)
		*dcd = RIG_DCD_OFF;
	else
		*dcd = RIG_DCD_ON;

	return RIG_OK;
}

/* ---------------------------------------------------------------------- */

static int ft817_read_ack(RIG *rig)
{
#if (FT817_POST_WRITE_DELAY == 0)
	char dummy;
	int n;

	if ((n = read_block(&rig->state.rigport, &dummy, 1)) < 0) {
		rig_debug(RIG_DEBUG_ERR, "ft817: error reading ack\n");
		return n;
	}

	rig_debug(RIG_DEBUG_TRACE,"ft817: ack received (%d)\n", dummy);

	if (dummy != 0)
		return -RIG_ERJCTED;
#endif

	return RIG_OK;
}

/*
 * private helper function to send a private command sequence.
 * Must only be complete sequences.
 */
static int ft817_send_cmd(RIG *rig, int index)
{
	struct ft817_priv_data *p = (struct ft817_priv_data *) rig->state.priv;

	if (p->pcs[index].ncomp == 0) {
		rig_debug(RIG_DEBUG_VERBOSE, "ft817: Incomplete sequence\n");
		return -RIG_EINTERNAL;
	}

	write_block(&rig->state.rigport, (char *) p->pcs[index].nseq, YAESU_CMD_LENGTH);
	return ft817_read_ack(rig);
}

/*
 * The same for incomplete commands.
 */
static int ft817_send_icmd(RIG *rig, int index, unsigned char *data)
{
	struct ft817_priv_data *p = (struct ft817_priv_data *) rig->state.priv;
	unsigned char cmd[YAESU_CMD_LENGTH];

	if (p->pcs[index].ncomp == 1) {
		rig_debug(RIG_DEBUG_VERBOSE, "ft817: Complete sequence\n");
		return -RIG_EINTERNAL;
	}

	cmd[YAESU_CMD_LENGTH - 1] = p->pcs[index].nseq[YAESU_CMD_LENGTH - 1];
	memcpy(cmd, data, YAESU_CMD_LENGTH - 1);

	write_block(&rig->state.rigport, (char *) cmd, YAESU_CMD_LENGTH);
	return ft817_read_ack(rig);
}

/* ---------------------------------------------------------------------- */

int ft817_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
	unsigned char data[YAESU_CMD_LENGTH - 1];

	if (vfo != RIG_VFO_CURR)
		return -RIG_ENTARGET;

	rig_debug(RIG_DEBUG_VERBOSE,"ft817: requested freq = %"PRIfreq" Hz\n", freq);

	/* fill in the frequency */
	to_bcd_be(data, (freq + 5) / 10, 8);

	rig_force_cache_timeout(
	    &((struct ft817_priv_data *)rig->state.priv)->fm_status_tv);

	return ft817_send_icmd(rig, FT817_NATIVE_CAT_SET_FREQ, data);
}

int ft817_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
	int index;	/* index of sequence to send */

	if (vfo != RIG_VFO_CURR)
		return -RIG_ENTARGET;

	rig_debug(RIG_DEBUG_VERBOSE,"ft817: generic mode = %x \n", mode);

	switch(mode) {

	case RIG_MODE_AM:
		index = FT817_NATIVE_CAT_SET_MODE_AM;
		break;

	case RIG_MODE_CW:
		index = FT817_NATIVE_CAT_SET_MODE_CW;
		break;

	case RIG_MODE_USB:
		index = FT817_NATIVE_CAT_SET_MODE_USB;
		break;

	case RIG_MODE_LSB:
		index = FT817_NATIVE_CAT_SET_MODE_LSB;
		break;

	case RIG_MODE_RTTY:
  case RIG_MODE_PKTUSB:
    /* user has to have correct DIG mode setup on rig */
    index = FT817_NATIVE_CAT_SET_MODE_DIG;
    break;

	case RIG_MODE_FM:
		index = FT817_NATIVE_CAT_SET_MODE_FM;
		break;

	case RIG_MODE_CWR:
		index = FT817_NATIVE_CAT_SET_MODE_CWR;
		break;

	case RIG_MODE_PKTFM:
		index = FT817_NATIVE_CAT_SET_MODE_PKT;
		break;

	default:
		return -RIG_EINVAL;
	}

	/* just ignore passband */
/* 	if (width != RIG_PASSBAND_NORMAL) */
/* 		return -RIG_EINVAL; */

	rig_force_cache_timeout(
	    &((struct ft817_priv_data *)rig->state.priv)->fm_status_tv);

	return ft817_send_cmd(rig, index);
}

int ft817_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
	int index, n;

	if (vfo != RIG_VFO_CURR)
		return -RIG_ENTARGET;

	rig_debug(RIG_DEBUG_VERBOSE, "ft817: ft817_set_ptt called\n");

	switch(ptt) {
	case RIG_PTT_ON:
		index = FT817_NATIVE_CAT_PTT_ON;
		break;
	case RIG_PTT_OFF:
		index = FT817_NATIVE_CAT_PTT_OFF;
		break;
	default:
		return -RIG_EINVAL;
	}

	n = ft817_send_cmd(rig, index);

	rig_force_cache_timeout(
	    &((struct ft817_priv_data *)rig->state.priv)->tx_status_tv);

	if (n < 0 && n != -RIG_ERJCTED)
		return n;

	return RIG_OK;
}

int ft817_set_func (RIG *rig, vfo_t vfo, setting_t func, int status)
{
	if (vfo != RIG_VFO_CURR)
		return -RIG_ENTARGET;

	switch (func) {
	case RIG_FUNC_LOCK:
		if (status)
			return ft817_send_cmd (rig, FT817_NATIVE_CAT_LOCK_ON);
		else
			return ft817_send_cmd (rig, FT817_NATIVE_CAT_LOCK_OFF);

	case RIG_FUNC_TONE:
		if (status)
			return ft817_send_cmd (rig, FT817_NATIVE_CAT_SET_CTCSS_ENC_ON);
		else
			return ft817_send_cmd (rig, FT817_NATIVE_CAT_SET_CTCSS_DCS_OFF);

	case RIG_FUNC_TSQL:
		if (status)
			return ft817_send_cmd (rig, FT817_NATIVE_CAT_SET_CTCSS_ON);
		else
			return ft817_send_cmd (rig, FT817_NATIVE_CAT_SET_CTCSS_DCS_OFF);

	default:
		return -RIG_EINVAL;
	}
}

int ft817_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code)
{
	unsigned char data[YAESU_CMD_LENGTH - 1];
/* 	int n; */

	if (vfo != RIG_VFO_CURR)
		return -RIG_ENTARGET;

	rig_debug(RIG_DEBUG_VERBOSE, "ft817: set DCS code (%d)\n", code);

	if (code == 0)
		return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_DCS_OFF);

	/* fill in the DCS code - the rig doesn't support separate codes... */
	to_bcd_be(data,     code, 4);
	to_bcd_be(data + 2, code, 4);


	/* FT-817 does not have the DCS_ENC_ON command, so we just set the tone here */

/* 	if ((n = ft817_send_icmd(rig, FT817_NATIVE_CAT_SET_DCS_CODE, data)) < 0) */
/* 		return n; */

/* 	return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_DCS_ENC_ON); */

	return ft817_send_icmd(rig, FT817_NATIVE_CAT_SET_DCS_CODE, data);
}

int ft817_set_dcs_sql (RIG *rig, vfo_t vfo, tone_t code)
{
	unsigned char data[YAESU_CMD_LENGTH - 1];
	int n;

	if (vfo != RIG_VFO_CURR)
		return -RIG_ENTARGET;

	rig_debug(RIG_DEBUG_VERBOSE, "ft817: set DCS sql (%d)\n", code);

	if (code == 0)
		return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_DCS_OFF);

	/* fill in the DCS code - the rig doesn't support separate codes... */
	to_bcd_be(data,     code, 4);
	to_bcd_be(data + 2, code, 4);

	if ((n = ft817_send_icmd(rig, FT817_NATIVE_CAT_SET_DCS_CODE, data)) < 0)
		return n;

	return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_DCS_ON);
}


int ft817_set_ctcss_tone (RIG *rig, vfo_t vfo, tone_t tone)
{
	unsigned char data[YAESU_CMD_LENGTH - 1];
	int n;

	if (vfo != RIG_VFO_CURR)
		return -RIG_ENTARGET;

	rig_debug(RIG_DEBUG_VERBOSE, "ft817: set CTCSS tone (%.1f)\n", tone / 10.0);

	if (tone == 0)
		return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_DCS_OFF);

	/* fill in the CTCSS freq - the rig doesn't support separate tones... */
	to_bcd_be(data,     tone, 4);
	to_bcd_be(data + 2, tone, 4);

	if ((n = ft817_send_icmd(rig, FT817_NATIVE_CAT_SET_CTCSS_FREQ, data)) < 0)
		return n;

	return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_ENC_ON);
}


int ft817_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
	unsigned char data[YAESU_CMD_LENGTH - 1];
	int n;

	if (vfo != RIG_VFO_CURR)
		return -RIG_ENTARGET;

	rig_debug(RIG_DEBUG_VERBOSE, "ft817: set CTCSS sql (%.1f)\n", tone / 10.0);

	if (tone == 0)
		return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_DCS_OFF);

	/* fill in the CTCSS freq - the rig doesn't support separate tones... */
	to_bcd_be(data,     tone, 4);
	to_bcd_be(data + 2, tone, 4);

	if ((n = ft817_send_icmd(rig, FT817_NATIVE_CAT_SET_CTCSS_FREQ, data)) < 0)
		return n;

	return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_ON);
}

int ft817_set_rptr_shift (RIG *rig, vfo_t vfo, rptr_shift_t shift)
{
	/* Note: this doesn't have effect unless FT817 is in FM mode
	   although the command is accepted in any mode.
	*/
	if (vfo != RIG_VFO_CURR)
		return -RIG_ENTARGET;

	rig_debug(RIG_DEBUG_VERBOSE, "ft817: set repeter shift = %i\n", shift);

	switch (shift) {

	case RIG_RPT_SHIFT_NONE:
		return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_RPT_SHIFT_SIMPLEX);

	case RIG_RPT_SHIFT_MINUS:
		return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_RPT_SHIFT_MINUS);

	case RIG_RPT_SHIFT_PLUS:
		return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_RPT_SHIFT_PLUS);

	}

	return -RIG_EINVAL;
}

int ft817_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs)
{
	unsigned char data[YAESU_CMD_LENGTH - 1];

	if (vfo != RIG_VFO_CURR)
		return -RIG_ENTARGET;

	rig_debug(RIG_DEBUG_VERBOSE, "ft817: set repeter offs = %li\n", offs);

	/* fill in the offset freq */
	to_bcd_be(data, offs / 10, 8);

	return ft817_send_icmd(rig, FT817_NATIVE_CAT_SET_RPT_OFFSET, data);
}

int ft817_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
	unsigned char data[YAESU_CMD_LENGTH - 1];
	int n;

	if (vfo != RIG_VFO_CURR)
		return -RIG_ENTARGET;

	rig_debug(RIG_DEBUG_VERBOSE, "ft817: set rit = %li)\n", rit);

	/* fill in the RIT freq */
	data[0] = (rit < 0) ? 255 : 0;
	data[1] = 0;
	to_bcd_be(data + 2, labs(rit) / 10, 4);

	if ((n = ft817_send_icmd(rig, FT817_NATIVE_CAT_SET_CLAR_FREQ, data)) < 0)
		return n;

	/* the rig rejects if these are repeated - don't confuse user with retcode */
	if (rit == 0)
		ft817_send_cmd(rig, FT817_NATIVE_CAT_CLAR_OFF);
	else
		ft817_send_cmd(rig, FT817_NATIVE_CAT_CLAR_ON);

	return RIG_OK;
}


int ft817_set_powerstat(RIG *rig, powerstat_t status)
{
	switch (status) {
	case RIG_POWER_OFF:
		return ft817_send_cmd(rig, FT817_NATIVE_CAT_PWR_OFF);
	case RIG_POWER_ON:
		return ft817_send_cmd(rig, FT817_NATIVE_CAT_PWR_ON);
	case RIG_POWER_STANDBY:
	default:
		return -RIG_EINVAL;
	}
}

int ft817_vfo_op         (RIG *rig, vfo_t vfo, vfo_op_t op)
{
	switch (op) {

	case RIG_OP_TOGGLE:
		return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_VFOAB);

	default:
		return -RIG_EINVAL;
	}
}


/* FIXME: this function silently ignores the vfo args and just turns
   split ON or OFF.
*/
int ft817_set_split_vfo  (RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
	int index, n;

/* 	if (vfo != RIG_VFO_CURR) */
/* 		return -RIG_ENTARGET; */

	rig_debug(RIG_DEBUG_VERBOSE, "ft817: ft817_set_split_vfo called\n");

	switch (split) {

	case RIG_SPLIT_ON:
		index = FT817_NATIVE_CAT_SPLIT_ON;
		break;

	case RIG_SPLIT_OFF:
		index = FT817_NATIVE_CAT_SPLIT_OFF;
		break;

	default:
		return -RIG_EINVAL;
	}

	n = ft817_send_cmd (rig, index);

	if (n < 0 && n != -RIG_ERJCTED)
		return n;

	return RIG_OK;

}



/* FIXME: currently ignores mode and freq */
/*
   No documentation on how to interpret it but the max number
   of bars on the display is 10 and I measure:
                          8 bars = 5W
                          5 bars = 2.5W
                          3 bars = 1W
                          1 bar  = 0.5W
*/
int ft817_power2mW       (RIG *rig, unsigned int *mwpower, float power,
                          freq_t freq, rmode_t mode)
{
    *mwpower = (int) (power*6000);
    return RIG_OK;
}


/* FIXME: currently ignores mode and freq */
int ft817_mW2power       (RIG *rig, float *power, unsigned int mwpower,
                          freq_t freq, rmode_t mode)
{
    *power = mwpower/6000.0;
    return RIG_OK;
}


/* ---------------------------------------------------------------------- */

