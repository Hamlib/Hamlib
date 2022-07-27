/*
 *  Hamlib Tentec Pegasus TT550 backend - main file
 *  Heavily modified for 550 support from the original tentec.c
 *               (c) Oct 2002, Jan,Feb 2004- Ken Koster N7IPB
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
#include <stdio.h>      /* Standard input/output definitions */
#include <string.h>     /* String function definitions */
#include <unistd.h>     /* UNIX standard function definitions */
#include <fcntl.h>      /* File control definitions */
#include <errno.h>      /* Error number definitions */

#include <hamlib/rig.h>
#include "serial.h"
#include "misc.h"
#include "cal.h"

#include "tt550.h"

/*
 * Filter table for 550 reciver support
 */
static int tt550_filters[] =
{
    6000, 5700, 5400, 5100, 4800, 4500, 4200, 3900, 3600, 3300, 3000, 2850,
    2700, 2550, 2400, 2250, 2100, 1950, 1800, 1650, 1500, 1350, 1200, 1050,
    900, 750, 675, 600, 525, 450, 375, 330, 300, 8000
};

/*
 * Filter table for 550 transmit support - The 550 allows the transmitter audio
 * filter bandwidth to be changed, but the filters allowed are only a subset of the
 * receive filters.  This table is used to restrict the filters to the allowable
 * range.
 */
static int tt550_tx_filters[] =
{
    3900, 3600, 3300, 3000, 2850, 2700, 2550, 2400, 2250, 2100, 1950, 1800,
    1650, 1500, 1350, 1200, 1050
};

/***************************Support Functions********************************/

/*
 * tt550_transaction
 * read exactly data_len bytes
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 */
int
tt550_transaction(RIG *rig, const char *cmd, int cmd_len, char *data,
                  int *data_len)
{
    int retval;
    struct rig_state *rs;

    rs = &rig->state;

    /*
     * set_transaction_active keeps the asynchronous decode routine from being called
     * when we get data back from a normal command.
     */
    set_transaction_active(rig);

    rig_flush(&rs->rigport);

    retval = write_block(&rs->rigport, (unsigned char *) cmd, strlen(cmd));

    if (retval != RIG_OK)
    {
        set_transaction_inactive(rig);
        return retval;
    }

    /*
     * no data expected, TODO: flush input?
     */
    if (!data || !data_len)
    {
        set_transaction_inactive(rig);
        return 0;
    }

    retval = read_string(&rs->rigport, (unsigned char *) data, *data_len, NULL, 0,
                         0, 1);

    if (retval == -RIG_ETIMEOUT)
    {
        retval = 0;
    }

    if (retval < 0)
    {
        return retval;
    }

    *data_len = retval;

    set_transaction_inactive(rig);

    return RIG_OK;
}



/*
 * tt550_tx_control - The 550 has a number of operations that control
 * the transmitter.  Commands like enable/disable tx, enable/disable
 * amplifier loop, enable/disable keep alive, etc.
 * This function provides for these commands.
 */
int
tt550_tx_control(RIG *rig, char oper)
{
    struct rig_state *rs = &rig->state;
    int retval;
    char cmdbuf[4];

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "#%c" EOM, oper);
    retval = write_block(&rs->rigport, (unsigned char *) cmdbuf, strlen(cmdbuf));
    /*
     * if (retval == RIG_OK) not currently saving the state of these operations I'm
     * not sure we need to, but if so, this is where it would go.
     */
    return retval;

}



/*
 * tt550_ldg_control - The 550 has a builtin LDG antenna tuner option.
 * This function controls the tuner operations.
 *
 *
 * The LDG tuner listens on the Pegasus' RS-232
 * Rx Data line. The interface is one-way. The tuner can't
 * respond at all. The Pegasus will respond with Z<cr> when
 * it sees the commands meant for the tuner. This is normal.
 * The LDG tuner is only listening on the serial line
 * when RF is applied. Therefore, RF must be applied before
 * the tuner will do anything.
 *
 * $0<cr> = Place tuner in bypass mode
 * $1<cr> = Start Tune process
 * $3<cr> = Cap Up
 * $4<cr> = Cap Dn
 * $5<cr> = Inductor Up
 * $6<cr> = Inductor Dn
 * This function provides for these commands.
 */
int
tt550_ldg_control(RIG *rig, char oper)
{
    int retval, lvl_len;
    char cmdbuf[4], lvlbuf[32];

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "$%c" EOM, oper);

    lvl_len = 3;
    retval = tt550_transaction(rig, cmdbuf, 3, lvlbuf, &lvl_len);
    /*
     * if (retval == RIG_OK) not currently saving the state of these operations I'm
     * not sure we need to, but if so, this is where it would go.
     */
    return retval;
}



/*
 * Tuning Factor Calculations
 * Used by both receive and transmit vfo routines
 * to calculate the desired tuning parameters.
 * tx - 0 for receive tuning, 1 for transmit tuning
 * Thanks to the unknown author of the GPL'd windows program
 * found on the Ten-Tec site.  Having working examples of the
 * calculations was invaluable.
 */
