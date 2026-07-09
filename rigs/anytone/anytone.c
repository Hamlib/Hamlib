// ---------------------------------------------------------------------------
//    AnyTone D578 Hamlib Backend
// ---------------------------------------------------------------------------
//
//  anytone.c
//
//  Created by Michael Black W9MDB
//  Copyright © 2023 Michael Black W9MDB.
//
//  Protocol analysis and fixes based on jrobertfisher's AT-D578UV-software-mic
//  BT-01 Bluetooth microphone protocol capture.
//  https://github.com/jrobertfisher/AT-D578UV-software-mic
//
//   This library is free software; you can redistribute it and/or
//   modify it under the terms of the GNU Lesser General Public
//   License as published by the Free Software Foundation; either
//   version 2.1 of the License, or (at your option) any later version.
//
//   This library is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//   Lesser General Public License for more details.
//
//   You should have received a copy of the GNU Lesser General Public
//   License along with this library; if not, write to the Free Software
//   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

// ---------------------------------------------------------------------------
//    SYSTEM INCLUDES
// ---------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// ---------------------------------------------------------------------------
//    HAMLIB INCLUDES
// ---------------------------------------------------------------------------

#include "hamlib/config.h"
#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"
#include "serial.h"
#include "misc.h"
#include "register.h"
#include "riglist.h"

// ---------------------------------------------------------------------------
//    ANYTONE INCLUDES
// ---------------------------------------------------------------------------

#include "anytone.h"

static int anytone_transaction(RIG *rig, unsigned char *cmd, int cmd_len,
                        unsigned char *reply, int reply_len, int expected_len);

DECLARE_INITRIG_BACKEND(anytone)
{
    int retval = RIG_OK;

    rig_register(&anytone_d578_caps);

    return retval;
}

// ---------------------------------------------------------------------------
// proberig_anytone
// ---------------------------------------------------------------------------
DECLARE_PROBERIG_BACKEND(anytone)
{
    int retval = RIG_OK;

    if (!port)
    {
        return RIG_MODEL_NONE;
    }

    if (port->type.rig != RIG_PORT_SERIAL)
    {
        return RIG_MODEL_NONE;
    }

    port->write_delay = port->post_write_delay = 0;
    port->parm.serial.stop_bits = 1;
    port->retry = 1;

    retval = serial_open(port);

    if (retval != RIG_OK)
    {
        retval = -RIG_EIO;
    }

    return retval;
}

// ---------------------------------------------------------------------------
// Keep-alive thread — branches on commode flag
//
// commode=0: sends raw 0x06 byte every second
// commode=1: sends +ADATA:00,001\r\n a \r\n every second
// ---------------------------------------------------------------------------
static void *anytone_thread(void *vrig)
{
    RIG *rig = (RIG *)vrig;
    hamlib_port_t *rp = RIGPORT(rig);
    anytone_priv_data_t *p = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_TRACE, "%s: anytone_thread started (commode=%d)\n",
              __func__, p->commode);
    p->runflag = 1;

    // ADATA heartbeat: +ADATA:00,001\r\na\r\n (18 bytes)
    unsigned char keepalive_adata[] = {
        0x2B, 0x41, 0x44, 0x41, 0x54, 0x41, 0x3A, 0x30,
        0x30, 0x2C, 0x30, 0x30, 0x31, 0x0D, 0x0A,
        0x61,
        0x0D, 0x0A
    };

    // Raw mic heartbeat: single 0x06 byte
    unsigned char keepalive_mic[] = { 0x06 };

    unsigned char *ka = p->commode ? keepalive_adata : keepalive_mic;
    int ka_len = p->commode ? (int)sizeof(keepalive_adata) : (int)sizeof(keepalive_mic);

    while (p->runflag)
    {
        // Skip keepalive if a raw command (rig_send_raw / rigctld 'w')
        // or a backend transaction is in progress
        if (STATE(rig)->transaction_active)
        {
            hl_usleep(1000 * 1000);
            continue;
        }

        if (pthread_mutex_trylock(&p->mutex) != 0)
        {
            hl_usleep(1000 * 1000);
            continue;
        }

        enum rig_debug_level_e debug_level_save;
        rig_get_debug(&debug_level_save);

        if (rig_need_debug(RIG_DEBUG_CACHE) == 0)
        {
            rig_set_debug(RIG_DEBUG_WARN);
        }

        write_block(rp, ka, ka_len);
        rig_flush(rp);

        if (rig_need_debug(RIG_DEBUG_CACHE) == 0)
        {
            rig_set_debug(debug_level_save);
        }

        MUTEX_UNLOCK(&p->mutex);
        hl_usleep(1000 * 1000);
    }

    return NULL;
}

