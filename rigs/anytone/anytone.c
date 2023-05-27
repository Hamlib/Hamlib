// ---------------------------------------------------------------------------
//    AnyTone D578 Hamlib Backend
// ---------------------------------------------------------------------------
//
//  d578.c
//
//  Created by Michael Black W9MDB
//  Copyright © 2023 Michael Black W9MDB.
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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

// ---------------------------------------------------------------------------
//    HAMLIB INCLUDES
// ---------------------------------------------------------------------------

#include <hamlib/rig.h>
#include "serial.h"
#include "misc.h"
#include "register.h"
#include "riglist.h"

// ---------------------------------------------------------------------------
//    ANYTONE INCLUDES
// ---------------------------------------------------------------------------

#include "anytone.h"

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
        retval = RIG_MODEL_NONE;
    }
    else
    {
        char acBuf[ ANYTONE_RESPSZ + 1 ];
        int  nRead = 0;

        memset(acBuf, 0, ANYTONE_RESPSZ + 1);

#if 0
        retval = write_block(port,
                             (unsigned char *)ADAT_CMD_DEF_STRING_GET_ID_CODE,
                             strlen(ADAT_CMD_DEF_STRING_GET_ID_CODE));
        nRead = read_string(port, (unsigned char *) acBuf, ANYTONE_RESPSZ,
                            ADAT_EOM, 1, 0, 1);
#endif
        close(port->fd);

        if ((retval != RIG_OK || nRead < 0))
        {
            retval = RIG_MODEL_NONE;
        }
        else
        {
            rig_debug(RIG_DEBUG_VERBOSE, "Received ID = %s.",
                      acBuf);

            retval = RIG_MODEL_ADT_200A;
        }
    }

    return retval;
}

// AnyTone needs a keep-alive to emulate the MIC
// Apparently to keep the rig from getting stuck in PTT if mic disconnects
void *anytone_thread(void *vrig)
{
    RIG *rig = (RIG *)vrig;
    anytone_priv_data_t *p = rig->state.priv;
    rig_debug(RIG_DEBUG_TRACE, "%s: anytone_thread started\n", __func__);
    p->runflag = 1;

    while (p->runflag)
    {
        char c = 0x06;
        MUTEX_LOCK(p->priv.mutex);
        write_block(&rig->state.rigport, (unsigned char *)&c, 1);
        hl_usleep(100 * 1000);
        rig_flush(&rig->state.rigport);
        MUTEX_UNLOCK(p->priv.mutex);
        hl_usleep(1000 * 1000); // 1-second loop

    }

    return NULL;
}

// ---------------------------------------------------------------------------
// anytone_send
// ---------------------------------------------------------------------------
int anytone_send(RIG  *rig,
                 char *cmd, int cmd_len)
{
    int               retval       = RIG_OK;
    struct rig_state *rs = &rig->state;

    ENTERFUNC;

    rig_flush(&rs->rigport);

    retval = write_block(&rs->rigport, (unsigned char *) cmd,
                         cmd_len);

    RETURNFUNC(retval);
}

// ---------------------------------------------------------------------------
// anytone_receive
// ---------------------------------------------------------------------------
int anytone_receive(RIG  *rig, char *buf, int buf_len, int expected)
{
    int               retval       = RIG_OK;
    struct rig_state *rs = &rig->state;

    ENTERFUNC;

    retval = read_string(&rs->rigport, (unsigned char *) buf, buf_len,
                         NULL, 0, 0, expected);

    if (retval > 0)
    {
        retval = RIG_OK;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: read %d byte=0x%02x\n", __func__, retval,
                  buf[0]);
    }

    RETURNFUNC(retval);
}

// ---------------------------------------------------------------------------
// anytone_transaction
// ---------------------------------------------------------------------------
int anytone_transaction(RIG *rig, char *cmd, int cmd_len, int expected_len)
{
    int retval   = RIG_OK;
    anytone_priv_data_t *p = rig->state.priv;

    ENTERFUNC;

    if (rig == NULL)
    {
        retval = -RIG_EARG;
    }
    else
    {
        MUTEX_LOCK(p->mutex);
        retval = anytone_send(rig, cmd, cmd_len);

        if (retval == RIG_OK && expected_len != 0)
        {
            char buf[16];
            anytone_receive(rig, buf, sizeof(buf), 1);
            if (buf[0] == 0xaa && buf[1] == 0x53)
            {
                p->vfo_curr = buf[8] == 0x00 ? RIG_VFO_A : RIG_VFO_B;
            }
        }

        MUTEX_LOCK(p->mutex);
    }

    RETURNFUNC(retval);
}

// ---------------------------------------------------------------------------
// Function anytone_init
// ---------------------------------------------------------------------------
int anytone_init(RIG *rig)
{
    int retval = RIG_OK;

    ENTERFUNC;
    // Check Params

    if (rig != NULL)
    {
        anytone_priv_data_ptr pPriv = NULL;

        // Get new Priv Data

        pPriv = calloc(1, sizeof(anytone_priv_data_t));

        if (pPriv == NULL)
        {
            retval = -RIG_ENOMEM;
        }

        rig->state.priv = pPriv;
        anytone_priv_data_t *p = rig->state.priv;
        p->vfo_curr = RIG_VFO_NONE;
#ifdef HAVE_PTHREAD
        pthread_mutex_init(&p->mutex, NULL);
#endif
    }

    RETURNFUNC(retval);
}

