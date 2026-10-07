/*
 *  Hamlib SmartSDR slices: split, VFO selection and VFO operations
 *  Copyright (c) 2026 by Mikael Nousiainen OH3BHX
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
/* SPDX-License-Identifier: LGPL-2.1-or-later */

/* A SmartSDR slice is a complete receiver with one frequency, and the radio */
/* owns as many as eight of them independently of any client. Hamlib expects */
/* two VFOs and one rig. This file reconciles the two: which slice this rig  */
/* drives, which one holds the transmitter, and how to make one appear when  */
/* Hamlib asks for something the radio has no slice for.                     */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hamlib/rig.h"
#include "hamlib/rig_state.h"
#include "misc.h"
#include "cache.h"
#include "smartsdr_priv.h"
#include "smartsdr_props.h"
#include "smartsdr_rig.h"
#include "smartsdr_session.h"
#include "smartsdr_slice.h"


/* Slices belong to the radio, so any of them may be the transmit slice. Track
 * just enough of every slice to answer split queries. */
void smartsdr_slice_info_absorb(struct smartsdr_priv_data *priv, int slicenum,
                                const char *line)
{
    const char *v;
    long l;
    double d;

    if (priv == NULL || line == NULL
            || slicenum < 0 || slicenum >= SMARTSDR_MAX_SLICES)
    {
        return;
    }

    if (smartsdr_status_as_long(smartsdr_status_find(line, "in_use"), &l))
    {
        priv->slices[slicenum].in_use = (uint8_t)(l != 0);
    }

    if (smartsdr_status_as_long(smartsdr_status_find(line, "tx"), &l))
    {
        priv->slices[slicenum].tx = (uint8_t)(l != 0);
    }

    if (smartsdr_status_as_double(smartsdr_status_find(line, "RF_frequency"), &d))
    {
        priv->slices[slicenum].freq_hz = d * 1e6;
    }

    if (smartsdr_status_as_long(smartsdr_status_find(line, "active"), &l))
    {
        priv->slices[slicenum].active = (uint8_t)(l != 0);
    }

    if (smartsdr_status_as_long(smartsdr_status_find(line, "filter_lo"), &l))
    {
        priv->slices[slicenum].filter_lo = (int)l;
    }

    if (smartsdr_status_as_long(smartsdr_status_find(line, "filter_hi"), &l))
    {
        priv->slices[slicenum].filter_hi = (int)l;
    }

    v = smartsdr_status_find(line, "mode");

    if (v != NULL)
    {
        char name[24];
        size_t n = 0;
        rmode_t m;

        while (v[n] != '\0' && v[n] != ' ' && v[n] != '\n'
                && n + 1 < sizeof(name))
        {
            name[n] = v[n];
            n++;
        }

        name[n] = '\0';

        if (smartsdr_parse_mode_status_value(name, &m) == 0)
        {
            priv->slices[slicenum].mode = m;
            priv->slices[slicenum].mode_seen = 1;
        }
    }
}


int smartsdr_tx_slice(const struct smartsdr_priv_data *priv)
{
    int i;

    /* An explicit configuration wins over whichever slice holds tx=1, for
     * reporting as well as for commands. */
    if (priv->split_slice_override >= 0
            && priv->split_slice_override < SMARTSDR_MAX_SLICES)
    {
        return priv->split_slice_override;
    }

    for (i = 0; i < SMARTSDR_MAX_SLICES; i++)
    {
        if (priv->slices[i].tx)
        {
            return i;
        }
    }

    return -1;
}


/* Hamlib addresses two VFOs; this radio has one receiver per slice. Main is
 * the slice this rig is bound to, Sub is the transmit slice. A/B are accepted
 * as aliases because vfo_fixup rewrites them into the Main family only when
 * the current VFO is already Main-ish, so the backend cannot rely on it. */
