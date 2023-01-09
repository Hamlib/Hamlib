/*
 *  Hamlib Kenwood backend - main header
 *  Copyright (c) 2000-2011 by Stephane Fillod
 *  Copyright (C) 2009,2010 Alessandro Zummo <a.zummo@towertech.it>
 *  Copyright (C) 2009,2010,2011,2012,2013 by Nate Bargmann, n0nb@n0nb.us
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

#ifndef _KENWOOD_H
#define _KENWOOD_H 1

#include <string.h>
#include "token.h"
#include "misc.h"
#include "idx_builtin.h"

#define BACKEND_VER "20230109"

#define EOM_KEN ';'
#define EOM_TH '\r'

#define KENWOOD_MODE_TABLE_MAX  24
#define KENWOOD_MAX_BUF_LEN   128 /* max answer len, arbitrary */


/* Tokens for Parameters common to multiple rigs.
 * Use token # >= 1 or <= 100.  Defined here so they will be
 * available in Kenwood name space.
 */
#define TOK_VOICE TOKEN_BACKEND(1)
#define TOK_FINE  TOKEN_BACKEND(2)
#define TOK_XIT   TOKEN_BACKEND(3)
#define TOK_RIT   TOKEN_BACKEND(4)
#define TOK_NO_ID TOKEN_BACKEND(5)

#define TOK_FUNC_FILTER_WIDTH_DATA TOKEN_BACKEND(6) // Data communications mode that affects SL/SH/FW commands

/* Token structure assigned to .cfgparams in rig_caps */
extern struct confparams kenwood_cfg_params[];


/*
 * modes in use by the "MD" command
 */
#define MD_NONE '0'
#define MD_LSB  '1'
#define MD_USB  '2'
#define MD_CW   '3'
#define MD_FM   '4'
#define MD_AM   '5'
#define MD_FSK  '6'
#define MD_CWR  '7'
// MD_DIG used by SDRPlay
#define MD_DIG  '8'
#define MD_FSKR '9'

/* S-meter calibration tables */
/* This one for the TS590 no doubt applies elsewhere */
#define TS590_SM_CAL { 10, \
  { \
    { 0, -54 }, \
    { 3, -48 }, \
    { 6, -36 }, \
    { 9, -24 }, \
    { 12, -12 }, \
    { 15, 0 }, \
    { 20, 20 }, \
    { 25, 40 }, \
    { 30, 60 }, \
  } }

#define RIG_IS_HPSDR     (rig->caps->rig_model == RIG_MODEL_HPSDR)
#define RIG_IS_K2        (rig->caps->rig_model == RIG_MODEL_K2)
#define RIG_IS_K3        (rig->caps->rig_model == RIG_MODEL_K3)
#define RIG_IS_K3S       (rig->caps->rig_model == RIG_MODEL_K3S)
#define RIG_IS_KX2       (rig->caps->rig_model == RIG_MODEL_KX2)
#define RIG_IS_KX3       (rig->caps->rig_model == RIG_MODEL_KX3)
#define RIG_IS_THD7A     (rig->caps->rig_model == RIG_MODEL_THD7A)
#define RIG_IS_THD74     (rig->caps->rig_model == RIG_MODEL_THD74)
#define RIG_IS_TS2000    (rig->caps->rig_model == RIG_MODEL_TS2000)
#define RIG_IS_TS50      (rig->caps->rig_model == RIG_MODEL_TS50)
#define RIG_IS_TS450S    (rig->caps->rig_model == RIG_MODEL_TS450S)
#define RIG_IS_TS480     (rig->caps->rig_model == RIG_MODEL_TS480)
#define RIG_IS_TS590S    (rig->caps->rig_model == RIG_MODEL_TS590S)
#define RIG_IS_TS590SG   (rig->caps->rig_model == RIG_MODEL_TS590SG)
#define RIG_IS_TS690S    (rig->caps->rig_model == RIG_MODEL_TS690S)
#define RIG_IS_TS790     (rig->caps->rig_model == RIG_MODEL_TS790)
#define RIG_IS_TS850     (rig->caps->rig_model == RIG_MODEL_TS850)
#define RIG_IS_TS890S    (rig->caps->rig_model == RIG_MODEL_TS890S)
#define RIG_IS_TS940     (rig->caps->rig_model == RIG_MODEL_TS940)
#define RIG_IS_TS950SDX  (rig->caps->rig_model == RIG_MODEL_TS950SDX)
#define RIG_IS_TS950S    (rig->caps->rig_model == RIG_MODEL_TS950S)
#define RIG_IS_TS990S    (rig->caps->rig_model == RIG_MODEL_TS990S)
#define RIG_IS_XG3       (rig->caps->rig_model == RIG_MODEL_XG3)
#define RIG_IS_PT8000A   (rig->caps->rig_model == RIG_MODEL_PT8000A)
#define RIG_IS_POWERSDR  (rig->caps->rig_model == RIG_MODEL_POWERSDR)
#define RIG_IS_MALACHITE (rig->caps->rig_model == RIG_MODEL_MALACHITE)
#define RIG_IS_QRPLABS (rig->caps->rig_model == RIG_MODEL_QRPLABS)

