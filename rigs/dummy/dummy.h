/*
 *  Hamlib Dummy backend - main header
 *  Copyright (c) 2001-2009 by Stephane Fillod
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

#ifndef _DUMMY_H
#define _DUMMY_H 1

#include "hamlib/rig.h"
#include "token.h"

/* backend conf */
#define TOK_CFG_MAGICCONF       TOKEN_BACKEND(1)
#define TOK_CFG_STATIC_DATA     TOKEN_BACKEND(2)
#define TOK_CFG_STREAM_MODE     TOKEN_BACKEND(3)
#define TOK_CFG_STREAM_TONE_FREQ TOKEN_BACKEND(4)
#define TOK_CFG_STREAM_TONE_AMP  TOKEN_BACKEND(5)
#define TOK_CFG_STREAM_IQ_OFFSET TOKEN_BACKEND(6)
#define TOK_CFG_STREAM_SYNTH_GAP TOKEN_BACKEND(7)


/* ext_level's and ext_parm's tokens */
#define TOK_EL_MAGICLEVEL    TOKEN_BACKEND(1)
#define TOK_EL_MAGICFUNC     TOKEN_BACKEND(2)
#define TOK_EL_MAGICOP       TOKEN_BACKEND(3)
#define TOK_EP_MAGICPARM     TOKEN_BACKEND(4)
#define TOK_EL_MAGICCOMBO    TOKEN_BACKEND(5)
#define TOK_EL_MAGICEXTFUNC  TOKEN_BACKEND(6)


#include "dummy_stream.h"

#define NB_CHAN 22      /* see caps->chan_list */

struct dummy_priv_data
{
    vfo_t curr_vfo;
    vfo_t last_vfo; /* VFO A or VFO B, when in MEM mode */

    split_t split;
    vfo_t tx_vfo;
    ptt_t ptt;
    powerstat_t powerstat;
    int bank;
    value_t parms[RIG_SETTING_MAX];
    int ant_option[4]; /* simulate 4 antennas */
    int trn; /* transceive */

    channel_t *curr;    /* points to vfo_a, vfo_b or mem[] */

    channel_t vfo_maina;
    channel_t vfo_mainb;
    channel_t vfo_suba;
    channel_t vfo_subb;
    channel_t vfo_c;
    channel_t mem[NB_CHAN];

    struct ext_list *ext_funcs;
    struct ext_list *ext_parms;

    char *magic_conf;
    int static_data;

    /* Streaming parameters (set via conf tokens, read at stream open) */
    int stream_mode;            /* DUMMY_STREAM_TONE/SILENCE/LOOPBACK */
    float stream_tone_freq;     /* Audio tone frequency in Hz */
    float stream_tone_amp;      /* Tone amplitude 0.0–1.0 */
    float stream_iq_offset;     /* I/Q offset frequency in Hz */
    long stream_synth_gap;      /* Inject one RX gap of N samples (0 = off) */

    /* Per-stream state tracking for loopback pairing. stream_states_lock guards
     * the table so open/close and the loopback thread's peer resolution cannot
     * race, and holds the peer alive across a loopback read. */
    struct dummy_stream_state *stream_states[RIG_STREAM_TYPE_COUNT][DUMMY_MAX_STREAMS_PER_TYPE];
    pthread_mutex_t stream_states_lock;
};

extern struct rig_caps dummy_caps;
extern struct rig_caps dummy_no_vfo_caps;
extern struct rig_caps netrigctl_caps;
extern struct rig_caps flrig_caps;
extern struct rig_caps trxmanager_caps;
extern struct rig_caps tci1x_caps;
extern struct rig_caps aclog_caps;
extern struct rig_caps sdrsharp_caps;
extern struct rig_caps quisk_caps;
extern struct rig_caps gqrx_caps;

int netrigctl_get_vfo_mode(RIG *);

#endif /* _DUMMY_H */
