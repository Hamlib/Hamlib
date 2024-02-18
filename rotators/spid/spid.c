/*
 * Hamlib Rotator backend - SPID Rot1Prog & Rot2Prog
 * Copyright (c) 2009-2011 by Norvald H. Ryeng, LA6YKA
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hamlib/rotator.h"
#include "serial.h"
#include "register.h"

#include "spid.h"

#define TOK_AZRES 1
#define TOK_ELRES 2

struct spid_rot2prog_priv_data
{
    int az_resolution;
    int el_resolution;
};

enum spid_rot2prog_framemagic
{
    ROT2PROG_FRAME_START_BYTE = 'W',
    ROT2PROG_FRAME_END_BYTE = ' ',
};

enum r2p_frame_parser_state
{
    ROT2PROG_PARSER_EXPECT_FRAME_START,
    ROT2PROG_PARSER_EXPECT_CR,
    ROT2PROG_PARSER_EXPECT_LF,
    ROT2PROG_PARSER_EXPECT_FRAME_END,
};

static int read_r2p_frame(hamlib_port_t *port, unsigned char *rxbuffer,
                          size_t count)
{
    // Some MD-01 firmware can apparently print debug messages to the same
    // serial port that is used for the control protocol. This awkwardly
    // intersperses the normal fixed-size frame response with a line-based
    // logs. Theoretically, a valid response frame will not actually be emitted
    // in the middle of a log message.
    //
    // Log messages are of the format <timestamp>: <message>\r\n, where
    // <timestamp> is a unix-ish timestamp (inasmuch as it is an integer) and
    // <message> is an ASCII string.

    // Due to poor(?) design decisions by the protocol designers, the frame
    // start and end bytes are both printable ASCII characters ('W' and ' '
    // respectively) and the MD-01 response frame contains no other validation
    // information (such as a CRC), which means that a valid log line can
    // contain a character sequence that is indistinguishable from a valid
    // response frame, without actually being a valid response frame.

    // However, since the log messages appear to be reasonably strictly
    // structured, we can make a small number of assumptions that will allow us
    // to reliably separate response frames from log lines without having to
    // fall back on a heuristic-based parsing strategy. These assumptions are
    // as follows:

    // 1. A log line will always begin with an ASCII character in the range
    //    [0-9], none of which are the frame start byte.
    // 2. A log line will never contain \r\n in the middle of the line (i.e.
    //    multi-line log messages do not exist). This means a log "frame" will
    //    always be of the form [0-9]<anything>\r\n.
    // 3. The controller will not emit a response frame in the middle of a log
    //    line.
    // 4. The operating system's serial port read buffer is large enough that we
    //    won't lose data while accumulating log messages between commands.

    // Provided the above assumptions are true, a simple state machine can be
    // used to parse the response by treating the log lines as a different type
    // of frame. This could be made much more robust by applying additional
    // heuristics for specific packets (e.g. get_position has some reasonably
    // strict numerical bounds that could be used to sanity check the contents
    // of the reply frame).

    int res = 0;
    unsigned char peek = 0;
    enum r2p_frame_parser_state pstate = ROT2PROG_PARSER_EXPECT_FRAME_START;

    // This will loop infinitely in the case of a badly-behaved serial device
    // that is producing log-like frames faster than we can consume them.
    // However, this is not expected to be a practical possibility, and there's
    // no concrete loop bounds we can use.
    while (1)
    {
        switch (pstate)
        {
        case ROT2PROG_PARSER_EXPECT_FRAME_START:
            res = read_block(port, &peek, 1);

            if (res < 0) { return res; }

            switch (peek)
            {
            case ROT2PROG_FRAME_START_BYTE:
                rxbuffer[0] = peek;
                pstate = ROT2PROG_PARSER_EXPECT_FRAME_END;
                break;

            default:
                pstate = ROT2PROG_PARSER_EXPECT_CR;
                break;
            }

            break;

        case ROT2PROG_PARSER_EXPECT_CR:
            res = read_block(port, &peek, 1);

            if (res < 0) { return res; }

            if (peek == '\r') { pstate = ROT2PROG_PARSER_EXPECT_LF; }

            break;

        case ROT2PROG_PARSER_EXPECT_LF:
            res = read_block(port, &peek, 1);

            if (res < 0) { return res; }

            if (peek == '\n')
            {
                pstate = ROT2PROG_PARSER_EXPECT_FRAME_START;
            }
            else
            {
                // we have stumbled across a \r that is not immediately
                // followed by \n. We could assume this is a weirdly formed
                // log message, but I think it makes more sense to be
                // defensive here and assume it is invalid for this to
                // happen.
                return -RIG_EPROTO;
            }

            break;

        case ROT2PROG_PARSER_EXPECT_FRAME_END:
            // we already read the frame start byte
            res = read_block(port, rxbuffer + 1, count - 1);

            if (res < 0) { return res; }

            if (rxbuffer[count - 1] != ROT2PROG_FRAME_END_BYTE)
            {
                return -RIG_EPROTO;
            }

            // account for the already-read start byte here
            return res + 1;

        default:
            return -RIG_EINTERNAL;
        }
    }
}

static int spid_write(hamlib_port_t *p, const unsigned char *txbuffer,
                      size_t count)
{
    int ret = rig_flush(p);

    if (ret < 0) { return ret; }

    return write_block(p, txbuffer, count);
}

static int spid_rot_init(ROT *rot)
{
    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    if (!rot || !rot->caps)
    {
        return -RIG_EINVAL;
    }

    if (rot->caps->rot_model == ROT_MODEL_SPID_ROT2PROG ||
            rot->caps->rot_model == ROT_MODEL_SPID_MD01_ROT2PROG)
    {
        struct spid_rot2prog_priv_data *priv;

        priv = (struct spid_rot2prog_priv_data *)calloc(1, sizeof(struct
                spid_rot2prog_priv_data));

        if (!priv)
        {
            return -RIG_ENOMEM;
        }

        rot->state.priv = (void *)priv;

        priv->az_resolution = 0;
        priv->el_resolution = 0;
    }

    return RIG_OK;
}

static int spid_rot_cleanup(ROT *rot)
{
    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    if (!rot)
    {
        return -RIG_EINVAL;
    }

    if (rot->state.priv && (rot->caps->rot_model == ROT_MODEL_SPID_ROT2PROG ||
                            rot->caps->rot_model == ROT_MODEL_SPID_MD01_ROT2PROG))
    {
        free(rot->state.priv);
    }

    rot->state.priv = NULL;

    return RIG_OK;
}

static int spid_get_conf2(ROT *rot, hamlib_token_t token, char *val, int val_len)
{
    const  struct spid_rot2prog_priv_data *priv = (struct spid_rot2prog_priv_data *)
            rot->state.priv;

    rig_debug(RIG_DEBUG_TRACE, "%s called %d\n", __func__, (int)token);

    if (rot->caps->rot_model != ROT_MODEL_SPID_ROT2PROG &&
            rot->caps->rot_model != ROT_MODEL_SPID_MD01_ROT2PROG)
    {
        return -RIG_EINVAL;
    }

    switch (token)
    {
    case TOK_AZRES:
        SNPRINTF(val, val_len, "%d", priv->az_resolution);
        break;

    case TOK_ELRES:
        SNPRINTF(val, val_len, "%d", priv->el_resolution);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static int spid_get_conf(ROT *rot, hamlib_token_t token, char *val)
{
    return spid_get_conf2(rot, token, val, 128);
}

static int spid_set_conf(ROT *rot, hamlib_token_t token, const char *val)
{
    struct spid_rot2prog_priv_data *priv = (struct spid_rot2prog_priv_data *)
                                           rot->state.priv;

    rig_debug(RIG_DEBUG_TRACE, "%s: called %d=%s\n", __func__, (int)token, val);

    if (rot->caps->rot_model != ROT_MODEL_SPID_ROT2PROG &&
            rot->caps->rot_model != ROT_MODEL_SPID_MD01_ROT2PROG)
    {
        return -RIG_EINVAL;
    }

    switch (token)
    {
    case TOK_AZRES:
        priv->az_resolution = atoi(val);
        break;

    case TOK_ELRES:
        priv->el_resolution = atoi(val);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static int spid_rot1prog_rot_set_position(ROT *rot, azimuth_t az,
        elevation_t el)
{
    int retval;
    char cmdstr[13];
    unsigned int u_az;

    rig_debug(RIG_DEBUG_TRACE, "%s called: %f %f\n", __func__, az, el);

    u_az = 360 + az;

    cmdstr[0] = 0x57;                       /* S   */
    cmdstr[1] = 0x30 + u_az / 100;          /* H1  */
    cmdstr[2] = 0x30 + (u_az % 100) / 10;   /* H2  */
    cmdstr[3] = 0x30 + (u_az % 10);         /* H3  */
    cmdstr[4] = 0x30;                       /* H4  */
    cmdstr[5] = 0x00;                       /* PH  */
    cmdstr[6] = 0x00;                       /* V1  */
    cmdstr[7] = 0x00;                       /* V2  */
    cmdstr[8] = 0x00;                       /* V3  */
    cmdstr[9] = 0x00;                       /* V4  */
    cmdstr[10] = 0x00;                      /* PV  */
    cmdstr[11] = 0x2F;                      /* K   */
    cmdstr[12] = 0x20;                      /* END */

    retval = spid_write(ROTPORT(rot), (unsigned char *) cmdstr, 13);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}