int smartsdr_vfo_is_sub(vfo_t vfo)
{
    switch (vfo)
    {
    case RIG_VFO_SUB:
    case RIG_VFO_SUB_A:
    case RIG_VFO_B:
    case RIG_VFO_MAIN_B:
    case RIG_VFO_TX:
        return 1;

    case RIG_VFO_MAIN:
    case RIG_VFO_MAIN_A:
    case RIG_VFO_A:
    case RIG_VFO_CURR:
    case RIG_VFO_NONE:
        return 0;

    default:
        return -1;
    }
}


/* The slice behind the Sub VFO. With create, one is made when neither the
 * override nor tx=1 gives a usable slice; without it, an absent slice is
 * reported unavailable rather than quietly conjured by a read. */
int smartsdr_sub_slice(RIG *rig, int create)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    int usable = 0;
    int slice;

    smartsdr_state_lock(priv);
    slice = smartsdr_tx_slice(priv);

    if (slice >= 0 && slice != priv->slicenum
            && (priv->slices[slice].in_use || slice == priv->split_slice_override))
    {
        /* A configured slice may not have been reported yet; trust the
         * configuration and let the command fail if it truly is absent. */
        usable = (priv->slices[slice].in_use || !create);
    }

    smartsdr_state_unlock(priv);

    if (usable)
    {
        return slice;
    }

    if (!create)
    {
        return -1;
    }

    if (smartsdr_create_split_slice(rig) != RIG_OK)
    {
        return -1;
    }

    smartsdr_state_lock(priv);
    slice = smartsdr_tx_slice(priv);
    smartsdr_state_unlock(priv);

    return slice;
}


/* The classic operating values, derived from the tracked properties so there
 * is one place where radio status becomes rig state. */
freq_t smartsdr_slice_freq(const struct smartsdr_priv_data *priv)
{
    /* The radio reports RF_frequency in MHz. */
    return (freq_t)(priv->props[SMARTSDR_P_RF_FREQUENCY].dval * 1e6);
}


void smartsdr_slice_set_freq(struct smartsdr_priv_data *priv, freq_t hz)
{
    priv->props[SMARTSDR_P_RF_FREQUENCY].dval = (double)hz / 1e6;
    priv->props[SMARTSDR_P_RF_FREQUENCY].ival = (int)((double)hz / 1e6);
    priv->props[SMARTSDR_P_RF_FREQUENCY].seen = 1;
    priv->props[SMARTSDR_P_RF_FREQUENCY].stamp_ms = smartsdr_now_ms();
}


rmode_t smartsdr_slice_mode(const struct smartsdr_priv_data *priv)
{
    rmode_t mode = RIG_MODE_NONE;

    if (priv->props[SMARTSDR_P_MODE].seen
            && smartsdr_parse_mode_status_value(priv->props[SMARTSDR_P_MODE].sval,
                                                &mode) == 0)
    {
        return mode;
    }

    return RIG_MODE_NONE;
}


pbwidth_t smartsdr_slice_width(const struct smartsdr_priv_data *priv)
{
    /* The radio reports the passband as filter edges relative to the carrier,
     * and both are negative for the lower-sideband modes, so the width is the
     * span rather than the upper edge. */
    return (pbwidth_t)(priv->props[SMARTSDR_P_FILTER_HI].ival
                       - priv->props[SMARTSDR_P_FILTER_LO].ival);
}


/* PTT is the radio-wide interlock state, but only reported for this rig when
 * its slice holds the transmitter. */
int smartsdr_slice_ptt(const struct smartsdr_priv_data *priv)
{
    if (!priv->props[SMARTSDR_P_TX].seen || priv->props[SMARTSDR_P_TX].ival == 0)
    {
        return 0;
    }

    return strcmp(priv->props[SMARTSDR_P_STATE].sval, "TRANSMITTING") == 0;
}


/* ------------------------------------------------------------------ */
/* Split                                                               */
/* ------------------------------------------------------------------ */

/* SmartSDR has no second VFO. A slice is a complete receiver with one
 * frequency, and exactly one slice at a time carries tx=1. Split is therefore
 * "the transmit slice is not the one I am receiving on", and setting it up
 * means creating a second slice -- which is what the radio's own SPLIT button
 * does. */


