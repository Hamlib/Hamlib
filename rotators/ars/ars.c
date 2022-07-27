/*
 *  Hamlib Rotator backend - ARS parallel port backend
 *  Copyright (c) 2010 by Stephane Fillod
 *  This code is inspired by work from Pablo GARCIA - EA4TX
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

#include <hamlib/config.h>

#include <stdlib.h>
#include <string.h>  /* String function definitions */
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif

#include "hamlib/rotator.h"
#include "parallel.h"
#include "misc.h"
#include "register.h"

#include "ars.h"

#define CTL_PIN_CS  PARPORT_CONTROL_AUTOFD /* pin14: Control Linefeed */
#define CTL_PIN_CLK PARPORT_CONTROL_STROBE /* pin01: Control /Strobe  */
#define STA_PIN_D0  PARPORT_STATUS_BUSY    /* pin11: Status  Busy     */
#define STA_PIN_D1  PARPORT_STATUS_ERROR   /* pin15: Status  /Error   */

#define DTA_PIN02   0x01    /* Data0 */
#define DTA_PIN03   0x02    /* Data1 */
#define DTA_PIN04   0x04    /* Data2 */
#define DTA_PIN05   0x08    /* Data3 */
#define DTA_PIN06   0x10    /* Data4 */
#define DTA_PIN07   0x20    /* Data5 */
#define DTA_PIN08   0x40    /* Data6 */
#define DTA_PIN09   0x80    /* Data7 */

#define CTL_PIN16   PARPORT_CONTROL_INIT
#define CTL_PIN17   PARPORT_CONTROL_SELECT

#define ARS_BRAKE_DELAY  (100*1000) /* usecs */
#define ARS_SETTLE_DELAY (500*1000) /* usecs */
#define PP_IO_PERIOD (25) /* usecs */
#define NUM_SAMPLES 3

/* TODO: take into account ADC res (8 bits -> 1.4 deg at 360 deg)
 * Rem: at 360 deg/mn, that's 6 deg/sec.
 */
#define AZ_RANGE 3.
#define EL_RANGE 2.

/* Check return value, with appropriate unlocking upon error.
 * Assumes "rot" variable is current ROT pointer.
 */
#define CHKPPRET(a) \
    do { int _retval = a; if (_retval != RIG_OK) \
        {par_unlock (&rot->state.rotport);return _retval; }} while(0)

static int ars_init(ROT *rot);
static int ars_cleanup(ROT *rot);
static int ars_open(ROT *rot);
static int ars_close(ROT *rot);
static int ars_stop(ROT *rot);
static int ars_move(ROT *rot, int direction, int speed);
static int ars_set_position_sync(ROT *rot, azimuth_t az, elevation_t el);
static int ars_set_position(ROT *rot, azimuth_t az, elevation_t el);
static int ars_get_position(ROT *rot, azimuth_t *az, elevation_t *el);

#ifdef HAVE_PTHREAD
static void *handle_set_position(void *);
#endif

/* ************************************************************************* */

static int ars_clear_ctrl_pin(ROT *rot, unsigned char pin)
{
    hamlib_port_t *pport = &rot->state.rotport;
    struct ars_priv_data *priv = (struct ars_priv_data *)rot->state.priv;

    priv->pp_control &= ~pin;

    return par_write_control(pport, priv->pp_control);
}

static int ars_set_ctrl_pin(ROT *rot, unsigned char pin)
{
    hamlib_port_t *pport = &rot->state.rotport;
    struct ars_priv_data *priv = (struct ars_priv_data *)rot->state.priv;

    priv->pp_control |= pin;

    return par_write_control(pport, priv->pp_control);
}

static int ars_clear_data_pin(ROT *rot, unsigned char pin)
{
    hamlib_port_t *pport = &rot->state.rotport;
    struct ars_priv_data *priv = (struct ars_priv_data *)rot->state.priv;

    priv->pp_data &= ~pin;

    return par_write_data(pport, priv->pp_data);
}

