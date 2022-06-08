/*
 *  Hamlib ELAD backend - main header
 *  Copyright (c) 2000-2011 by Stephane Fillod
 *  Copyright (C) 2009,2010 Alessandro Zummo <a.zummo@towertech.it>
 *  Copyright (C) 2009,2010,2011,2012,2013 by Nate Bargmann, n0nb@n0nb.us
 *  Copyright (C) 2018 by Giovanni Franza, info@hb9eik.ch
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

#ifndef _ELAD_H
#define _ELAD_H 1

#include <string.h>
#include "token.h"

#define BACKEND_VER "20220608"

#define EOM_KEN ';'
#define EOM_TH '\r'

#define ELAD_MODE_TABLE_MAX  24
#define ELAD_MAX_BUF_LEN   128 /* max answer len, arbitrary */


/* Tokens for Parameters common to multiple rigs.
 * Use token # >= 1 or <= 100.  Defined here so they will be
 * available in Kenwood name space.
 */
#define TOK_VOICE TOKEN_BACKEND(1)
#define TOK_FINE  TOKEN_BACKEND(2)
#define TOK_XIT   TOKEN_BACKEND(3)
#define TOK_RIT   TOKEN_BACKEND(4)

/* Token structure assigned to .cfgparams in rig_caps */
extern const struct confparams elad_cfg_params[];


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
#define MD_FSKR '9'

struct elad_priv_caps {
    char cmdtrm;    /* Command termination chars (ken=';' or th='\r') */
    int if_len;     /* length of IF; answer excluding ';' terminator */
    rmode_t *mode_table;
};

struct elad_priv_data {
    char info[ELAD_MAX_BUF_LEN];
    split_t split;    /* current split state */
    int k2_ext_lvl;   /* Initial K2 extension level */
    int k3_ext_lvl;   /* Initial K3 extension level */
    int k2_md_rtty;   /* K2 RTTY mode available flag, 1 = RTTY, 0 = N/A */
    char *fw_rev;   /* firmware revision level */
    int trn_state;  /* AI state discovered at startup */
    unsigned fw_rev_uint; /* firmware revision as a number 1.07 -> 107 */
    char verify_cmd[4];   /* command used to verify set commands */
    int is_emulation;     /* flag for TS-2000 emulations */
    void * data;          /* model specific data */
    rmode_t curr_mode;     /* used for is_emulation to avoid get_mode on VFOB */
};


#define elad_caps(rig) ((struct elad_priv_caps *)(rig)->caps->priv)


extern rmode_t elad_mode_table[ELAD_MODE_TABLE_MAX];

extern const tone_t elad38_ctcss_list[];
extern const tone_t elad42_ctcss_list[];

int elad_transaction(RIG *rig, const char *cmdstr, char *data, size_t datasize);
int elad_safe_transaction(RIG *rig, const char *cmd, char *buf,
        size_t buf_size, size_t expected);

rmode_t elad2rmode(unsigned char mode, const rmode_t mode_table[]);
char rmode2elad(rmode_t mode, const rmode_t mode_table[]);

int elad_init(RIG *rig);
int elad_cleanup(RIG *rig);
int elad_open(RIG *rig);
int elad_close(RIG *rig);

int elad_set_vfo(RIG *rig, vfo_t vfo);
int elad_set_vfo_main_sub(RIG *rig, vfo_t vfo);
int elad_get_vfo_if(RIG *rig, vfo_t *vfo);
int elad_get_vfo_main_sub(RIG *rig, vfo_t *vfo);
int elad_set_split(RIG *rig, vfo_t vfo , split_t split, vfo_t txvfo);
int elad_set_split_vfo(RIG *rig, vfo_t vfo , split_t split, vfo_t txvfo);
int elad_get_split_vfo_if(RIG *rig, vfo_t rxvfo, split_t *split, vfo_t *txvfo);

int elad_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int elad_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int elad_get_freq_if(RIG *rig, vfo_t vfo, freq_t *freq);
int elad_set_rit(RIG * rig, vfo_t vfo, shortfreq_t rit);
int elad_get_rit(RIG *rig, vfo_t vfo, shortfreq_t * rit);
int elad_set_xit(RIG * rig, vfo_t vfo, shortfreq_t rit);
int elad_get_xit(RIG *rig, vfo_t vfo, shortfreq_t * rit);
int elad_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int elad_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int elad_get_mode_if(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int elad_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int elad_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int elad_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int elad_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int elad_set_ext_parm(RIG *rig, token_t token, value_t val);
int elad_get_ext_parm(RIG *rig, token_t token, value_t *val);
int elad_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone);
int elad_set_ctcss_tone_tn(RIG *rig, vfo_t vfo, tone_t tone);
int elad_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone);
int elad_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone);
int elad_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone);
int elad_set_powerstat(RIG *rig, powerstat_t status);
int elad_get_powerstat(RIG *rig, powerstat_t *status);
int elad_reset(RIG *rig, reset_t reset);
int elad_send_morse(RIG *rig, vfo_t vfo, const char *msg);
int elad_set_ant (RIG * rig, vfo_t vfo, ant_t ant, value_t option);
int elad_set_ant_no_ack(RIG * rig, vfo_t vfo, ant_t ant);
int elad_get_ant (RIG * rig, vfo_t vfo, ant_t dummy, value_t *option, ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx);
int elad_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
int elad_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
int elad_set_ptt_safe(RIG *rig, vfo_t vfo, ptt_t ptt);
int elad_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);
int elad_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);
int elad_set_mem(RIG *rig, vfo_t vfo, int ch);
int elad_get_mem(RIG *rig, vfo_t vfo, int *ch);
int elad_get_mem_if(RIG *rig, vfo_t vfo, int *ch);
int elad_get_channel(RIG *rig, channel_t *chan);
int elad_set_channel(RIG *rig, const channel_t *chan);
int elad_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch);
const char * elad_get_info(RIG *rig);
int elad_get_id(RIG *rig, char *buf);

int elad_set_trn(RIG *rig, int trn);
int elad_get_trn(RIG *rig, int *trn);

/* only use if returned string has length 6, e.g. 'SQ011;' */
int get_elad_level(RIG *rig, const char *cmd, float *f);
int get_elad_func(RIG *rig, const char *cmd, int *status);

extern const struct rig_caps fdm_duo_caps;

/* use when not interested in the answer, but want to check its len */
static int inline elad_simple_transaction(RIG *rig, const char *cmd, size_t expected)
{
  struct elad_priv_data *priv = rig->state.priv;
  return elad_safe_transaction(rig, cmd, priv->info, ELAD_MAX_BUF_LEN, expected);
}

#endif /* _ELAD_H */