static void
tt550_tuning_factor_calc(RIG *rig, int tx)
{
    struct tt550_priv_data *priv;

    int Bfo = 700;
    double TFreq = 0, IVal, radio_freq = 0;
    int NVal, FVal;       // N value/finetune value
    int TBfo = 0;         // temporary BFO
    int IBfo = 1500;      // Intermediate BFO Freq
    int FilterBw;         // Filter Bandwidth determined from table
    int Mode, PbtAdj, RitAdj, XitAdj;

    priv = (struct tt550_priv_data *) rig->state.priv;

    Mode = (tx ? priv->tx_mode : priv->rx_mode);
    radio_freq = ((tx ? priv->tx_freq : priv->rx_freq)) / (double) MHz(1);
    FilterBw = priv->width;
    PbtAdj = priv->pbtadj;
    RitAdj = priv->rit;
    XitAdj = priv->xit;

    if (tx)
    {
        int bwBFO = (FilterBw / 2) + 200;

        IBfo = (bwBFO > IBfo) ? bwBFO : IBfo;

        if (Mode == RIG_MODE_USB)
        {
            TFreq =
                radio_freq + (double)(IBfo / 1e6) + (double)(XitAdj / 1e6);
            IBfo = (int)(IBfo * 2.73);
        }


        if (Mode == RIG_MODE_LSB)
        {
            TFreq =
                radio_freq - (double)(IBfo / 1e6) + (double)(XitAdj / 1e6);
            IBfo = (int)(IBfo * 2.73);
        }


        if (Mode == RIG_MODE_CW)
        {
            // CW Mode uses LSB Mode
            IBfo = 1500;

            TFreq =
                radio_freq - (double)(IBfo / 1e6) + (double)(Bfo / 1e6) +
                (double)(XitAdj / 1e6);
            IBfo = (int)(Bfo * 2.73);
        }

        if (Mode == RIG_MODE_FM)
        {
            IBfo = 0;
            TFreq =
                radio_freq - (double)(IBfo / 1e6) + (double)(Bfo / 1e6) +
                (double)(XitAdj / 1e6);
            IBfo = 0;
        }

        if (Mode == RIG_MODE_AM)
        {
            IBfo = 0;
            TFreq =
                radio_freq - (double)(IBfo / 1e6) + (double)(Bfo / 1e6) +
                (double)(XitAdj / 1e6);
            IBfo = 0;
        }


    }
    else
    {

        radio_freq = radio_freq + (double)(RitAdj / 1e6);

        if (Mode == RIG_MODE_USB)
        {
            IBfo = (FilterBw / 2) + 200;
            TFreq =
                radio_freq + (double)(IBfo / 1e6) + (double)(PbtAdj / 1e6) +
                (double)(RitAdj / 1e6);
            IBfo = IBfo + PbtAdj ;
        }


        if (Mode == RIG_MODE_LSB)
        {
            IBfo = (FilterBw / 2) + 200;
            TFreq =
                radio_freq - (double)(IBfo / 1e6) - (double)(PbtAdj / 1e6) +
                (double)(RitAdj / 1e6);
            IBfo = IBfo + PbtAdj ;
        }

        if (Mode == RIG_MODE_CW)
        {
            /* CW Mode uses LSB Mode */
            if (((FilterBw / 2) + 300) <= Bfo)
            {
                IBfo = 0;
                TFreq =
                    radio_freq - (double)(IBfo / 1e6) - (double)(PbtAdj / 1e6) +
                    (double)(RitAdj / 1e6);
                IBfo = IBfo + Bfo + PbtAdj;
            }
            else
            {

                IBfo = (FilterBw / 2) + 300;
                TFreq =
                    radio_freq - (double)(IBfo / 1e6) + (double)(Bfo / 1e6) -
                    (double)(PbtAdj / 1e6) + (double)(RitAdj / 1e6);
                IBfo = IBfo + PbtAdj ;
            }

        }

        if (Mode == RIG_MODE_FM)
        {
            IBfo = 0;
            TFreq =
                radio_freq - (double)(IBfo / 1e6) + (double)(Bfo / 1e6) -
                (double)(PbtAdj / 1e6) + (double)(RitAdj / 1e6);
            IBfo = 0;
        }

        if (Mode == RIG_MODE_AM)
        {
            IBfo = 0;
            TFreq =
                radio_freq - (double)(IBfo / 1e6) + (double)(Bfo / 1e6) -
                (double)(PbtAdj / 1e6) + (double)(RitAdj / 1e6);
            IBfo = 0;
        }
    }

    TFreq = TFreq - 0.00125;
    NVal = (int)(TFreq * 400);
    IVal = (double)((TFreq * 400.0) - NVal);
    FVal = (int)(IVal * 2500.0 * 5.46);
    NVal = (NVal + 18000);
    TBfo = (tx ? IBfo : (int)(((double) IBfo + 8000.0) * 2.73));
    priv->ctf = NVal;
    priv->ftf = FVal;
    priv->btf = TBfo;
}

/*************************End of Support Functions**************************/

/*
 * tt550_init:
 * Basically, it just sets up *priv with some sane defaults
 *
 */
