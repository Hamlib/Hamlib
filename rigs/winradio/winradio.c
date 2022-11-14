/*
 *  Hamlib WiNRADiO backend - main file for interface through /dev/winradio API
 *  Copyright (C) 2001 pab@users.sourceforge.net
 *  Derived from hamlib code (C) 2000-2009 Stephane Fillod.
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

#include "winradio.h"   /* config.h */

#include <string.h>  /* String function definitions */
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "register.h"



#ifdef WINRADIO_IOCTL

#include <linradio/wrapi.h>
#include <linradio/radio_ioctl.h>

#define DEFAULT_WINRADIO_PATH "/dev/winradio0"

#define WINRADIO_MODES RIG_MODE_AM | RIG_MODE_CW | RIG_MODE_LSB | RIG_MODE_USB | RIG_MODE_WFM | RIG_MODE_FM

int wr_rig_init(RIG *rig)
{
    rig->state.rigport.type.rig = RIG_PORT_DEVICE;
    strncpy(rig->state.rigport.pathname, DEFAULT_WINRADIO_PATH,
            HAMLIB_FILPATHLEN - 1);

    return RIG_OK;
}

int wr_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    unsigned long f;

    if (freq > GHz(4.2))
    {
        return -RIG_EINVAL;
    }

    f = (unsigned long)freq;

    if (ioctl(rig->state.rigport.fd, RADIO_SET_FREQ, &f)) { return -RIG_EINVAL; }

    return RIG_OK;
}

int wr_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    unsigned long f;

    if (ioctl(rig->state.rigport.fd, RADIO_GET_FREQ, &f) < 0) { return -RIG_EINVAL; }

    *freq = (freq_t)f;
    return RIG_OK;
}

int wr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    unsigned long m;

    switch (mode)
    {
    case RIG_MODE_AM:  m = RMD_AM; break;

    case RIG_MODE_CW:  m = RMD_CW; break;

    case RIG_MODE_LSB: m = RMD_LSB; break;

    case RIG_MODE_USB: m = RMD_USB; break;

    case RIG_MODE_WFM: m = RMD_FMW; break;

    case RIG_MODE_FM:
        switch (width)
        {
        case RIG_PASSBAND_NORMAL:
        case (int)kHz(17):
        case (int)kHz(15): m = RMD_FMN; break;

        case (int)kHz(6): m = RMD_FM6; break;

        case (int)kHz(50): m = RMD_FMM; break;

        default: return -RIG_EINVAL;
        }

        break;

    default: return -RIG_EINVAL;
    }

    if (ioctl(rig->state.rigport.fd, RADIO_SET_MODE, &m)) { return -RIG_EINVAL; }

    return  RIG_OK;
}

int wr_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    unsigned long m;

    if (ioctl(rig->state.rigport.fd, RADIO_GET_MODE, &m)) { return -RIG_EINVAL; }

    *width = RIG_PASSBAND_NORMAL;

    switch (m)
    {
    case RMD_CW: *mode = RIG_MODE_CW; break;

    case RMD_AM: *mode = RIG_MODE_AM; break;

    case RMD_FMN: *mode = RIG_MODE_FM; break; /* 15kHz or 17kHz on WR-3100 */

    case RMD_FM6: *mode = RIG_MODE_FM; break; /* 6kHz */

    case RMD_FMM: *mode = RIG_MODE_FM; break; /* 50kHz */

    case RMD_FMW: *mode = RIG_MODE_WFM; break;

    case RMD_LSB: *mode = RIG_MODE_LSB; break;

    case RMD_USB: *mode = RIG_MODE_USB; break;

    default: return -RIG_EINVAL;
    }

    if (*width == RIG_PASSBAND_NORMAL)
    {
        *width = rig_passband_normal(rig, *mode);
    }

    return RIG_OK;
}

int wr_set_powerstat(RIG *rig, powerstat_t status)
{
    unsigned long p = 1;
    p = status == RIG_POWER_ON ? 1 : 0;

    if (ioctl(rig->state.rigport.fd, RADIO_SET_POWER, &p)) { return -RIG_EINVAL; }

    return RIG_OK;
}
int wr_get_powerstat(RIG *rig, powerstat_t *status)
{
    unsigned long p;

    if (ioctl(rig->state.rigport.fd, RADIO_GET_POWER, &p)) { return -RIG_EINVAL; }

    *status = p ? RIG_POWER_ON : RIG_POWER_OFF;
    return RIG_OK;
}

