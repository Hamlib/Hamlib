/*
 *  Hamlib backend - SmartSDR TCP on port 4952
 *  See https://github.com/flexradio/smartsdr-api-docs/wiki/SmartSDR-TCPIP-API
 *  Copyright (c) 2024 by Michael Black W9MDB
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

#include <stdlib.h>
#include <string.h>

#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"
#include "misc.h"
#include "bandplan.h"
#include "idx_builtin.h"
#include "smartsdr_conf.h"
#include "smartsdr_level.h"
#include "smartsdr_priv.h"
#include "smartsdr_rig.h"
#include "smartsdr_session.h"
#include "smartsdr_slice.h"
#include "smartsdr_stream.h"

static int smartsdr_init(RIG *rig);
static int smartsdr_open(RIG *rig);
static int smartsdr_close(RIG *rig);
static int smartsdr_cleanup(RIG *rig);


/* The port every SmartSDR radio listens on for the control API. */
#define SMARTSDR_PORT 4992
#define DEFAULTPATH "127.0.0.1:4992"

#define SMARTSDR_FUNC (RIG_FUNC_MUTE|RIG_FUNC_SQL|RIG_FUNC_NR|RIG_FUNC_NB|\
                       RIG_FUNC_ANL|RIG_FUNC_ANF|RIG_FUNC_APF|RIG_FUNC_LOCK|\
                       RIG_FUNC_RIT|RIG_FUNC_XIT|RIG_FUNC_DIVERSITY|\
                       RIG_FUNC_VOX|RIG_FUNC_COMP|RIG_FUNC_FBKIN|\
                       RIG_FUNC_MON)
/* RIG_FUNC_TUNER means the antenna tuner. SmartSDR's "transmit set tune" is
 * the tune carrier, so mapping the two put the radio on the air; the ATU is a
 * separate object whose enable is read-only. Not advertised rather than
 * advertised wrongly. */
#define SMARTSDR_LEVEL (RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQL|RIG_LEVEL_AGC|\
                        RIG_LEVEL_NR|RIG_LEVEL_NB|RIG_LEVEL_APF|\
                        RIG_LEVEL_BALANCE|RIG_LEVEL_RFPOWER|\
                        RIG_LEVEL_MICGAIN|RIG_LEVEL_COMP|RIG_LEVEL_VOXGAIN|\
                        RIG_LEVEL_VOXDELAY|RIG_LEVEL_KEYSPD|RIG_LEVEL_CWPITCH|\
                        RIG_LEVEL_BKIN_DLYMS|RIG_LEVEL_MONITOR_GAIN|\
                        SMARTSDR_METER_LEVEL)
/* Read from the radio's meter stream. RIG_LEVEL_SET() drops these from
 * has_set_level, since every one of them is read-only. */
#define SMARTSDR_METER_LEVEL (RIG_LEVEL_STRENGTH|RIG_LEVEL_SWR|RIG_LEVEL_ALC|\
                              RIG_LEVEL_RFPOWER_METER|\
                              RIG_LEVEL_RFPOWER_METER_WATTS|\
                              RIG_LEVEL_TEMP_METER|RIG_LEVEL_VD_METER|\
                              RIG_LEVEL_ID_METER)
/* A parm is a rig-wide setting with no VFO, and the ones Hamlib defines --
 * BEEP, BACKLIGHT, KEYLIGHT, SCREENSAVER, TIME, ANN -- describe a front panel
 * this radio does not have in that sense: a SmartSDR radio is driven by
 * clients, and none of those knobs exist in its API. The rig-wide settings
 * that do matter here are chosen once at connect rather than adjusted while
 * running, so they are configuration tokens (tx_audio_source, slice, spectrum
 * and the rest) rather than parms. */
#define SMARTSDR_PARM  RIG_PARM_NONE

#define SMARTSDR_MODES (RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_PKTUSB|\
                        RIG_MODE_PKTLSB|RIG_MODE_CW|RIG_MODE_AM|\
                        RIG_MODE_FM|RIG_MODE_FMN|RIG_MODE_PKTFM|\
                        RIG_MODE_SAM|RIG_MODE_RTTY)

/* Slices are independent receivers, so Main/Sub rather than A/B, which
 * would also collide with the radio's own A-H slice lettering.
 * smartsdr_vfo_is_sub() accepts A/B as aliases. */