int
tt550_init(RIG *rig)
{
    struct tt550_priv_data *priv;

    rig->state.priv = (struct tt550_priv_data *) calloc(1, sizeof(
                          struct tt550_priv_data));

    if (!rig->state.priv)
    {
        /*
         * whoops! memory shortage!
         */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    memset(priv, 0, sizeof(struct tt550_priv_data));

    /*
     * set arbitrary initial status
     */
    priv->rx_freq = MHz(3.985);
    priv->tx_freq = MHz(3.985);
    priv->rx_mode = RIG_MODE_LSB;
    priv->tx_mode = RIG_MODE_LSB;
    priv->width = kHz(2.4);
    priv->tx_width = kHz(2.4);
    priv->tx_cwbfo = priv->cwbfo = kHz(0.7);
    priv->agc = 2;        /* medium */
    priv->lineout = priv->spkvol = 0.0;   /* mute */
    priv->stepsize = 100;     /* default to 100Hz tuning step */

    return RIG_OK;
}



/*
 * Tentec generic tt550_cleanup routine
 * the serial port is closed by the frontend
 */
int
tt550_cleanup(RIG *rig)
{
    if (rig->state.priv)
    {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    return RIG_OK;
}



/*
 * Software restart
 */
int
tt550_reset(RIG *rig, reset_t reset)
{
    int retval, reset_len;
    char reset_buf[32];

    reset_len = 16;
    retval = tt550_transaction(rig, "XX" EOM, 3, reset_buf, &reset_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    reset_len = 16;

    if (strstr(reset_buf, "DSP START"))
    {
        retval = tt550_transaction(rig, "P1" EOM, 3, reset_buf, &reset_len);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    if (!strstr(reset_buf, "RADIO START"))
    {
        return -RIG_EPROTO;
    }

    return RIG_OK;
}



/*
 * Tentec 550 transceiver open routine
 * Restart and set program to execute.
 */
int
tt550_trx_open(RIG *rig)
{

    struct tt550_priv_data *priv;

    priv = (struct tt550_priv_data *) rig->state.priv;

    /*
     * Reset the radio and start it's program running
     * We'll try twice to reset before giving up
     */
    if (tt550_reset(rig, RIG_RESET_SOFT) != RIG_OK)
    {
        if (tt550_reset(rig, RIG_RESET_SOFT) != RIG_OK)
        {
            return -RIG_EPROTO;
        }
    }

#ifdef BYPASS_KEEPALIVE
    /*
     * Temporarily Disable the transmitter Keep alive. The 550 expects the software
     * to execute a serial command at least once every two seconds or it will
     * disable TX.
     */
    tt550_tx_control(rig, DISABLE_KEEPALIVE);

#endif

    /*
     * Program the radio with the default mode,freq,filter
     */
    tt550_set_tx_mode(rig, RIG_VFO_CURR, priv->tx_mode, priv->tx_width);
    tt550_set_rx_mode(rig, RIG_VFO_CURR, priv->rx_mode, priv->width);
    tt550_set_tx_freq(rig, RIG_VFO_CURR, priv->tx_freq);
    tt550_set_rx_freq(rig, RIG_VFO_CURR, priv->rx_freq);

    /*
     * Enable TX
     */
    tt550_tx_control(rig, ENABLE_TX);

    /*
     * Bypass automatic tuner
     */
    tt550_ldg_control(rig, '0');

    return RIG_OK;
}



/*
 * tt550_set_freq
 * Set the receive frequency to that requested and if
 * Split mode is OFF do the transmitter too
 */
int
tt550_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    int retval;
    struct tt550_priv_data *priv = (struct tt550_priv_data *) rig->state.priv;

    retval = tt550_set_rx_freq(rig, vfo, freq);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (priv->split == RIG_SPLIT_OFF)
    {
        return tt550_set_tx_freq(rig, vfo, freq);
    }

    return retval;
}


/*
 * tt550_get_freq
 * Get the current receive frequency
 */
int
tt550_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct tt550_priv_data *priv = (struct tt550_priv_data *) rig->state.priv;

    *freq = priv->rx_freq;

    return RIG_OK;
}


/*
 * tt550_set_mode
 * Set the receive mode and if NOT in split mode
 * set the transmitter to the same mode/width
 */
int
tt550_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    int retval;
    struct tt550_priv_data *priv = (struct tt550_priv_data *) rig->state.priv;

    retval = tt550_set_rx_mode(rig, vfo, mode, width);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (priv->split == RIG_SPLIT_OFF)
    {
        return tt550_set_tx_mode(rig, vfo, mode, width);
    }

    return retval;
}


/*
 * tt550_get_mode
 * GET the current receive mode/width
 */
int
tt550_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct tt550_priv_data *priv = (struct tt550_priv_data *) rig->state.priv;

    *mode = priv->rx_mode;
    *width = priv->width;


    return RIG_OK;
}


/*
 * tt550_set_rx_freq
 * Set the receiver to the requested frequency
 */
int
tt550_set_rx_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct tt550_priv_data *priv;
    struct rig_state *rs = &rig->state;
    int retval;
    char freqbuf[16];

    priv = (struct tt550_priv_data *) rig->state.priv;

    priv->rx_freq = freq;

    tt550_tuning_factor_calc(rig, RECEIVE);

    SNPRINTF(freqbuf, sizeof(freqbuf), "N%c%c%c%c%c%c" EOM,
             priv->ctf >> 8, priv->ctf & 0xff, priv->ftf >> 8,
             priv->ftf & 0xff, priv->btf >> 8, priv->btf & 0xff);

    retval = write_block(&rs->rigport, (unsigned char *) freqbuf, strlen(freqbuf));

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}



/*
 * tt550_set_tx_freq
 * Set the current transmit frequency
 */