/* Create the transmit slice on this slice's frequency and hand it the
 * transmitter. The caller sets the transmit frequency separately. */
/* Ask the radio for a new slice and return its number, or negative on failure.
 * A frequency has to be supplied: a bare "slice create" reports success with an
 * empty body and no slice number, which is useless and, read naively, would
 * name slice 0. The radio picks the index itself -- it cannot be requested. */
int smartsdr_slice_create(RIG *rig, double freq_hz, const char *ant,
                          const char *mode)
{
    char cmd[128];
    char resp[SMARTSDR_RESP_LEN];
    const char *body;
    char *end = NULL;
    long created = -1;
    int retval;

    snprintf(cmd, sizeof(cmd), "slice create freq=%.6f ant=%s mode=%s",
             freq_hz / 1e6, ant, mode);
    retval = smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* The R-line body is the new slice number. An empty body must not be read
     * as slice 0, or turning split off would remove the operator's own slice. */
    body = strrchr(resp, '|');

    if (body != NULL)
    {
        created = strtol(body + 1, &end, 10);

        if (end == body + 1)
        {
            created = -1;
        }
    }

    if (created < 0 || created >= SMARTSDR_MAX_SLICES)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: slice create gave no usable id (resp=\"%s\")\n",
                  __func__, resp);
        return -RIG_EPROTO;
    }

    return (int)created;
}


/* Record which slice holds the transmitter. The radio reports this in status,
 * but only after it has processed the command, and a caller that sets split
 * and immediately tunes the transmit frequency would otherwise be answered
 * with the slice that held it before. Setting tx=1 on one slice clears it on
 * every other, so the local view follows the same rule. Caller must not hold
 * the state lock. */
static void smartsdr_note_transmit_slice(struct smartsdr_priv_data *priv,
                                         int slice)
{
    int i;

    smartsdr_state_lock(priv);

    for (i = 0; i < SMARTSDR_MAX_SLICES; i++)
    {
        priv->slices[i].tx = (uint8_t)(i == slice);
    }

    smartsdr_state_unlock(priv);
}


int smartsdr_create_split_slice(RIG *rig)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    char cmd[128];
    char ant[sizeof(priv->props[0].sval)];
    char mode[sizeof(priv->props[0].sval)];
    freq_t freq;
    int created;
    int retval;

    smartsdr_state_lock(priv);
    freq = smartsdr_slice_freq(priv);
    snprintf(ant, sizeof(ant), "%s", priv->props[SMARTSDR_P_RXANT].seen
             ? priv->props[SMARTSDR_P_RXANT].sval : "ANT1");
    snprintf(mode, sizeof(mode), "%s", priv->props[SMARTSDR_P_MODE].seen
             ? priv->props[SMARTSDR_P_MODE].sval : "USB");
    smartsdr_state_unlock(priv);

    created = smartsdr_slice_create(rig, (double)freq, ant, mode);

    if (created < 0)
    {
        return created;
    }

    smartsdr_state_lock(priv);
    priv->split_created_slice = created;
    priv->slices[created].in_use = 1;
    priv->slices[created].freq_hz = (double)freq;
    smartsdr_state_unlock(priv);

    snprintf(cmd, sizeof(cmd), "slice set %d tx=1", created);
    retval = smartsdr_transaction_resp(rig, cmd, NULL, 0);

    if (retval == RIG_OK)
    {
        smartsdr_note_transmit_slice(priv, created);
    }

    return retval;
}


/* Default state for a slice created because the configured one was absent.
 * A frequency is required (see smartsdr_slice_create), and nothing is known
 * about the operator's setup at this point, so these are plain defaults the
 * application is expected to change immediately. */
#define SMARTSDR_NEW_SLICE_HZ    14100000.0
#define SMARTSDR_NEW_SLICE_ANT   "ANT1"
#define SMARTSDR_NEW_SLICE_MODE  "USB"

/* Make sure the slice this rig is bound to actually exists. Slices come and go
 * on the radio, and a rig addressing one that is not there accepts every
 * command and does nothing, which is the failure this guards against. */