// ---------------------------------------------------------------------------
// Function anytone_cleanup
// ---------------------------------------------------------------------------
int anytone_cleanup(RIG *rig)
{
    int retval = RIG_OK;

    ENTERFUNC;

    if (rig == NULL)
    {
        retval = -RIG_EARG;
    }
    else
    {
        if (rig->state.priv != NULL)
        {
            free(rig->state.priv);
            rig->state.priv = NULL;
        }
    }

    RETURNFUNC(retval);
}

// ---------------------------------------------------------------------------
// Function anytone_open
// ---------------------------------------------------------------------------
int anytone_open(RIG *rig)
{
    int retval = RIG_OK;

    ENTERFUNC;
    // Check Params

    if (rig == NULL)
    {
        retval = -RIG_EARG;
    }
    else
    {
        // grace period for the radio to be there

        // hl_usleep(500); // do we need this for AnyTone?

        // can we ask for any information?  Maybe just toggle A/B?
    }

    pthread_t id;
    int err = pthread_create(&id, NULL, anytone_thread, (void *)rig);

    if (err)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: pthread_create error: %s\n", __func__,
                  strerror(errno));
        RETURNFUNC(-RIG_EINTERNAL);
    }


    RETURNFUNC(retval);
}

// ---------------------------------------------------------------------------
// Function anytone_close
// ---------------------------------------------------------------------------
int anytone_close(RIG *rig)
{
    int retval = RIG_OK;
#if 0
    anytone_priv_data_ptr pPriv = (anytone_priv_data_ptr) rig->state.priv;

    if (pPriv->pcCmd != NULL) { free(pPriv->pcCmd); }

    if (pPriv->pcResult != NULL) { free(pPriv->pcResult); }

#endif

    ENTERFUNC;
#if 0
    // Now switch to interactive mode

    retval = anytone_transaction(rig, &anytone_cmd_list_close_adat);
#endif

    RETURNFUNC(retval);
}

// ---------------------------------------------------------------------------
// Function anytone_get_vfo
// ---------------------------------------------------------------------------
int anytone_get_vfo(RIG *rig, vfo_t *vfo)
{
    int retval = RIG_OK;

    ENTERFUNC;
    // Check Params

    if (rig == NULL)
    {
        retval = -RIG_EARG;
    }
    else
    {
        anytone_priv_data_ptr pPriv = (anytone_priv_data_ptr) rig->state.priv;

        *vfo = pPriv->vfo_curr;
    }

    RETURNFUNC(retval);
}

// ---------------------------------------------------------------------------
// Function anytone_set_vfo
// ---------------------------------------------------------------------------
int anytone_set_vfo(RIG *rig, vfo_t vfo)
{
    int retval = RIG_OK;

    ENTERFUNC;
    // Check Params

    if (rig == NULL)
    {
        retval = -RIG_EARG;
    }
    else
    {
        // can we use status reponse to deteremin which VFO is active?
        if (vfo == RIG_VFO_A)
        {
            char buf1[8] = { 0x41, 0x00, 0x01, 0x00, 0x0d, 0x00, 0x00, 0x06 };
            char buf2[8] = { 0x41, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x06 };
            anytone_transaction(rig, buf1, 8, 0);
            hl_usleep(100 * 1000);
            anytone_transaction(rig, buf2, 8, 15);
        }
        else
        {
            char buf1[8] = { 0x41, 0x00, 0x01, 0x00, 0x0d, 0x00, 0x00, 0x06 };
            char buf2[8] = { 0x41, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x06 };
            anytone_transaction(rig, buf1, 8, 0);
            hl_usleep(100 * 1000);
            anytone_transaction(rig, buf2, 8, 15);
        }

    }

    RETURNFUNC(retval);
}

// ---------------------------------------------------------------------------
// Function anytone_get_ptt
// ---------------------------------------------------------------------------
int anytone_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    int retval = RIG_OK;

    ENTERFUNC;
    // Check Params

    if (rig == NULL)
    {
        retval = -RIG_EARG;
    }
    else
    {
    }

    return retval;
}
// ---------------------------------------------------------------------------
// anytone_set_ptt
// ---------------------------------------------------------------------------
int anytone_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    int retval = RIG_OK;

    ENTERFUNC;

    if (rig == NULL)
    {
        retval = -RIG_EARG;
    }
    else
    {
        char buf[8] = { 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06 };

        if (ptt) { buf[1] = 0x01; }

        anytone_transaction(rig, buf, 8, 1);
    }

    RETURNFUNC(retval);
}

// ---------------------------------------------------------------------------
// END OF FILE
// ---------------------------------------------------------------------------
