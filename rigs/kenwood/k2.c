/*
 *  Hamlib Kenwood backend - Elecraft K2 description
 *  Copyright (c) 2002-2009 by Stephane Fillod
 *  Copyright (c) 2010 by Nate Bargmann, n0nb@arrl.net
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
 *  See the file 'COPYING.LIB' in the main Hamlib distribution directory for
 *  the complete text of the GNU Lesser Public License version 2.1.
 *
 */

#include <hamlib/config.h>

#include <stdlib.h>
#include <string.h>

#include <hamlib/rig.h>
#include "kenwood.h"
#include "elecraft.h"


#define K2_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB)

#define K2_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_LOCK)

#define K2_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_PREAMP|RIG_LEVEL_AGC|RIG_LEVEL_SQL|\
    RIG_LEVEL_STRENGTH|RIG_LEVEL_RFPOWER|RIG_LEVEL_KEYSPD)

#define K2_VFO (RIG_VFO_A|RIG_VFO_B)
#define K2_VFO_OP (RIG_OP_UP|RIG_OP_DOWN)

#define K2_ANTS (RIG_ANT_1|RIG_ANT_2)

static rmode_t k2_mode_table[KENWOOD_MODE_TABLE_MAX] =
{
    [0] = RIG_MODE_NONE,
    [1] = RIG_MODE_LSB,
    [2] = RIG_MODE_USB,
    [3] = RIG_MODE_CW,
    [4] = RIG_MODE_NONE,
    [5] = RIG_MODE_NONE,
    [6] = RIG_MODE_PKTLSB,        /* AFSK */
    [7] = RIG_MODE_CWR,
    [8] = RIG_MODE_NONE,          /* TUNE mode */
    [9] = RIG_MODE_PKTUSB         /* AFSK */
};

/* kenwood_transaction() will add this to command strings
 * sent to the rig and remove it from strings returned from
 * the rig, so no need to append ';' manually to command strings.
 */
static struct kenwood_priv_caps k2_priv_caps  =
{
    .cmdtrm =  EOM_KEN,
    .mode_table = k2_mode_table,
};


/* K2 Filter list, four per mode */
struct k2_filt_s
{
    shortfreq_t width;  /* Filter width in Hz */
    char fslot;     /* Crystal filter slot number--1-4 */
    char afslot;        /* AF filter slot number--0-2 */
};

/* Number of filter slot arrays to allocate (TNX Diane, VA3DB) */
#define K2_FILT_NUM 4

/* K2 Filter List
 *
 * This struct will be populated as modes are queried or in response
 * to a request to set a given mode.  This way a cache can be maintained
 * of the installed filters and an appropriate filter can be selected
 * for a requested bandwidth.  Each mode has up to four filter slots available.
 */
struct k2_filt_lst_s
{
    struct k2_filt_s filt_list[K2_FILT_NUM];
};

struct k2_filt_lst_s k2_fwmd_ssb;
struct k2_filt_lst_s k2_fwmd_cw;
struct k2_filt_lst_s k2_fwmd_rtty;

/* K2 specific rig_caps API function declarations */
int k2_open(RIG *rig);
int k2_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int k2_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int k2_get_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t *val);

/* Private function declarations */
int k2_probe_mdfw(RIG *rig, struct kenwood_priv_data *priv);
int k2_mdfw_rest(RIG *rig, const char *mode, const char *fw);
int k2_pop_fw_lst(RIG *rig, const char *cmd);


/*
 * KIO2 rig capabilities.
 * This kit can recognize a large subset of TS-570 commands.
 *
 * Part of info comes from http://www.elecraft.com/K2_Manual_Download_Page.htm#K2
 * look for KIO2 Programmer's Reference PDF
 */