#define SMARTSDR_VFO (RIG_VFO_MAIN|RIG_VFO_SUB)

#define SMARTSDR_ANTS 3


/* Shared by every SmartSDR model (smartsdr_caps.h points .stream_caps here). */
static const struct rig_stream_caps smartsdr_stream_caps[] =
{
    {
        .type = RIG_STREAM_TYPE_AUDIO_RX,
        .formats = RIG_STREAM_FORMAT_PCM_F32,
        .sample_rates = { 24000, 0 },
        /* DAX audio is a stereo pair on the wire. A mono reader is served by
         * the frontend's mono/stereo map rather than by the radio. */
        .channels = { 2, 0 },
        .max_streams = 1,
        /* The backend can carry the radio's own time when the radio has a
         * clock worth carrying; each read says which source it actually got. */
        .caps_flags = RIG_STREAM_CAP_HW_TIME,
    },
    {
        .type = RIG_STREAM_TYPE_AUDIO_TX,
        .formats = RIG_STREAM_FORMAT_PCM_F32,
        .sample_rates = { 24000, 0 },
        /* DAX audio is a stereo pair on the wire. A mono reader is served by
         * the frontend's mono/stereo map rather than by the radio. */
        .channels = { 2, 0 },
        .max_streams = 1,
        /* DAX TX is continuous play-out: coarse start-at-T gating
         * only — the radio ignores hardware TX timestamps. */
        .caps_flags = RIG_STREAM_CAP_TIMED_TX_COARSE
                    | RIG_STREAM_CAP_BURST_PTT,
        .tx_schedule_horizon_ms = 30000,
    },
    /* No IQ_TX. DAX I/Q is receive-only: measured on a FLEX-8400M, streaming
     * a half-scale tone to a dax_iq stream with PTT keyed produces exactly the
     * same 0.001 W as streaming silence, so the radio is not modulating from
     * it. The radio has no distinct transmit stream type either -- both
     * "type=dax_iq ... tx=1" and "type=dax_iq_tx" come back as plain dax_iq
     * with endpoint_type=Not Assigned, and a dax_iq stream binds to a
     * panadapter, which is a receive object. Transmit from a client is
     * "type=dax_tx", the audio path, which AUDIO_TX above uses. */
    {
        .type = RIG_STREAM_TYPE_IQ_RX,
        .formats = RIG_STREAM_FORMAT_IQ_CF32,
        .sample_rates = { 24000, 48000, 96000, 192000, 0 },
        /* One complex stream: I and Q are the parts of a sample, not
         * two channels. */
        .channels = { 1, 0 },
        .max_streams = 1,
        .caps_flags = RIG_STREAM_CAP_HW_TIME,
    },
    { 0 },
};

/* The radio's slice is chosen with the slice= setting, which defaults to A.
 * The per-slice models below name one slice each, which is the other way to
 * reach the same thing and what an existing configuration is likely to use. */
struct rig_caps smartsdr_rig_caps =
{
    RIG_MODEL(RIG_MODEL_SMARTSDR),
    .model_name =     "SmartSDR",
#include "smartsdr_caps.h"
};

struct rig_caps smartsdr_a_rig_caps =
{
    RIG_MODEL(RIG_MODEL_SMARTSDR_A),
    .model_name =     "SmartSDR Slice A",
#include "smartsdr_caps.h"
};

struct rig_caps smartsdr_b_rig_caps =
{
    RIG_MODEL(RIG_MODEL_SMARTSDR_B),
    .model_name =     "SmartSDR Slice B",
#include "smartsdr_caps.h"
};

struct rig_caps smartsdr_c_rig_caps =
{
    RIG_MODEL(RIG_MODEL_SMARTSDR_C),
    .model_name =     "SmartSDR Slice C",
#include "smartsdr_caps.h"
};

struct rig_caps smartsdr_d_rig_caps =
{
    RIG_MODEL(RIG_MODEL_SMARTSDR_D),
    .model_name =     "SmartSDR Slice D",
#include "smartsdr_caps.h"
};

struct rig_caps smartsdr_e_rig_caps =
{
    RIG_MODEL(RIG_MODEL_SMARTSDR_E),
    .model_name =     "SmartSDR Slice E",
#include "smartsdr_caps.h"
};