int
tt550_set_tx_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct tt550_priv_data *priv;
    struct rig_state *rs = &rig->state;
    int retval;
    char freqbuf[16];

    priv = (struct tt550_priv_data *) rig->state.priv;

    priv->tx_freq = freq;

    tt550_tuning_factor_calc(rig, TRANSMIT);

    SNPRINTF(freqbuf, sizeof(freqbuf), "T%c%c%c%c%c%c" EOM,
             priv->ctf >> 8, priv->ctf & 0xff, priv->ftf >> 8,
             priv->ftf & 0xff, priv->btf >> 8, priv->btf & 0xff);

    retval = write_block(&rs->rigport, (unsigned char *) freqbuf, strlen(freqbuf));

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}



/*
 * tt550_get_tx_freq
 * Get the current transmit frequency
 */
int
tt550_get_tx_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct tt550_priv_data *priv = (struct tt550_priv_data *) rig->state.priv;

    *freq = priv->tx_freq;

    return RIG_OK;
}


/*
 * tt550_set_rx_mode
 * SET the current receive mode
 */
int
tt550_set_rx_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct tt550_priv_data *priv = (struct tt550_priv_data *) rig->state.priv;
    struct rig_state *rs = &rig->state;
    char ttmode;
    rmode_t saved_mode;
    pbwidth_t saved_width;
    int ttfilter = -1, retval;
    char mdbuf[48];

    /*
     * Find mode for receive
     */
    switch (mode)
    {
    case RIG_MODE_USB:
        ttmode = TT_USB;
        break;

    case RIG_MODE_LSB:
        ttmode = TT_LSB;
        break;

    case RIG_MODE_CW:
        ttmode = TT_CW;
        break;

    case RIG_MODE_AM:
        ttmode = TT_AM;
        break;

    case RIG_MODE_FM:
        ttmode = TT_FM;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    /*
     * backup current values in case we fail to write to port
     */
    saved_mode = priv->rx_mode;
    saved_width = priv->width;

    if (width != RIG_PASSBAND_NOCHANGE)
    {
        if (width == RIG_PASSBAND_NORMAL)
        {
            width = rig_passband_normal(rig, mode);
        }

        for (ttfilter = 0; tt550_filters[ttfilter] != 0; ttfilter++)
        {
            if (tt550_filters[ttfilter] == width)
            {
                break;
            }
        }

        if (tt550_filters[ttfilter] != width)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported width %d\n",
                      __func__, (int)width);
            return -RIG_EINVAL;

        }

        priv->width = width;
    }

    priv->rx_mode = mode;

    tt550_tuning_factor_calc(rig, RECEIVE);

    SNPRINTF(mdbuf, sizeof(mdbuf), "M%c%c" EOM, ttmode, ttmode);
    retval = write_block(&rs->rigport, (unsigned char *) mdbuf, strlen(mdbuf));

    if (retval != RIG_OK)
    {
        priv->rx_mode = saved_mode;
        priv->width = saved_width;
        return retval;
    }

    if (width != RIG_PASSBAND_NOCHANGE)
    {
        SNPRINTF(mdbuf, sizeof(mdbuf), "W%c" EOM
                 "N%c%c%c%c%c%c" EOM,
                 ttfilter,
                 priv->ctf >> 8, priv->ctf & 0xff, priv->ftf >> 8,
                 priv->ftf & 0xff, priv->btf >> 8, priv->btf & 0xff);
        retval = write_block(&rs->rigport, (unsigned char *) mdbuf, strlen(mdbuf));

        if (retval != RIG_OK)
        {
            priv->width = saved_width;
            return retval;
        }
    }

    return RIG_OK;
}


/*
 * tt550_set_tx_mode
 * Set the current transmit mode/filter
 * Since the transmitter uses a subset of the filters used
 * by the receiver we set the filter if possible, if not we use
 * the nearest value.
 */
int
tt550_set_tx_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct tt550_priv_data *priv = (struct tt550_priv_data *) rig->state.priv;
    struct rig_state *rs = &rig->state;
    char ttmode;
    rmode_t saved_mode;
    pbwidth_t saved_width;
    int ttfilter = -1, retval;
    char mdbuf[48];

    switch (mode)
    {
    case RIG_MODE_USB:
        ttmode = TT_USB;
        break;

    case RIG_MODE_LSB:
        ttmode = TT_LSB;
        break;

    case RIG_MODE_CW:
        ttmode = TT_CW;
        break;

    case RIG_MODE_AM:
        ttmode = TT_AM;
        break;

    case RIG_MODE_FM:
        ttmode = TT_FM;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported tx mode %s\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    /*
     * backup current values in case we fail to write to port
     */
    saved_mode = priv->tx_mode;
    saved_width = priv->tx_width;

    if (width != RIG_PASSBAND_NOCHANGE)
    {
        /*
         * Limit the transmitter bandwidth - it's not the same as the receiver
         */
        if (width < 1050)
        {
            width = 1050;
        }

        if (width > 3900)
        {
            width = 3900;
        }

        if (width == RIG_PASSBAND_NORMAL)
        {
            width = rig_passband_normal(rig, mode);
        }

        for (ttfilter = 0; tt550_tx_filters[ttfilter] != 0; ttfilter++)
        {
            if (tt550_tx_filters[ttfilter] == width)
            {
                break;
            }
        }

        if (tt550_tx_filters[ttfilter] != width)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: unsupported tx width %d,%d\n", __func__, (int)width,
                      ttfilter);
            return -RIG_EINVAL;

        }

        /*
         * The tx filter array contains just the allowed filter values, but the
         * command assumes that the first allowed value is at offset 7.  We add
         * 7 to compensate for the array difference
         */

        ttfilter += 7;
        priv->tx_width = width;
    }

    priv->tx_mode = mode;

    tt550_tuning_factor_calc(rig, TRANSMIT);

    SNPRINTF(mdbuf, sizeof(mdbuf), "M%c%c" EOM, ttmode, ttmode);
    retval = write_block(&rs->rigport, (unsigned char *) mdbuf, strlen(mdbuf));

    if (retval != RIG_OK)
    {
        priv->tx_mode = saved_mode;
        priv->tx_width = saved_width;
        return retval;
    }

    if (width != RIG_PASSBAND_NOCHANGE)
    {
        SNPRINTF(mdbuf, sizeof(mdbuf), "C%c" EOM
                 "T%c%c%c%c%c%c" EOM,
                 ttfilter,
                 priv->ctf >> 8, priv->ctf & 0xff, priv->ftf >> 8,
                 priv->ftf & 0xff, priv->btf >> 8, priv->btf & 0xff);
        retval = write_block(&rs->rigport, (unsigned char *) mdbuf, strlen(mdbuf));

        if (retval != RIG_OK)
        {
            priv->tx_width = saved_width;
            return retval;
        }
    }

    return RIG_OK;
}