static int spid_rot2prog_rot_set_position(ROT *rot, azimuth_t az,
        elevation_t el)
{
    struct rot_state *rs = &rot->state;
    hamlib_port_t *rotp = ROTPORT(rot);
    const  struct spid_rot2prog_priv_data *priv = (struct spid_rot2prog_priv_data *)
            rs->priv;
    int retval;
    int retry_read = 0;
    char cmdstr[13];
    unsigned int u_az, u_el;

    rig_debug(RIG_DEBUG_TRACE, "%s called: %f %f\n", __func__, az, el);

    if (!priv->az_resolution || !priv->el_resolution)
    {
        do
        {
            retval = spid_write(rotp,
                                (unsigned char *) "\x57\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x1F\x20", 13);

            if (retval != RIG_OK)
            {
                return retval;
            }

            memset(cmdstr, 0, 12);
            retval = read_r2p_frame(rotp, (unsigned char *) cmdstr, 12);
        }
        while (retval < 0 && retry_read++ < rotp->retry);

        if (retval < 0)
        {
            return retval;
        }
    }
    else
    {
        cmdstr[5] = priv->az_resolution;    /* PH  */
        cmdstr[10] = priv->el_resolution;   /* PV  */
    }

    u_az = cmdstr[5] * (360 + az);
    u_el = cmdstr[10] * (360 + el);

    cmdstr[0] = 0x57;                       /* S   */
    cmdstr[1] = 0x30 + u_az / 1000;         /* H1  */
    cmdstr[2] = 0x30 + (u_az % 1000) / 100; /* H2  */
    cmdstr[3] = 0x30 + (u_az % 100) / 10;   /* H3  */
    cmdstr[4] = 0x30 + (u_az % 10);         /* H4  */

    cmdstr[6] = 0x30 + u_el / 1000;         /* V1  */
    cmdstr[7] = 0x30 + (u_el % 1000) / 100; /* V2  */
    cmdstr[8] = 0x30 + (u_el % 100) / 10;   /* V3  */
    cmdstr[9] = 0x30 + (u_el % 10);         /* V4  */

    cmdstr[11] = 0x2F;                      /* K   */
    cmdstr[12] = 0x20;                      /* END */

    retval = spid_write(rotp, (unsigned char *) cmdstr, 13);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* Unlike the original Rot2Prog, MD-01 and MD-02 return the position
       after receiving the set position command. */
    if (rot->caps->rot_model == ROT_MODEL_SPID_MD01_ROT2PROG)
    {
        retry_read = 0;

        do
        {
            retval = read_r2p_frame(rotp, (unsigned char *) cmdstr, 12);
        }
        while ((retval < 0) && (retry_read++ < rotp->retry));
    }

    return RIG_OK;
}

