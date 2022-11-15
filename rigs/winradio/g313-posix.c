/*
 *  Hamlib WiNRADiO backend - WR-G313
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


#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"

#ifdef HAVE_DLFCN_H
#  include <dlfcn.h>
#endif

#include <hamlib/rig.h>

#include "winradio.h"
#include "linradio/wrg313api.h"


#define G313_FUNC  RIG_FUNC_NONE
#define G313_LEVEL (RIG_LEVEL_ATT | RIG_LEVEL_AGC | RIG_LEVEL_RF | RIG_LEVEL_STRENGTH | RIG_LEVEL_RAWSTR)
#define G313_MODES (RIG_MODE_USB)

#define TOK_SHM_AUDIO 0x150901
#define TOK_SHM_IF 0x150902
#define TOK_SHM_SPECTRUM 0x150903

#define FIFO_PATHNAME_SIZE 64


const struct confparams g313_cfg_params[] =
{
    {
        TOK_SHM_AUDIO, "audio_path", "audio path name",
        "POSIX shared memory path name to the audio ringbuffer",
        "", RIG_CONF_STRING,
    },
    {
        TOK_SHM_IF, "if_path", "I/F path name",
        "POSIX shared memory path name to the I/F ringbuffer",
        "", RIG_CONF_STRING,
    },
    {
        TOK_SHM_SPECTRUM, "spectrum_path", "spectrum path name",
        "POSIX shared memory path name to the spectrum ringbuffer",
        "", RIG_CONF_STRING,
    },
    { RIG_CONF_END, NULL, }
};

struct g313_fifo_data
{
    int fd;
    char path[FIFO_PATHNAME_SIZE];
};

struct g313_priv_data
{
    void *hWRAPI;
    int hRadio;
    int Opened;
    struct g313_fifo_data if_buf;
    struct g313_fifo_data audio_buf;
    struct g313_fifo_data spectrum_buf;
};

static void g313_audio_callback(short *buffer, int count, void *arg);
static void g313_if_callback(short *buffer, int count, void *arg);
static void  g313_spectrum_callback(float *buffer, int count, void *arg);

void *g313_init_api(void)
{
    void *hWRAPI = dlopen("wrg313api.so", RTLD_LAZY);

    if (hWRAPI == 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: Unable to load G313 shared library wrg313api.so\n", __func__);
        return 0;
    }

    if (InitAPI(hWRAPI) == 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unable to initialise G313 api\n", __func__);
        return 0;
    }

    return hWRAPI;
}

int g313_init(RIG *rig)
{
    struct g313_priv_data *priv;

    priv = (struct g313_priv_data *)calloc(1, sizeof(struct g313_priv_data));

    if (!priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    memset(priv, 0, sizeof(struct g313_priv_data));

    priv->hWRAPI = g313_init_api();

    if (priv->hWRAPI)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Initialised G313 API\n", __func__);
    }

    /* otherwise try again when open rig */

    rig->state.priv = (void *)priv;

    return RIG_OK;
}