/*
 * tt550_get_tx_mode
 */
int
tt550_get_tx_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct tt550_priv_data *priv = (struct tt550_priv_data *) rig->state.priv;

    *mode = priv->tx_mode;
    *width = priv->tx_width;

    return RIG_OK;
}

/*
 * Set the RIT value and force receive frequency to change
 */
int
tt550_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    struct tt550_priv_data *priv = (struct tt550_priv_data *) rig->state.priv;

    priv->rit = rit;
    tt550_set_rx_freq(rig, vfo, priv->rx_freq);
    return RIG_OK;

}

/*
 * Get The current RIT value
 */
int
tt550_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
    struct tt550_priv_data *priv = (struct tt550_priv_data *) rig->state.priv;

    *rit = priv->rit;

    return RIG_OK;
}

/*
 * Set the XIT value and force the Transmit frequency to change
 */
int
tt550_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit)
{
    struct tt550_priv_data *priv = (struct tt550_priv_data *) rig->state.priv;

    priv->xit = xit;
    tt550_set_tx_freq(rig, vfo, priv->tx_freq);
    return RIG_OK;
}


/*
 *  Get the Current XIT value
 */
int
tt550_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit)
{
    struct tt550_priv_data *priv = (struct tt550_priv_data *) rig->state.priv;

    *xit = priv->xit;

    return RIG_OK;
}


/*
 * tt550_set_level
 */
int
tt550_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    struct tt550_priv_data *priv = (struct tt550_priv_data *) rig->state.priv;
    struct rig_state *rs = &rig->state;
    int retval, ditfactor, dahfactor, spcfactor;
    char cmdbuf[32];

    switch (level)
    {
    case RIG_LEVEL_AGC:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "G%c" EOM,
                 val.i >= 3 ? '3' : (val.i < 2 ? '1' : '2'));
        retval = write_block(&rs->rigport, (unsigned char *) cmdbuf, strlen(cmdbuf));

        if (retval == RIG_OK)
        {
            priv->agc = val.i;
        }

        return retval;

    case RIG_LEVEL_AF:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "V%c" EOM, (int)(val.f * 255));
        retval = write_block(&rs->rigport, (unsigned char *) cmdbuf, strlen(cmdbuf));

        if (retval == RIG_OK)
        {
            priv->spkvol = val.f;
        }

        return retval;
#ifdef RIG_LEVEL_LINEOUT

    case RIG_LEVEL_LINEOUT:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "L%c" EOM, (int)(val.f * 63));
        retval = write_block(&rs->rigport, cmdbuf, strlen(cmdbuf));

        if (retval == RIG_OK)
        {
            priv->lineout = val.f;
        }

        return retval;