static int ars_set_data_pin(ROT *rot, unsigned char pin)
{
    hamlib_port_t *pport = &rot->state.rotport;
    struct ars_priv_data *priv = (struct ars_priv_data *)rot->state.priv;

    priv->pp_data |= pin;

    return par_write_data(pport, priv->pp_data);
}

static int ars_has_el(const ROT *rot)
{
    return (rot->caps->rot_type & ROT_FLAG_ELEVATION);
}

/* ************************************************************************* */

int
ars_init(ROT *rot)
{
    struct ars_priv_data *priv;

    if (!rot)
    {
        return -RIG_EINVAL;
    }

    rot->state.priv = (struct ars_priv_data *)calloc(1,
                      sizeof(struct ars_priv_data));

    if (!rot->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rot->state.priv;

    priv->pp_control = 0;
    priv->pp_data = 0;

    /* Always use 10 bit resolution, which supports also 8 bits
     * at the cost of 2 potentially wrong lsb */
    priv->adc_res = 10; /* 8 bits vs. 10 bits ADC */
    priv->brake_off = 0; /* i.e. brake is ON */
    priv->curr_move = 0;

    return RIG_OK;
}

int
ars_cleanup(ROT *rot)
{
    if (!rot)
    {
        return -RIG_EINVAL;
    }

    if (rot->state.priv)
    {
        free(rot->state.priv);
        rot->state.priv = NULL;
    }

    return RIG_OK;
}

int
ars_open(ROT *rot)
{
    /* make it idle, and known state */
    ars_stop(rot);

#ifdef HAVE_PTHREAD
    {
        struct ars_priv_data *priv = (struct ars_priv_data *)rot->state.priv;
        pthread_attr_t attr;
        int retcode;

        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        /* create behind-the-scene thread doing the ars_set_position_sync() */
        retcode = pthread_create(&priv->thread, &attr, handle_set_position, rot);

        if (retcode != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: pthread_create: %s\n", __func__,
                      strerror(retcode));
            return -RIG_ENOMEM;
        }
    }
#endif

    return RIG_OK;
}

int
ars_close(ROT *rot)
{
#ifdef HAVE_PTHREAD
    struct ars_priv_data *priv = (struct ars_priv_data *)rot->state.priv;

    pthread_cancel(priv->thread);
#endif

    /* leave it in safe state */
    ars_stop(rot);

    return RIG_OK;
}

int
ars_stop(ROT *rot)
{
    struct ars_priv_data *priv = (struct ars_priv_data *)rot->state.priv;
    hamlib_port_t *pport = &rot->state.rotport;

    rig_debug(RIG_DEBUG_TRACE, "%s called, brake was %s\n", __func__,
              priv->brake_off ? "OFF" : "ON");

#ifdef HAVE_PTHREAD
    priv->set_pos_active = 0;
#endif

    par_lock(pport);

    priv->brake_off = 0;
    priv->curr_move = 0;

    // Relay AUX -> Off
    CHKPPRET(ars_clear_data_pin(rot, DTA_PIN02 | DTA_PIN04 | DTA_PIN08));
    CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN17 | CTL_PIN16));

    // Elevation Relays -> Off
    CHKPPRET(ars_clear_data_pin(rot, DTA_PIN03 | DTA_PIN07));

    par_unlock(pport);

    return RIG_OK;
}