int wr_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    switch (func)
    {
    case RIG_FUNC_FAGC:
    {
        unsigned long v = status ? 1 : 0;

        if (ioctl(rig->state.rigport.fd, RADIO_SET_AGC, &v)) { return -RIG_EINVAL; }

        return RIG_OK;
    }

    default:
        return -RIG_EINVAL;
    }
}

int wr_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    switch (func)
    {
    case RIG_FUNC_FAGC:
    {
        unsigned long v;

        if (ioctl(rig->state.rigport.fd, RADIO_GET_AGC, &v)) { return -RIG_EINVAL; }

        *status = v;
        return RIG_OK;
    }

    default:
        return -RIG_EINVAL;
    }
}


int wr_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    switch (level)
    {
    case RIG_LEVEL_AF:
    {
        unsigned long v;

        if (ioctl(rig->state.rigport.fd, RADIO_GET_MAXVOL, &v)) { return -RIG_EINVAL; }

        v *= val.f;

        if (ioctl(rig->state.rigport.fd, RADIO_SET_VOL, &v)) { return -RIG_EINVAL; }

        return RIG_OK;
    }

    case RIG_LEVEL_ATT:
    {
        unsigned long v = val.i ? 1 : 0;

        if (ioctl(rig->state.rigport.fd, RADIO_SET_ATTN, &v)) { return -RIG_EINVAL; }

        return RIG_OK;
    }

    case RIG_LEVEL_IF:
    {
        long v = val.i;

        if (ioctl(rig->state.rigport.fd, RADIO_SET_IFS, &v)) { return -RIG_EINVAL; }

        return RIG_OK;
    }

    case RIG_LEVEL_RF:
    {
        long v = val.f * 100; /* iMaxIFGain on wHWVer > RHV_3150 */

        if (ioctl(rig->state.rigport.fd, RADIO_SET_IFG, &v)) { return -RIG_EINVAL; }

        return RIG_OK;
    }

    default:
        return -RIG_EINVAL;
    }
}

int wr_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    switch (level)
    {
    case RIG_LEVEL_AF:
    {
        unsigned long v, mv;

        if (ioctl(rig->state.rigport.fd, RADIO_GET_MAXVOL, &mv)) { return -RIG_EINVAL; }

        if (ioctl(rig->state.rigport.fd, RADIO_GET_VOL, &v)) { return -RIG_EINVAL; }

        val->f = (float)v / mv;
        return RIG_OK;
    }

    case RIG_LEVEL_ATT:
    {
        unsigned long v;

        if (ioctl(rig->state.rigport.fd, RADIO_GET_VOL, &v)) { return -RIG_EINVAL; }

        val->i = v ? rig->state.attenuator[0] : 0;
        return RIG_OK;
    }

    case RIG_LEVEL_STRENGTH:
    {
        unsigned long v;

        if (ioctl(rig->state.rigport.fd, RADIO_GET_SS, &v)) { return -RIG_EINVAL; }

        val->i = v - 60; /* 0..120, Hamlib assumes S9 = 0dB */
        return RIG_OK;
    }

    case RIG_LEVEL_IF:
    {
        long v;

        if (ioctl(rig->state.rigport.fd, RADIO_GET_IFS, &v)) { return -RIG_EINVAL; }

        val->i = v;
        return RIG_OK;
    }

    case RIG_LEVEL_RF:
    {
        long v;

        if (ioctl(rig->state.rigport.fd, RADIO_GET_IFG, &v)) { return -RIG_EINVAL; }

        val->f = (float)v / 100; /* iMaxIFGain on wHWVer > RHV_3150 */
        return RIG_OK;
    }

    default:
        return -RIG_EINVAL;
    }
}

/*
 * FIXME: static buf does not allow reentrancy!
 */
const char *wr_get_info(RIG *rig)
{
    static char buf[100];

    if (ioctl(rig->state.rigport.fd, RADIO_GET_DESCR, buf) < 0) { return "?"; }

    return buf;
}

#endif  /* WINRADIO_IOCTL */

DECLARE_INITRIG_BACKEND(winradio)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

#ifdef WINRADIO_IOCTL
    rig_register(&wr1000_caps);
    rig_register(&wr1500_caps);
    rig_register(&wr1550_caps);
    rig_register(&wr3100_caps);
    rig_register(&wr3150_caps);
    rig_register(&wr3500_caps);
    rig_register(&wr3700_caps);
#endif  /* WINRADIO_IOCTL */

    /* Receivers with DLL only available under Windows */
#ifdef _WIN32
#ifdef __CYGWIN__
    rig_register(&g303_caps);
    rig_register(&g305_caps);
#endif
#endif

    /* Available on Linux and MS Windows */
#ifndef OTHER_POSIX
    rig_register(&g313_caps);
#endif

    return RIG_OK;
}