static int spid_rot_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    hamlib_port_t *rotp = ROTPORT(rot);
    int retval;
    int retry_read = 0;
    char posbuf[12];

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    do
    {
        retval = spid_write(rotp,
                            (unsigned char *) "\x57\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x1F\x20", 13);

        if (retval != RIG_OK)
        {
            return retval;
        }

        memset(posbuf, 0, 12);

        if (rot->caps->rot_model == ROT_MODEL_SPID_ROT1PROG)
        {
            retval = read_r2p_frame(rotp, (unsigned char *) posbuf, 5);
        }
        else if (rot->caps->rot_model == ROT_MODEL_SPID_ROT2PROG ||
                 rot->caps->rot_model == ROT_MODEL_SPID_MD01_ROT2PROG)
        {
            retval = read_r2p_frame(rotp, (unsigned char *) posbuf, 12);
        }
        else
        {
            retval = -RIG_EINVAL;
        }
    }
    while (retval < 0 && retry_read++ < rotp->retry);

    if (retval < 0)
    {
        return retval;
    }

    *az  = posbuf[1] * 100;
    *az += posbuf[2] * 10;
    *az += posbuf[3];

    if (rot->caps->rot_model == ROT_MODEL_SPID_ROT2PROG ||
            rot->caps->rot_model == ROT_MODEL_SPID_MD01_ROT2PROG)
    {
        *az += posbuf[4] / 10.0;
    }

    *az -= 360;

    *el = 0.0;

    if (rot->caps->rot_model == ROT_MODEL_SPID_ROT2PROG ||
            rot->caps->rot_model == ROT_MODEL_SPID_MD01_ROT2PROG)
    {
        *el  = posbuf[6] * 100;
        *el += posbuf[7] * 10;
        *el += posbuf[8];
        *el += posbuf[9] / 10.0;
        *el -= 360;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: (az, el) = (%.1f, %.1f)\n",
              __func__, *az, *el);

    return RIG_OK;
}