// ---------------------------------------------------------------------------
// anytone_send
// ---------------------------------------------------------------------------
static int anytone_send(RIG *rig, unsigned char *cmd, int cmd_len)
{
    int retval = RIG_OK;
    hamlib_port_t *rp = RIGPORT(rig);

    ENTERFUNC;

    rig_flush(rp);

    retval = write_block(rp, cmd, cmd_len);

    RETURNFUNC(retval);
}

// ---------------------------------------------------------------------------
// anytone_receive
// ---------------------------------------------------------------------------
static int anytone_receive(RIG *rig, unsigned char *buf, int buf_len,
                           int expected)
{
    int retval = RIG_OK;
    hamlib_port_t *rp = RIGPORT(rig);

    ENTERFUNC;

    retval = read_block(rp, buf, expected);

    if (retval > 0)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: read %d bytes\n", __func__, retval);
        retval = RIG_OK;
    }

    RETURNFUNC(retval);
}

// ---------------------------------------------------------------------------
// anytone_transaction
// ---------------------------------------------------------------------------
static int anytone_transaction(RIG *rig, unsigned char *cmd, int cmd_len,
                        unsigned char *reply, int reply_len, int expected_len)
{
    int retval = RIG_OK;

    ENTERFUNC;

    retval = anytone_send(rig, cmd, cmd_len);

    if (retval == RIG_OK && expected_len != 0)
    {
        int len = anytone_receive(rig, reply, reply_len, expected_len);
        rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): rx len=%d\n", __func__, __LINE__,
                  len);
    }

    RETURNFUNC(retval);
}

// ---------------------------------------------------------------------------
// anytone_send_button — send a button press+release via +ADATA framing
// ---------------------------------------------------------------------------
static int anytone_send_button(RIG *rig, unsigned char key)
{
    hamlib_port_t *rp = RIGPORT(rig);

    unsigned char press[25] = {
        0x2B, 0x41, 0x44, 0x41, 0x54, 0x41, 0x3A, 0x30,
        0x30, 0x2C, 0x30, 0x30, 0x38, 0x0D, 0x0A,
        0x41, 0x00, 0x01, 0x00, key, 0x00, 0x00, 0x06,
        0x0D, 0x0A
    };

    unsigned char release[25] = {
        0x2B, 0x41, 0x44, 0x41, 0x54, 0x41, 0x3A, 0x30,
        0x30, 0x2C, 0x30, 0x30, 0x38, 0x0D, 0x0A,
        0x41, 0x00, 0x00, 0x00, key, 0x00, 0x00, 0x06,
        0x0D, 0x0A
    };

    write_block(rp, press, sizeof(press));
    hl_usleep(100 * 1000);
    write_block(rp, release, sizeof(release));
    hl_usleep(100 * 1000);

    return RIG_OK;
}

// ---------------------------------------------------------------------------
// anytone_init
// ---------------------------------------------------------------------------
int anytone_init(RIG *rig)
{
    int retval = RIG_OK;

    ENTERFUNC;

    if (rig != NULL)
    {
        anytone_priv_data_ptr p = calloc(1, sizeof(anytone_priv_data_t));

        if (p == NULL)
        {
            RETURNFUNC(-RIG_ENOMEM);
        }

        STATE(rig)->priv = p;
        p->vfo_curr = RIG_VFO_NONE;
        p->commode = 0;
        pthread_mutex_init(&p->mutex, NULL);
    }

    RETURNFUNC(retval);
}

