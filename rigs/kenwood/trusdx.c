/*
 *  Hamlib Kenwood backend - (tr)uSDX description
 *  Copyright (c) 2000-2004 by Stephane Fillod and Juergen Rinas
 *  Copyright (c) 2021 by Mikael Nousiainen
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

#include <string.h>

#include "hamlib/rig.h"
#include "idx_builtin.h"
#include "kenwood.h"
#include "cache.h"

/*
 * The (tr)uSDX implements only the CAT subset documented at
 * https://dl2man.de/5-trusdx-details/ .  In particular, IF only documents
 * frequency and mode; its other fields must not be used as rig status.
 * Keep these callbacks separate from the full TS-480 capability set.
 *
 * Firmware history:
 * https://dl2man.de/wp-content/uploads/2022/01/wp.php/releases.txt
 * R2.00t disabled AM/FM; R2.00v re-enabled them. The mode list below
 * describes firmware with AM/FM enabled, not every historical release.
 * R2.00p beta notes introduce 115200 baud; the user manual requires it
 * for R2.00t onward. Older firmware uses 38400 baud.
 * R2.00u improves TX resolution to 1 Hz and changes FL handling; R2.00v
 * enables a TX CAT meter. Neither entry defines the wire format, so this
 * backend does not advertise filter control or meter queries.
 * R2.00w enables semi-QSK by default; R2.00x defaults it back to OFF and
 * removes a CAT display update that slowed FT8CN TX/RX transitions.
 * Audio streaming extensions are not implemented by this backend.
 *
 * Band ranges cover the union of Lo (80/60/40/30/20 m), Hi
 * (20/17/15/12/10 m), and Classic (80/40/20/15/10 m) filter boards.
 * An individual radio supports the five bands of its fitted board.
 * https://dl2man.de/2-trusdx-assembly/
 * Power is nominally 0.5 W on USB supply and 5 W on external supply;
 * these limits describe the radio, not a CAT-adjustable power setting.
 * https://dl2man.de/
 */
#define TRUSDX_MODES (RIG_MODE_LSB | RIG_MODE_USB | RIG_MODE_CW | RIG_MODE_FM | RIG_MODE_AM)

static rmode_t trusdx_modes[KENWOOD_MODE_TABLE_MAX] =
{
    RIG_MODE_NONE, RIG_MODE_LSB, RIG_MODE_USB, RIG_MODE_CW,
    RIG_MODE_FM, RIG_MODE_AM
};

static struct kenwood_priv_caps trusdx_priv_caps =
{
    .cmdtrm = EOM_KEN,
    .mode_table = trusdx_modes,
};

static int trusdx_set_vfo(RIG *rig, vfo_t vfo)
{
    /* CAT exposes a single frequency through FA, with no FR/FT commands. */
    (void)rig;
    return vfo == RIG_VFO_A || vfo == RIG_VFO_CURR ? RIG_OK : -RIG_ENTARGET;
}

static int trusdx_get_vfo(RIG *rig, vfo_t *vfo)
{
    (void)rig;
    *vfo = RIG_VFO_A;
    return RIG_OK;
}

static int trusdx_open(RIG *rig)
{
    char id[16];
    int retval = kenwood_safe_transaction(rig, "ID", id, sizeof(id), 5);

    if (retval != RIG_OK) { return retval; }

    if (strcmp(id, "ID020") != 0) { return -RIG_EPROTO; }

    rig_set_current_vfo_state(rig, RIG_VFO_A);
    return RIG_OK;
}

static int trusdx_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    int retval = trusdx_set_vfo(rig, vfo);
    if (retval != RIG_OK) { return retval; }
    return kenwood_set_freq(rig, RIG_VFO_A, freq);
}

static int trusdx_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    int retval = trusdx_set_vfo(rig, vfo);
    if (retval != RIG_OK) { return retval; }
    return kenwood_get_freq(rig, RIG_VFO_A, freq);
}

static int trusdx_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    int retval = trusdx_set_vfo(rig, vfo);
    if (retval != RIG_OK) { return retval; }

    /* No variable filter-width command is documented. */
    if (width != RIG_PASSBAND_NORMAL && width != RIG_PASSBAND_NOCHANGE)
    {
        return -RIG_ENAVAIL;
    }

    return kenwood_set_mode(rig, RIG_VFO_A, mode, RIG_PASSBAND_NOCHANGE);
}

