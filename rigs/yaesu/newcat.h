/*
 * hamlib - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *              and the Hamlib Group (hamlib-developer at lists.sourceforge.net)
 *
 * newcat.h - (C) Nate Bargmann 2007 (n0nb at arrl.net)
 *            (C) Terry Embry 2008-2010
 *
 * This shared library provides the backend API for communicating
 * via serial interface to any Yaesu radio using the new "CAT"
 * interface commands that are similar to the Kenwood command set.
 *
 * Models this code aims to support are FTDX-9000*, FT-2000,
 * FT-950, FT-450.  Much testing remains.  -N0NB
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


#ifndef _NEWCAT_H
#define _NEWCAT_H 1

#include <tones.h>
#include <token.h>

/* Handy constants */

#ifndef TRUE
#define TRUE 1
#endif
#define ON TRUE
#ifndef FALSE
#define FALSE 0
#endif
#define OFF FALSE

typedef char ncboolean;

/* shared function version */
#define NEWCAT_VER "20230109"

/* Hopefully large enough for future use, 128 chars plus '\0' */
#define NEWCAT_DATA_LEN                 129

/* arbitrary value for now.  11 bits (8N2+1) == 2.2917 mS @ 4800 bps */
#define NEWCAT_DEFAULT_READ_TIMEOUT     (NEWCAT_DATA_LEN * 5)


#define NEWCAT_MEM_CAP {    \
    .freq = 1,      \
    .mode = 1,      \
    .rit = 1,       \
    .xit = 1,       \
    .rptr_shift = 1, \
    .ctcss_tone = 1,\
    .ctcss_sql = 1,\
}

#define YAESU_DEFAULT_VD_METER_200W_CAL \
{ \
    3, \
    { \
        {0, 0.0f}, \
        {196, 50.0f}, \
        {255, 65.0f}, \
    } \
}



extern const struct confparams newcat_cfg_params[];

/*
 * Private caps for newcat rigs
 */

#define NEWCAT_ROOFING_FILTER_COUNT 12

struct newcat_roofing_filter
{
    /* Index of the roofing filter in the ext level combo */
    int index;
    /* Value to set the selected roofing filter, must be 0 for a get-only choice */
    char set_value;
    /* Value returned by the rig for this roofing filter, must be 0 for a set-only choice */
    char get_value;
    /* Width of the filter in Hz */
    int width;
    /* 0 = Always available, 1 = Optional filter */
    int optional;
};

struct newcat_priv_caps
{
    int roofing_filter_count;
    struct newcat_roofing_filter roofing_filters[NEWCAT_ROOFING_FILTER_COUNT];
};

/*
 * Private state for newcat rigs
 */
struct newcat_priv_data
{
    char                cmd_str[NEWCAT_DATA_LEN];       /* command string buffer */
    char
    ret_data[NEWCAT_DATA_LEN];      /* returned data--max value, most are less */
    int
    current_mem;                    /* private memory channel number */
    int
    rig_id;                         /* rig id from CAT Command ID; */
    int trn_state;  /* AI state found at startup */
    int fast_set_commands; /* do not check for ACK/NAK; needed for high throughput > 100 commands/s */
    int width_frequency; /* found at startup */
    struct timespec cache_start;
    char last_if_response[NEWCAT_DATA_LEN];
    int poweron; /* to prevent powering on more than once */
    int question_mark_response_means_rejected; /* the question mark response has multiple meanings */
    char front_rear_status; /* e.g. FTDX5000 EX103 status */
};

/*
 * Functions considered to be Stable:
 *
 * Functions considered to be Beta:
 *
 * Functions considered to be Alpha:
 *      newcat_set_freq
 *      newcat_get_freq
 *      newcat_set_vfo
 *      newcat_get_vfo
 *      newcat_get_rit
 *      newcat_set_rit
 *      newcat_get_xit
 *      newcat_set_xit
 *      power2mW
 *      mW2power
 *      newcat_get_ant
 *      newcat_set_ant
 *      newcat_get_ptt
 * Functions not yet implemented
 * most everything at this time.
 *
 * At this time, CAT documentation for the FT-450 can be obtained from
 * the Yaesu website at: http://www.yaesu.com/downloadFile.cfm?FileID=2600&FileCatID=158&FileName=FT%2D450%5FCAT%5FOperation%5FReference%5FBook.pdf&FileContentType=application%2Fpdf
 *
 */

/*
 * newcat function definitions.
 *
 */

int newcat_get_cmd(RIG *rig);
int newcat_set_cmd(RIG *rig);

int newcat_init(RIG *rig);
int newcat_cleanup(RIG *rig);
int newcat_open(RIG *rig);
int newcat_close(RIG *rig);

int newcat_set_conf(RIG *rig, token_t token, const char *val);
int newcat_get_conf(RIG *rig, token_t token, char *val);
int newcat_get_conf2(RIG *rig, token_t token, char *val, int val_len);

int newcat_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int newcat_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);

