/*
 i  Hamlib Raspberry Pi backend - main file
 *  Copyright (c) 2019 by HA5KFU ham radio club
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <hamlib/rig.h>

#include <wiringPi.h>

#include "rpi.h"
#include "register.h"

int rpi_init(RIG* rig){
    if(wiringPiSetup() == -1)
        return -RIG_ECONF;
    return RIG_OK;
}

int rpi_cleanup(RIG* rig){
    return RIG_OK;
}

int rpi_open(RIG* rig){
    pinMode(PIN_PTT, OUTPUT);
    pinMode(PIN_DCD, INPUT);
    return RIG_OK;
}

int rpi_close(RIG* rig){
    return RIG_OK;
}

int rpi_set_ptt(RIG* rig, vfo_t vfo, ptt_t ptt){
    digitalWrite(PIN_PTT, ptt);
    return RIG_OK;
}

int rpi_get_ptt(RIG* rig, vfo_t vfo, ptt_t *ptt){
    if(ptt)
        *ptt = digitalRead(PIN_PTT);
    return RIG_OK;
}

int rpi_get_dcd(RIG* rig, vfo_t vfo, dcd_t *dcd){
    if(dcd)
        *dcd = digitalRead(PIN_DCD);
    return RIG_OK;
}

const struct rig_caps rpi_caps = {
    .rig_model = RIG_MODEL_RPI_WIRINGPI,
    .model_name = "WiringPI",
    .mfg_name = "Raspberry PI",
    .version = "0.1",
    .copyright = "LGPL",
    .status = RIG_STATUS_ALPHA,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .ptt_type = RIG_PTT_RIG,
    .dcd_type = RIG_DCD_RIG,
    .port_type = RIG_PORT_NONE,
    .transceive = RIG_TRN_OFF,
    .rig_init = rpi_init,
    .rig_cleanup = rpi_cleanup,
    .rig_open = rpi_open,
    .rig_close = rpi_close,

    .set_ptt = rpi_set_ptt,
    .get_ptt = rpi_get_ptt,
    .get_dcd = rpi_get_dcd,
};

/*
 * proberigs_rpi
 * rig_model_t probeallrigs_rpi(port_t *port, rig_probe_func_t cfunc, rig_ptr_t data)
 */
DECLARE_PROBERIG_BACKEND(rpi)
{
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
  return RIG_MODEL_RPI_WIRINGPI;
}

/*
 * initrigs_rpi is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(rpi)
{
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  rig_register(&rpi_caps);

  return RIG_OK;
}