const struct rig_caps k2_caps =
{
    RIG_MODEL(RIG_MODEL_K2),
    .model_name =       "K2",
    .mfg_name =     "Elecraft",
    .version =      BACKEND_VER ".0",
    .copyright =        "LGPL",
    .status =       RIG_STATUS_STABLE,
    .rig_type =     RIG_TYPE_TRANSCEIVER,
    .ptt_type =     RIG_PTT_RIG,
    .dcd_type =     RIG_DCD_RIG,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  4800,
    .serial_data_bits = 8,
    .serial_stop_bits = 2,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =      0,  /* Timing between bytes */
    .post_write_delay = 100,    /* Timing between command strings */
    // Note that 2000 timeout exceeds usleep but hl_usleep handles it
    .timeout =      2000,   /* FA and FB make take up to 500 ms on band change */
    .retry =        10,

    .has_get_func =     K2_FUNC_ALL,
    .has_set_func =     K2_FUNC_ALL,
    .has_get_level =    K2_LEVEL_ALL,
    .has_set_level =    RIG_LEVEL_SET(K2_LEVEL_ALL),
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_NONE,  /* FIXME: parms */
    .level_gran =       {},     /* FIXME: granularity */
    .parm_gran =        {},
    .extlevels =        elecraft_ext_levels,
    .extparms =     kenwood_cfg_params,
    .preamp =       { 14, RIG_DBLST_END, },
    .attenuator =       { 10, RIG_DBLST_END, },
    .max_rit =      Hz(9990),
    .max_xit =      Hz(9990),
    .max_ifshift =      Hz(0),
    .vfo_ops =      K2_VFO_OP,
    .targetable_vfo =   RIG_TARGETABLE_FREQ,
    .transceive =       RIG_TRN_RIG,
    .bank_qty =     0,
    .chan_desc_sz =     0,

    .chan_list =        { RIG_CHAN_END },

    .rx_range_list1 =  {
        {kHz(500), MHz(30), K2_MODES, -1, -1, K2_VFO, K2_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  {
        {kHz(1810), kHz(1850) - 1, K2_MODES, 10, W(15), K2_VFO, K2_ANTS}, /* 15W class */
        {kHz(3500), kHz(3800) - 1, K2_MODES, 10, W(15), K2_VFO, K2_ANTS},
        {MHz(7), kHz(7100), K2_MODES, 10, W(15), K2_VFO, K2_ANTS},
        {kHz(10100), kHz(10150), K2_MODES, 10, W(15), K2_VFO, K2_ANTS},
        {MHz(14), kHz(14350), K2_MODES, 10, W(15), K2_VFO, K2_ANTS},
        {kHz(18068), kHz(18168), K2_MODES, 10, W(15), K2_VFO, K2_ANTS},
        {MHz(21), kHz(21450), K2_MODES, 10, W(15), K2_VFO, K2_ANTS},
        {kHz(24890), kHz(24990), K2_MODES, 10, W(15), K2_VFO, K2_ANTS},
        {MHz(28), kHz(29700), K2_MODES, 10, W(15), K2_VFO, K2_ANTS},
        RIG_FRNG_END,
    }, /* tx range */

    .rx_range_list2 =  {
        {kHz(500), MHz(30), K2_MODES, -1, -1, K2_VFO, K2_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        {kHz(1800), MHz(2) - 1, K2_MODES, 10, W(15), K2_VFO, K2_ANTS}, /* 15W class */
        {kHz(3500), MHz(4) - 1, K2_MODES, 10, W(15), K2_VFO, K2_ANTS},
        {MHz(7), kHz(7300), K2_MODES, 10, W(15), K2_VFO, K2_ANTS},
        {kHz(10100), kHz(10150), K2_MODES, 10, W(15), K2_VFO, K2_ANTS},
        {MHz(14), kHz(14350), K2_MODES, 10, W(15), K2_VFO, K2_ANTS},
        {kHz(18068), kHz(18168), K2_MODES, 10, W(15), K2_VFO, K2_ANTS},
        {MHz(21), kHz(21450), K2_MODES, 10, W(15), K2_VFO, K2_ANTS},
        {kHz(24890), kHz(24990), K2_MODES, 10, W(15), K2_VFO, K2_ANTS},
        {MHz(28), kHz(29700), K2_MODES, 10, W(15), K2_VFO, K2_ANTS},
        RIG_FRNG_END,
    }, /* tx range */
    .tuning_steps =  {
        {K2_MODES, 10},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.5)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(500)},
        {RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, kHz(2.5)},
        RIG_FLT_END,
    },
    .priv = (void *)& k2_priv_caps,

    .rig_init =     kenwood_init,
    .rig_cleanup =      kenwood_cleanup,
    .rig_open =     k2_open,
    .rig_close =        kenwood_close,
    .set_freq =     kenwood_set_freq,
    .get_freq =     kenwood_get_freq,
    .set_mode =     k2_set_mode,
    .get_mode =     k2_get_mode,
    .set_vfo =      kenwood_set_vfo,
    .get_vfo =      kenwood_get_vfo_if,
    .set_split_vfo =    kenwood_set_split_vfo,
    .get_split_vfo =    kenwood_get_split_vfo_if,
    .set_rit =      kenwood_set_rit,
    .get_rit =      kenwood_get_rit,
    .set_xit =      kenwood_set_xit,
    .get_xit =      kenwood_get_xit,
    .get_ptt =      kenwood_get_ptt,
    .set_ptt =      kenwood_set_ptt,
    .get_dcd =      kenwood_get_dcd,
    .set_func =     kenwood_set_func,
    .get_func =     kenwood_get_func,
    .set_ext_parm =     kenwood_set_ext_parm,
    .get_ext_parm =     kenwood_get_ext_parm,
    .set_level =        kenwood_set_level,
    .get_level =        kenwood_get_level,
    .get_ext_level =    k2_get_ext_level,
    .vfo_op =       kenwood_vfo_op,
    .set_trn =      kenwood_set_trn,
    .get_powerstat =    kenwood_get_powerstat,
    .get_trn =      kenwood_get_trn,
    .set_ant =      kenwood_set_ant,
    .get_ant =      kenwood_get_ant,
    .send_morse =       kenwood_send_morse,
    .wait_morse =       rig_wait_morse,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


/*
 * K2 extension function definitions follow
 */

/* k2_open()
 *
 */
int k2_open(RIG *rig)
{
    int err;
    struct kenwood_priv_data *priv = rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    err = elecraft_open(rig);

    if (err != RIG_OK)
    {
        return err;
    }

    err = k2_probe_mdfw(rig, priv);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}


/* k2_set_mode()
 *
 * Based on the passed in bandwidth, looks up the nearest bandwidth filter
 * wider than the passed value and sets the radio accordingly.
 */

int k2_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{

    int err;
    char f = '*';
    struct k2_filt_lst_s *flt;
    struct kenwood_priv_data *priv = rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    /* Select the filter array per mode. */
    switch (mode)
    {
    case RIG_MODE_LSB:
    case RIG_MODE_USB:
        flt = &k2_fwmd_ssb;
        break;

    case RIG_MODE_CW:
    case RIG_MODE_CWR:
        flt = &k2_fwmd_cw;
        break;

    case RIG_MODE_PKTLSB:
    case RIG_MODE_PKTUSB:
        if (priv->k2_md_rtty == 0)
        {
            return -RIG_EINVAL;    /* RTTY module not installed */
        }
        else
        {
            flt = &k2_fwmd_rtty;
        }

        break;

    default:
        return -RIG_EINVAL;
    }

    if (width != RIG_PASSBAND_NOCHANGE)
    {
        shortfreq_t freq = 0;

        if (width < 0)
        {
            width = labs(width);
        }

        /* Step through the filter list looking for the best match
         * for the passed in width.  The choice is to select the filter
         * that is wide enough for the width without being too narrow
         * if possible.
         */
        if (width == RIG_PASSBAND_NORMAL)
        {
            width = rig_passband_normal(rig, mode);
        }

        if ((width > flt->filt_list[0].width) || (width > flt->filt_list[1].width))
        {
            width = flt->filt_list[0].width;
            f = '1';
        }
        else if ((flt->filt_list[1].width >= width)
                 && (width > flt->filt_list[2].width))
        {
            width = flt->filt_list[1].width;
            f = '2';
        }
        else if ((flt->filt_list[2].width >= width)
                 && (width > flt->filt_list[3].width))
        {
            width = flt->filt_list[2].width;
            f = '3';
        }
        else if ((flt->filt_list[3].width >= width) && (width >= freq))
        {
            width = flt->filt_list[3].width;
            f = '4';
        }
        else
        {
            return -RIG_EINVAL;
        }
    }

    /* kenwood_set_mode() ignores width value for K2/K3/TS-570 */
    err = kenwood_set_mode(rig, vfo, mode, width);

    if (err != RIG_OK)
    {
        return err;
    }

    if (width != RIG_PASSBAND_NOCHANGE)
    {
        char fcmd[16];

        err = kenwood_transaction(rig, "K22", NULL, 0);

        if (err != RIG_OK)
        {
            return err;
        }

        /* Construct the filter command and set the radio mode and width*/
        SNPRINTF(fcmd, sizeof(fcmd), "FW0000%c", f);

        /* Set the filter slot */
        err = kenwood_transaction(rig, fcmd, NULL, 0);

        if (err != RIG_OK)
        {
            return err;
        }

        err = kenwood_transaction(rig, "K20", NULL, 0);

        if (err != RIG_OK)
        {
            return err;
        }
    }

    return RIG_OK;
}


/* k2_get_mode()
 *
 * Uses the FW command in K22 mode to query the filter bandwidth reported
 * by the radio and returns it to the caller.
 */

int k2_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int err;
    char buf[KENWOOD_MAX_BUF_LEN];
    char tmp[16];
    char *bufptr;
    pbwidth_t temp_w;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!mode || !width)
    {
        return -RIG_EINVAL;
    }

    err = kenwood_get_mode(rig, vfo, mode, &temp_w);

    if (err != RIG_OK)
    {
        return err;
    }

    err = kenwood_transaction(rig, "K22", NULL, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    err = kenwood_safe_transaction(rig, "FW", buf, KENWOOD_MAX_BUF_LEN, 8);

    if (err != RIG_OK)
    {
        return err;
    }

    err = kenwood_transaction(rig, "K20", NULL, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    /* Convert received filter string value's first four digits to width */
    bufptr = buf;

    strncpy(tmp, bufptr + 2, 4);
    tmp[4] = '\0';
    *width = atoi(tmp);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: Mode: %s, Width: %d\n", __func__,
              rig_strrmode(*mode), (int)*width);

    return RIG_OK;
}


/* TQ command is a quick transmit status query--K2/K3 only.
 *
 * token    Defined in elecraft.h or this file
 * val      Type depends on token type from confparams structure:
 *      NUMERIC: val.f
 *      COMBO: val.i, starting from 0 Index to a string table.
 *      STRING: val.cs for set, val.s for get
 *      CHECKBUTTON: val.i 0/1
 */
int k2_get_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t *val)
{
    char buf[KENWOOD_MAX_BUF_LEN];
    int err;
    const struct confparams *cfp;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!val)
    {
        return -RIG_EINVAL;
    }

    cfp = rig_ext_lookup_tok(rig, token);

    switch (token)
    {
    case TOK_TX_STAT:
        err = kenwood_safe_transaction(rig, "TQ", buf, KENWOOD_MAX_BUF_LEN, 3);

        if (err != RIG_OK)
        {
            return err;
        }

        if (cfp->type == RIG_CONF_CHECKBUTTON)
        {
            val->i = atoi(&buf[2]);
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "%s: protocol error, invalid token type\n",
                      __func__);
            return -RIG_EPROTO;
        }

        break;

    default:
        rig_debug(RIG_DEBUG_WARN, "%s: Unsupported get_ext_level %s\n",
                  __func__, rig_strlevel(token));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/* K2 private helper functions follow */

