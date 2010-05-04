/*
 *  Hamlib Rotator backend - ARS parallel port backend
 *  Copyright (c) 2010 by Stephane Fillod
 *  This code is inspired by work from Pablo GARCIA - EA4TX
 *
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
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

#define ARS_BRAKE_DELAY 100000 /* usecs */
#define PP_IO_PERIOD 4000 /* usecs */
#define NUM_SAMPLES 3

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
static int ars_set_position(ROT *rot, azimuth_t az, elevation_t el);
static int ars_get_position(ROT *rot, azimuth_t *az, elevation_t *el);

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
        return -RIG_EINVAL;

    priv = (struct ars_priv_data*)malloc(sizeof(struct ars_priv_data));
    if (!priv) {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv->pp_control = 0;
    priv->pp_data = 0;

    priv->dac_res = 8; /* 8 bits vs. 10 bits DAC */
    priv->brake_off = 0;

    rot->state.priv = (void*)priv;

    return RIG_OK;
}

int
ars_cleanup(ROT *rot)
{
    if (!rot)
        return -RIG_EINVAL;

    if (rot->state.priv) {
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

    return RIG_OK;
}

int
ars_close(ROT *rot)
{
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

    priv->brake_off = 0;

    par_lock (pport);

    // Relay AUX -> Off
    CHKPPRET(ars_clear_data_pin(rot, DTA_PIN02 | DTA_PIN04 | DTA_PIN08));
    CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN17 | CTL_PIN16));

    // Elevation Relays -> Off
    CHKPPRET(ars_clear_data_pin(rot, DTA_PIN03 | DTA_PIN07));

    par_unlock (pport);

    return RIG_OK;
}

int
ars_move(ROT *rot, int direction, int speed)
{
    struct ars_priv_data *priv = (struct ars_priv_data *)rot->state.priv;
    hamlib_port_t *pport = &rot->state.rotport;

    rig_debug(RIG_DEBUG_TRACE, "%s called %d\n", __func__, direction);

    par_lock (pport);

    if (!priv->brake_off && (direction & (ROT_MOVE_LEFT|ROT_MOVE_RIGHT))) {
        /* release the brake */
        if (ars_has_el(rot)) {
            // RCI Model Azim & Elev
            // Desactivated CCW/CW relays
            CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN17 | CTL_PIN16));
            // Relay Aux
            CHKPPRET(ars_set_data_pin(rot, DTA_PIN02 | DTA_PIN04 | DTA_PIN06 | DTA_PIN08));
            CHKPPRET(ars_clear_data_pin (rot, DTA_PIN09));
        } else {
            // RCI Model Azimuth only
            // Desactivated CCW/CW relays
            CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN17 | CTL_PIN16));
            // Relay Aux
            CHKPPRET(ars_set_data_pin(rot, DTA_PIN02 | DTA_PIN04 | DTA_PIN06 | DTA_PIN07 | DTA_PIN08 | DTA_PIN09));
            CHKPPRET(ars_clear_data_pin(rot, DTA_PIN03 | DTA_PIN05));
        }
        priv->brake_off = 1;
        usleep(ARS_BRAKE_DELAY);
    }


    if (ars_has_el(rot)) {
        if (direction & ROT_MOVE_UP) {
            /* UP */
            CHKPPRET(ars_set_data_pin(rot, DTA_PIN03 | DTA_PIN06 | DTA_PIN07));
            CHKPPRET(ars_clear_data_pin(rot, DTA_PIN05 | DTA_PIN09));
        } else
        if (direction & ROT_MOVE_DOWN) {
            CHKPPRET(ars_set_data_pin(rot, DTA_PIN03 | DTA_PIN05 | DTA_PIN06 | DTA_PIN07));
            CHKPPRET(ars_clear_data_pin(rot, DTA_PIN09));
        }
        else {
            // Elevation Relays -> Off
            CHKPPRET(ars_clear_data_pin(rot, DTA_PIN03 | DTA_PIN07));
        }
    }

    if (direction & ROT_MOVE_LEFT) {
        // Relay Left
        if (ars_has_el(rot)) {
            // RCI Model Azim & Elev
            CHKPPRET(ars_set_data_pin(rot, DTA_PIN02 | DTA_PIN04 | DTA_PIN06));
            CHKPPRET(ars_set_ctrl_pin(rot, CTL_PIN16));
            CHKPPRET(ars_clear_data_pin(rot, DTA_PIN09));
            CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN17));
        } else {
            // RCI Model Azimuth only
            CHKPPRET(ars_set_data_pin(rot, DTA_PIN02 | DTA_PIN04 | DTA_PIN06 | DTA_PIN07 | DTA_PIN08));
            CHKPPRET(ars_set_ctrl_pin(rot, CTL_PIN16));
            CHKPPRET(ars_clear_data_pin(rot, DTA_PIN03 | DTA_PIN05));
            CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN17));
        }
    } else
    if (direction & ROT_MOVE_RIGHT) {
        // Relay Right
        if (ars_has_el(rot)) {
            // RCI Model Azim & Elev
            CHKPPRET(ars_set_data_pin (rot, DTA_PIN02 | DTA_PIN04 | DTA_PIN06));
            CHKPPRET(ars_set_ctrl_pin (rot, CTL_PIN17));
            CHKPPRET(ars_clear_data_pin(rot, DTA_PIN09));
            CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN16));
        } else {
            // RCI Model Azimuth only
            CHKPPRET(ars_set_data_pin(rot, DTA_PIN02 | DTA_PIN04 | DTA_PIN06 | DTA_PIN07 | DTA_PIN08));
            CHKPPRET(ars_set_ctrl_pin(rot, CTL_PIN17));
            CHKPPRET(ars_clear_data_pin(rot, DTA_PIN03 | DTA_PIN05));
            CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN16));
        }
    } else {
        // Relay AUX -> Off
        CHKPPRET(ars_clear_data_pin(rot, DTA_PIN02 | DTA_PIN04 | DTA_PIN08));
        CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN17 | CTL_PIN16));
        // AZ stop 
    }

    par_unlock (pport);

    return RIG_OK;
}