int smartsdr_ensure_slice(RIG *rig)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    int created;
    int in_use;

    smartsdr_state_lock(priv);
    in_use = priv->slices[priv->slicenum].in_use;
    smartsdr_state_unlock(priv);

    if (in_use)
    {
        return RIG_OK;
    }

    if (priv->slice_missing_fails)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: slice %c does not exist on the radio\n",
                  __func__, 'A' + priv->slicenum);
        return -RIG_ENAVAIL;
    }

    created = smartsdr_slice_create(rig, SMARTSDR_NEW_SLICE_HZ,
                                    SMARTSDR_NEW_SLICE_ANT,
                                    SMARTSDR_NEW_SLICE_MODE);

    if (created < 0)
    {
        /* The radio limits how many slices may exist, so being refused here is
         * an ordinary outcome rather than a protocol error. */
        rig_debug(RIG_DEBUG_ERR, "%s: could not create a slice for %c\n",
                  __func__, 'A' + priv->slicenum);
        return -RIG_ENAVAIL;
    }

    smartsdr_state_lock(priv);
    priv->slices[created].in_use = 1;
    smartsdr_state_unlock(priv);

    if (created == priv->slicenum)
    {
        priv->opened_slice = created;
        return RIG_OK;
    }

    /* The radio chooses the index; it cannot be asked for one. Taking a
     * different slice than the one that was named would quietly control the
     * wrong receiver, so that is only acceptable when no slice was named. */
    if (priv->slice_explicit)
    {
        char cmd[64];

        snprintf(cmd, sizeof(cmd), "slice remove %d", created);
        (void)smartsdr_transaction_resp(rig, cmd, NULL, 0);
        smartsdr_state_lock(priv);
        priv->slices[created].in_use = 0;
        smartsdr_state_unlock(priv);
        rig_debug(RIG_DEBUG_ERR,
                  "%s: slice %c does not exist and the radio gave %c instead\n",
                  __func__, 'A' + priv->slicenum, 'A' + created);
        return -RIG_ENAVAIL;
    }

    rig_debug(RIG_DEBUG_WARN, "%s: no slice was named, using %c\n", __func__,
              'A' + created);
    priv->slicenum = created;
    priv->opened_slice = created;
    return RIG_OK;
}


/* Remove a slice created because the configured one was absent. One that was
 * already on the radio is left alone. */
void smartsdr_release_opened_slice(RIG *rig)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    char cmd[64];

    if (priv->opened_slice < 0)
    {
        return;
    }

    snprintf(cmd, sizeof(cmd), "slice remove %d", priv->opened_slice);
    (void)smartsdr_transaction_resp(rig, cmd, NULL, 0);
    smartsdr_state_lock(priv);
    priv->slices[priv->opened_slice].in_use = 0;
    smartsdr_state_unlock(priv);
    priv->opened_slice = -1;
}


int smartsdr_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    int tx_slice;

    ENTERFUNC;

    smartsdr_state_lock(priv);
    tx_slice = smartsdr_tx_slice(priv);
    smartsdr_state_unlock(priv);

    *split = (tx_slice >= 0 && tx_slice != priv->slicenum)
             ? RIG_SPLIT_ON : RIG_SPLIT_OFF;

    /* Transmit is on another slice exactly when split is on, and another slice
     * is what this backend calls Sub. */
    *tx_vfo = (*split == RIG_SPLIT_ON) ? RIG_VFO_SUB : RIG_VFO_MAIN;

    RETURNFUNC(RIG_OK);
}


int smartsdr_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    int tx_slice;

    ENTERFUNC;

    smartsdr_state_lock(priv);
    tx_slice = smartsdr_tx_slice(priv);

    if (tx_slice < 0)
    {
        smartsdr_state_unlock(priv);
        RETURNFUNC(-RIG_ENAVAIL);
    }

    *tx_freq = (freq_t)priv->slices[tx_slice].freq_hz;
    smartsdr_state_unlock(priv);

    RETURNFUNC(RIG_OK);
}