// ---------------------------------------------------------------------------
// anytone_cleanup
// ---------------------------------------------------------------------------
int anytone_cleanup(RIG *rig)
{
    if (rig == NULL)
    {
        return -RIG_EARG;
    }

    ENTERFUNC;

    free(STATE(rig)->priv);
    STATE(rig)->priv = NULL;

    RETURNFUNC(RIG_OK);
}

// ---------------------------------------------------------------------------
// anytone_set_conf / anytone_get_conf — backend configuration
// ---------------------------------------------------------------------------
int anytone_set_conf(RIG *rig, hamlib_token_t token, const char *val)
{
    anytone_priv_data_t *p = STATE(rig)->priv;

    ENTERFUNC;

    switch (token)
    {
    case TOK_COMMODE:
        p->commode = atoi(val) ? 1 : 0;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: commode=%d\n", __func__, p->commode);
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(RIG_OK);
}

int anytone_get_conf(RIG *rig, hamlib_token_t token, char *val)
{
    anytone_priv_data_t *p = STATE(rig)->priv;

    ENTERFUNC;

    switch (token)
    {
    case TOK_COMMODE:
        SNPRINTF(val, 128, "%d", p->commode);
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(RIG_OK);
}

// ---------------------------------------------------------------------------
// anytone_open
//
// commode=0: no handshake, just start keepalive thread
// commode=1: BT-01 COM MODE handshake, then keepalive thread
// ---------------------------------------------------------------------------
int anytone_open(RIG *rig)
{
    hamlib_port_t *rp = RIGPORT(rig);
    anytone_priv_data_t *p = STATE(rig)->priv;

    ENTERFUNC;

    if (p->commode)
    {
        // --- Wake-up heartbeat ---
        unsigned char wakeup[18] = {
            0x2B, 0x41, 0x44, 0x41, 0x54, 0x41, 0x3A, 0x30,
            0x30, 0x2C, 0x30, 0x30, 0x31, 0x0D, 0x0A,
            0x61,
            0x0D, 0x0A
        };

        write_block(rp, wakeup, sizeof(wakeup));
        hl_usleep(500 * 1000);

        // --- Enter COM MODE ---
        unsigned char commode[33] = {
            0x2B, 0x41, 0x44, 0x41, 0x54, 0x41, 0x3A, 0x30,
            0x30, 0x2C, 0x30, 0x31, 0x36, 0x0D, 0x0A,
            0x01,
            'D', '5', '7', '8', 'U', 'V', ' ', 'C', 'O', 'M', ' ', 'M', 'O', 'D', 'E',
            0x0D, 0x0A
        };

        write_block(rp, commode, sizeof(commode));
        hl_usleep(500 * 1000);
        rig_flush(rp);
    }

    // Start keep-alive thread (both modes)
    int err = pthread_create(&p->thread_id, NULL, anytone_thread, (void *)rig);

    if (err)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: pthread_create error: %s\n", __func__,
                  strerror(errno));
        RETURNFUNC(-RIG_EINTERNAL);
    }

    RETURNFUNC(RIG_OK);
}

// ---------------------------------------------------------------------------
// anytone_close
// ---------------------------------------------------------------------------
int anytone_close(RIG *rig)
{
    anytone_priv_data_t *p = STATE(rig)->priv;

    ENTERFUNC;

    p->runflag = 0;
    pthread_join(p->thread_id, NULL);

    if (p->commode)
    {
        unsigned char release[17] = {
            0x2B, 0x41, 0x44, 0x41, 0x54, 0x41, 0x3A, 0x30,
            0x30, 0x2C, 0x30, 0x30, 0x30, 0x0D, 0x0A,
            0x0D, 0x0A
        };

        anytone_transaction(rig, release, sizeof(release), NULL, 0, 0);
    }

    RETURNFUNC(RIG_OK);
}