int
ars_move(ROT *rot, int direction, int speed)
{
    struct ars_priv_data *priv = (struct ars_priv_data *)rot->state.priv;
    hamlib_port_t *pport = &rot->state.rotport;
    int need_settle_delay = 0;

    rig_debug(RIG_DEBUG_TRACE, "%s called%s%s%s%s%s\n", __func__,
              (direction & ROT_MOVE_LEFT)  ? " LEFT" : "",
              (direction & ROT_MOVE_RIGHT) ? " RIGHT" : "",
              (direction & ROT_MOVE_UP)    ? " UP" : "",
              (direction & ROT_MOVE_DOWN)  ? " DOWN" : "",
              (direction == 0)             ? " STOP" : "");

    par_lock(pport);

    /* Allow the rotator to settle down when changing direction */
    if (((priv->curr_move & ROT_MOVE_LEFT) && (direction & ROT_MOVE_RIGHT)) ||
            ((priv->curr_move & ROT_MOVE_RIGHT) && (direction & ROT_MOVE_LEFT)))
    {

        // Relay AUX -> Off
        CHKPPRET(ars_clear_data_pin(rot, DTA_PIN02 | DTA_PIN04 | DTA_PIN08));
        CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN17 | CTL_PIN16));
        need_settle_delay = 1;
        priv->brake_off = 0;
    }

    if (ars_has_el(rot) &&
            (((priv->curr_move & ROT_MOVE_UP) && (direction & ROT_MOVE_DOWN)) ||
             ((priv->curr_move & ROT_MOVE_DOWN) && (direction & ROT_MOVE_UP))))
    {

        // Elevation Relays -> Off
        CHKPPRET(ars_clear_data_pin(rot, DTA_PIN03 | DTA_PIN07));
        need_settle_delay = 1;
    }

    if (need_settle_delay)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s need settling delay\n", __func__);

        hl_usleep(ARS_SETTLE_DELAY);
    }

    priv->curr_move = direction;


    /* Brake handling, only for AZ */
    if (!priv->brake_off && (direction & (ROT_MOVE_LEFT | ROT_MOVE_RIGHT)))
    {
        /* release the brake */
        if (ars_has_el(rot))
        {
            // RCI Model Azim & Elev
            // Deactivated CCW/CW relays
            CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN17 | CTL_PIN16));
            // Relay Aux
            CHKPPRET(ars_set_data_pin(rot, DTA_PIN02 | DTA_PIN04 | DTA_PIN06 | DTA_PIN08));
            CHKPPRET(ars_clear_data_pin(rot, DTA_PIN09));
        }
        else
        {
            // RCI Model Azimuth only
            // Deactivated CCW/CW relays
            CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN17 | CTL_PIN16));
            // Relay Aux
            CHKPPRET(ars_set_data_pin(rot,
                                      DTA_PIN02 | DTA_PIN04 | DTA_PIN06 | DTA_PIN07 | DTA_PIN08 | DTA_PIN09));
            CHKPPRET(ars_clear_data_pin(rot, DTA_PIN03 | DTA_PIN05));
        }

        priv->brake_off = 1;
        hl_usleep(ARS_BRAKE_DELAY);
    }


    if (ars_has_el(rot))
    {
        if (direction & ROT_MOVE_UP)
        {
            /* UP */
            CHKPPRET(ars_set_data_pin(rot, DTA_PIN03 | DTA_PIN06 | DTA_PIN07));
            CHKPPRET(ars_clear_data_pin(rot, DTA_PIN05 | DTA_PIN09));
        }
        else if (direction & ROT_MOVE_DOWN)
        {
            CHKPPRET(ars_set_data_pin(rot, DTA_PIN03 | DTA_PIN05 | DTA_PIN06 | DTA_PIN07));
            CHKPPRET(ars_clear_data_pin(rot, DTA_PIN09));
        }
        else
        {
            // Elevation Relays -> Off
            CHKPPRET(ars_clear_data_pin(rot, DTA_PIN03 | DTA_PIN07));
        }
    }

    if (direction & ROT_MOVE_LEFT)
    {
        // Relay Left
        if (ars_has_el(rot))
        {
            // RCI Model Azim & Elev
            CHKPPRET(ars_set_data_pin(rot, DTA_PIN02 | DTA_PIN04 | DTA_PIN06));
            CHKPPRET(ars_set_ctrl_pin(rot, CTL_PIN16));
            CHKPPRET(ars_clear_data_pin(rot, DTA_PIN09));
            CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN17));
        }
        else
        {
            // RCI Model Azimuth only
            CHKPPRET(ars_set_data_pin(rot,
                                      DTA_PIN02 | DTA_PIN04 | DTA_PIN06 | DTA_PIN07 | DTA_PIN08));
            CHKPPRET(ars_set_ctrl_pin(rot, CTL_PIN16));
            CHKPPRET(ars_clear_data_pin(rot, DTA_PIN03 | DTA_PIN05));
            CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN17));
        }
    }
    else if (direction & ROT_MOVE_RIGHT)
    {
        // Relay Right
        if (ars_has_el(rot))
        {
            // RCI Model Azim & Elev
            CHKPPRET(ars_set_data_pin(rot, DTA_PIN02 | DTA_PIN04 | DTA_PIN06));
            CHKPPRET(ars_set_ctrl_pin(rot, CTL_PIN17));
            CHKPPRET(ars_clear_data_pin(rot, DTA_PIN09));
            CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN16));
        }
        else
        {
            // RCI Model Azimuth only
            CHKPPRET(ars_set_data_pin(rot,
                                      DTA_PIN02 | DTA_PIN04 | DTA_PIN06 | DTA_PIN07 | DTA_PIN08));
            CHKPPRET(ars_set_ctrl_pin(rot, CTL_PIN17));
            CHKPPRET(ars_clear_data_pin(rot, DTA_PIN03 | DTA_PIN05));
            CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN16));
        }
    }
    else
    {
        // Relay AUX -> Off
        CHKPPRET(ars_clear_data_pin(rot, DTA_PIN02 | DTA_PIN04 | DTA_PIN08));
        CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN17 | CTL_PIN16));
        // AZ stop
    }

    par_unlock(pport);

    return RIG_OK;
}