#endif

    case RIG_LEVEL_RF:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "A%c" EOM, (int)(val.f * 255));
        retval = write_block(&rs->rigport, (unsigned char *) cmdbuf, strlen(cmdbuf));

        if (retval == RIG_OK)
        {
            priv->rflevel = val.f;
        }

        return retval;

    case RIG_LEVEL_SQL:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "S%c" EOM, (int)(val.f * 19));
        retval = write_block(&rs->rigport, (unsigned char *) cmdbuf, strlen(cmdbuf));

        if (retval == RIG_OK)
        {
            priv->sql = val.f;
        }

        return retval;

    case RIG_LEVEL_NR:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "D%c" EOM, (int)(val.f * 7));
        retval = write_block(&rs->rigport, (unsigned char *) cmdbuf, strlen(cmdbuf));

        if (retval == RIG_OK)
        {
            priv->nr = val.f;
        }

        return retval;

    case RIG_LEVEL_ATT:
        /*
         * attenuator is either on or off
         */
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "B%c" EOM, val.i < 15 ? '0' : '1');
        retval = write_block(&rs->rigport, (unsigned char *) cmdbuf, strlen(cmdbuf));

        if (retval == RIG_OK)
        {
            priv->att = val.i;
        }

        return retval;

    case RIG_LEVEL_KEYSPD:
        ditfactor = spcfactor =
                        (int)(((double) 0.50 /
                               (val.i * (double) 0.4166 * (double) 0.0001667)));
        dahfactor = ditfactor * 3;

        SNPRINTF(cmdbuf, sizeof(cmdbuf), "E%c%c%c%c%c%c" EOM,
                 ditfactor >> 8, ditfactor & 0xff, dahfactor >> 8,
                 dahfactor & 0xff, spcfactor >> 8,
                 spcfactor & 0xff);
        retval = write_block(&rs->rigport, (unsigned char *) cmdbuf, strlen(cmdbuf));

        if (retval == RIG_OK)
        {
            priv->keyspd = val.i;
        }

        return retval;

    case RIG_LEVEL_RFPOWER:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "P%c" EOM, (int)(val.f * 255));
        retval = write_block(&rs->rigport, (unsigned char *) cmdbuf, strlen(cmdbuf));

        if (retval == RIG_OK)
        {
            priv->rfpower = val.f;
        }

        return retval;

    case RIG_LEVEL_VOXGAIN:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "UG%c" EOM, (int)(val.f * 255));
        retval = write_block(&rs->rigport, (unsigned char *) cmdbuf, strlen(cmdbuf));

        if (retval == RIG_OK)
        {
            priv->voxgain = val.f;
        }

        return retval;

    case RIG_LEVEL_VOXDELAY:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "UH%c" EOM, (int)(val.f * 255));
        retval = write_block(&rs->rigport, (unsigned char *) cmdbuf, strlen(cmdbuf));

        if (retval == RIG_OK)
        {
            priv->voxdelay = val.f;
        }

        return retval;

    case RIG_LEVEL_ANTIVOX:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "UA%c" EOM, (int)(val.f * 255));
        retval = write_block(&rs->rigport, (unsigned char *) cmdbuf, strlen(cmdbuf));

        if (retval == RIG_OK)
        {
            priv->antivox = val.f;
        }

        return retval;

    case RIG_LEVEL_COMP:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "Y%c" EOM, (int)(val.f * 127));
        retval = write_block(&rs->rigport, (unsigned char *) cmdbuf, strlen(cmdbuf));

        if (retval == RIG_OK)
        {
            priv->speechcomp = val.f;
        }

        return retval;

    case RIG_LEVEL_MICGAIN:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "O1%c%c" EOM, 0, (int)(val.f * 15));
        retval = write_block(&rs->rigport, (unsigned char *) cmdbuf, strlen(cmdbuf));

        if (retval == RIG_OK)
        {
            priv->mikegain = val.f;
        }

        return retval;

    case RIG_LEVEL_BKINDL:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "UQ%c" EOM, (int)(val.f * 255));
        retval = write_block(&rs->rigport, (unsigned char *) cmdbuf, strlen(cmdbuf));

        if (retval == RIG_OK)
        {
            priv->bkindl = val.f;
        }

        return retval;

    case RIG_LEVEL_IF:
        priv->pbtadj = val.i;
        retval = tt550_set_rx_freq(rig, vfo, priv->tx_freq);
        return retval;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported set_level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * tt550_get_level
 */
int
tt550_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    struct tt550_priv_data *priv = (struct tt550_priv_data *) rig->state.priv;
    int retval, lvl_len;
    char lvlbuf[32];

    switch (level)
    {
    case RIG_LEVEL_STRENGTH:
        /*
         * read A/D converted value
         */
        lvl_len = 7;
        retval = tt550_transaction(rig, "?S" EOM, 3, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len != 6)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "tt550_get_level: wrong answer" "len=%d\n", lvl_len);
            return -RIG_ERJCTED;
        }

        /*
         * Crude but it should work,  the first and second digits are
         * the ascii value for the S number (0x30 = S0 etc.) followed by
         * a two byte fractional binary portion - We only use the first
         * portion for now.
         */
        val->i = (((lvlbuf[2] - 0x30) * 6) - 54);

        break;

    case RIG_LEVEL_RAWSTR:
        /*
         * read A/D converted value
         */
        lvl_len = 6;
        retval = tt550_transaction(rig, "?X" EOM, 3, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len != 5)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "tt550_get_level: wrong answer" "len=%d\n", lvl_len);
            return -RIG_ERJCTED;
        }

        val->i = (lvlbuf[1] << 8) + lvlbuf[2];
        break;

    case RIG_LEVEL_AGC:
        val->f = priv->agc;
        break;

    case RIG_LEVEL_AF:
        val->f = priv->spkvol;
        break;

#ifdef RIG_LEVEL_LINEOUT

    case RIG_LEVEL_LINEOUT:
        val->f = priv->lineout;
        break;
#endif

    case RIG_LEVEL_RF:
        val->f = priv->rflevel;
        break;

    case RIG_LEVEL_SQL:
        val->f = priv->sql;
        break;

    case RIG_LEVEL_ATT:
        val->i = priv->att;
        break;

    case RIG_LEVEL_KEYSPD:
        val->i = priv->keyspd;
        break;

    case RIG_LEVEL_NR:
        val->f = priv->nr;
        break;

    case RIG_LEVEL_RFPOWER:
        val->f = priv->rfpower;
        break;

    case RIG_LEVEL_VOXGAIN:
        val->f = priv->voxgain;
        break;

    case RIG_LEVEL_VOXDELAY:
        val->f = priv->voxdelay;
        break;

    case RIG_LEVEL_ANTIVOX:
        val->f = priv->antivox;
        break;

    case RIG_LEVEL_COMP:
        val->f = priv->speechcomp;
        break;

    case RIG_LEVEL_MICGAIN:
        val->f = priv->mikegain;
        break;

    case RIG_LEVEL_BKINDL:
        val->f = priv->bkindl;
        break;

    case RIG_LEVEL_IF:
        val->i = priv->pbtadj;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported get_level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;

    }

    return RIG_OK;
}