/* Probes for mode and filter settings, based on information
 * by Chris Bryant, G3WIE.
 */
int k2_probe_mdfw(RIG *rig, struct kenwood_priv_data *priv)
{
    int err, i, c;
    char buf[KENWOOD_MAX_BUF_LEN];
    char mode[16];
    char fw[16];
    char cmd[16];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!priv)
    {
        return -RIG_EINVAL;
    }

    /* The K2 extension level has been stored by elecraft_open().  Now set rig
     * to K22 for detailed query of mode and filter width values...
     */
    err = kenwood_transaction(rig, "K22", NULL, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    /* Check for mode and store it for later. */
    err = kenwood_safe_transaction(rig, "MD", buf, KENWOOD_MAX_BUF_LEN, 3);

    if (err != RIG_OK)
    {
        return err;
    }

    strcpy(mode, buf);

    /* Check for filter width and store it for later. */
    err = kenwood_safe_transaction(rig, "FW", buf, KENWOOD_MAX_BUF_LEN, 8);

    if (err != RIG_OK)
    {
        return err;
    }

    strcpy(fw, buf);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: Mode value: %s, Filter Width value: %s\n",
              __func__, mode, fw);

    /* Now begin the process of querying the available modes and filters. */

    /* First try to put the K2 into RTTY mode and check if it's available. */
    priv->k2_md_rtty = 0;       /* Assume RTTY module not installed */
    err = kenwood_transaction(rig, "MD6", NULL, 0);

    if (err != RIG_OK && err != -RIG_ERJCTED)
    {
        return err;
    }

    if (RIG_OK == err)
    {
        /* Read back mode and test to see if K2 reports RTTY. */
        err = kenwood_safe_transaction(rig, "MD", buf, KENWOOD_MAX_BUF_LEN, 3);

        if (err != RIG_OK)
        {
            return err;
        }

        if (!strcmp("MD6", buf))
        {
            priv->k2_md_rtty = 1;    /* set flag for RTTY mode enabled */
        }
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: RTTY flag is: %d\n", __func__,
              priv->k2_md_rtty);

    i = (priv->k2_md_rtty == 1) ? 2 : 1;

    /* Now loop through the modes checking for installed filters. */
    for (c = 0; i > -1; i--, c++)
    {
        if (c == 0)
        {
            strcpy(cmd, "MD1");    /* SSB */
        }
        else if (c == 1)
        {
            strcpy(cmd, "MD3");    /* CW */
        }
        else if (c == 2)
        {
            strcpy(cmd, "MD6");    /* RTTY */
        }
        else                    /* Oops! */
        {
            err = k2_mdfw_rest(rig, mode, fw);

            if (err != RIG_OK)
            {
                return err;
            }

            return -RIG_EINVAL;
        }

        /* Now populate the Filter arrays */
        err = k2_pop_fw_lst(rig, cmd);

        if (err != RIG_OK)
        {
            return err;
        }
    }

    /* Restore mode, filter, extension level */
    if (strlen(fw) == 8)
    {
        fw[7] = '\0';    /* Truncate AFSlot to set filter slot */
    }

    err = k2_mdfw_rest(rig, mode, fw);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}