static int spid_rot_stop(ROT *rot)
{
    hamlib_port_t *rotp = ROTPORT(rot);
    int retval;
    int retry_read = 0;
    char posbuf[12];

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    do
    {
        retval = spid_write(rotp,
                            (unsigned char *) "\x57\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x0F\x20", 13);

        if (retval != RIG_OK)
        {
            return retval;
        }

        memset(posbuf, 0, 12);

        if (rot->caps->rot_model == ROT_MODEL_SPID_ROT1PROG)
        {
            retval = read_r2p_frame(rotp, (unsigned char *) posbuf, 5);
        }
        else if (rot->caps->rot_model == ROT_MODEL_SPID_ROT2PROG ||
                 rot->caps->rot_model == ROT_MODEL_SPID_MD01_ROT2PROG)
        {
            retval = read_r2p_frame(rotp, (unsigned char *) posbuf, 12);
        }
    }
    while (retval < 0 && retry_read++ < rotp->retry);

    if (retval < 0)
    {
        return retval;
    }

    return RIG_OK;
}

static int spid_md01_rot2prog_rot_move(ROT *rot, int direction, int speed)
{
    char dir = 0x00;
    int retval;
    char cmdstr[13];

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    switch (direction)
    {
    case ROT_MOVE_UP:
        dir = 0x04;
        break;

    case ROT_MOVE_DOWN:
        dir = 0x08;
        break;

    case ROT_MOVE_LEFT:
        dir = 0x01;
        break;

    case ROT_MOVE_RIGHT:
        dir = 0x02;
        break;
    }

    cmdstr[0] = 0x57;                       /* S   */
    cmdstr[1] = dir;                        /* H1  */
    cmdstr[2] = 0x00;                       /* H2  */
    cmdstr[3] = 0x00;                       /* H3  */
    cmdstr[4] = 0x00;                       /* H4  */
    cmdstr[6] = 0x00;                       /* V1  */
    cmdstr[7] = 0x00;                       /* V2  */
    cmdstr[8] = 0x00;                       /* V3  */
    cmdstr[9] = 0x00;                       /* V4  */
    cmdstr[11] = 0x14;                      /* K   */
    cmdstr[12] = 0x20;                      /* END */

    /* The rotator must be stopped before changing directions. Since
       we don't know which direction we're already moving in (if
       moving at all), always send the stop command first. */
    spid_rot_stop(rot);

    retval = spid_write(ROTPORT(rot), (unsigned char *) cmdstr, 13);
    return retval;
}