/*
 * tt550_get_info
 */
const char *
tt550_get_info(RIG *rig)
{
    static char buf[16];
    int firmware_len, retval;

    /*
     * protocol version
     */
    firmware_len = 10;
    retval = tt550_transaction(rig, "?V" EOM, 3, buf, &firmware_len);

    if (retval != RIG_OK || firmware_len != 9)
    {
        rig_debug(RIG_DEBUG_ERR, "tt550_get_info: ack NG, len=%d\n",
                  firmware_len);
        return NULL;
    }

    buf[firmware_len] = '\0';
    return buf;
}


/*
 * tt550_set_ptt
 */
int
tt550_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    struct rig_state *rs = &rig->state;
    char cmdbuf[16];

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "Q%c" EOM, ptt == 0 ? '0' : '1');
    return (write_block(&rs->rigport, (unsigned char *) cmdbuf, strlen(cmdbuf)));

}

/*
 * tt550_get_ptt
 */
int
tt550_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    static char buf[10];
    int len, retval;

    /*
     * The 550 doesn't have an explicit command to return ptt status, so we fake it
     * with the request for signal strength which returns a 'T' for the first
     * character if we're transmitting
     */
    len = 7;
    retval = tt550_transaction(rig, "?S" EOM, 3, buf, &len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /*
     * buf should contain either Sxx for Receive Signal strength
     * or Txx for Transmit power/reflected power
     */

    *ptt = buf[0] == 'T' ? RIG_PTT_ON : RIG_PTT_OFF;

    return RIG_OK;

}

/*
 * tt550_set_split_vfo
 */
int
tt550_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    struct tt550_priv_data *priv = (struct tt550_priv_data *) rig->state.priv;

    priv->split = split;

    return RIG_OK;
}


/*
 * tt550_get_split_vfo
 */
int
tt550_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
    struct tt550_priv_data *priv = (struct tt550_priv_data *) rig->state.priv;

    *split = priv->split;

    return RIG_OK;
}


int
tt550_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    unsigned char fctbuf[16];
    struct tt550_priv_data *priv = (struct tt550_priv_data *) rig->state.priv;
    struct rig_state *rs = &rig->state;

    /* Optimize:
     *   sort the switch cases with the most frequent first
     */
    switch (func)
    {
    case RIG_FUNC_VOX:
        SNPRINTF((char *) fctbuf, sizeof(fctbuf), "U%c" EOM, status == 0 ? '0' : '1');
        priv->vox = status;
        return write_block(&rs->rigport, fctbuf, strlen((char *)fctbuf));

    case RIG_FUNC_NR:
        SNPRINTF((char *) fctbuf, sizeof(fctbuf), "K%c%c" EOM, status == 0 ? '0' : '1',
                 priv->anf == 0 ? '0' : '1');
        priv->en_nr = status;
        return write_block(&rs->rigport, fctbuf, strlen((char *)fctbuf));

    case RIG_FUNC_ANF:
        SNPRINTF((char *) fctbuf, sizeof(fctbuf), "K%c%c" EOM,
                 priv->en_nr == 0 ? '0' : '1',
                 status == 0 ? '0' : '1');
        priv->anf = status;
        return write_block(&rs->rigport, fctbuf, strlen((char *)fctbuf));


    case RIG_FUNC_TUNER:
        priv->tuner = status;

        if (status == '0')
        {
            tt550_ldg_control(rig, 0);
        }

        return RIG_OK;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported set_func %s", __func__,
                  rig_strfunc(func));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int
tt550_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    struct tt550_priv_data *priv = (struct tt550_priv_data *) rig->state.priv;

    /* Optimize:
     *   sort the switch cases with the most frequent first
     */
    switch (func)
    {
    case RIG_FUNC_VOX:
        *status = priv->vox;
        break;

    case RIG_FUNC_NR:
        *status = priv->en_nr;
        break;

    case RIG_FUNC_ANF:
        *status = priv->anf;
        break;

    case RIG_FUNC_TUNER:
        *status = priv->tuner;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported get_func %s", __func__,
                  rig_strfunc(func));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * tt550_set_tuning_step
 */
int
tt550_set_tuning_step(RIG *rig, vfo_t vfo, shortfreq_t stepsize)
{
    struct tt550_priv_data *priv;
    struct rig_state *rs;

    rs = &rig->state;
    priv = (struct tt550_priv_data *) rs->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: tt550_set_tuning_step - %d\n",
              __func__, (int)stepsize);

    priv->stepsize = stepsize;

    return RIG_OK;
}


/*
 * tt550_get_tuning_step
 */
int
tt550_get_tuning_step(RIG *rig, vfo_t vfo, shortfreq_t *stepsize)
{
    struct tt550_priv_data *priv;
    struct rig_state *rs;

    rs = &rig->state;
    priv = (struct tt550_priv_data *) rs->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: tt550_get_tuning_step - %d\n",
              __func__, (int)priv->stepsize);

    *stepsize = priv->stepsize;

    return RIG_OK;
}


/*
 * Tune the radio using the LDG antenna tuner
 */
