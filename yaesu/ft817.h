/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ft817.h - (C) Chris Karpinsky 2001 (aa1vl@arrl.net)
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

#ifndef _FT817_H
#define _FT817_H 1

/*
 * No need to wait between written characters.
 */
#define FT817_WRITE_DELAY		0

/*
 * Wait 'delay' milliseconds after writing a command sequence.
 *
 * Setting this to zero means no delay but wait for an acknowledgement
 * from the rig after a command. This is undocumented but seems to work.
 * It's also the most optimal way as long as it works...
 *
 * A non-zero value disables waiting for the ack. Processing a command
 * seems to take about 60 ms so set this to 80 or so to be safe.
 */
#define FT817_POST_WRITE_DELAY		0

/*
 * Read timeout.
 */
#define FT817_TIMEOUT			3000

/*
 * Return from TX to RX may have a delay. If status is not changed
 * on the first attempt, wait this amount of milliseconds before
 * each next next attempts.
 */
#define FT817_RETRY_DELAY		100

/*
 * The time the TX, RX and FREQ/MODE status are cached (in millisec).
 * This optimises the common case of doing eg. rig_get_freq() and
 * rig_get_mode() in a row.
 *
 * The timeout is deliberately set lower than the time taken to process
 * a single command (~ 60 ms) so that a sequence
 *
 *   rig_get_freq();
 *   rig_set_freq();
 *   rig_get_freq();
 *
 * doesn't return a bogus (cached) value in the last rig_get_freq().
 */
#define FT817_CACHE_TIMEOUT		50


enum ft817_native_cmd_e {
	FT817_NATIVE_CAT_LOCK_ON = 0,
	FT817_NATIVE_CAT_LOCK_OFF,
	FT817_NATIVE_CAT_PTT_ON,
	FT817_NATIVE_CAT_PTT_OFF,
	FT817_NATIVE_CAT_SET_FREQ,
	FT817_NATIVE_CAT_SET_MODE_LSB,
	FT817_NATIVE_CAT_SET_MODE_USB,
	FT817_NATIVE_CAT_SET_MODE_CW,
	FT817_NATIVE_CAT_SET_MODE_CWR,
	FT817_NATIVE_CAT_SET_MODE_AM,
	FT817_NATIVE_CAT_SET_MODE_FM,
	FT817_NATIVE_CAT_SET_MODE_FM_N,
	FT817_NATIVE_CAT_SET_MODE_DIG,
	FT817_NATIVE_CAT_SET_MODE_PKT,
	FT817_NATIVE_CAT_CLAR_ON,
	FT817_NATIVE_CAT_CLAR_OFF,
	FT817_NATIVE_CAT_SET_CLAR_FREQ,
	FT817_NATIVE_CAT_SET_VFOAB,
	FT817_NATIVE_CAT_SPLIT_ON,
	FT817_NATIVE_CAT_SPLIT_OFF,
	FT817_NATIVE_CAT_SET_RPT_SHIFT_MINUS,
	FT817_NATIVE_CAT_SET_RPT_SHIFT_PLUS,
	FT817_NATIVE_CAT_SET_RPT_SHIFT_SIMPLEX,
	FT817_NATIVE_CAT_SET_RPT_OFFSET,
	FT817_NATIVE_CAT_SET_DCS_ON,
	FT817_NATIVE_CAT_SET_CTCSS_ON,
	FT817_NATIVE_CAT_SET_CTCSS_ENC_ON,
	FT817_NATIVE_CAT_SET_CTCSS_DCS_OFF,
	FT817_NATIVE_CAT_SET_CTCSS_FREQ,
	FT817_NATIVE_CAT_SET_DCS_CODE,
	FT817_NATIVE_CAT_GET_RX_STATUS,
	FT817_NATIVE_CAT_GET_TX_STATUS,
	FT817_NATIVE_CAT_GET_FREQ_MODE_STATUS,
	FT817_NATIVE_CAT_PWR_WAKE,
	FT817_NATIVE_CAT_PWR_ON,
	FT817_NATIVE_CAT_PWR_OFF,
  FT817_NATIVE_CAT_EEPROM_READ,
	FT817_NATIVE_SIZE		/* end marker */
};


typedef enum ft817_native_cmd_e ft817_native_cmd_t;


struct ft817_priv_data {
	yaesu_cmd_set_t pcs[FT817_NATIVE_SIZE];  /* TODO:  why? */

	/* rx status */
	struct timeval rx_status_tv;
	unsigned char rx_status;

	/* tx status */
	struct timeval tx_status_tv;
	unsigned char tx_status;

	/* freq & mode status */
	struct timeval fm_status_tv;
	unsigned char fm_status[YAESU_CMD_LENGTH+1];
};

/* fixme: why declare static? it has no effect */
static int ft817_init           (RIG *rig);
static int ft817_open           (RIG *rig);
static int ft817_cleanup        (RIG *rig);
static int ft817_close          (RIG *rig);
static int ft817_set_freq       (RIG *rig, vfo_t vfo, freq_t freq);
static int ft817_get_freq       (RIG *rig, vfo_t vfo, freq_t *freq);
static int ft817_set_mode       (RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int ft817_get_mode       (RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int ft817_set_ptt        (RIG *rig, vfo_t vfo, ptt_t ptt);
static int ft817_get_ptt        (RIG *rig, vfo_t vfo, ptt_t *ptt);
static int ft817_get_level      (RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static int ft817_set_func       (RIG *rig, vfo_t vfo, setting_t func, int status);
static int ft817_set_dcs_code   (RIG *rig, vfo_t vfo, tone_t code);
static int ft817_set_ctcss_tone (RIG *rig, vfo_t vfo, tone_t code);
static int ft817_set_dcs_sql    (RIG *rig, vfo_t vfo, tone_t code);
static int ft817_set_ctcss_sql  (RIG *rig, vfo_t vfo, tone_t tone);
static int ft817_set_rptr_shift (RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift);
static int ft817_set_rptr_offs  (RIG *rig, vfo_t vfo, shortfreq_t offs);
static int ft817_set_rit        (RIG *rig, vfo_t vfo, shortfreq_t rit);
static int ft817_get_dcd        (RIG *rig, vfo_t vfo, dcd_t *dcd);
static int ft817_set_powerstat  (RIG *rig, powerstat_t status);
static int ft817_vfo_op         (RIG *rig, vfo_t vfo, vfo_op_t op);
static int ft817_get_split_vfo  (RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo);
static int ft817_set_split_vfo  (RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo);
static int ft817_power2mW       (RIG *rig, unsigned int *mwpower, float power,
                                 freq_t freq, rmode_t mode);
static int ft817_mW2power       (RIG *rig, float *power, unsigned int mwpower,
                                 freq_t freq, rmode_t mode);

#endif /* _FT817_H */