const struct confparams spid_cfg_params[] =
{
    {
        TOK_AZRES, "az_resolution", "Azimuth resolution", "Number of pulses per degree, 0 = auto sense",
        "0", RIG_CONF_NUMERIC, { .n = { 0, 0xff, 1 } }
    },
    {
        TOK_ELRES, "el_resolution", "Eleveation resolution", "Number of pulses per degree, 0 = auto sense",
        "0", RIG_CONF_NUMERIC, { .n = { 0, 0xff, 1 } }
    },
    { RIG_CONF_END, NULL, }
};

const struct rot_caps spid_rot1prog_rot_caps =
{
    ROT_MODEL(ROT_MODEL_SPID_ROT1PROG),
    .model_name =        "Rot1Prog",
    .mfg_name =          "SPID",
    .version =           "20220109.0",
    .copyright =         "LGPL",
    .status =            RIG_STATUS_STABLE,
    .rot_type =          ROT_TYPE_AZIMUTH,
    .port_type =         RIG_PORT_SERIAL,
    .serial_rate_min =   1200,
    .serial_rate_max =   1200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =     RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =       0,
    .post_write_delay =  300,
    .timeout =           400,
    .retry =             3,

    .min_az =            -180.0,
    .max_az =            540.0,
    .min_el =            0.0,
    .max_el =            0.0,

    .cfgparams =         spid_cfg_params,
    .get_conf =          spid_get_conf,
    .get_conf2 =         spid_get_conf2,
    .set_conf =          spid_set_conf,

    .rot_init =          spid_rot_init,
    .rot_cleanup =       spid_rot_cleanup,
    .get_position =      spid_rot_get_position,
    .set_position =      spid_rot1prog_rot_set_position,
    .stop =              spid_rot_stop,
};

const struct rot_caps spid_rot2prog_rot_caps =
{
    ROT_MODEL(ROT_MODEL_SPID_ROT2PROG),
    .model_name =        "Rot2Prog",
    .mfg_name =          "SPID",
    .version =           "20220109.0",
    .copyright =         "LGPL",
    .status =            RIG_STATUS_STABLE,
    .rot_type =          ROT_TYPE_AZEL,
    .port_type =         RIG_PORT_SERIAL,
    .serial_rate_min =   600,
    .serial_rate_max =   600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =     RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =       0,
    .post_write_delay =  300,
    .timeout =           400,
    .retry =             3,

    .min_az =            -180.0,
    .max_az =            540.0,
    .min_el =            -20.0,
    .max_el =            210.0,

    .cfgparams =         spid_cfg_params,
    .get_conf =          spid_get_conf,
    .set_conf =          spid_set_conf,
    .get_conf2 =         spid_get_conf2,

    .rot_init =          spid_rot_init,
    .rot_cleanup =       spid_rot_cleanup,
    .get_position =      spid_rot_get_position,
    .set_position =      spid_rot2prog_rot_set_position,
    .stop =              spid_rot_stop,
};

const struct rot_caps spid_md01_rot2prog_rot_caps =
{
    ROT_MODEL(ROT_MODEL_SPID_MD01_ROT2PROG),
    .model_name =        "MD-01/02 (ROT2 mode)",
    .mfg_name =          "SPID",
    .version =           "20220109.0",
    .copyright =         "LGPL",
    .status =            RIG_STATUS_STABLE,
    .rot_type =          ROT_TYPE_AZEL,
    .port_type =         RIG_PORT_SERIAL,
    .serial_rate_min =   600,
    .serial_rate_max =   460800,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =     RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =       0,
    .post_write_delay =  300,
    .timeout =           400,
    .retry =             3,

    .min_az =            -180.0,
    .max_az =            540.0,
    .min_el =            -20.0,
    .max_el =            210.0,

    .cfgparams =         spid_cfg_params,
    .get_conf =          spid_get_conf,
    .get_conf2 =         spid_get_conf2,
    .set_conf =          spid_set_conf,

    .rot_init =          spid_rot_init,
    .rot_cleanup =       spid_rot_cleanup,
    .get_position =      spid_rot_get_position,
    .set_position =      spid_rot2prog_rot_set_position,
    .move =              spid_md01_rot2prog_rot_move,
    .stop =              spid_rot_stop,
};

DECLARE_INITROT_BACKEND(spid)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rot_register(&spid_rot1prog_rot_caps);
    rot_register(&spid_rot2prog_rot_caps);
    rot_register(&spid_md01_rot2prog_rot_caps);

    return RIG_OK;
}