struct rig_caps smartsdr_f_rig_caps =
{
    RIG_MODEL(RIG_MODEL_SMARTSDR_F),
    .model_name =     "SmartSDR Slice F",
#include "smartsdr_caps.h"
};

struct rig_caps smartsdr_g_rig_caps =
{
    RIG_MODEL(RIG_MODEL_SMARTSDR_G),
    .model_name =     "SmartSDR Slice G",
#include "smartsdr_caps.h"
};

struct rig_caps smartsdr_h_rig_caps =
{
    RIG_MODEL(RIG_MODEL_SMARTSDR_H),
    .model_name =     "SmartSDR Slice H",
#include "smartsdr_caps.h"
};


/* ************************************************************************* */

int smartsdr_init(RIG *rig)
{
    struct smartsdr_priv_data *priv;
    struct rig_state *rs = STATE(rig);
    hamlib_port_t *rp = RIGPORT(rig);

    ENTERFUNC;

    rs->priv = (struct smartsdr_priv_data *)calloc(1, sizeof(
                   struct smartsdr_priv_data));

    if (!rs->priv)
    {
        /* whoops! memory shortage! */
        RETURNFUNC(-RIG_ENOMEM);
    }

    priv = rs->priv;

    /* calloc would otherwise mean "slice A" and "we created slice 0". */
    priv->split_slice_override = -1;
    priv->split_created_slice = -1;
    /* calloc's 0 is stdin, which is a real fd to close by mistake. */
    priv->udp_sock = -1;
    priv->opened_slice = -1;
    /* Silence is what ends a session, so zero would end every one at once. */
    priv->liveness_timeout_ms = SMARTSDR_LIVENESS_TIMEOUT_MS;
    priv->vita_port = SMARTSDR_VITA_UDP_PORT;

    /* 0 is a meter index the radio uses, so an unnamed meter must not hold
     * one: a value for meter 0 would be attributed to whichever slot still
     * held it, and the wait for the radio's meter descriptions would think
     * every meter had already been named. */
    {
        int m;

        for (m = 0; m < SMARTSDR_MTR_COUNT; m++)
        {
            priv->meter_wire_id[m] = -1;
        }
    }

    /* A per-slice model names its slice as surely as the setting does. */
    priv->slice_explicit = (rs->rig_model != RIG_MODEL_SMARTSDR);

    pthread_mutex_init(&priv->tcp_mutex, NULL);
    pthread_mutex_init(&priv->stream_lock, NULL);
    pthread_mutex_init(&priv->state_lock, NULL);
    pthread_mutex_init(&priv->reply_lock, NULL);
    pthread_cond_init(&priv->reply_cond, NULL);
    pthread_cond_init(&priv->udp_idle, NULL);

    strncpy(rp->pathname, DEFAULTPATH, sizeof(rp->pathname));

    /* Naming the radio is enough: "-r <host>" alone would otherwise be taken
     * to rigctld's port, where nothing answers. */
    rp->default_port = SMARTSDR_PORT;

    switch (rs->rig_model)
    {
    case RIG_MODEL_SMARTSDR_A: priv->slicenum = 0; break;

    case RIG_MODEL_SMARTSDR_B: priv->slicenum = 1; break;

    case RIG_MODEL_SMARTSDR_C: priv->slicenum = 2; break;

    case RIG_MODEL_SMARTSDR_D: priv->slicenum = 3; break;

    case RIG_MODEL_SMARTSDR_E: priv->slicenum = 4; break;

    case RIG_MODEL_SMARTSDR_F: priv->slicenum = 5; break;

    case RIG_MODEL_SMARTSDR_G: priv->slicenum = 6; break;

    case RIG_MODEL_SMARTSDR_H: priv->slicenum = 7; break;

    /* The slice= setting chooses the slice for this model; A is the default
     * so that the setting can be left out entirely. */
    case RIG_MODEL_SMARTSDR: priv->slicenum = 0; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unknown rig model=%s\n", __func__,
                  rs->model_name);
        pthread_mutex_destroy(&priv->tcp_mutex);
        pthread_mutex_destroy(&priv->stream_lock);
        pthread_mutex_destroy(&priv->state_lock);
        pthread_mutex_destroy(&priv->reply_lock);
        pthread_cond_destroy(&priv->reply_cond);
        pthread_cond_destroy(&priv->udp_idle);
        free(priv);
        rs->priv = NULL;
        RETURNFUNC(-RIG_ENIMPL);
    }

    {
        /* Environment alias for the nat_traversal conf token. */
        const char *ew = getenv("HAMLIB_SMARTSDR_WAN");

        priv->nat_traversal = (ew != NULL && ew[0] == '1' && ew[1] == '\0');
    }

    RETURNFUNC(RIG_OK);
}