static int angle_in_range(float angle, float angle_base, float range)
{
    return (angle >= angle_base-range && angle <= angle_base+range);
}

/*
 * TODO: watchdog: when rotor is not moving any more
 * TODO: make set_position asynchronous, with wait loop in background (pthread)
 */
int
ars_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    azimuth_t curr_az;
    elevation_t curr_el;
    int retval;
    int az_move, el_move;

    rig_debug(RIG_DEBUG_TRACE, "%s called: %f %f\n", __func__, az, el);

    ars_stop(rot);

    retval = ars_get_position(rot, &curr_az, &curr_el);
    if (retval != RIG_OK)
        return retval;

    /* TODO: take into account DAC res (8 bits -> 1.4 deg at 360 deg) */
#define AZ_RANGE 2.
#define EL_RANGE 2.
    while (!angle_in_range(curr_az, az, AZ_RANGE) ||
           (ars_has_el(rot) && !angle_in_range(curr_el, el, EL_RANGE))
          ) {

        if (curr_az < (az-AZ_RANGE))
            az_move = ROT_MOVE_RIGHT;
        else if (curr_az > (az+AZ_RANGE))
            az_move = ROT_MOVE_LEFT;
        else
            az_move = 0;

        if (ars_has_el(rot)) {
            if (curr_el < (el-EL_RANGE))
                el_move = ROT_MOVE_UP;
            else if (curr_el > (el+EL_RANGE))
                el_move = ROT_MOVE_DOWN;
            else
                el_move = 0;
        } else {
            el_move = 0;
        }

        retval = ars_move(rot, az_move|el_move, 0);
        if (retval != RIG_OK)
            return retval;

#if 0
        /* wait some?
         * At 360 deg/mn, that's 6 deg/sec.
         */
        usleep(100000);
#endif

        retval = ars_get_position(rot, &curr_az, &curr_el);
        if (retval != RIG_OK)
            return retval;
    }

    return ars_stop(rot);
}