int g313_open(RIG *rig)
{
    int ret;
    struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;
    RADIO_DESC *List;
    int Count;

    void *audio_callback = g313_audio_callback;
    void *if_callback = g313_if_callback;
    void *spectrum_callback = g313_spectrum_callback;

    if (priv->hWRAPI == 0) /* might not be done yet, must be done now! */
    {
        priv->hWRAPI = g313_init_api();

        if (priv->hWRAPI)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: Initialised G313 API\n", __func__);
        }
        else
        {
            return -RIG_EIO; /* can't go any further */
        }
    }

    if (priv->Opened)
    {
        return RIG_OK;
    }

    ret = GetDeviceList(&List, &Count);

    if (ret < 0 || Count == 0)
    {
        return -RIG_EIO;
    }

    /* Open Winradio receiver handle (default to first device?) */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: found %d rigs 0 is %s\n", __func__, Count,
              List[0].Path);

    if (rig->state.rigport.pathname[0])
    {
        priv->hRadio = OpenDevice(rig->state.rigport.pathname);
    }
    else
    {
        priv->hRadio = OpenDevice(List[0].Path);
    }

    DestroyDeviceList(List);


    if (priv->hRadio < 0)
    {
        return -RIG_EIO;    /* huh! */
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: Opened G313\n", __func__);

    /* Make sure the receiver is switched on */
    SetPower(priv->hRadio, 1);

    priv->audio_buf.fd = open(priv->audio_buf.path, O_WRONLY | O_NONBLOCK);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: audio path %s fifo: %d\n", __func__,
              priv->audio_buf.path, priv->audio_buf.fd);

    if (priv->audio_buf.fd == -1)
    {
        audio_callback = NULL;
    }

    priv->if_buf.fd = open(priv->if_buf.path, O_WRONLY | O_NONBLOCK);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: if path %s fifo: %d\n", __func__,
              priv->if_buf.path, priv->if_buf.fd);

    if (priv->if_buf.fd == -1)
    {
        if_callback = NULL;
    }

    priv->spectrum_buf.fd = open(priv->spectrum_buf.path, O_WRONLY | O_NONBLOCK);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: spectrum path %s fifo: %d\n", __func__,
              priv->spectrum_buf.path, priv->spectrum_buf.fd);

    if (priv->spectrum_buf.fd == -1)
    {
        spectrum_callback = NULL;
    }

    ret = StartStreaming(priv->hRadio, audio_callback, if_callback,
                         spectrum_callback, priv);

    if (ret)
    {
        return -RIG_EIO;
    }

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: told G313 to start streaming audio: %d, if: %d, spec: %d\n",
              __func__,
              audio_callback ? 1 : 0, if_callback ? 1 : 0, spectrum_callback ? 1 : 0);

    priv->Opened = 1;

    return RIG_OK;
}

int g313_close(RIG *rig)
{
    struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;

    if (!priv->Opened)
    {
        return RIG_OK;
    }

    priv->Opened = 0;

    /*
        rig_debug(RIG_DEBUG_VERBOSE, "%s: stop streaming\n", __func__);
        StopStreaming(priv->hRadio);

        req.tv_sec=0;
        req.tv_nsec=500000000L;
        nanosleep(&req, NULL);
    */
    rig_debug(RIG_DEBUG_VERBOSE, "%s: Closing G313\n", __func__);
    CloseDevice(priv->hRadio);

    return RIG_OK;
}

int g313_cleanup(RIG *rig)
{
    struct g313_priv_data *priv;

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    priv = (struct g313_priv_data *)rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: close fifos\n", __func__);

    if (priv->audio_buf.fd >= 0)
    {
        close(priv->audio_buf.fd);
    }

    if (priv->if_buf.fd >= 0)
    {
        close(priv->if_buf.fd);
    }

    if (priv->spectrum_buf.fd)
    {
        close(priv->spectrum_buf.fd);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: Uninitialising G313 API\n", __func__);

    if (priv->hWRAPI)
    {
        dlclose(priv->hWRAPI);
    }

    free(rig->state.priv);

    rig->state.priv = NULL;
    return RIG_OK;
}

int g313_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: %u\n", __func__, (unsigned int)freq);
    ret = SetFrequency(priv->hRadio, (unsigned int)(freq));
    ret = ret == 0 ? RIG_OK : -RIG_EIO;

    return ret;
}

int g313_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;

    unsigned int f;
    int ret = GetFrequency(priv->hRadio, &f);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: ret: %d f: %u\n", __func__, ret, f);

    if (ret)
    {
        return -RIG_EIO;
    }

    *freq = (freq_t)f;
    return RIG_OK;
}

int g313_set_powerstat(RIG *rig, powerstat_t status)
{
    struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;

    int p = status == RIG_POWER_ON ? 1 : 0;
    int ret = SetPower(priv->hRadio, p);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: ret: %d state: %d\n", __func__, ret, p);

    return ret == 0 ? RIG_OK : -RIG_EIO;
}

int g313_get_powerstat(RIG *rig, powerstat_t *status)
{
    struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;

    int p;
    int ret = GetPower(priv->hRadio, &p);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: ret: %d state: %d\n", __func__, ret, p);

    if (ret)
    {
        return -RIG_EIO;
    }

    *status = p ? RIG_POWER_ON : RIG_POWER_OFF;

    return RIG_OK;
}