// ---------------------------------------------------------------------------
// anytone_get_vfo — requires commode=1
// ---------------------------------------------------------------------------
int anytone_get_vfo(RIG *rig, vfo_t *vfo)
{
    int retval = RIG_OK;
    anytone_priv_data_t *p = STATE(rig)->priv;
    unsigned char reply[512];

    ENTERFUNC;

    if (!p->commode)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: requires -C commode=1\n", __func__);
        RETURNFUNC(-RIG_ENAVAIL);
    }

    unsigned char cmd[23] = {
        0x2B, 0x41, 0x44, 0x41, 0x54, 0x41, 0x3A, 0x30,
        0x30, 0x2C, 0x30, 0x30, 0x36, 0x0D, 0x0A,
        0x04, 0x05, 0x00, 0x00, 0x00, 0x00,
        0x0D, 0x0A
    };

    MUTEX_LOCK(&p->mutex);
    anytone_transaction(rig, cmd, sizeof(cmd), reply, sizeof(reply), 116);
    MUTEX_UNLOCK(&p->mutex);

    if (reply[113] == 0x9b)
    {
        *vfo = RIG_VFO_A;
    }
    else if (reply[113] == 0x9c)
    {
        *vfo = RIG_VFO_B;
    }
    else
    {
        *vfo = RIG_VFO_A;
        rig_debug(RIG_DEBUG_ERR, "%s: unknown vfo byte=0x%02x\n", __func__,
                  reply[113]);
    }

    p->vfo_curr = *vfo;

    RETURNFUNC(retval);
}

// ---------------------------------------------------------------------------
// anytone_set_vfo — toggle VFO via Sub A/B button press, requires commode=1
// ---------------------------------------------------------------------------
int anytone_set_vfo(RIG *rig, vfo_t vfo)
{
    anytone_priv_data_t *p = STATE(rig)->priv;
    vfo_t curr_vfo;

    ENTERFUNC;

    if (!p->commode)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: requires -C commode=1\n", __func__);
        RETURNFUNC(-RIG_ENAVAIL);
    }

    anytone_get_vfo(rig, &curr_vfo);

    if (curr_vfo == vfo)
    {
        RETURNFUNC(RIG_OK);
    }

    MUTEX_LOCK(&p->mutex);
    anytone_send_button(rig, ANYTONE_KEY_SUBAB);
    MUTEX_UNLOCK(&p->mutex);

    p->vfo_curr = vfo;

    RETURNFUNC(RIG_OK);
}

// ---------------------------------------------------------------------------
// anytone_get_ptt
// ---------------------------------------------------------------------------
int anytone_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    ENTERFUNC;

    anytone_priv_data_t *p = STATE(rig)->priv;
    *ptt = p->ptt;

    RETURNFUNC(RIG_OK);
}

// ---------------------------------------------------------------------------
// anytone_set_ptt
//
// commode=0: raw 8-byte 0x41 button command, no ADATA framing
//   ON:  0x41 0x01 0x00 0x00 0x00 0x00 0x00 0x06
//   OFF: 0x41 0x00 0x00 0x00 0x00 0x00 0x00 0x06
//
// commode=1: ADATA-wrapped 0x56 command (40 bytes)
//   ON:  +ADATA:00,023\r\n 0x56 0x01 + 21 zeros \r\n
//   OFF: +ADATA:00,023\r\n 0x56 0x00 + 21 zeros \r\n
// ---------------------------------------------------------------------------
int anytone_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    anytone_priv_data_t *p = STATE(rig)->priv;

    ENTERFUNC;

    MUTEX_LOCK(&p->mutex);

    if (p->commode)
    {
        unsigned char ptton[40] = {
            0x2B, 0x41, 0x44, 0x41, 0x54, 0x41, 0x3A, 0x30,
            0x30, 0x2C, 0x30, 0x32, 0x33, 0x0D, 0x0A,
            0x56, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x0D, 0x0A
        };

        unsigned char pttoff[40] = {
            0x2B, 0x41, 0x44, 0x41, 0x54, 0x41, 0x3A, 0x30,
            0x30, 0x2C, 0x30, 0x32, 0x33, 0x0D, 0x0A,
            0x56, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x0D, 0x0A
        };

        unsigned char *cmd = ptt ? ptton : pttoff;
        anytone_transaction(rig, cmd, 40, NULL, 0, 0);
    }
    else
    {
        unsigned char ptton[8]  = { 0x41, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06 };
        unsigned char pttoff[8] = { 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06 };

        unsigned char *cmd = ptt ? ptton : pttoff;
        write_block(RIGPORT(rig), cmd, 8);
    }

    p->ptt = ptt;
    MUTEX_UNLOCK(&p->mutex);

    RETURNFUNC(RIG_OK);
}