int smartsdr_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    char cmd[96];
    int tx_slice;
    int retval;

    ENTERFUNC;

    smartsdr_state_lock(priv);
    tx_slice = smartsdr_tx_slice(priv);
    smartsdr_state_unlock(priv);

    if (tx_slice < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: no transmit slice to tune\n", __func__);
        RETURNFUNC(-RIG_ENAVAIL);
    }

    snprintf(cmd, sizeof(cmd), "slice tune %d %.6f autopan=1", tx_slice,
             (double)tx_freq / 1e6);
    retval = smartsdr_transaction_resp(rig, cmd, NULL, 0);

    if (retval == RIG_OK)
    {
        smartsdr_state_lock(priv);
        priv->slices[tx_slice].freq_hz = (double)tx_freq;
        smartsdr_state_unlock(priv);
    }

    RETURNFUNC(retval);
}


int smartsdr_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    char cmd[128];
    int tx_slice;
    int retval;

    ENTERFUNC;

    smartsdr_state_lock(priv);
    tx_slice = smartsdr_tx_slice(priv);
    smartsdr_state_unlock(priv);

    if (split == RIG_SPLIT_OFF)
    {
        /* Put the transmitter back on this slice. Setting tx=1 anywhere clears
         * it everywhere else, so no second command is needed. */
        snprintf(cmd, sizeof(cmd), "slice set %d tx=1", priv->slicenum);
        retval = smartsdr_transaction_resp(rig, cmd, NULL, 0);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        /* Only remove a slice this backend created; the operator or another
         * client may be using any other. */
        if (priv->split_created_slice >= 0)
        {
            snprintf(cmd, sizeof(cmd), "slice remove %d",
                     priv->split_created_slice);
            (void)smartsdr_transaction_resp(rig, cmd, NULL, 0);
            smartsdr_state_lock(priv);
            priv->slices[priv->split_created_slice].in_use = 0;
            priv->split_created_slice = -1;
            smartsdr_state_unlock(priv);
        }

        RETURNFUNC(RIG_OK);
    }

    if (tx_slice >= 0 && tx_slice != priv->slicenum)
    {
        RETURNFUNC(RIG_OK);      /* already split */
    }

    RETURNFUNC(smartsdr_create_split_slice(rig));
}


/* ------------------------------------------------------------------ */
/* VFO selection                                                        */
/* ------------------------------------------------------------------ */

int smartsdr_set_vfo(RIG *rig, vfo_t vfo)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    int is_sub = smartsdr_vfo_is_sub(vfo);
    char cmd[64];
    int slice;

    ENTERFUNC;

    if (is_sub < 0)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (is_sub)
    {
        slice = smartsdr_sub_slice(rig, 1);

        if (slice < 0)
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }
    }
    else
    {
        slice = priv->slicenum;
    }

    /* active= marks the slice the radio considers selected, and like tx= the
     * radio clears it on every other slice by itself. */
    snprintf(cmd, sizeof(cmd), "slice set %d active=1", slice);
    RETURNFUNC(smartsdr_transaction_resp(rig, cmd, NULL, 0));
}


int smartsdr_get_vfo(RIG *rig, vfo_t *vfo)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    int sub;

    ENTERFUNC;

    sub = smartsdr_sub_slice(rig, 0);

    smartsdr_state_lock(priv);
    *vfo = (sub >= 0 && priv->slices[sub].active) ? RIG_VFO_SUB : RIG_VFO_MAIN;
    smartsdr_state_unlock(priv);

    RETURNFUNC(RIG_OK);
}


/* ------------------------------------------------------------------ */
/* Split mode                                                          */
/* ------------------------------------------------------------------ */