static int comparunsigned(const void *a, const void *b)
{
    const unsigned ua=*(const unsigned*)a, ub = *(const unsigned*)b;

    return ua==ub ? 0 : ua<ub ? -1 : 1;
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

    par_lock (pport);

    /* flush last sampled value, with a "short" read */
    CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN_CLK));
    usleep (PP_IO_PERIOD);

    CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN_CS));
    usleep (PP_IO_PERIOD);

    CHKPPRET(ars_set_ctrl_pin(rot, CTL_PIN_CLK));
    usleep (PP_IO_PERIOD);

    CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN_CLK));
    usleep (PP_IO_PERIOD);

    CHKPPRET(ars_set_ctrl_pin(rot, CTL_PIN_CS));
    /* end of "short" read */


    for (num_sample=0; num_sample < NUM_SAMPLES; num_sample++) {

        /* read ADC value TLC(1)549 (8/10 bits), by SPI bitbanging */

        usleep (PP_IO_PERIOD);
        CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN_CLK));
        usleep (PP_IO_PERIOD);

        CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN_CS));
        usleep (PP_IO_PERIOD);

        az_samples[num_sample] = 0;
        el_samples[num_sample] = 0;

        for (i = 0; i < priv->dac_res; i++) {
            CHKPPRET(ars_set_ctrl_pin(rot, CTL_PIN_CLK));
            usleep (PP_IO_PERIOD);

            CHKPPRET(par_read_status(pport, &status));

            az_samples[num_sample] <<= 1;
            az_samples[num_sample] |= (status & STA_PIN_D0) ? 1 : 0;

            el_samples[num_sample] <<= 1;
            el_samples[num_sample] |= (status & STA_PIN_D1) ? 1 : 0;

            CHKPPRET(ars_clear_ctrl_pin(rot, CTL_PIN_CLK));
            usleep (PP_IO_PERIOD);
        }

        CHKPPRET(ars_set_ctrl_pin(rot, CTL_PIN_CS));

        rig_debug(RIG_DEBUG_TRACE, "%s: raw samples: az %u, el %u\n",
                __func__, az_samples[num_sample], el_samples[num_sample]);

        usleep (PP_IO_PERIOD);
	}

    par_unlock (pport);

    qsort (az_samples, NUM_SAMPLES, sizeof(unsigned), comparunsigned);
    qsort (el_samples, NUM_SAMPLES, sizeof(unsigned), comparunsigned);

    /* select median value in order to rule out glitches */
    az_value = az_samples[NUM_SAMPLES/2];
    el_value = el_samples[NUM_SAMPLES/2];

    *az = rs->min_az + ((float)az_value * (rs->max_az - rs->min_az)) / ((1 << priv->dac_res)-1);
    *el = rs->min_el + ((float)el_value * (rs->max_el - rs->min_el)) / ((1 << priv->dac_res)-1);

    rig_debug(RIG_DEBUG_TRACE, "%s: az=%.1f el=%.1f\n", __func__, *az, *el);

    return RIG_OK;
}


/* ************************************************************************* */
/*
 * ARS rotator capabilities.
 */

const struct rot_caps rci_se8_rot_caps = {
  .rot_model =      ROT_MODEL_RCI_SE8,
  .model_name =     "ARS RCI-SE8",
  .mfg_name =       "EA4TX",
  .version =        "0.1",
  .copyright = 	    "LGPL",
  .status =         RIG_STATUS_ALPHA,
  .rot_type =       ROT_TYPE_AZEL,
  .port_type =      RIG_PORT_PARALLEL,
  .write_delay =    0,
  .post_write_delay =  10,
  .timeout =  0,
  .retry =  3,

  .min_az = 	0,
  .max_az =  	360,
  .min_el = 	0,
  .max_el =  	180,    /* TBC */

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

	rot_register(&rci_se8_rot_caps);

	return RIG_OK;
}

/* ************************************************************************* */
/* end of file */