// ---------------------------------------------------------------------------
// anytone_get_freq — requires commode=1
// ---------------------------------------------------------------------------
int anytone_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    char cmd[32];
    int retval;
    hamlib_port_t *rp = RIGPORT(rig);
    anytone_priv_data_t *p = STATE(rig)->priv;

    ENTERFUNC;

    if (!p->commode)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: requires -C commode=1\n", __func__);
        RETURNFUNC(-RIG_ENAVAIL);
    }

    SNPRINTF(cmd, sizeof(cmd), "+ADATA:00,006\r\n");
    cmd[15] = 0x04;
    cmd[16] = 0x2c;
    cmd[17] = 0x07;
    cmd[18] = 0x00;
    cmd[19] = 0x00;
    cmd[20] = 0x00;
    cmd[21] = 0x00;
    cmd[22] = 0x00;
    cmd[23] = 0x0d;
    cmd[24] = 0x0a;

    if (vfo == RIG_VFO_B) { cmd[16] = 0x2d; }

    int retry = 2;
    MUTEX_LOCK(&p->mutex);
    rig_flush(rp);

    do
    {
        write_block(rp, (unsigned char *)cmd, 25);
        unsigned char buf[512];
        retval = read_block(rp, buf, 135);

        if (retval == 135)
        {
            *freq = from_bcd_be(&buf[17], 8) * 10;
            rig_debug(RIG_DEBUG_VERBOSE, "%s: freq=%g\n", __func__, *freq);
            retval = RIG_OK;
            break;
        }
    }
    while (--retry > 0);

    MUTEX_UNLOCK(&p->mutex);

    RETURNFUNC(retval == RIG_OK ? RIG_OK : -RIG_EIO);
}

// ---------------------------------------------------------------------------
// anytone_set_freq — requires commode=1
// ---------------------------------------------------------------------------
int anytone_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    anytone_priv_data_t *p = STATE(rig)->priv;

    ENTERFUNC;

    if (!p->commode)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: requires -C commode=1\n", __func__);
        RETURNFUNC(-RIG_ENAVAIL);
    }

    while (freq > 0 && freq < 100000000)
    {
        freq *= 10;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s freq=%g\n", __func__,
              rig_strvfo(vfo), freq);

    unsigned char vfo_sel[22] = {
        0x2B, 0x41, 0x44, 0x41, 0x54, 0x41, 0x3A, 0x30,
        0x30, 0x2C, 0x30, 0x30, 0x35, 0x0D, 0x0A,
        0x5A, 0x02, 0x00, 0x00, 0x00,
        0x0D, 0x0A
    };

    if (vfo == RIG_VFO_B)
    {
        vfo_sel[16] = 0x01;
    }

    unsigned char freq_data[40] = {
        0x2B, 0x41, 0x44, 0x41, 0x54, 0x41, 0x3A, 0x30,
        0x30, 0x2C, 0x30, 0x32, 0x33, 0x0D, 0x0A,
        0x2F, 0x03, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x15, 0x50, 0x00, 0x00,
        0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xCF, 0x09, 0x00,
        0x0D, 0x0A
    };

    to_bcd_be(&freq_data[18], (unsigned long long)(freq / 10), 8);

    unsigned char ack[22];

    MUTEX_LOCK(&p->mutex);
    rig_flush(RIGPORT(rig));

    write_block(RIGPORT(rig), vfo_sel, sizeof(vfo_sel));
    read_block(RIGPORT(rig), ack, 22);

    write_block(RIGPORT(rig), freq_data, sizeof(freq_data));
    read_block(RIGPORT(rig), ack, 22);

    MUTEX_UNLOCK(&p->mutex);

    RETURNFUNC(RIG_OK);
}