/* Restore mode, filter, and ext_lvl to original values */
int k2_mdfw_rest(RIG *rig, const char *mode, const char *fw)
{
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!mode || !fw)
    {
        return -RIG_EINVAL;
    }

    if (strlen(mode) != 3 || strlen(fw) != 7)
    {
        return -RIG_EINVAL;
    }

    err = kenwood_transaction(rig, mode, NULL, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    err = kenwood_transaction(rig, fw, NULL, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    err = kenwood_transaction(rig, "K20", NULL, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}


/* Populate k2_filt_lst_s structure for each mode */
int k2_pop_fw_lst(RIG *rig, const char *cmd)
{
    int err, f;
    char fcmd[16];
    char buf[KENWOOD_MAX_BUF_LEN];
    char tmp[16];
    struct k2_filt_lst_s *flt;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!cmd)
    {
        return -RIG_EINVAL;
    }

    /* Store filter data in the correct structure depending on mode */
    if (strcmp(cmd, "MD1") == 0)
    {
        flt = &k2_fwmd_ssb;
    }
    else if (strcmp(cmd, "MD3") == 0)
    {
        flt = &k2_fwmd_cw;
    }
    else if (strcmp(cmd, "MD6") == 0)
    {
        flt = &k2_fwmd_rtty;
    }
    else
    {
        return -RIG_EINVAL;
    }

    /* Set the mode */
    err = kenwood_transaction(rig, cmd, NULL, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    for (f = 1; f < 5; f++)
    {
        char *bufptr = buf;

        SNPRINTF(fcmd, sizeof(fcmd), "FW0000%d", f);

        err = kenwood_transaction(rig, fcmd, NULL, 0);

        if (err != RIG_OK)
        {
            return err;
        }

        err = kenwood_safe_transaction(rig, "FW", buf, KENWOOD_MAX_BUF_LEN, 8);

        if (err != RIG_OK)
        {
            return err;
        }

        /* buf should contain a string "FWxxxxfa;" which corresponds to:
         * xxxx = filter width in Hz
         * f = crystal filter slot number--1-4
         * a = audio filter slot number--0-2
         */
        strncpy(tmp, bufptr + 2, 4);
        tmp[4] = '\0';
        flt->filt_list[f - 1].width = atoi(tmp);

        strncpy(tmp, bufptr + 6, 1);
        tmp[1] = '\0';
        flt->filt_list[f - 1].fslot = atoi(tmp);

        strncpy(tmp, bufptr + 7, 1);
        tmp[1] = '\0';
        flt->filt_list[f - 1].afslot = atoi(tmp);

        rig_debug(RIG_DEBUG_VERBOSE, "%s: Width: %04li, FSlot: %i, AFSlot %i\n",
                  __func__, flt->filt_list[f - 1].width, flt->filt_list[f - 1].fslot,
                  flt->filt_list[f - 1].afslot);
    }

    return RIG_OK;
}