struct kenwood_filter_width
{
    rmode_t modes;
    int value;
    pbwidth_t width_hz;
};

struct kenwood_slope_filter
{
    rmode_t modes;
    int data_mode_filter;
    int value;
    pbwidth_t frequency_hz;
};

struct kenwood_priv_caps
{
    char cmdtrm;    /* Command termination chars (ken=';' or th='\r') */
    int if_len;     /* length of IF; answer excluding ';' terminator */
    rmode_t *mode_table;
    struct kenwood_filter_width *filter_width; /* Last entry should have value == -1 and width_hz == -1 */
    struct kenwood_slope_filter *slope_filter_high; /* Last entry should have value == -1 and frequency_hz == -1 */
    struct kenwood_slope_filter *slope_filter_low; /* Last entry should have value == -1 and frequency_hz == -1 */
};

struct kenwood_priv_data
{
    char info[KENWOOD_MAX_BUF_LEN];
    split_t split;    /* current split state */
    vfo_t tx_vfo;     /* split tx vfo */
    int k2_ext_lvl;   /* Initial K2 extension level */
    int k3_ext_lvl;   /* Initial K3 extension level */
    int k2_md_rtty;   /* K2 RTTY mode available flag, 1 = RTTY, 0 = N/A */
    int has_kpa3;     /* Elecraft K3 has k3pa for PC command */
    int has_kpa100;   /* Elecraft KX3/KX2 has kpa100 for PC command */
    char *fw_rev;     /* firmware revision level */
    int trn_state;    /* AI state discovered at startup */
    unsigned fw_rev_uint; /* firmware revision as a number 1.07 -> 107 */
    char verify_cmd[4];   /* command used to verify set commands */
    int is_emulation;     /* flag for TS-2000 emulations */
    void *data;           /* model specific data */
    rmode_t curr_mode;    /* used for is_emulation to avoid get_mode on VFOB */
    struct timespec cache_start;
    char last_if_response[KENWOOD_MAX_BUF_LEN];
    int poweron;   /* to avoid powering on more than once */
    int has_ps;    /* rig has PS cmd */
    int ag_format; /* which AG command is being used...see LEVEL_AF in kenwood.c*/
    int has_rit2; /* rig has set 2 rit command -- can set rit 0-99999 directly */
    int micgain_min, micgain_max; /* varies by rig so we figure it out automagically */
    int is_k2;
    int is_k3;
    int is_k3s;
    int is_kx3;
    int is_kx2;
    int is_k4;
    int is_k4d;
    int is_k4hd;
    int no_id;  // if true will not send ID; with every set command
    int opened; // true once rig_open is called to avoid setting VFOA every open call
    rmode_t modeA;
    rmode_t modeB;
    int datamodeA; // datamode status from get_mode or set_mode
    int datamodeB; // datamode status from get_mode or set_mode
};


#define kenwood_caps(rig) ((struct kenwood_priv_caps *)(rig)->caps->priv)


extern rmode_t kenwood_mode_table[KENWOOD_MODE_TABLE_MAX];

extern tone_t kenwood38_ctcss_list[];
extern tone_t kenwood42_ctcss_list[];

int kenwood_transaction(RIG *rig, const char *cmdstr, char *data, size_t datasize);
int kenwood_safe_transaction(RIG *rig, const char *cmd, char *buf,
                             size_t buf_size, size_t expected);

rmode_t kenwood2rmode(unsigned char mode, const rmode_t mode_table[]);
char rmode2kenwood(rmode_t mode, const rmode_t mode_table[]);

int kenwood_init(RIG *rig);
int kenwood_cleanup(RIG *rig);
int kenwood_open(RIG *rig);
int kenwood_close(RIG *rig);

int kenwood_set_vfo(RIG *rig, vfo_t vfo);
int kenwood_set_vfo_main_sub(RIG *rig, vfo_t vfo);
int kenwood_get_vfo_if(RIG *rig, vfo_t *vfo);
int kenwood_get_vfo_main_sub(RIG *rig, vfo_t *vfo);
int kenwood_set_split(RIG *rig, vfo_t vfo, split_t split, vfo_t txvfo);
int kenwood_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t txvfo);
int kenwood_get_split_vfo_if(RIG *rig, vfo_t rxvfo, split_t *split,
                             vfo_t *txvfo);