// ---------------------------------------------------------------------------
// anytone_set_clock — requires commode=1
//
// Probe confirmed: 0x55 with 6-byte payload modifies the radio's clock.
// Sending all zeros set the date to 2070-01-01 00:00, suggesting the firmware
// may apply a +70 year offset or use a non-standard epoch. The encoding
// below uses straight BCD (YY MM DD HH MM) — if the radio shows the wrong
// year, adjust the offset in the year byte calculation.
// ---------------------------------------------------------------------------
int anytone_set_clock(RIG *rig, int year, int month, int day,
                      int hour, int min, int sec, double msec, int utc_offset)
{
    anytone_priv_data_t *p = STATE(rig)->priv;

    ENTERFUNC;

    if (!p->commode)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: requires -C commode=1\n", __func__);
        RETURNFUNC(-RIG_ENAVAIL);
    }

    // +ADATA:00,006\r\n  0x55 YY MM DD HH MM  \r\n  = 23 bytes
    unsigned char cmd[23] = {
        0x2B, 0x41, 0x44, 0x41, 0x54, 0x41, 0x3A, 0x30,
        0x30, 0x2C, 0x30, 0x30, 0x36, 0x0D, 0x0A,
        0x55, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x0D, 0x0A
    };

    int yy = year % 100;
    cmd[16] = (unsigned char)(((yy / 10) << 4) | (yy % 10));
    cmd[17] = (unsigned char)(((month / 10) << 4) | (month % 10));
    cmd[18] = (unsigned char)(((day / 10) << 4) | (day % 10));
    cmd[19] = (unsigned char)(((hour / 10) << 4) | (hour % 10));
    cmd[20] = (unsigned char)(((min / 10) << 4) | (min % 10));

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: setting clock to %04d-%02d-%02d %02d:%02d (BCD: %02X %02X %02X %02X %02X)\n",
              __func__, year, month, day, hour, min,
              cmd[16], cmd[17], cmd[18], cmd[19], cmd[20]);

    unsigned char ack[22];

    MUTEX_LOCK(&p->mutex);
    rig_flush(RIGPORT(rig));
    write_block(RIGPORT(rig), cmd, sizeof(cmd));
    read_block(RIGPORT(rig), ack, 22);
    MUTEX_UNLOCK(&p->mutex);

    RETURNFUNC(RIG_OK);
}

// ---------------------------------------------------------------------------
// anytone_get_clock — requires commode=1
//
// Not yet confirmed by probe. Command 0x58 (same len==6 check as 0x55) is
// the most likely getter candidate but returned only a 5-byte ACK during
// probing (possibly because the zero payload was invalid). Needs testing
// with the radio to confirm the response format.
// ---------------------------------------------------------------------------
int anytone_get_clock(RIG *rig, int *year, int *month, int *day,
                      int *hour, int *min, int *sec, double *msec,
                      int *utc_offset)
{
    anytone_priv_data_t *p = STATE(rig)->priv;

    ENTERFUNC;

    if (!p->commode)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: requires -C commode=1\n", __func__);
        RETURNFUNC(-RIG_ENAVAIL);
    }

    rig_debug(RIG_DEBUG_WARN,
              "%s: get_clock not yet confirmed — command 0x58 needs radio testing\n",
              __func__);

    RETURNFUNC(-RIG_ENIMPL);
}

// ---------------------------------------------------------------------------
// END OF FILE
// ---------------------------------------------------------------------------