int newcat_set_vfo(RIG *rig, vfo_t vfo);
int newcat_get_vfo(RIG *rig, vfo_t *vfo);

int newcat_set_tx_vfo(RIG *rig, vfo_t tx_vfo);
int newcat_get_tx_vfo(RIG *rig, vfo_t *tx_vfo);

int newcat_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int newcat_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);

int newcat_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
int newcat_get_ptt(RIG * rig, vfo_t vfo, ptt_t * ptt);
int newcat_set_ant(RIG * rig, vfo_t vfo, ant_t ant, value_t option);
int newcat_get_ant(RIG * rig, vfo_t vfo, ant_t dummy, value_t * option, ant_t * ant_curr, ant_t * ant_tx, ant_t *ant_rx);
int newcat_set_level(RIG * rig, vfo_t vfo, setting_t level, value_t val);
int newcat_get_level(RIG * rig, vfo_t vfo, setting_t level, value_t * val);
int newcat_set_func(RIG * rig, vfo_t vfo, setting_t func, int status);
int newcat_get_func(RIG * rig, vfo_t vfo, setting_t func, int *status);
int newcat_set_mem(RIG * rig, vfo_t vfo, int ch);
int newcat_get_mem(RIG * rig, vfo_t vfo, int *ch);
int newcat_vfo_op(RIG * rig, vfo_t vfo, vfo_op_t op);
const char *newcat_get_info(RIG * rig);

int newcat_get_rit(RIG * rig, vfo_t vfo, shortfreq_t * rit);
int newcat_set_rit(RIG * rig, vfo_t vfo, shortfreq_t rit);
int newcat_get_xit(RIG * rig, vfo_t vfo, shortfreq_t * xit);
int newcat_set_xit(RIG * rig, vfo_t vfo, shortfreq_t xit);
int newcat_power2mW(RIG * rig, unsigned int *mwpower, float power, freq_t freq, rmode_t mode);
int newcat_mW2power(RIG * rig, float *power, unsigned int mwpower, freq_t freq, rmode_t mode);
int newcat_set_split_vfo(RIG * rig, vfo_t vfo, split_t split, vfo_t tx_vfo);
int newcat_get_split_vfo(RIG * rig, vfo_t vfo, split_t * split, vfo_t *tx_vfo);
int newcat_set_rptr_shift(RIG * rig, vfo_t vfo, rptr_shift_t rptr_shift);
int newcat_get_rptr_shift(RIG * rig, vfo_t vfo, rptr_shift_t * rptr_shift);
int newcat_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs);
int newcat_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *offs);
int newcat_set_ctcss_tone(RIG * rig, vfo_t vfo, tone_t tone);
int newcat_get_ctcss_tone(RIG * rig, vfo_t vfo, tone_t * tone);
int newcat_set_ctcss_sql(RIG * rig, vfo_t vfo, tone_t tone);
int newcat_get_ctcss_sql(RIG * rig, vfo_t vfo, tone_t * tone);
int newcat_set_powerstat(RIG * rig, powerstat_t status);
int newcat_get_powerstat(RIG * rig, powerstat_t * status);
int newcat_set_ts(RIG * rig, vfo_t vfo, shortfreq_t ts);
int newcat_get_ts(RIG * rig, vfo_t vfo, shortfreq_t * ts);
int newcat_set_trn(RIG * rig, int trn);
int newcat_get_trn(RIG * rig, int *trn);
int newcat_set_channel(RIG * rig, vfo_t vfo, const channel_t * chan);
int newcat_get_channel(RIG * rig, vfo_t vfo, channel_t * chan, int read_only);
rmode_t newcat_rmode(char mode);
char newcat_modechar(rmode_t rmode);
rmode_t newcat_rmode_width(RIG *rig, vfo_t vfo, char mode, pbwidth_t *width);

int newcat_set_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t val);
int newcat_get_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t *val);

int newcat_send_morse(RIG *rig, vfo_t vfo, const char *msg);
int newcat_send_voice_mem(RIG *rig, vfo_t vfo, int ch);
int newcat_set_clock(RIG *rig, int year, int month, int day, int hour, int min,
                     int sec, double msec, int utc_offset);
int newcat_get_clock(RIG *rig, int *year, int *month, int *day, int *hour,
                     int *min, int *sec, double *msec, int *utc_offset);
int newcat_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch);

#define TOKEN_BACKEND(t) (t)

#define TOK_ROOFING_FILTER TOKEN_BACKEND(100)
#define TOK_KEYER          TOKEN_BACKEND(101)
#define TOK_APF_FREQ       TOKEN_BACKEND(102)
#define TOK_APF_WIDTH      TOKEN_BACKEND(103)
#define TOK_CONTOUR        TOKEN_BACKEND(104)
#define TOK_CONTOUR_FREQ   TOKEN_BACKEND(105)
#define TOK_CONTOUR_LEVEL  TOKEN_BACKEND(106)
#define TOK_CONTOUR_WIDTH  TOKEN_BACKEND(107)

#endif /* _NEWCAT_H */