int kenwood_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int kenwood_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int kenwood_get_freq_if(RIG *rig, vfo_t vfo, freq_t *freq);
int kenwood_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit);
int kenwood_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit);
int kenwood_set_xit(RIG *rig, vfo_t vfo, shortfreq_t rit);
int kenwood_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *rit);
int kenwood_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int kenwood_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int kenwood_get_mode_if(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int kenwood_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int kenwood_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int kenwood_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int kenwood_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int kenwood_set_ext_parm(RIG *rig, token_t token, value_t val);
int kenwood_get_ext_parm(RIG *rig, token_t token, value_t *val);
int kenwood_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone);
int kenwood_set_ctcss_tone_tn(RIG *rig, vfo_t vfo, tone_t tone);
int kenwood_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone);
int kenwood_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone);
int kenwood_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone);
int kenwood_set_powerstat(RIG *rig, powerstat_t status);
int kenwood_get_powerstat(RIG *rig, powerstat_t *status);
int kenwood_reset(RIG *rig, reset_t reset);
int kenwood_send_morse(RIG *rig, vfo_t vfo, const char *msg);
int kenwood_set_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t option);
int kenwood_set_ant_no_ack(RIG *rig, vfo_t vfo, ant_t ant, value_t option);
int kenwood_get_ant(RIG *rig, vfo_t vfo, ant_t dummy, value_t *option, ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx);
int kenwood_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
int kenwood_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
int kenwood_set_ptt_safe(RIG *rig, vfo_t vfo, ptt_t ptt);
int kenwood_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);
int kenwood_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);
int kenwood_set_mem(RIG *rig, vfo_t vfo, int ch);
int kenwood_get_mem(RIG *rig, vfo_t vfo, int *ch);
int kenwood_get_mem_if(RIG *rig, vfo_t vfo, int *ch);
int kenwood_get_channel(RIG *rig, vfo_t vfo, channel_t *chan, int read_only);
int kenwood_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan);
int kenwood_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch);
const char *kenwood_get_info(RIG *rig);
int kenwood_get_id(RIG *rig, char *buf);
int kenwood_get_if(RIG *rig);

int kenwood_set_trn(RIG *rig, int trn);
int kenwood_get_trn(RIG *rig, int *trn);

/* only use if returned string has length 6, e.g. 'SQ011;' */
int get_kenwood_level(RIG *rig, const char *cmd, float *fval, int *ival);
int get_kenwood_func(RIG *rig, const char *cmd, int *status);

extern const struct rig_caps ts950s_caps;
extern const struct rig_caps ts950sdx_caps;
extern const struct rig_caps ts50s_caps;
extern const struct rig_caps ts140_caps;
extern const struct rig_caps ts450s_caps;
extern const struct rig_caps ts570d_caps;
extern const struct rig_caps ts570s_caps;
extern const struct rig_caps ts680s_caps;
extern const struct rig_caps ts690s_caps;
extern const struct rig_caps ts790_caps;
extern const struct rig_caps ts850_caps;
extern const struct rig_caps ts870s_caps;
extern const struct rig_caps ts930_caps;
extern const struct rig_caps ts2000_caps;
extern const struct rig_caps k2_caps;
extern const struct rig_caps k3_caps;
extern const struct rig_caps k3s_caps;
extern const struct rig_caps kx2_caps;
extern const struct rig_caps kx3_caps;
extern const struct rig_caps k4_caps;
extern const struct rig_caps xg3_caps;
extern const struct rig_caps trc80_caps;

extern const struct rig_caps thd7a_caps;
extern const struct rig_caps thd72a_caps;
extern const struct rig_caps thd74_caps;
extern const struct rig_caps tmd700_caps;
extern const struct rig_caps thf7a_caps;
extern const struct rig_caps thf7e_caps;
extern const struct rig_caps thg71_caps;
extern const struct rig_caps tmv7_caps;
extern const struct rig_caps tmv71_caps;
extern const struct rig_caps tmd710_caps;

extern const struct rig_caps ts440_caps;
extern const struct rig_caps ts940_caps;
extern const struct rig_caps ts711_caps;
extern const struct rig_caps ts811_caps;
extern const struct rig_caps r5000_caps;

extern const struct rig_caps ts480_caps;
extern const struct rig_caps ts590_caps;
extern const struct rig_caps ts590sg_caps;
extern const struct rig_caps thf6a_caps;

extern const struct rig_caps transfox_caps;

extern const struct rig_caps f6k_caps;
extern const struct rig_caps powersdr_caps;
extern const struct rig_caps pihpsdr_caps;
extern const struct rig_caps ts890s_caps;
extern const struct rig_caps pt8000a_caps;
extern const struct rig_caps malachite_caps;
extern const struct rig_caps tx500_caps;
extern const struct rig_caps sdruno_caps;
extern const struct rig_caps qrplabs_caps;

/* use when not interested in the answer, but want to check its len */
static int inline kenwood_simple_transaction(RIG *rig, const char *cmd,
        size_t expected)
{
    struct kenwood_priv_data *priv = rig->state.priv;
    return kenwood_safe_transaction(rig, cmd, priv->info, KENWOOD_MAX_BUF_LEN,
                                    expected);
}

#endif /* _KENWOOD_H */