static int trusdx_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int retval = trusdx_set_vfo(rig, vfo);
    if (retval != RIG_OK) { return retval; }
    return kenwood_get_mode(rig, RIG_VFO_A, mode, width);
}

static int trusdx_set_powerstat(RIG *rig, powerstat_t status)
{
    /* PS1 is documented, but PS0 (power off) is not. */
    if (status != RIG_POWER_ON) { return -RIG_ENAVAIL; }
    return kenwood_transaction(rig, "PS1", NULL, 0);
}

struct rig_caps trudx_caps =
{
    RIG_MODEL(RIG_MODEL_TRUSDX),
    .model_name = "(tr)uSDX",
    .mfg_name = "DL2MAN",
    .version = BACKEND_VER ".3",
    .copyright = "LGPL",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .ptt_type = RIG_PTT_RIG_MICDATA,
    .dcd_type = RIG_DCD_NONE,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 38400,
    .serial_rate_max = 115200,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .timeout = 500,
    .retry = 3,
    .transceive = RIG_TRN_OFF,

    .rx_range_list1 = {
        {kHz(3500),  kHz(29700), TRUSDX_MODES, -1, -1, RIG_VFO_A},
        RIG_FRNG_END,
    }, /*!< Receive frequency range list for ITU region 1 */
    .tx_range_list1 = {
        {kHz(3500),  kHz(3800),  TRUSDX_MODES, 500, 5000, RIG_VFO_A},
        {kHz(5250),  kHz(5450),  TRUSDX_MODES, 500, 5000, RIG_VFO_A},
        {MHz(7),     kHz(7200),  TRUSDX_MODES, 500, 5000, RIG_VFO_A},
        {kHz(10100), kHz(10150), TRUSDX_MODES, 500, 5000, RIG_VFO_A},
        {MHz(14),    kHz(14350), TRUSDX_MODES, 500, 5000, RIG_VFO_A},
        {kHz(18068), kHz(18168), TRUSDX_MODES, 500, 5000, RIG_VFO_A},
        {MHz(21),    kHz(21450), TRUSDX_MODES, 500, 5000, RIG_VFO_A},
        {kHz(24890), kHz(24990), TRUSDX_MODES, 500, 5000, RIG_VFO_A},
        {MHz(28),    kHz(29700), TRUSDX_MODES, 500, 5000, RIG_VFO_A},
        RIG_FRNG_END,
    },  /*!< Transmit frequency range list for ITU region 1 */
    .rx_range_list2 = {
        {kHz(3500),  kHz(29700), TRUSDX_MODES, -1, -1, RIG_VFO_A},
        RIG_FRNG_END,
    },  /*!< Receive frequency range list for ITU region 2 */
    .tx_range_list2 = {
        {kHz(3500),  MHz(4) - 1, TRUSDX_MODES, 500, 5000, RIG_VFO_A},
        {kHz(5250),  kHz(5450),  TRUSDX_MODES, 500, 5000, RIG_VFO_A},
        {MHz(7),     kHz(7300),  TRUSDX_MODES, 500, 5000, RIG_VFO_A},
        {kHz(10100), kHz(10150), TRUSDX_MODES, 500, 5000, RIG_VFO_A},
        {MHz(14),    kHz(14350), TRUSDX_MODES, 500, 5000, RIG_VFO_A},
        {kHz(18068), kHz(18168), TRUSDX_MODES, 500, 5000, RIG_VFO_A},
        {MHz(21),    kHz(21450), TRUSDX_MODES, 500, 5000, RIG_VFO_A},
        {kHz(24890), kHz(24990), TRUSDX_MODES, 500, 5000, RIG_VFO_A},
        {MHz(28),    kHz(29700), TRUSDX_MODES, 500, 5000, RIG_VFO_A},
        RIG_FRNG_END,
    }, /*!< Transmit frequency range list for ITU region 2 */
    .tuning_steps = {
        {TRUSDX_MODES, 1},
        RIG_TS_END,
    },
    .priv = &trusdx_priv_caps,
    .rig_init = kenwood_init,
    .rig_open = trusdx_open,
    .rig_cleanup = kenwood_cleanup,
    .set_freq = trusdx_set_freq,
    .get_freq = trusdx_get_freq,
    .set_mode = trusdx_set_mode,
    .get_mode = trusdx_get_mode,
    .set_vfo = trusdx_set_vfo,
    .get_vfo = trusdx_get_vfo,
    .set_ptt = kenwood_set_ptt,
    .set_powerstat = trusdx_set_powerstat,
    .get_powerstat = kenwood_get_powerstat,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
