/*
 *  Hamlib Kenwood backend - main header
 *  Copyright (c) 2000-2002 by Stephane Fillod
 *
 *	$Id: kenwood.h,v 1.26 2002-11-13 20:35:18 fillods Exp $
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef _KENWOOD_H
#define _KENWOOD_H 1

#define EOM_KEN ";"
#define EOM_TH "\r"

struct kenwood_priv_caps {
    /* read-only values */
    const char *cmdtrm;    /* Command termination chars (ken=';' or th='\r') */
    /* changable values */
        // nothing
};

#if 0  /* No private data for Kenwood backends. */
struct kenwood_priv_data {
    int dummy;  // placeholder for real entries.
};
#endif

extern int kenwood_init(RIG *rig);
extern int kenwood_cleanup(RIG *rig);


extern const int kenwood38_ctcss_list[];

int kenwood_transaction(RIG *rig, const char *cmd, int cmd_len, char *data,
				size_t *data_len);
int kenwood_set_vfo(RIG *rig, vfo_t vfo);
int kenwood_get_vfo(RIG *rig, vfo_t *vfo);
int kenwood_old_set_vfo(RIG *rig, vfo_t vfo);
int kenwood_old_get_vfo(RIG *rig, vfo_t *vfo);
int kenwood_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int kenwood_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int kenwood_set_rit(RIG * rig, vfo_t vfo, shortfreq_t rit);
int kenwood_get_rit(RIG *rig, vfo_t vfo, shortfreq_t * rit);
int kenwood_set_xit(RIG * rig, vfo_t vfo, shortfreq_t rit);
int kenwood_get_xit(RIG *rig, vfo_t vfo, shortfreq_t * rit);
int kenwood_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int kenwood_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int kenwood_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int kenwood_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int kenwood_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int kenwood_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int kenwood_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone);
int kenwood_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone);
int kenwood_set_powerstat(RIG *rig, powerstat_t status);
int kenwood_get_powerstat(RIG *rig, powerstat_t *status);
int kenwood_reset(RIG *rig, reset_t reset);
int kenwood_send_morse(RIG *rig, vfo_t vfo, const char *msg);
int kenwood_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
int kenwood_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
int kenwood_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);
int kenwood_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);
int kenwood_set_mem(RIG *rig, vfo_t vfo, int ch);
int kenwood_get_mem(RIG *rig, vfo_t vfo, int *ch);
const char* kenwood_get_info(RIG *rig);

int kenwood_set_trn(RIG *rig, int trn);
int kenwood_get_trn(RIG *rig, int *trn);

extern const struct rig_caps ts950sdx_caps;
extern const struct rig_caps ts50s_caps;
extern const struct rig_caps ts440_caps;
extern const struct rig_caps ts450s_caps;
extern const struct rig_caps ts570d_caps;
extern const struct rig_caps ts570s_caps;
extern const struct rig_caps ts790_caps;
extern const struct rig_caps ts850_caps;
extern const struct rig_caps ts870s_caps;
extern const struct rig_caps ts2000_caps;
extern const struct rig_caps thd7a_caps;
extern const struct rig_caps thf7a_caps;
extern const struct rig_caps thf7e_caps;
extern const struct rig_caps k2_caps;

extern BACKEND_EXPORT(int) initrigs_kenwood(void *be_handle);
extern BACKEND_EXPORT(rig_model_t) proberigs_kenwood(port_t *port);


#endif /* _KENWOOD_H */