static int angle_in_range(float angle, float angle_base, float range)
{
    return (angle >= angle_base - range && angle <= angle_base + range);
}

/*
 * Thread handler
 */
#ifdef HAVE_PTHREAD
static void *handle_set_position(void *arg)
{
    ROT *rot = (ROT *) arg;
    struct ars_priv_data *priv = (struct ars_priv_data *)rot->state.priv;
    int retcode;

    while (1)
    {

        if (!priv->set_pos_active)
        {
            /* TODO: replace polling period by cond var */
            hl_usleep(100 * 1000 - 1);
            continue;
        }

        retcode = ars_set_position_sync(rot, priv->target_az, priv->target_el);
        priv->set_pos_active = 0;

        if (retcode != RIG_OK)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: ars_set_position_sync() failed: %s\n",
                      __func__, rigerror(retcode));
            hl_usleep(1000 * 1000);
            continue;
        }
    }

    return NULL;
}
#endif

/*
 * ars_set_position_sync() is synchronous.
 * See handle_set_position_async() for asynchronous thread handler,
 * with wait loop in background
 */
int
ars_set_position_sync(ROT *rot, azimuth_t az, elevation_t el)
{
    azimuth_t curr_az, prev_az;
    elevation_t curr_el, prev_el;
    int retval;
    int az_move, el_move;
    struct timeval last_pos_az_tv, last_pos_el_tv;

    rig_debug(RIG_DEBUG_TRACE, "%s called: %.1f %.1f\n", __func__, az, el);

    ars_stop(rot);

    retval = ars_get_position(rot, &curr_az, &curr_el);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* watchdog init */
    prev_az = curr_az;
    prev_el = curr_el;
    gettimeofday(&last_pos_az_tv, NULL);
    last_pos_el_tv = last_pos_az_tv;

    while (!angle_in_range(curr_az, az, AZ_RANGE) ||
            (ars_has_el(rot) && !angle_in_range(curr_el, el, EL_RANGE))
          )
    {

        if (curr_az < (az - AZ_RANGE))
        {
            az_move = ROT_MOVE_RIGHT;
        }
        else if (curr_az > (az + AZ_RANGE))
        {
            az_move = ROT_MOVE_LEFT;
        }
        else
        {
            az_move = 0;
        }

        if (ars_has_el(rot))
        {
            if (curr_el < (el - EL_RANGE))
            {
                el_move = ROT_MOVE_UP;
            }
            else if (curr_el > (el + EL_RANGE))
            {
                el_move = ROT_MOVE_DOWN;
            }
            else
            {
                el_move = 0;
            }
        }
        else
        {
            el_move = 0;
        }

        retval = ars_move(rot, az_move | el_move, 0);

        if (retval != RIG_OK)
        {
            ars_stop(rot);
            return retval;
        }

        /* wait a little */
        hl_usleep(10 * 1000);

        retval = ars_get_position(rot, &curr_az, &curr_el);

        if (retval != RIG_OK)
        {
            ars_stop(rot);
            return retval;
        }

        /* Watchdog detecting when rotor is blocked unexpectedly */
#define AZ_WATCHDOG 5000 /* ms */
#define EL_WATCHDOG 5000 /* ms */

        if (az_move != 0 && angle_in_range(curr_az, prev_az, AZ_RANGE))
        {
            if (rig_check_cache_timeout(&last_pos_az_tv, AZ_WATCHDOG))
            {
                ars_stop(rot);
                return -RIG_ETIMEOUT;
            }
        }
        else
        {
            prev_az = curr_az;
            gettimeofday(&last_pos_az_tv, NULL);
        }

        if (el_move != 0 && ars_has_el(rot) &&
                angle_in_range(curr_el, prev_el, EL_RANGE))
        {
            if (rig_check_cache_timeout(&last_pos_el_tv, EL_WATCHDOG))
            {
                ars_stop(rot);
                return -RIG_ETIMEOUT;
            }
        }
        else
        {
            prev_el = curr_el;
            gettimeofday(&last_pos_el_tv, NULL);
        }
    }

    return ars_stop(rot);
}