/* Register with the radio, learn its state and take the slice this rig
 * drives. The control thread is already reading by the time this runs, which
 * is what makes the replies below arrive at all. */
static int smartsdr_open_session(RIG *rig)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    hamlib_port_t *rp = RIGPORT(rig);
    char resp[SMARTSDR_RESP_LEN];
    int saved_timeout = rp->timeout;

    rp->timeout = (saved_timeout > 8000) ? saved_timeout : 8000;

    {
        int rc = smartsdr_register_session(rig);

        if (rc != RIG_OK)
        {
            rp->timeout = saved_timeout;
            return rc;
        }
    }

    /* Pull delayed S|slice / S|display pan lines that follow the R for sub pan. */
    (void)smartsdr_transaction_resp(rig, "ping", resp, sizeof(resp));

    /* RF_frequency can lag the command R-line on loaded radios; retry sync.
     * Do not loop on slice_pan_id==0 alone (simulators keep pan=0 until prep). */
    {
        int attempt;

        for (attempt = 0; attempt < 4 && smartsdr_slice_freq(priv) <= 0.0;
                attempt++)
        {
            if (attempt > 0)
            {
                hl_usleep(50 * 1000);
            }

            (void)smartsdr_sync_display_slice_status(rig);
        }
    }

    if (smartsdr_slice_freq(priv) <= 0.0)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: slice RF_frequency not in status yet\n", __func__);
    }

    rp->timeout = saved_timeout;

    smartsdr_query_info(rig);

    {
        /* Slice status has been absorbed by now, so whether the configured
         * slice exists is known. */
        int rc = smartsdr_ensure_slice(rig);

        if (rc != RIG_OK)
        {
            rp->timeout = saved_timeout;
            return rc;
        }
    }

    if (priv->tx_audio_source[0])
    {
        smartsdr_apply_tx_audio_source(rig);
    }

    if (priv->spectrum_enabled)
    {
        (void)smartsdr_spectrum_start(rig);
    }

    return smartsdr_session_start_threads(rig);
}


int smartsdr_open(RIG *rig)
{
    int retval;

    ENTERFUNC;

    /* Nothing can be commanded until something is reading the answers. */
    retval = smartsdr_control_start(rig);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    retval = smartsdr_open_session(rig);

    if (retval != RIG_OK)
    {
        /* The frontend closes the port and does not call rig_close after a
         * failed open, so the reader has to be taken down here. */
        smartsdr_control_stop(rig);
    }

    RETURNFUNC(retval);
}

int smartsdr_close(RIG *rig)
{
    ENTERFUNC;

    /* Stop reconnecting first: closing is deliberate, and the thread would
     * otherwise race to rebuild what is being dismantled. */
    smartsdr_session_stop_reconnect(rig);

    /* Metering and spectrum each hold a reference to the shared UDP socket. */
    smartsdr_meters_stop(rig);
    smartsdr_spectrum_stop(rig);

    /* A slice created because the configured one was absent goes with us. */
    smartsdr_release_opened_slice(rig);

    smartsdr_session_stop_threads(rig);

    /* Last, because everything above still commands the radio and needs its
     * replies delivered. */
    smartsdr_control_stop(rig);

    RETURNFUNC(RIG_OK);
}

int smartsdr_cleanup(RIG *rig)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;

    ENTERFUNC;

    if (priv)
    {
        pthread_mutex_destroy(&priv->tcp_mutex);
        pthread_mutex_destroy(&priv->stream_lock);
        pthread_mutex_destroy(&priv->state_lock);
        pthread_mutex_destroy(&priv->reply_lock);
        pthread_cond_destroy(&priv->reply_cond);
        pthread_cond_destroy(&priv->udp_idle);
        free(priv);
    }

    STATE(rig)->priv = NULL;

    RETURNFUNC(RIG_OK);
}