int g313_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;
    int ret, agc;

    switch (level)
    {
    case RIG_LEVEL_ATT:
        ret = SetAttenuator(priv->hRadio, val.i != 0 ? 1 : 0);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: ret: %d Attenuator: %d\n", __func__, ret,
                  val.i);
        break;

    case RIG_LEVEL_AGC:
        switch (val.i)
        {
        case RIG_AGC_OFF:
            agc = 0;
            break;

        case RIG_AGC_SLOW:
            agc = 1;
            break;

        case RIG_AGC_MEDIUM:
            agc = 2;
            break;

        case RIG_AGC_FAST:
            agc = 3;
            break;

        default:
            return -RIG_EINVAL;
        }

        ret = SetAGC(priv->hRadio, agc);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: ret: %d AGC: %d\n", __func__, ret, val.i);
        break;

    case RIG_LEVEL_RF:
        ret = SetIFGain(priv->hRadio, (int)(val.f * 100));
        rig_debug(RIG_DEBUG_VERBOSE, "%s: ret: %d Gain: %f\n", __func__, ret, val.f);
        break;

    default:
        return -RIG_EINVAL;
    }

    return ret == 0 ? RIG_OK : -RIG_EIO;
}

int g313_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;
    int ret;
    int value;
    unsigned int uvalue;
    double dbl;
    unsigned char ch;

    switch (level)
    {
    case RIG_LEVEL_ATT:
        ret = GetAttenuator(priv->hRadio, &value);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: ret: %d Attenuator: %d\n", __func__, ret,
                  value);

        if (ret)
        {
            return -RIG_EIO;
        }

        val->i = value ? rig->caps->attenuator[0] : 0;
        break;

    case RIG_LEVEL_AGC:
        ret = GetAGC(priv->hRadio, &value);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: ret: %d AGC: %d\n", __func__, ret, value);

        if (ret)
        {
            return -RIG_EIO;
        }

        switch (value)
        {
        case 0:
            val->i = RIG_AGC_OFF;
            break;

        case 1:
            val->i = RIG_AGC_SLOW;
            break;

        case 2:
            val->i = RIG_AGC_MEDIUM;
            break;

        case 3:
            val->i = RIG_AGC_FAST;
            break;

        default:
            return -RIG_EINVAL;
        }

        break;

    case RIG_LEVEL_RF:
        ret = GetIFGain(priv->hRadio, &uvalue);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: ret: %d Gain: %u\n", __func__, ret, uvalue);

        if (ret)
        {
            return -RIG_EIO;
        }

        val->f = ((float)uvalue) / 100.0;
        break;

    case RIG_LEVEL_STRENGTH:
        ret = GetSignalStrength(priv->hRadio, &dbl);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: ret: %d sigstr: %f\n", __func__, ret, dbl);

        if (ret)
        {
            return -RIG_EIO;
        }

        val->i = ((int)dbl / 1.0) + 73;
        break;

    case RIG_LEVEL_RAWSTR:
        ret = GetRawSignalStrength(priv->hRadio, &ch);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: ret: %d Raw Sigstr: %u\n", __func__, ret,
                  (unsigned int)ch);

        if (ret)
        {
            return -RIG_EIO;
        }

        val->i = ch;
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static const char *g313_get_info(RIG *rig)
{
    struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;
    static RADIO_INFO info;
    int ret;

    info.Size = sizeof(RADIO_INFO);

    ret = GetRadioInfo(priv->hRadio, &info);

    if (ret)
    {
        return NULL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ret: %d sernum: %s\n", __func__, ret,
              info.SerNum);
    return info.SerNum;
}

int g313_set_conf(RIG *rig, token_t token, const char *val)
{
    struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;

    size_t len = strlen(val);

    switch (token)
    {
    case TOK_SHM_AUDIO:
        if (len > (FIFO_PATHNAME_SIZE - 1))
        {
            rig_debug(RIG_DEBUG_WARN, "%s: set audio_path %s is too long\n", __func__, val);
            return -RIG_EINVAL;
        }

        memset(priv->audio_buf.path, 0, FIFO_PATHNAME_SIZE);
        strcpy(priv->audio_buf.path, val);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: set audio_path %s\n", __func__,
                  priv->audio_buf.path);
        break;

    case TOK_SHM_IF:
        if (len > (FIFO_PATHNAME_SIZE - 1))
        {
            rig_debug(RIG_DEBUG_WARN, "%s: set if_path %s is too long\n", __func__, val);
            return -RIG_EINVAL;
        }

        memset(priv->if_buf.path, 0, FIFO_PATHNAME_SIZE);
        strcpy(priv->if_buf.path, val);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: set if_path %s\n", __func__,
                  priv->if_buf.path);
        break;

    case TOK_SHM_SPECTRUM:
        if (len > (FIFO_PATHNAME_SIZE - 1))
        {
            rig_debug(RIG_DEBUG_WARN, "%s: set spectrum_path %s is too long\n", __func__,
                      val);
            return -RIG_EINVAL;
        }

        memset(priv->spectrum_buf.path, 0, FIFO_PATHNAME_SIZE);
        strcpy(priv->spectrum_buf.path, val);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: set spectrum_path %s\n", __func__,
                  priv->spectrum_buf.path);
    }

    return RIG_OK;
}