int
ars_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
#ifdef HAVE_PTHREAD
    struct ars_priv_data *priv = (struct ars_priv_data *)rot->state.priv;

    /* will be picked by handle_set_position() next polling tick */
    priv->target_az = az;
    priv->target_el = el;
    priv->set_pos_active = 1;

    return RIG_OK;
#else
    return ars_set_position_sync(rot, az, el);
#endif
}

static int comparunsigned(const void *a, const void *b)
{
    const unsigned ua = *(const unsigned *)a, ub = *(const unsigned *)b;

    return ua == ub ? 0 : ua < ub ? -1 : 1;
}

int
ars_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    struct ars_priv_data *priv = (struct ars_priv_data *)rot->state.priv;
    struct rot_state *rs = &rot->state;
    hamlib_port_t *pport = &rs->rotport;
    int i, num_sample;
    unsigned az_samples[NUM_SAMPLES], az_value;
    unsigned el_samples[NUM_SAMPLES], el_value;
    unsigned char status;

    par_lock(pport);

    /* flush last sampled value, with a dummy read */
    CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN_CLK));
    hl_usleep(PP_IO_PERIOD);

    CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN_CS));
    hl_usleep(PP_IO_PERIOD);

    for (i = 0; i < priv->adc_res; i++)
    {
        CHKPPRET(ars_set_ctrl_pin(rot, CTL_PIN_CLK));
        hl_usleep(PP_IO_PERIOD);

        CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN_CLK));
        hl_usleep(PP_IO_PERIOD);
    }

    CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN_CLK));
    hl_usleep(PP_IO_PERIOD);

    CHKPPRET(ars_set_ctrl_pin(rot, CTL_PIN_CS));
    /* end of dummy read */

    for (num_sample = 0; num_sample < NUM_SAMPLES; num_sample++)
    {

        /* read ADC value TLC(1)549 (8/10 bits), by SPI bitbanging */

        hl_usleep(PP_IO_PERIOD);
        CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN_CLK));
        hl_usleep(PP_IO_PERIOD);

        CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN_CS));
        hl_usleep(PP_IO_PERIOD);

        az_samples[num_sample] = 0;
        el_samples[num_sample] = 0;

        for (i = 0; i < priv->adc_res; i++)
        {
            CHKPPRET(ars_set_ctrl_pin(rot, CTL_PIN_CLK));
            hl_usleep(PP_IO_PERIOD);

            CHKPPRET(par_read_status(pport, &status));

            az_samples[num_sample] <<= 1;
            az_samples[num_sample] |= (status & STA_PIN_D0) ? 1 : 0;

            el_samples[num_sample] <<= 1;
            el_samples[num_sample] |= (status & STA_PIN_D1) ? 1 : 0;

            CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN_CLK));
            hl_usleep(PP_IO_PERIOD);
        }

        CHKPPRET(ars_set_ctrl_pin(rot, CTL_PIN_CS));

        rig_debug(RIG_DEBUG_TRACE, "%s: raw samples: az %u, el %u\n",
                  __func__, az_samples[num_sample], el_samples[num_sample]);

        hl_usleep(PP_IO_PERIOD);
    }

    par_unlock(pport);

    qsort(az_samples, NUM_SAMPLES, sizeof(unsigned), comparunsigned);
    qsort(el_samples, NUM_SAMPLES, sizeof(unsigned), comparunsigned);

    /* select median value in order to rule out glitches */
    az_value = az_samples[NUM_SAMPLES / 2];
    el_value = el_samples[NUM_SAMPLES / 2];

    *az = rs->min_az + ((float)az_value * (rs->max_az - rs->min_az)) / ((
                1 << priv->adc_res) - 1);
    *el = rs->min_el + ((float)el_value * (rs->max_el - rs->min_el)) / ((
                1 << priv->adc_res) - 1);

    rig_debug(RIG_DEBUG_TRACE, "%s: az=%.1f el=%.1f\n", __func__, *az, *el);

    return RIG_OK;
}