int smartsdr_set_split_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    char cmd[96];
    const char *name = smartsdr_mode_name(mode);
    int slice = smartsdr_sub_slice(rig, 1);
    int retval;

    ENTERFUNC;

    if (name == NULL)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (slice < 0)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    snprintf(cmd, sizeof(cmd), "slice set %d mode=%s", slice, name);
    retval = smartsdr_transaction_resp(rig, cmd, NULL, 0);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    /* Only this rig's own slice has its properties tracked from status, so
     * record what the transmit slice accepted, as prop_set does for ours. */
    smartsdr_state_lock(priv);
    priv->slices[slice].mode = mode;
    priv->slices[slice].mode_seen = 1;
    smartsdr_state_unlock(priv);

    if (width != RIG_PASSBAND_NOCHANGE && width != RIG_PASSBAND_NORMAL)
    {
        snprintf(cmd, sizeof(cmd), "filt %d 0 %ld", slice, (long)width);
        retval = smartsdr_transaction_resp(rig, cmd, NULL, 0);

        if (retval == RIG_OK)
        {
            smartsdr_state_lock(priv);
            priv->slices[slice].filter_lo = 0;
            priv->slices[slice].filter_hi = (int)width;
            smartsdr_state_unlock(priv);
        }
    }

    RETURNFUNC(retval);
}


int smartsdr_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    int slice = smartsdr_sub_slice(rig, 0);

    ENTERFUNC;

    if (slice < 0)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    smartsdr_state_lock(priv);

    /* Only this rig's own slice has its properties tracked in full, so the
     * transmit slice's mode is reported from the per-slice table. */
    if (!priv->slices[slice].mode_seen)
    {
        smartsdr_state_unlock(priv);
        RETURNFUNC(-RIG_ENAVAIL);
    }

    *mode = priv->slices[slice].mode;
    *width = (pbwidth_t)(priv->slices[slice].filter_hi
                         - priv->slices[slice].filter_lo);
    smartsdr_state_unlock(priv);

    RETURNFUNC(RIG_OK);
}


/* ------------------------------------------------------------------ */
/* VFO operations                                                      */
/* ------------------------------------------------------------------ */

int smartsdr_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    char cmd[96];
    int slice;
    freq_t mine;
    int retval;

    ENTERFUNC;

    slice = smartsdr_sub_slice(rig, op == RIG_OP_CPY);

    if (slice < 0)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    smartsdr_state_lock(priv);
    mine = smartsdr_slice_freq(priv);
    smartsdr_state_unlock(priv);

    switch (op)
    {
    case RIG_OP_CPY:
        /* Main -> Sub */
        snprintf(cmd, sizeof(cmd), "slice tune %d %.6f autopan=1", slice,
                 (double)mine / 1e6);
        retval = smartsdr_transaction_resp(rig, cmd, NULL, 0);

        if (retval == RIG_OK)
        {
            smartsdr_state_lock(priv);
            priv->slices[slice].freq_hz = (double)mine;
            smartsdr_state_unlock(priv);
        }

        RETURNFUNC(retval);

    case RIG_OP_TOGGLE:
    {
        /* Exchange the two slices' frequencies. */
        freq_t theirs;

        smartsdr_state_lock(priv);
        theirs = (freq_t)priv->slices[slice].freq_hz;
        smartsdr_state_unlock(priv);

        snprintf(cmd, sizeof(cmd), "slice tune %d %.6f autopan=1", slice,
                 (double)mine / 1e6);
        retval = smartsdr_transaction_resp(rig, cmd, NULL, 0);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        smartsdr_state_lock(priv);
        priv->slices[slice].freq_hz = (double)mine;
        smartsdr_state_unlock(priv);

        snprintf(cmd, sizeof(cmd), "slice tune %d %.6f autopan=1",
                 priv->slicenum, (double)theirs / 1e6);
        retval = smartsdr_transaction_resp(rig, cmd, NULL, 0);

        if (retval == RIG_OK)
        {
            smartsdr_state_lock(priv);
            smartsdr_slice_set_freq(priv, theirs);
            smartsdr_state_unlock(priv);
            rig_set_cache_freq(rig, RIG_VFO_A, theirs);
        }

        RETURNFUNC(retval);
    }

    default:
        RETURNFUNC(-RIG_ENIMPL);
    }
}