int g313_get_conf(RIG *rig, token_t token, char *val)
{
    struct g313_priv_data *priv = (struct g313_priv_data *)rig->state.priv;

    switch (token)
    {
    case TOK_SHM_AUDIO:
        strcpy(val, priv->audio_buf.path);
        break;

    case TOK_SHM_IF:
        strcpy(val, priv->if_buf.path);
        break;

    case TOK_SHM_SPECTRUM:
        strcpy(val, priv->spectrum_buf.path);
    }

    return RIG_OK;
}

/* no need to check return from write - if not all can be written, accept overruns */

static void g313_audio_callback(short *buffer, int count, void *arg)
{
    struct g313_priv_data *priv = (struct g313_priv_data *)arg;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
    write(priv->audio_buf.fd, buffer, count * sizeof(short));
#pragma GCC diagnostic pop
}

static void g313_if_callback(short *buffer, int count, void *arg)
{
    struct g313_priv_data *priv = (struct g313_priv_data *)arg;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
    write(priv->if_buf.fd, buffer, count * sizeof(short));
#pragma GCC diagnostic pop
}

static void  g313_spectrum_callback(float *buffer, int count, void *arg)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
    struct g313_priv_data *priv = (struct g313_priv_data *)arg;
    write(priv->spectrum_buf.fd, buffer, count * sizeof(float));
#pragma GCC diagnostic pop
}

const struct rig_caps g313_caps =
{
    RIG_MODEL(RIG_MODEL_G313),
    .model_name =     "WR-G313",
    .mfg_name =       "Winradio",
    .version =        "20191224.0",
    .copyright =        "LGPL", /* This wrapper, not the G313 shared library or driver */
    .status =         RIG_STATUS_BETA,
    .rig_type =       RIG_TYPE_PCRECEIVER,
    .port_type =      RIG_PORT_NONE,
    .targetable_vfo =    0,
    .ptt_type =       RIG_PTT_NONE,
    .dcd_type =       RIG_DCD_NONE,
    .has_get_func =   G313_FUNC,
    .has_set_func =   G313_FUNC,
    .has_get_level =  G313_LEVEL,
    .has_set_level =  RIG_LEVEL_SET(G313_LEVEL),
    .has_get_parm =      RIG_PARM_NONE,
    .has_set_parm =      RIG_PARM_NONE,
    .ctcss_list =    NULL,
    .dcs_list =      NULL,
    .chan_list =     { RIG_CHAN_END },
    .transceive =     RIG_TRN_OFF,
    .max_ifshift =   kHz(2),
    .attenuator =     { 20, RIG_DBLST_END, },   /* TBC */
    .rx_range_list1 =  { {
            .startf = kHz(9), .endf = MHz(30), .modes = G313_MODES,
            .low_power = -1, .high_power = -1, .vfo = RIG_VFO_A
        },
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  { {
            .startf = kHz(9), .endf = MHz(30), .modes = G313_MODES,
            .low_power = -1, .high_power = -1, .vfo = RIG_VFO_A
        },
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, },

    .tuning_steps =  { {G313_MODES, 1},
        RIG_TS_END,
    },

    .filters =       { {G313_MODES, kHz(12)},
        RIG_FLT_END,
    },

    .cfgparams = g313_cfg_params,
    .set_conf = g313_set_conf,
    .get_conf = g313_get_conf,

    .rig_init =     g313_init,
    .rig_cleanup =  g313_cleanup,
    .rig_open =     g313_open,
    .rig_close =    g313_close,

    .set_freq =     g313_set_freq,
    .get_freq =     g313_get_freq,

    .set_powerstat =   g313_set_powerstat,
    .get_powerstat =   g313_get_powerstat,
    .set_level =     g313_set_level,
    .get_level =     g313_get_level,

    .get_info =      g313_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
