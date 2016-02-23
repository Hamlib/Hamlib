/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@@ix.netcom.com)
 *
 * ft757gx.h - (C) Frank Singleton 2000 (vk3fcs@@ix.netcom.com)
 * This shared library provides an API for communicating
 * via serial interface to an FT-757GX using the "CAT" interface
 * box (FIF-232C) or similar (max232 + some capacitors :-)
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


#ifndef _FT757GX_H
#define _FT757GX_H 1

#define FT757GX_STATUS_UPDATE_DATA_LENGTH   75

#define FT757GX_PACING_INTERVAL             0
#define FT757GX_PACING_DEFAULT_VALUE        0
#define FT757GX_WRITE_DELAY                 50

/* number of BCD digits--2 digits per octet */
#define BCD_LEN     8


/* Sequential fast writes confuse my FT757GX without this delay */
#define FT757GX_POST_WRITE_DELAY            5


/* Rough safe value for default timeout */
#define FT757GX_DEFAULT_READ_TIMEOUT    FT757GX_STATUS_UPDATE_DATA_LENGTH * (5 + (FT757GX_PACING_INTERVAL * FT757GX_PACING_DEFAULT_VALUE))

/*
 * Some useful offsets in the status update map (offset = book byte value - 1)
 *
 * Status Update Chart, FT757GXII
 */
#define STATUS_CURR_FREQ    5   /* Operating Frequency */
#define STATUS_CURR_MODE    9
#define STATUS_VFOA_FREQ    10
#define STATUS_VFOA_MODE    14
#define STATUS_VFOB_FREQ    15
#define STATUS_VFOB_MODE    19

/* Mode values for both set and get */
#define MODE_LSB    0x00
#define MODE_USB    0x01
#define MODE_CWW    0x02
#define MODE_CWN    0x03
#define MODE_AM     0x04
#define MODE_FM     0x05

/* TODO: get better measure numbers */
#define FT757GXII_STR_CAL { 2, { \
    {  0, -60 },    /* S0 -6dB */ \
    { 15,  60 }     /* +60 */ \
    } }

#define FT757_MEM_CAP {	\
    .freq = 1,     \
    .mode = 1,     \
    .width = 1     \
    }


/*
 * Receiver caps
 */
#define FT757GX_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)


/*
 * TX caps
 */
#define FT757GX_ALL_TX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)


/* Backend fucntion prototypes.  These map to frontend functions. */
static int ft757_init(RIG *rig);
static int ft757_cleanup(RIG *rig);
static int ft757_open(RIG *rig);

static int ft757gx_get_conf(RIG *rig, token_t token, char *val);
static int ft757gx_set_conf(RIG *rig, token_t token, const char *val);

static int ft757_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ft757_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int ft757gx_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);

static int ft757_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width); /* select mode */
static int ft757_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width); /* get mode */

static int ft757_set_vfo(RIG *rig, vfo_t vfo); /* select vfo */
static int ft757_get_vfo(RIG *rig, vfo_t *vfo); /* get vfo */

static int ft757_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
static int ft757_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);


#endif /* _FT757GX_H */