int
tt550_tune(RIG *rig)
{
    value_t current_power;
    rmode_t current_mode;
    value_t lowpower;
    struct tt550_priv_data *priv = (struct tt550_priv_data *) rig->state.priv;

    /* Set our lowpower level to about 10 Watts */
    lowpower.f = 0.12;

    /* Get the current power and save it, */
    current_power.f = priv->rfpower;

    /* Set power to approx 10w */
    tt550_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_RFPOWER, lowpower);

    /* Get the current mode,  and save */
    current_mode = priv->tx_mode;

    /* Set the mode to cw, keep the old frequency and bandwidth */
    tt550_set_tx_mode(rig, RIG_VFO_CURR, RIG_MODE_CW, priv->tx_width);
    tt550_set_tx_freq(rig, RIG_VFO_CURR, priv->tx_freq);

    /* key the radio */
    tt550_set_ptt(rig, RIG_VFO_CURR, 1);

    /* Wait long enough for the transmitter to key up */
    sleep(1);

    /* Start the tuner */
    tt550_ldg_control(rig, '1');

    /*
     * Wait for tuner to finish
     * NOTE:  Using sleep and blocking like this is BAD, we
     * really should have a way to tell that the tuner is finished.
     * What we should be doing here is probably:
     *      1. wait one second for tuner to start.
     *      2. Unkey the radio - the LDG tuner will keep it keyed until
     *      it is done. (I think)
     *  NOTE:  I was wrong, the LDG does not key the rig so this won't work.
     *         Have to come up with something else.
     *      3. Keep checking for the Radio to be unkeyed
     *      4. Stop the tuner and restore everything.
     * The above should all be done asynchronous to this function so
     * that we don't stall the calling routine.
     */
    sleep(4);

    /* Unkey the Radio */
    tt550_set_ptt(rig, RIG_VFO_CURR, 0);

    /* Restore the mode and frequency */
    tt550_set_tx_mode(rig, RIG_VFO_CURR, current_mode, priv->tx_width);
    tt550_set_tx_freq(rig, RIG_VFO_CURR, priv->tx_freq);

    /* Restore the original Power setting */
    tt550_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_RFPOWER, current_power);

    return RIG_OK;
}


/*
 * tt550_vfo_op
 */
int
tt550_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{

    switch (op)
    {
    case RIG_OP_TUNE:
        tt550_tune(rig);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "tt550_vfo_op: unsupported op %#x\n", op);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}



#define MAXFRAMELEN 7
/*
 * tt550_decode is called by sa_sigio, when asynchronous data
 * has been received from the rig
 *
 * A lot more can be done in this routine.  Things like allowing F2
 * to switch the encoder between frequency, audio, power control. Or
 * letting a function key cycle thru various bands.
 * For now it just handles the encoder for frequency change and F1 for
 * changing the step size.
 */
int
tt550_decode_event(RIG *rig)
{
    struct tt550_priv_data *priv;
    struct rig_state *rs;
    unsigned char buf[MAXFRAMELEN];
    int data_len;
//  char key;


    rig_debug(RIG_DEBUG_VERBOSE, "%s/tt: tt550_decode_event called\n", __func__);

    rs = &rig->state;
    priv = (struct tt550_priv_data *) rs->priv;


    data_len = read_string(&rs->rigport, buf, MAXFRAMELEN, "\n\r", 2, 0,
                           1);


    if (data_len == -RIG_ETIMEOUT)
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: tt550_decode got a timeout before the first character\n", __func__);
        return RIG_OK;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: tt550_decode %p\n", __func__, &buf);

    /*
     * The first byte must be either 'U' for keypad operations
     * or '!' for encoder operations.
     */
    switch (*buf)
    {

    /*
     *  For now we'll assume that the encoder is only used for
     *  frequency control, but since it's really a general purpose
     *  device we could later use it for other purposes.
     *  Tied in with priv->stepsize to allow the step rate to change
     */
    case '!':
        if (rig->callbacks.freq_event)
        {
            int movement = buf[1] << 8;
            movement = movement | buf[2];
//      key = buf[3];
            rig_debug(RIG_DEBUG_VERBOSE,
                      "%s: Step Direction = %d\n", __func__, movement);

            if (movement > 0)
            {
                priv->rx_freq += priv->stepsize;
            }

            if (movement < 0)
            {
                priv->rx_freq -= priv->stepsize;
            }

            rig->callbacks.freq_event(rig, RIG_VFO_CURR, priv->rx_freq,
                                      rig->callbacks.freq_arg);
        }

        break;

    /*
     * Keypad Function Key support - for now only F1
     * Numeric pad can be done later
     */
    case 'U':
        switch (buf[1])
        {
        case KEY_F1_DOWN:

            /* F1 changes the Step size from 1hz to 1mhz */
            if (priv->stepsize < 10000)
            {
                /* In powers of ten */
                priv->stepsize = priv->stepsize * 10;
            }
            else
            {
                priv->stepsize = 1;
            }

            break;

        case KEY_F2_DOWN:
        case KEY_F3_DOWN:
        case KEY_F1_UP:
        case KEY_F2_UP:
        case KEY_F3_UP:
        default:
            rig_debug(RIG_DEBUG_VERBOSE,
                      "tt550_decode:  KEY " "unsupported %d\n", buf[1]);
            return -RIG_ENIMPL;
        }

        break;

    default:
        rig_debug(RIG_DEBUG_VERBOSE,
                  "tt550_decode:  response " "unsupported %s\n", buf);
        return -RIG_ENIMPL;
    }

    return RIG_OK;
}