/* ************************************************************************* */
/*
 * ARS rotator capabilities.
 */

/*
 * RCI/RCI-SE, with Elevation daugtherboard/unit.
 */
const struct rot_caps rci_azel_rot_caps =
{
    ROT_MODEL(ROT_MODEL_RCI_AZEL),
    .model_name =     "ARS RCI AZ&EL",
    .mfg_name =       "EA4TX",
    .version =        "20200112.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rot_type =       ROT_TYPE_AZEL,  /* AZ&EL units */
    .port_type =      RIG_PORT_PARALLEL,
    .write_delay =    0,
    .post_write_delay =  10,
    .timeout =  0,
    .retry =  3,

    .min_az =     0,
    .max_az =     360,
    .min_el =     0,
    .max_el =     180,    /* TBC */

    .rot_init     = ars_init,
    .rot_cleanup  = ars_cleanup,
    .rot_open     = ars_open,
    .rot_close    = ars_close,
    .set_position = ars_set_position,
    .get_position = ars_get_position,
    .stop         = ars_stop,
    .move         = ars_move,
};

/*
 * RCI/RCI-SE, without Elevation daugtherboard/unit.
 * Azimuth only
 */
const struct rot_caps rci_az_rot_caps =
{
    ROT_MODEL(ROT_MODEL_RCI_AZ),
    .model_name =     "ARS RCI AZ",
    .mfg_name =       "EA4TX",
    .version =        "20200112.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rot_type =       ROT_TYPE_AZIMUTH,    /* AZ-only unit */
    .port_type =      RIG_PORT_PARALLEL,
    .write_delay =    0,
    .post_write_delay =  10,
    .timeout =  0,
    .retry =  3,

    .min_az =     0,
    .max_az =     360,
    .min_el =     0,
    .max_el =     180,    /* TBC */

    .rot_init     = ars_init,
    .rot_cleanup  = ars_cleanup,
    .rot_open     = ars_open,
    .rot_close    = ars_close,
    .set_position = ars_set_position,
    .get_position = ars_get_position,
    .stop         = ars_stop,
    .move         = ars_move,
};


/* ************************************************************************* */

DECLARE_INITROT_BACKEND(ars)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rot_register(&rci_az_rot_caps);
    rot_register(&rci_azel_rot_caps);

    return RIG_OK;
}

/* ************************************************************************* */
/* end of file */
