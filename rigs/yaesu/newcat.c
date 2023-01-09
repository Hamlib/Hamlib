/*
 * hamlib - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *              and the Hamlib Group (hamlib-developer at lists.sourceforge.net)
 *
 * newcat.c - (C) Nate Bargmann 2007 (n0nb at arrl.net)
 *            (C) Stephane Fillod 2008-2010
 *            (C) Terry Embry 2008-2010
 *            (C) David Fannin (kk6df at arrl.net)
 *
 * This shared library provides an API for communicating
 * via serial interface to any newer Yaesu radio using the
 * "new" text CAT interface.
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
#include <math.h>

#include "hamlib/rig.h"
#include "iofunc.h"
#include "misc.h"
#include "cal.h"
#include "newcat.h"

/* global variables */
static const char cat_term = ';';             /* Yaesu command terminator */
// static const char cat_unknown_cmd[] = "?;";   /* Yaesu ? */

/* Internal Backup and Restore VFO Memory Channels */
#define NC_MEM_CHANNEL_NONE  2012
#define NC_MEM_CHANNEL_VFO_A 2013
#define NC_MEM_CHANNEL_VFO_B 2014

/* ID 0310 == 310, Must drop leading zero */
typedef enum nc_rigid_e
{
    NC_RIGID_NONE            = 0,
    NC_RIGID_FT450           = 241,
    NC_RIGID_FT450D          = 244,
    NC_RIGID_FT950           = 310,
    NC_RIGID_FT891           = 135,
    NC_RIGID_FT991           = 570,
    NC_RIGID_FT991A          = 670,
    NC_RIGID_FT2000          = 251,
    NC_RIGID_FT2000D         = 252,
    NC_RIGID_FTDX1200        = 583,
    NC_RIGID_FTDX9000D       = 101,
    NC_RIGID_FTDX9000Contest = 102,
    NC_RIGID_FTDX9000MP      = 103,
    NC_RIGID_FTDX5000        = 362,
    NC_RIGID_FTDX3000        = 460,
    NC_RIGID_FTDX3000DM      = 462, // an undocumented FT-DX3000DM 50W rig
    NC_RIGID_FTDX101D        = 681,
    NC_RIGID_FTDX101MP       = 682,
    NC_RIGID_FT710           = 800,
} nc_rigid_t;


/*
 * The following table defines which commands are valid for any given
 * rig supporting the "new" CAT interface.
 */

typedef struct _yaesu_newcat_commands
{
    char                *command;
    ncboolean           ft450;
    ncboolean           ft950;
    ncboolean           ft891;
    ncboolean           ft991;
    ncboolean           ft2000;
    ncboolean           ft9000;
    ncboolean           ft5000;
    ncboolean           ft1200;
    ncboolean           ft3000;
    ncboolean           ft101d;
    ncboolean           ftdx10;
    ncboolean           ft101mp;
    ncboolean           ft710;
} yaesu_newcat_commands_t;

/**
 * Yaesu FT-991 S-meter scale, default for new Yaesu rigs.
 * Determined by data from W6HN -- seems to be pretty linear
 *
 * SMeter, rig answer, %fullscale
 * S0    SM0000 0
 * S2    SM0026 10
 * S4    SM0051 20
 * S6    SM0081 30
 * S7.5  SM0105 40
 * S9    SM0130 50
 * +12db SM0157 60
 * +25db SM0186 70
 * +35db SM0203 80
 * +50db SM0237 90
 * +60db SM0255 100
 *
 * 114dB range over 0-255 referenced to S0 of -54dB
 */
const cal_table_t yaesu_default_str_cal =
{
    11,
    {
        { 0, -54, }, // S0
        { 26, -42, }, // S2
        { 51, -30, }, // S4
        { 81, -18, }, // S6
        { 105, -9, }, // S7.5
        { 130, 0, }, // S9
        { 157, 12, }, // S9+12dB
        { 186, 25, }, // S9+25dB
        { 203, 35, }, // S9+35dB
        { 237, 50, }, // S9+50dB
        { 255, 60, }, // S9+60dB
    }
};

/**
 * First cut at generic Yaesu table, need more points probably
 * based on testing by Adam M7OTP on FT-991
 */
const cal_table_float_t yaesu_default_swr_cal =
{
    5,
    {
        {12, 1.0f},
        {39, 1.35f},
        {65, 1.5f},
        {89, 2.0f},
        {242, 5.0f}
    }
};

// TODO: Provide sane defaults
const cal_table_float_t yaesu_default_alc_cal =
{
    2,
    {
        {0, 0.0f},
        {64, 1.0f}
    }
};

const cal_table_float_t yaesu_default_comp_meter_cal =
{
    9,
    {
        { 0,   0.0f  },
        { 40,  2.5f  },
        { 60,  5.0f  },
        { 85,  7.5f  },
        { 135, 10.0f },
        { 150, 12.5f },
        { 175, 15.0f },
        { 195, 17.5f },
        { 220, 20.0f }
    }
};

// TODO: Provide sane defaults
const cal_table_float_t yaesu_default_rfpower_meter_cal =
{
    3,
    {
        {0, 0.0f},
        {148, 50.0f},
        {255, 100.0f},
    }
};

const cal_table_float_t yaesu_default_vd_meter_cal =
{
    3,
    {
        {0, 0.0f},
        {196, 13.8f},
        {255, 17.95f},
    }
};

const cal_table_float_t yaesu_default_id_meter_cal =
{
    3,
    {
        {0, 0.0f},
        {100, 10.0f},
        {255, 25.5f},
    }
};

// Easy reference to rig model -- it is set in newcat_valid_command
static ncboolean is_ft450;
static ncboolean is_ft710;
static ncboolean is_ft891;
static ncboolean is_ft950;
static ncboolean is_ft991;
static ncboolean is_ft2000;
static ncboolean is_ftdx9000;
static ncboolean is_ftdx5000;
static ncboolean is_ftdx1200;
static ncboolean is_ftdx3000;
static ncboolean is_ftdx3000dm;
static ncboolean is_ftdx101d;
static ncboolean is_ftdx101mp;
static ncboolean is_ftdx10;

/*
 * Even thought this table does make a handy reference, it could be depreciated as it is not really needed.
 * All of the CAT commands used in the newcat interface are available on the FT-950, FT-2000, FT-5000, and FT-9000.
 * There are 5 CAT commands used in the newcat interface that are not available on the FT-450.
 * Thesec CAT commands are XT -TX Clarifier ON/OFF, AN - Antenna select, PL - Speech Proc Level,
 * PR - Speech Proc ON/OFF, and BC - Auto Notch filter ON/OFF.
 * The FT-450 returns -RIG_ENVAIL for these unavailable CAT commands.
 *
 * NOTE: The following table must be in alphabetical order by the
 * command.  This is because it is searched using a binary search
 * to determine whether or not a command is valid for a given rig.
 *
 * The list of supported commands is obtained from the rig's operator's
 * or CAT programming manual.
 *
 */
static const yaesu_newcat_commands_t valid_commands[] =
{
    /* Command FT-450 FT-950 FT-891 FT-991  FT-2000 FT-9000 FT-5000 FT-1200 FT-3000 FTDX101D FTDX10 FTDX101MP FT710 */
    {"AB",     FALSE, TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"AC",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"AG",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"AI",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"AM",     FALSE, TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"AN",     FALSE, TRUE,  FALSE, FALSE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    FALSE, TRUE,     FALSE },
    {"AO",     FALSE, FALSE, FALSE, FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  TRUE,    TRUE,  TRUE,     TRUE  },
    {"BA",     FALSE, FALSE, TRUE,  TRUE,   FALSE,  FALSE,  FALSE,   TRUE,   TRUE,  TRUE,    TRUE,  TRUE,     TRUE  },
    {"BC",     FALSE, TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"BD",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"BI",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"BM",     FALSE, FALSE, FALSE, FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  TRUE,    TRUE,  TRUE,     TRUE  },
    {"BP",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"BS",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"BU",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"BY",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"CH",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"CN",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"CO",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"CS",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"CT",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"DA",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"DN",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"DP",     FALSE, TRUE,  FALSE, FALSE,  TRUE,   TRUE,   TRUE,   FALSE,  FALSE,  FALSE,   FALSE, FALSE,    FALSE },
    {"DS",     TRUE,  FALSE, FALSE, FALSE,  TRUE,   TRUE,   TRUE,   FALSE,  FALSE,  FALSE,   FALSE, FALSE,    FALSE },
    {"DT",     FALSE, FALSE, TRUE,  TRUE,   FALSE,  FALSE,  FALSE,  TRUE,   FALSE,  TRUE,    TRUE,  TRUE,     TRUE  },
    {"ED",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"EK",     FALSE, TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   FALSE,  TRUE,   TRUE,   FALSE,   FALSE, TRUE,     FALSE },
    {"EM",     FALSE, FALSE, FALSE, FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  TRUE,    TRUE,  FALSE,    TRUE  },
    {"EN",     FALSE, FALSE, FALSE, FALSE,  FALSE,  FALSE,  FALSE,   TRUE,  TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"EU",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"EX",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"FA",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"FB",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"FK",     FALSE, TRUE,  FALSE, FALSE,  TRUE,   TRUE,   FALSE,  FALSE,  FALSE,  FALSE,   FALSE, FALSE,    FALSE },
    {"FN",     FALSE, FALSE, FALSE, FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  TRUE,    TRUE,  TRUE,     TRUE  },
    {"FR",     FALSE, TRUE,  FALSE, FALSE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"FS",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"FT",     TRUE,  TRUE,  FALSE, TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"GT",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"ID",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"IF",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"IS",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"KM",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"KP",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"KR",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"KS",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"KY",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"LK",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"LM",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"MA",     FALSE, TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"MB",     FALSE, FALSE, FALSE, FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  TRUE,    TRUE,  TRUE,     TRUE  },
    {"MC",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"MD",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"MG",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"MK",     TRUE,  TRUE,  FALSE, FALSE,  TRUE,   TRUE,   TRUE,   FALSE,  FALSE,  FALSE,   FALSE, FALSE,    FALSE },
    {"ML",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"MR",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"MS",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"MT",     FALSE, FALSE, FALSE, FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  TRUE,    TRUE,  TRUE,     TRUE  },
    {"MW",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"MX",     FALSE, TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"NA",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   FALSE,  TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"NB",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"NL",     FALSE, TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"NR",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"OI",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"OS",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"PA",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"PB",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"PC",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"PL",     FALSE, TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"PR",     FALSE, TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"PS",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"QI",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"QR",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"QS",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"RA",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"RC",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"RD",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"RF",     FALSE, TRUE,  FALSE, FALSE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"RG",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"RI",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"RL",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"RM",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"RO",     FALSE, TRUE,  FALSE, FALSE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   FALSE,   FALSE, FALSE,    FALSE },
    {"RP",     TRUE,  FALSE, FALSE, FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  FALSE,   FALSE, FALSE,    FALSE },
    {"RS",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"RT",     TRUE,  TRUE,  FALSE, TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"RU",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"SC",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"SD",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"SF",     FALSE, TRUE,  FALSE, FALSE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"SH",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"SM",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"SQ",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"SS",     FALSE, FALSE, FALSE, FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  TRUE,    TRUE,  TRUE,     TRUE  },
    // ST command has two meanings Step or Split Status
    // If new rig is added that has ST ensure it means Split
    // Otherwise modify newcat_get_tx_vfo
    {"ST",     TRUE,  FALSE, FALSE, FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  TRUE,    TRUE,  TRUE,     TRUE  },
    {"SV",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"SY",     FALSE, FALSE, FALSE, FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  TRUE,   FALSE,  TRUE,     FALSE },
    {"TS",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"TX",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"UL",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"UP",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"VD",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"VF",     FALSE, TRUE,  FALSE, FALSE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   FALSE,  FALSE,  TRUE,     FALSE },
    {"VG",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"VM",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"VR",     TRUE,  FALSE, FALSE, FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  TRUE,     FALSE },
    {"VS",     TRUE,  TRUE,  FALSE, FALSE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"VT",     FALSE, FALSE, FALSE, FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  TRUE,   FALSE,  TRUE,     TRUE  },
    {"VV",     TRUE,  FALSE, FALSE, FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  TRUE,     FALSE },
    {"VX",     TRUE,  TRUE,  TRUE,  TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"XT",     FALSE, TRUE,  FALSE, TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,   TRUE,    TRUE,  TRUE,     TRUE  },
    {"ZI",     FALSE, FALSE, TRUE,  TRUE,   FALSE,  FALSE,  FALSE,  FALSE,  FALSE,  TRUE,    TRUE,  TRUE,     TRUE  },
} ;

int valid_commands_count = sizeof(valid_commands) / sizeof(
                               yaesu_newcat_commands_t);

/*
 * configuration Tokens
 *
 */

#define TOK_FAST_SET_CMD TOKEN_BACKEND(1)

const struct confparams newcat_cfg_params[] =
{
    {
        TOK_FAST_SET_CMD, "fast_commands_token", "High throughput of commands", "Enabled high throughput of >200 messages/sec by not waiting for ACK/NAK of messages", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } }
    },
    { RIG_CONF_END, NULL, }
};

/* NewCAT Internal Functions */
static ncboolean newcat_is_rig(RIG *rig, rig_model_t model);

static int newcat_set_vfo_from_alias(RIG *rig, vfo_t *vfo);
static int newcat_scale_float(int scale, float fval);
static int newcat_get_rx_bandwidth(RIG *rig, vfo_t vfo, rmode_t mode,
                                   pbwidth_t *width);
static int newcat_set_rx_bandwidth(RIG *rig, vfo_t vfo, rmode_t mode,
                                   pbwidth_t width);
static int newcat_set_narrow(RIG *rig, vfo_t vfo, ncboolean narrow);
static int newcat_get_narrow(RIG *rig, vfo_t vfo, ncboolean *narrow);
static int newcat_set_faststep(RIG *rig, ncboolean fast_step);
static int newcat_get_faststep(RIG *rig, ncboolean *fast_step);
static int newcat_get_rigid(RIG *rig);
static int newcat_get_vfo_mode(RIG *rig, vfo_t vfo, rmode_t *vfo_mode);
static int newcat_vfomem_toggle(RIG *rig);
static int set_roofing_filter(RIG *rig, vfo_t vfo, int index);
static int set_roofing_filter_for_width(RIG *rig, vfo_t vfo, int width);
static int get_roofing_filter(RIG *rig, vfo_t vfo,
                              struct newcat_roofing_filter **roofing_filter);
static int newcat_set_apf_frequency(RIG *rig, vfo_t vfo, int freq);
static int newcat_get_apf_frequency(RIG *rig, vfo_t vfo, int *freq);
static int newcat_set_apf_width(RIG *rig, vfo_t vfo, int choice);
static int newcat_get_apf_width(RIG *rig, vfo_t vfo, int *choice);
static int newcat_set_contour(RIG *rig, vfo_t vfo, int status);
static int newcat_get_contour(RIG *rig, vfo_t vfo, int *status);
static int newcat_set_contour_frequency(RIG *rig, vfo_t vfo, int freq);
static int newcat_get_contour_frequency(RIG *rig, vfo_t vfo, int *freq);
static int newcat_set_contour_level(RIG *rig, vfo_t vfo, int level);
static int newcat_get_contour_level(RIG *rig, vfo_t vfo, int *level);
static int newcat_set_contour_width(RIG *rig, vfo_t vfo, int width);
static int newcat_get_contour_width(RIG *rig, vfo_t vfo, int *width);
static ncboolean newcat_valid_command(RIG *rig, char const *const command);

/*
 * The BS command needs to know what band we're on so we can restore band info
 * So this converts freq to band index
 */
static int newcat_band_index(freq_t freq)
{
    int band = 11; // general

    // restrict band memory recall to ITU 1,2,3 band ranges
    // using < instead of <= for the moment
    // does anybody work LSB or RTTYR at the upper band edge?
    // what about band 13 -- what is it?
    if (freq >= MHz(420) && freq < MHz(470)) { band = 16; }
    else if (freq >= MHz(144) && freq < MHz(148)) { band = 15; }
    // band 14 is RX only
    // override band 15 with 14 if needed
    else if (freq >= MHz(118) && freq < MHz(164)) { band = 14; }
    else if (freq >= MHz(70) && freq < MHz(70.5)) { band = 17; }
    else if (freq >= MHz(50) && freq < MHz(55)) { band = 10; }
    else if (freq >= MHz(28) && freq < MHz(29.7)) { band = 9; }
    else if (freq >= MHz(24.890) && freq < MHz(24.990)) { band = 8; }
    else if (freq >= MHz(21) && freq < MHz(21.45)) { band = 7; }
    else if (freq >= MHz(18) && freq < MHz(18.168)) { band = 6; }
    else if (freq >= MHz(14) && freq < MHz(14.35)) { band = 5; }
    else if (freq >= MHz(10) && freq < MHz(10.15)) { band = 4; }
    else if (freq >= MHz(7) && freq < MHz(7.3)) { band = 3; }
    else if (freq >= MHz(5.3515) && freq < MHz(5.3665)) { band = 2; }
    else if (freq >= MHz(3.5) && freq < MHz(4)) { band = 1; }
    else if (freq >= MHz(1.8) && freq < MHz(2)) { band = 0; }
    else if (freq >= MHz(0.5) && freq < MHz(1.705)) { band = 12; } // MW Medium Wave

    rig_debug(RIG_DEBUG_TRACE, "%s: freq=%g, band=%d\n", __func__, freq, band);
    return (band);
}

/*
 * ************************************
 *
 * Hamlib API functions
 *
 * ************************************
 */

/*
 * rig_init
 *
 */

int newcat_init(RIG *rig)
{
    struct newcat_priv_data *priv;

    ENTERFUNC;

    rig->state.priv = (struct newcat_priv_data *) calloc(1,
                      sizeof(struct newcat_priv_data));

    if (!rig->state.priv)                                  /* whoops! memory shortage! */
    {
        RETURNFUNC(-RIG_ENOMEM);
    }

    priv = rig->state.priv;

    //    priv->current_vfo =  RIG_VFO_MAIN;          /* default to whatever */
//    priv->current_vfo = RIG_VFO_A;

    priv->rig_id = NC_RIGID_NONE;
    priv->current_mem = NC_MEM_CHANNEL_NONE;
    priv->fast_set_commands = FALSE;

    /*
     * Determine the type of rig from the model number.  Note it is
     * possible for several model variants to exist; i.e., all the
     * FT-9000 variants.
     */

    is_ft450 = newcat_is_rig(rig, RIG_MODEL_FT450);
    is_ft891 = newcat_is_rig(rig, RIG_MODEL_FT891);
    is_ft950 = newcat_is_rig(rig, RIG_MODEL_FT950);
    is_ft991 = newcat_is_rig(rig, RIG_MODEL_FT991);
    is_ft2000 = newcat_is_rig(rig, RIG_MODEL_FT2000);
    is_ftdx9000 = newcat_is_rig(rig, RIG_MODEL_FT9000);
    is_ftdx5000 = newcat_is_rig(rig, RIG_MODEL_FTDX5000);
    is_ftdx1200 = newcat_is_rig(rig, RIG_MODEL_FTDX1200);
    is_ftdx3000 = newcat_is_rig(rig, RIG_MODEL_FTDX3000);
    is_ftdx3000dm = FALSE; // Detected dynamically
    is_ftdx101d = newcat_is_rig(rig, RIG_MODEL_FTDX101D);
    is_ftdx101mp = newcat_is_rig(rig, RIG_MODEL_FTDX101MP);
    is_ftdx10 = newcat_is_rig(rig, RIG_MODEL_FTDX10);
    is_ft710 = newcat_is_rig(rig, RIG_MODEL_FT710);

    RETURNFUNC(RIG_OK);
}


/*
 * rig_cleanup
 *
 * the serial port is closed by the frontend
 *
 */

int newcat_cleanup(RIG *rig)
{

    ENTERFUNC;

    if (rig->state.priv)
    {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    RETURNFUNC(RIG_OK);
}


/*
 * rig_open
 *
 * New CAT does not support pacing
 *
 */

int newcat_open(RIG *rig)
{
    struct newcat_priv_data *priv = rig->state.priv;
    struct rig_state *rig_s = &rig->state;
    const char *handshake[3] = {"None", "Xon/Xoff", "Hardware"};
    int err;
    int set_only = 0;

    ENTERFUNC;

    rig_debug(RIG_DEBUG_TRACE, "%s: Rig=%s, version=%s\n", __func__,
              rig->caps->model_name, rig->caps->version);
    rig_debug(RIG_DEBUG_TRACE, "%s: write_delay = %i msec\n",
              __func__, rig_s->rigport.write_delay);

    rig_debug(RIG_DEBUG_TRACE, "%s: post_write_delay = %i msec\n",
              __func__, rig_s->rigport.post_write_delay);

    rig_debug(RIG_DEBUG_TRACE, "%s: serial_handshake = %s \n",
              __func__, handshake[rig->caps->serial_handshake]);

    /* Ensure rig is powered on */
    if (priv->poweron == 0 && rig_s->auto_power_on)
    {
        rig_set_powerstat(rig, 1);
        priv->poweron = 1;
    }

    priv->question_mark_response_means_rejected = 0;

    /* get current AI state so it can be restored */
    priv->trn_state = -1;

    // for this sequence we will shorten the timeout so we can detect rig is powered off faster
    int timeout = rig->state.rigport.timeout;
    rig->state.rigport.timeout = 100;
    newcat_get_trn(rig, &priv->trn_state);  /* ignore errors */

    /* Currently we cannot cope with AI mode so turn it off in case
       last client left it on */
    if (priv->trn_state > 0)
    {
        newcat_set_trn(rig, RIG_TRN_OFF);
    } /* ignore status in case it's not supported */

    /* Initialize rig_id in case any subsequent commands need it */
    (void)newcat_get_rigid(rig);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: rig_id=%d\n", __func__, priv->rig_id);
    rig->state.rigport.timeout = timeout;

    // some rigs have a CAT TOT timeout that defaults to 10ms
    // so we'll increase CAT timeout to 100ms
    // 10ms seemed problematic on some rigs/computers
    if (priv->rig_id == NC_RIGID_FT2000
            || priv->rig_id == NC_RIGID_FT2000D
            || priv->rig_id == NC_RIGID_FT891
            || priv->rig_id == NC_RIGID_FT991
            || priv->rig_id == NC_RIGID_FT991A
            || priv->rig_id == NC_RIGID_FT950
            || priv->rig_id == NC_RIGID_FTDX3000
            || priv->rig_id == NC_RIGID_FTDX3000DM)
    {
        char *cmd = "EX0291;EX029;"; // FT2000/D

        if (priv->rig_id == NC_RIGID_FT950 || rig->caps->rig_model == RIG_MODEL_FT950) { cmd = "EX0271;EX027;"; }
        else if (priv->rig_id == NC_RIGID_FT891
                 || rig->caps->rig_model == RIG_MODEL_FT891) { cmd = "EX05071;EX0507;"; }
        else if (priv->rig_id == NC_RIGID_FT991
                 || rig->caps->rig_model == RIG_MODEL_FT991) { cmd = "EX0321;EX032;"; }
        else if (priv->rig_id == NC_RIGID_FT991A
                 || rig->caps->rig_model == RIG_MODEL_FT991) { cmd = "EX0321;EX032;"; }
        else if (priv->rig_id == NC_RIGID_FTDX3000
                 || rig->caps->rig_model == RIG_MODEL_FTDX3000) { cmd = "EX0391;"; set_only = 1; }
        else if (priv->rig_id == NC_RIGID_FTDX3000DM
                 || rig->caps->rig_model == RIG_MODEL_FTDX3000) { cmd = "EX0391;"; set_only = 1; }
        else if (priv->rig_id == NC_RIGID_FTDX5000
                 || rig->caps->rig_model == RIG_MODEL_FTDX5000) { cmd = "EX0331;EX033"; }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s", cmd);

        if (set_only)
        {
            err = newcat_set_cmd(rig);
        }
        else
        {
            err = newcat_get_cmd(rig);
        }

        if (err != RIG_OK)
        {
            RETURNFUNC(err);
        }
    }

    if (priv->rig_id == NC_RIGID_FTDX3000 || priv->rig_id == NC_RIGID_FTDX3000DM)
    {
        rig->state.disable_yaesu_bandselect = 1;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: disabling FTDX3000 band select\n", __func__);
    }

    if (is_ftdx5000)
    {
        // Remember EX103 status
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX103;");
        rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);

        if (set_only)
        {
            err = newcat_set_cmd(rig);
        }
        else
        {
            err = newcat_get_cmd(rig);
        }

        if (err != RIG_OK)
        {
            RETURNFUNC(err);
        }

        if (priv->ret_data[6] == ';') { priv->front_rear_status = priv->ret_data[5]; }
    }

    RETURNFUNC(RIG_OK);
}


/*
 * rig_close
 *
 */

int newcat_close(RIG *rig)
{

    struct newcat_priv_data *priv = rig->state.priv;
    struct rig_state *rig_s = &rig->state;

    ENTERFUNC;

    if (!no_restore_ai && priv->trn_state >= 0)
    {
        /* restore AI state */
        newcat_set_trn(rig, priv->trn_state); /* ignore status in
                                                   case it's not
                                                   supported */
    }

    if (priv->poweron != 0 && rig_s->auto_power_off)
    {
        rig_set_powerstat(rig, 0);
        priv->poweron = 0;
    }

    if (is_ftdx5000)
    {
        // Restore EX103 status
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX103%c;",
                 priv->front_rear_status);
        rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);
        newcat_set_cmd(rig); // don't care about the return
    }

    RETURNFUNC(RIG_OK);
}


/*
 * rig_set_config
 *
 * Set Configuration Token for Yaesu Radios
 */

int newcat_set_conf(RIG *rig, token_t token, const char *val)
{
    int ret = RIG_OK;
    struct newcat_priv_data *priv;

    ENTERFUNC;

    priv = (struct newcat_priv_data *)rig->state.priv;

    if (priv == NULL)
    {
        RETURNFUNC(-RIG_EINTERNAL);
    }

    switch (token)
    {
        char *end;
        long value;

    case TOK_FAST_SET_CMD: ;
        //using strtol because atoi can lead to undefined behaviour
        value = strtol(val, &end, 10);

        if (end == val)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        if ((value == 0) || (value == 1))
        {
            priv->fast_set_commands = (int)value;
        }
        else
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        break;

    default:
        ret = -RIG_EINVAL;
    }

    RETURNFUNC(ret);
}


/*
 * rig_get_config
 *
 * Get Configuration Token for Yaesu Radios
 */

int newcat_get_conf2(RIG *rig, token_t token, char *val, int val_len)
{
    int ret = RIG_OK;
    struct newcat_priv_data *priv;

    ENTERFUNC;

    priv = (struct newcat_priv_data *)rig->state.priv;

    if (priv == NULL)
    {
        RETURNFUNC(-RIG_EINTERNAL);
    }

    switch (token)
    {
    case TOK_FAST_SET_CMD:
        if (sizeof(val) < 2)
        {
            RETURNFUNC(-RIG_ENOMEM);
        }

        SNPRINTF(val, val_len, "%d", priv->fast_set_commands);
        break;

    default:
        ret = -RIG_EINVAL;
    }

    RETURNFUNC(ret);
}

static int freq_60m[] = { 5332000, 5348000, 5358500, 5373000, 5405000 };

/* returns 0 if no exeption or 1 if rig needs special handling */
int newcat_60m_exception(RIG *rig, freq_t freq, mode_t mode)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    int channel = -1;
    int i;
    rmode_t tmode;
    pbwidth_t twidth;

    if (!(freq > 5.2 && freq < 5.5)) // we're not on 60M
    {
        return 0;
    }

    if (mode != RIG_MODE_CW && mode != RIG_MODE_USB && mode != RIG_MODE_PKTUSB
            && mode != RIG_MODE_RTTYR)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: only USB, PKTUSB, RTTYR, and CW allowed for 60M operations\n", __func__);
        return -RIG_EINVAL;
    }

    if (!is_ftdx10 && !is_ft710 && !is_ftdx101d && !is_ftdx101mp) { return 0; }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: 60M exception ignoring freq/mode commands\n",
              __func__);
    // If US mode we need to use memory channels
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX0301%c", cat_term);

    if ((err = newcat_get_cmd(rig)) != RIG_OK)
    {
        RETURNFUNC2(err);
    }

    // 01 is the only exception so far -- others may be like UK and have full control too
    if (strncmp(&priv->ret_data[6], "01", 2) != 0) { return 0; } // no exception

    // so now we should have a rig that has fixed memory channels 501-510 in USB/CW-U mode

    // toggle vfo mode if we need to

    rig_debug(RIG_DEBUG_VERBOSE, "%s: 60M exception ignoring freq/mode commands\n",
              __func__);

    rig_get_mode(rig, RIG_VFO_A, &tmode, &twidth);
    if (tmode != RIG_VFO_MEM)
    {
        err = newcat_vfomem_toggle(rig);
        if (err != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Error toggling VFO_MEM\n", __func__);
            return -err;
        }
    }
    // find the nearest slot below what is requested
    for (i = 0; i < 5; ++i)
    {
        if ((long)freq == freq_60m[i]) { channel = i; }
    }

    if (channel < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: 60M allowed frequencies are 5.332, 5.348, 5.3585, 5.373,5.405, got %g\n",
                  __func__, freq / 1000);
        return -RIG_EINVAL;
    }

    if (mode == RIG_MODE_CW) { channel += 5; }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MC%3d%c", channel + 501,
             cat_term);

    if ((err = newcat_set_cmd(rig)) != RIG_OK)
    {
        RETURNFUNC2(err);
    }

    return 1;
}

/*
 * newcat_set_freq
 *
 * Set frequency for a given VFO
 * RIG_TARGETABLE_VFO
 * Does not SET priv->current_vfo
 *
 */
int newcat_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char c;
    char target_vfo;
    int err;
    const struct rig_caps *caps;
    struct newcat_priv_data *priv;
    int special_60m = 0;
    rmode_t tmode;
    pbwidth_t twidth;

    ENTERFUNC;

    if (newcat_60m_exception(rig, freq, rig->state.cache.modeMainA))
    {
        // we don't try to set freq on 60m for some rigs since we must be in memory mode
        // and we can't run split mode on 60M memory mode either
        if (rig->state.cache.split == RIG_SPLIT_ON)
        {
            rig_set_split_vfo(rig, RIG_VFO_A, RIG_VFO_A, RIG_SPLIT_OFF);
        }

        RETURNFUNC(RIG_OK);
    } // we don't set freq in this case

    if (!newcat_valid_command(rig, "FA"))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (!newcat_valid_command(rig, "FB"))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    priv = (struct newcat_priv_data *)rig->state.priv;
    caps = rig->caps;

    rig_get_mode(rig, RIG_VFO_A, &tmode, &twidth);
    if (tmode == RIG_VFO_MEM)
    {
        // then we need to toggle back to VFO mode
        newcat_vfomem_toggle(rig);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = %s\n", __func__, rig_strvfo(vfo));
//    rig_debug(RIG_DEBUG_TRACE, "%s: translated vfo = %s\n", __func__, rig_strvfo(tvfo));
    rig_debug(RIG_DEBUG_TRACE, "%s: passed freq = %"PRIfreq" Hz\n", __func__, freq);

    err = newcat_set_vfo_from_alias(rig, &vfo);

    if (err < 0)
    {
        ERRMSG(err, "newcat_set_vfo_from_alias");
        RETURNFUNC(err);
    }

    /* vfo should now be modified to a valid VFO constant. */
    /* DX3000/DX5000/450 can only do VFO_MEM on 60M */
    /* So we will not change freq in that case */
    // did have FTDX3000 as not capable of 60M set_freq but as of 2021-01-21 it works
    // special_60m = newcat_is_rig(rig, RIG_MODEL_FTDX3000);
    /* duplicate the following line to add more rigs */
    // disabled to check 2019 firmware on FTDX5000 and FT450 behavior
    //special_60m = newcat_is_rig(rig, RIG_MODEL_FTDX5000);
    //special_60m |= newcat_is_rig(rig, RIG_MODEL_FT450);
    rig_debug(RIG_DEBUG_TRACE,
              "%s: special_60m=%d, 60m freq=%d, is_ftdx3000=%d,is_ftdx3000dm=%d\n",
              __func__, special_60m, freq >= 5300000
              && freq <= 5410000, is_ftdx3000, is_ftdx3000dm);

    if (special_60m && (freq >= 5300000 && freq <= 5410000))
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: 60M VFO_MEM exception, no freq change done\n",
                  __func__);
        RETURNFUNC(RIG_OK); /* make it look like we changed */
    }

    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_MAIN:
    case RIG_VFO_MEM:
        c = 'A';
        break;

    case RIG_VFO_B:
    case RIG_VFO_SUB:
        c = 'B';
        break;

    default:
        RETURNFUNC(-RIG_ENIMPL);             /* Only VFO_A or VFO_B are valid */
    }

    target_vfo = 'A' == c ? '0' : '1';

    // some rigs like FTDX101D cannot change non-TX vfo freq
    // but they can change the TX vfo
    if ((is_ftdx101d || is_ftdx101mp) && rig->state.cache.ptt == RIG_PTT_ON)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: ftdx101 check vfo OK, vfo=%s, tx_vfo=%s\n",
                  __func__, rig_strvfo(vfo), rig_strvfo(rig->state.tx_vfo));

        // when in split we can change VFOB but not VFOA
        if (rig->state.cache.split == RIG_SPLIT_ON && target_vfo == '0') { return -RIG_ENTARGET; }

        // when not in split we can't change VFOA at all
        if (rig->state.cache.split == RIG_SPLIT_OFF && target_vfo == '0') { return -RIG_ENTARGET; }

        if (vfo != rig->state.tx_vfo) { return -RIG_ENTARGET; }
    }

    if (is_ftdx3000 || is_ftdx3000dm || is_ftdx5000 || is_ftdx1200)
    {
        // we have a few rigs that can't set freq while PTT_ON
        // so we'll try a few times to see if we just need to wait a bit
        // 3 retries should be about 400ms -- hopefully more than enough
        ptt_t ptt;
        int retry = 3;

        do
        {
            if (RIG_OK != (err = newcat_get_ptt(rig, vfo, &ptt)))
            {
                ERRMSG(err, "newcat_set_cmd failed");
                RETURNFUNC(err);
            }

            if (ptt == RIG_PTT_ON)
            {
                rig_debug(RIG_DEBUG_WARN, "%s: ptt still on...retry#%d\n", __func__, retry);
                hl_usleep(100 * 1000); // 100ms pause if ptt still on
            }
        }
        while (err == RIG_OK && ptt == RIG_PTT_ON && retry-- > 0);

        if (ptt) { return -RIG_ENTARGET; }
    }

    if (RIG_MODEL_FT450 == caps->rig_model)
    {
        /* The FT450 only accepts F[A|B]nnnnnnnn; commands for the
           current VFO so we must use the VS[0|1]; command to check
           and select the correct VFO before setting the frequency
        */
        // Plus we can't do the VFO swap if transmitting
        if (target_vfo == '1' && rig->state.cache.ptt == RIG_PTT_ON) { RETURNFUNC(-RIG_ENTARGET); }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VS%c", cat_term);

        if (RIG_OK != (err = newcat_get_cmd(rig)))
        {
            ERRMSG(err, "newcat_get_cmd");
            RETURNFUNC(err);
        }

        if (priv->ret_data[2] != target_vfo)
        {
            HAMLIB_TRACE;
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VS%c%c", target_vfo, cat_term);
            rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);

            if (RIG_OK != (err = newcat_set_cmd(rig)))
            {
                ERRMSG(err, "newcat_set_cmd failed");
                RETURNFUNC(err);
            }
        }
    }

    // W1HKJ
    // creation of the priv structure guarantees that the string can be NEWCAT_DATA_LEN
    // bytes in length.  the SNPRINTF will only allow (NEWCAT_DATA_LEN - 1) chars
    // followed by the NULL terminator.
    // CAT command string for setting frequency requires that 8 digits be sent
    // including leading fill zeros
    // Call this after open to set width_frequency for later use
    if (priv->width_frequency == 0)
    {
        rmode_t vfo_mode;
        newcat_get_vfo_mode(rig, vfo, &vfo_mode);
    }

    //
    // Restore band memory if we can and band is changing -- we do it before we set the frequency
    // And only when not in split mode (note this check has been removed for testing)
    int changing;

    rig_debug(RIG_DEBUG_TRACE, "%s(%d)%s: rig->state.current_vfo=%s\n", __FILE__,
              __LINE__, __func__, rig_strvfo(rig->state.current_vfo));

    CACHE_RESET;

    if (vfo == RIG_VFO_A || vfo == RIG_VFO_MAIN)
    {
        freq_t freqA;
        rig_get_freq(rig, RIG_VFO_A, &freqA);
        rig_debug(RIG_DEBUG_TRACE, "%s(%d)%s: checking VFOA for band change \n",
                  __FILE__, __LINE__, __func__);

        changing = newcat_band_index(freq) != newcat_band_index(freqA);
        rig_debug(RIG_DEBUG_TRACE, "%s: VFO_A band changing=%d\n", __func__, changing);
    }
    else
    {
        freq_t freqB;
        rig_get_freq(rig, RIG_VFO_B, &freqB);
        rig_debug(RIG_DEBUG_TRACE, "%s(%d)%s: checking VFOB for band change \n",
                  __FILE__, __LINE__, __func__);

        changing = newcat_band_index(freq) != newcat_band_index(freqB);
        rig_debug(RIG_DEBUG_TRACE, "%s: VFO_B band changing=%d\n", __func__, changing);
    }

    if (newcat_valid_command(rig, "BS") && changing
            && !rig->state.disable_yaesu_bandselect
            // remove the split check here -- hopefully works OK
            //&& !rig->state.cache.split
            // seems some rigs are problematic
            // && !(is_ftdx3000 || is_ftdx3000dm)
            // some rigs can't do BS command on 60M
            // && !(is_ftdx3000 || is_ftdx3000dm && newcat_band_index(freq) == 2)
            && !(is_ft2000 && newcat_band_index(freq) == 2)
            && !(is_ftdx1200 && newcat_band_index(freq) == 2)
            && !is_ft891 // 891 does not remember bandwidth so don't do this
            && !is_ft991 // 991 does not behave well with bandstack changes
            && rig->caps->get_vfo != NULL
            && rig->caps->set_vfo != NULL) // gotta' have get_vfo too
    {
        HAMLIB_TRACE;

        if (rig->state.current_vfo != vfo)
        {
            int vfo1 = 1, vfo2 = 0;

            if (vfo  == RIG_VFO_A || vfo == RIG_VFO_MAIN)
            {
                vfo1 = 0;
                vfo2 = 1;
            }

            // we need to change vfos, BS, and change back
            if (!is_ft991 && !is_ft891 && newcat_valid_command(rig, "VS"))
            {
                SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VS%d;BS%02d%c",
                         vfo1, newcat_band_index(freq), cat_term);
            }
            else
            {
                SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BS%02d%c",
                         newcat_band_index(freq), cat_term);
            }


            if (RIG_OK != (err = newcat_set_cmd(rig)))
            {
                rig_debug(RIG_DEBUG_ERR, "%s: Unexpected error with BS command#1=%s\n",
                          __func__, rigerror(err));
            }

            hl_usleep(500 * 1000); // wait for BS to do it's thing and swap back

            if (newcat_valid_command(rig, "VS"))
            {
                SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VS%d;", vfo2);

                if (RIG_OK != (err = newcat_set_cmd(rig)))
                {
                    rig_debug(RIG_DEBUG_ERR, "%s: Unexpected error with BS command#3=%s\n",
                              __func__, rigerror(err));
                }
            }
        }
        else
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BS%02d%c",
                     newcat_band_index(freq), cat_term);

            if (RIG_OK != (err = newcat_set_cmd(rig)))
            {
                rig_debug(RIG_DEBUG_ERR, "%s: Unexpected error with BS command#2=%s\n",
                          __func__, rigerror(err));
            }

            hl_usleep(500 * 1000); // wait for BS to do it's thing
        }


#if 0 // disable for testing
        else
        {
            // Also need to do this for the other VFO on some Yaesu rigs
            // is redundant for rigs where band stack includes both vfos
            vfo_t vfotmp;
            freq_t freqtmp;
            err = rig_get_vfo(rig, &vfotmp);

            if (err != RIG_OK) { RETURNFUNC(err); }

            if (rig->state.vfo_list & RIG_VFO_MAIN)
            {
                err = rig_set_vfo(rig, vfotmp == RIG_VFO_MAIN ? RIG_VFO_SUB : RIG_VFO_MAIN);
            }
            else
            {
                err = rig_set_vfo(rig, vfotmp == RIG_VFO_A ? RIG_VFO_B : RIG_VFO_A);
            }

            if (err != RIG_OK) { RETURNFUNC(err); }

            rig_get_freq(rig, RIG_VFO_CURR, &freqtmp);

            if (err != RIG_OK) { RETURNFUNC(err); }

            // Cross band should work too
            // If the BS works on both VFOs then VFOB will have the band select answer
            // so now change needed
            // If the BS is by VFO then we'll need to do BS for the other VFO too
            if (newcat_band_index(freqtmp) != newcat_band_index(freq))
            {

                SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BS%02d%c",
                         newcat_band_index(freq), cat_term);

                if (RIG_OK != (err = newcat_set_cmd(rig)))
                {
                    rig_debug(RIG_DEBUG_ERR, "%s: Unexpected error with BS command#3=%s\n",
                              __func__,
                              rigerror(err));
                }
            }

            // switch back to the starting vfo
            if (rig->state.vfo_list & RIG_VFO_MAIN)
            {
                err = rig_set_vfo(rig, vfotmp == RIG_VFO_MAIN ? RIG_VFO_MAIN : RIG_VFO_SUB);
            }
            else
            {
                err = rig_set_vfo(rig, vfotmp == RIG_VFO_A ? RIG_VFO_A : RIG_VFO_B);
            }

            if (err != RIG_OK)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: rig_set_vfo failed: %s\n", __func__,
                          rigerror(err));
                RETURNFUNC(err);
            }

            // after band select re-read things -- may not have to change anything
            freq_t tmp_freqA, tmp_freqB;
            rmode_t tmp_mode;
            pbwidth_t tmp_width;

            rig_get_freq(rig, RIG_VFO_MAIN, &tmp_freqA);
            rig_get_freq(rig, RIG_VFO_SUB, &tmp_freqB);
            rig_get_mode(rig, RIG_VFO_MAIN, &tmp_mode, &tmp_width);
            rig_get_mode(rig, RIG_VFO_SUB, &tmp_mode, &tmp_width);

            if ((target_vfo == 0 && tmp_freqA == freq)
                    || (target_vfo == 1 && tmp_freqB == freq))
            {
                rig_debug(RIG_DEBUG_VERBOSE,
                          "%s: freq after band select already set to %"PRIfreq"\n", __func__, freq);
                RETURNFUNC(RIG_OK); // we're done then!!
            }
        }

#endif
        // after band select re-read things -- may not have to change anything
        // reading both VFOs is really only needed for rigs with just one VFO stack
        // but we read them all to ensure we cover both types
        freq_t tmp_freqA = 0, tmp_freqB = 0;
        rmode_t tmp_mode;
        pbwidth_t tmp_width;

        // we need to update some info that BS may have caused
        CACHE_RESET;

        if (vfo == RIG_VFO_A || vfo == RIG_VFO_MAIN)
        {
            rig_get_freq(rig, RIG_VFO_SUB, &tmp_freqA);
        }
        else
        {
            rig_get_freq(rig, RIG_VFO_MAIN, &tmp_freqB);
        }

        rig_get_mode(rig, RIG_VFO_MAIN, &tmp_mode, &tmp_width);
        rig_get_mode(rig, RIG_VFO_SUB, &tmp_mode, &tmp_width);

        if ((target_vfo == 0 && tmp_freqA == freq)
                || (target_vfo == 1 && tmp_freqB == freq))
        {
            rig_debug(RIG_DEBUG_VERBOSE,
                      "%s: freq after band select already set to %"PRIfreq"\n", __func__, freq);
            RETURNFUNC(RIG_OK); // we're done then!!
        }

        // just drop through
    }

    if (RIG_MODEL_FT450 == caps->rig_model)
    {
        if (c == 'B')
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VS1;F%c%0*"PRIll";VS0;", c,
                     priv->width_frequency, (int64_t)freq);
        }
        else
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "F%c%0*"PRIll";", c,
                     priv->width_frequency, (int64_t)freq);
        }
    }
    else
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "F%c%0*"PRIll";", c,
                 priv->width_frequency, (int64_t)freq);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s:%d cmd_str = %s\n", __func__, __LINE__,
              priv->cmd_str);

    if (RIG_OK != (err = newcat_set_cmd(rig)))
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s:%d command err = %d\n", __func__, __LINE__,
                  err);
        RETURNFUNC(err);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: band changing? old=%d, new=%d\n", __func__,
              newcat_band_index(freq), newcat_band_index(rig->state.current_freq));

    if (RIG_MODEL_FT450 == caps->rig_model && priv->ret_data[2] != target_vfo)
    {
        /* revert current VFO */
        rig_debug(RIG_DEBUG_TRACE, "%s:%d cmd_str = %s\n", __func__, __LINE__,
                  priv->ret_data);

        if (RIG_OK != (err = newcat_set_cmd(rig)))
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s:%d command err = %d\n", __func__, __LINE__,
                      err);
            RETURNFUNC(err);
        }
    }

    RETURNFUNC(RIG_OK);
}


/*
 * rig_get_freq
 *
 * Return Freq for a given VFO
 * RIG_TARGETABLE_FREQ
 * Does not SET priv->current_vfo
 *
 */
int newcat_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    char command[3];
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    char c;
    int err;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = %s\n", __func__, rig_strvfo(vfo));

    if (!newcat_valid_command(rig, "FA"))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (!newcat_valid_command(rig, "FB"))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    err = newcat_set_vfo_from_alias(rig, &vfo);

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_MAIN: // what about MAIN_A/MAIN_B?
        c = 'A';
        break;

    case RIG_VFO_B:
    case RIG_VFO_SUB: // what about SUB_A/SUB_B?
        c = 'B';
        break;

    case RIG_VFO_MEM:
        c = 'A';
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported vfo=%s\n", __func__, rig_strvfo(vfo));
        RETURNFUNC(-RIG_EINVAL);         /* sorry, unsupported VFO */
    }

    /* Build the command string */
    SNPRINTF(command, sizeof(command), "F%c", c);

    if (!newcat_valid_command(rig, command))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%c", command, cat_term);

    rig_debug(RIG_DEBUG_TRACE, "cmd_str = %s\n", priv->cmd_str);

    /* get freq */
    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        RETURNFUNC(err);
    }

    /* convert the read frequency string into freq_t and store in *freq */
    sscanf(priv->ret_data + 2, "%"SCNfreq, freq);

    rig_debug(RIG_DEBUG_TRACE,
              "%s: freq = %"PRIfreq" Hz for vfo %s\n", __func__, *freq, rig_strvfo(vfo));

    RETURNFUNC(RIG_OK);
}


int newcat_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct newcat_priv_data *priv;
    int err;
    rmode_t tmode;
    pbwidth_t twidth;
    split_t split_save = rig->state.cache.split;

    priv = (struct newcat_priv_data *)rig->state.priv;

    ENTERFUNC;

    if (newcat_60m_exception(rig, rig->state.cache.freqMainA, mode)) { RETURNFUNC(RIG_OK); } // we don't set mode in this case

    if (!newcat_valid_command(rig, "MD"))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }


    err = newcat_set_vfo_from_alias(rig, &vfo);

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    // if vfo is current the same don't do anything
    // we don't want to use cache in case the user is twiddling the rig
    if (vfo == RIG_VFO_B || vfo == RIG_VFO_SUB)
    {
        rig_get_mode(rig, vfo, &tmode, &twidth);

        if (mode == tmode && (twidth == width || width == RIG_PASSBAND_NOCHANGE))
        {
            RETURNFUNC(RIG_OK);
        }

        if (mode != tmode)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: mode changing from %s to %s\n", __func__,
                      rig_strrmode(mode), rig_strrmode(tmode));
        }

        if (width != twidth)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: width changing from %d to %d\n", __func__,
                      (int)width, (int)twidth);
        }
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MD0x%c", cat_term);

    priv->cmd_str[3] = newcat_modechar(mode);

    if (priv->cmd_str[3] == '0')
    {
        RETURNFUNC(-RIG_EINVAL);
    }


    /* FT9000 RIG_TARGETABLE_MODE (mode and width) */
    /* FT2000 mode only */
    if (rig->caps->targetable_vfo & RIG_TARGETABLE_MODE)
    {
        priv->cmd_str[2] = (RIG_VFO_B == vfo || RIG_VFO_SUB == vfo) ? '1' : '0';
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: generic mode = %s \n",
              __func__, rig_strrmode(mode));


    err = newcat_set_cmd(rig);

    if (err != RIG_OK)
    {
        RETURNFUNC(err);
    }

    if (vfo == RIG_VFO_A || vfo == RIG_VFO_MAIN)
    {
        rig->state.cache.modeMainA = mode;
    }
    else
    {
        rig->state.cache.modeMainB = mode;
    }

    if (RIG_PASSBAND_NOCHANGE == width) { RETURNFUNC(err); }

    if (RIG_PASSBAND_NORMAL == width)
    {
        width = rig_passband_normal(rig, mode);
    }

    /* Set width after mode has been set */
    err = newcat_set_rx_bandwidth(rig, vfo, mode, width);

    // some rigs if you set mode on VFOB it will turn off split
    // so if we started in split we query split and turn it back on if needed
    if (split_save)
    {
        split_t split;
        vfo_t tx_vfo;
        err = rig_get_split_vfo(rig, RIG_VFO_A, &split, &tx_vfo);

        // we'll just reset to split to what we want if we need to
        if (!split)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: turning split back on...buggy rig\n", __func__);
            err = rig_set_split_vfo(rig, RIG_VFO_A, split_save, RIG_VFO_B);
        }
    }

    RETURNFUNC(err);
}

int newcat_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    char c;
    int err;
    char main_sub_vfo = '0';

    ENTERFUNC;

    if (!newcat_valid_command(rig, "MD"))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    err = newcat_set_vfo_from_alias(rig, &vfo);

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    if (rig->caps->targetable_vfo & RIG_TARGETABLE_MODE)
    {
        main_sub_vfo = (RIG_VFO_B == vfo || RIG_VFO_SUB == vfo)  ? '1' : '0';
    }

    /* Build the command string */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MD%c%c", main_sub_vfo,
             cat_term);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);

    /* Get MODE */
    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        RETURNFUNC(err);
    }

    /*
     * The current mode value is a digit '0' ... 'C'
     * embedded at ret_data[3] in the read string.
     */
    c = priv->ret_data[3];

    /* default, unless set otherwise */
    *width = RIG_PASSBAND_NORMAL;

    *mode = newcat_rmode_width(rig, vfo, c, width);

    if (*mode == '0')
    {
        rig_debug(RIG_DEBUG_ERR, "%s: *mode = '0'??\n", __func__);
        RETURNFUNC(-RIG_EPROTO);
    }

    if (RIG_PASSBAND_NORMAL == *width)
    {
        *width = rig_passband_normal(rig, *mode);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: returning newcat_get_rx_bandwidth\n", __func__);
    RETURNFUNC(newcat_get_rx_bandwidth(rig, vfo, *mode, width));
}

/*
 * newcat_set_vfo
 *
 * set vfo and store requested vfo for later RIG_VFO_CURR
 * requests.
 *
 */

int newcat_set_vfo(RIG *rig, vfo_t vfo)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    char c;
    int err, mem;
    rmode_t vfo_mode;
    char command[] = "VS";

    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;
    priv->cache_start.tv_sec = 0; // invalidate the cache


    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = %s\n", __func__,
              rig_strvfo(vfo));

    // we can't change VFO while transmitting
    if (rig->state.cache.ptt == RIG_PTT_ON) { RETURNFUNC(RIG_OK); }

    if (!newcat_valid_command(rig, command))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    err = newcat_set_vfo_from_alias(rig,
                                    &vfo);   /* passes RIG_VFO_MEM, RIG_VFO_A, RIG_VFO_B */

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_B:
    case RIG_VFO_MAIN:
    case RIG_VFO_SUB:
        if (vfo == RIG_VFO_B || vfo == RIG_VFO_SUB)
        {
            c = '1';
        }
        else
        {
            c = '0';
        }

        err = newcat_get_vfo_mode(rig, RIG_VFO_A, &vfo_mode);

        if (err != RIG_OK)
        {
            RETURNFUNC(err);
        }

        if (vfo_mode == RIG_VFO_MEM)
        {
            priv->current_mem = NC_MEM_CHANNEL_NONE;
            state->current_vfo = RIG_VFO_A;
            err = newcat_vfomem_toggle(rig);
            RETURNFUNC(err);
        }

        break;

    case RIG_VFO_MEM:
        if (priv->current_mem == NC_MEM_CHANNEL_NONE)
        {
            /* Only works correctly for VFO A */
            if (state->current_vfo != RIG_VFO_A && state->current_vfo != RIG_VFO_MAIN)
            {
                RETURNFUNC(-RIG_ENTARGET);
            }

            /* get current memory channel */
            err = newcat_get_mem(rig, vfo, &mem);

            if (err != RIG_OK)
            {
                RETURNFUNC(err);
            }

            /* turn on memory channel */
            err = newcat_set_mem(rig, vfo, mem);

            if (err != RIG_OK)
            {
                RETURNFUNC(err);
            }

            /* Set current_mem now */
            priv->current_mem = mem;
        }

        /* Set current_vfo now */
        state->current_vfo = vfo;
        RETURNFUNC(RIG_OK);

    default:
        RETURNFUNC(-RIG_ENIMPL);         /* sorry, VFO not implemented */
    }

    /* Build the command string */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%c%c", command, c, cat_term);

    rig_debug(RIG_DEBUG_TRACE, "cmd_str = %s\n", priv->cmd_str);

    err = newcat_set_cmd(rig);

    if (err != RIG_OK)
    {
        RETURNFUNC(err);
    }

    state->current_vfo = vfo;    /* if set_vfo worked, set current_vfo */

    rig_debug(RIG_DEBUG_TRACE, "%s: rig->state.current_vfo = %s\n", __func__,
              rig_strvfo(vfo));

    RETURNFUNC(RIG_OK);
}

// Either returns a valid RIG_VFO* or if < 0 an error code
static vfo_t newcat_set_vfo_if_needed(RIG *rig, vfo_t vfo)
{
    vfo_t oldvfo = rig->state.current_vfo;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s, oldvfo=%s\n", __func__, rig_strvfo(vfo),
              rig_strvfo(oldvfo));

    if (oldvfo != vfo)
    {
        int ret;
        ret = newcat_set_vfo(rig, vfo);

        if (ret != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: error setting vfo=%s\n", __func__,
                      rig_strvfo(vfo));
            RETURNFUNC(ret);
        }
    }

    RETURNFUNC(oldvfo);
}

/*
 * rig_get_vfo
 *
 * get current RX vfo/mem and store requested vfo for
 * later RIG_VFO_CURR requests plus pass the tested vfo/mem
 * back to the frontend.
 *
 */

int newcat_get_vfo(RIG *rig, vfo_t *vfo)
{
    struct rig_state *state = &rig->state;
    struct newcat_priv_data *priv  = (struct newcat_priv_data *)rig->state.priv;
    int err;
    rmode_t vfo_mode;
    char const *command = "VS";

    ENTERFUNC;

    if (!vfo)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    /* Build the command string */
    if (!newcat_valid_command(rig, command))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s;", command);
    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);

    /* Get VFO */
    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        RETURNFUNC(err);
    }

    /*
     * The current VFO value is a digit ('0' or '1' ('A' or 'B'
     * respectively)) embedded at ret_data[2] in the read string.
     */
    switch (priv->ret_data[2])
    {
    case '0':
        if (rig->state.vfo_list & RIG_VFO_MAIN) { *vfo = RIG_VFO_MAIN; }
        else { *vfo = RIG_VFO_A; }

        break;

    case '1':
        if (rig->state.vfo_list & RIG_VFO_SUB) { *vfo = RIG_VFO_SUB; }
        else { *vfo = RIG_VFO_B; }

        break;

    default:
        RETURNFUNC(-RIG_EPROTO);         /* sorry, wrong current VFO */
    }

    /* Check to see if RIG is in MEM mode */
    err = newcat_get_vfo_mode(rig, RIG_VFO_A, &vfo_mode);

    if (err != RIG_OK)
    {
        RETURNFUNC(err);
    }

    if (vfo_mode == RIG_VFO_MEM)
    {
        *vfo = RIG_VFO_MEM;
    }

    state->current_vfo = *vfo;       /* set now */

    rig_debug(RIG_DEBUG_TRACE, "%s: rig->state.current_vfo = %s\n", __func__,
              rig_strvfo(state->current_vfo));

    RETURNFUNC(RIG_OK);
}


int newcat_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    struct newcat_priv_data *priv  = (struct newcat_priv_data *)rig->state.priv;
    int err = -RIG_EPROTO;
    char txon[] = "TX1;";
    char txoff[] = "TX0;";

    ENTERFUNC;

    priv->cache_start.tv_sec = 0; // invalidate the cache

    if (!newcat_valid_command(rig, "TX"))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    switch (ptt)
    {
    case RIG_PTT_ON_MIC:

        /* Build the command string */
        // the FTDX5000 uses menu 103 for front/rear audio in USB mode
        if (is_ftdx5000)
        {
            // Ensure FT5000 is back to MIC input
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX1030;");
            rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);
            newcat_set_cmd(rig); // don't care about the return
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s", txon);
        rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);
        err = newcat_set_cmd(rig);
        break;

    case RIG_PTT_ON_DATA:

        /* Build the command string */
        // the FTDX5000 uses menu 103 for front/rear audio in USB mode
        if (is_ftdx5000)
        {
            // Ensure FT5000 is back to MIC input
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX1031;");
            rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);
            newcat_set_cmd(rig); // don't care about the return
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s", txon);
        rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);
        err = newcat_set_cmd(rig);
        break;

    case RIG_PTT_ON:
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s", txon);
        rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);
        err = newcat_set_cmd(rig);

        break;

    case RIG_PTT_OFF:
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s", txoff);
        rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);
        err = newcat_set_cmd(rig);

        // some rigs like the FT991 need time before doing anything else like set_freq
        // We won't mess with CW mode -- no freq change expected hopefully
        if (rig->state.current_mode != RIG_MODE_CW
                && rig->state.current_mode != RIG_MODE_CWR
                && rig->state.current_mode != RIG_MODE_CWN
           )
        {
            hl_usleep(100 * 1000);
        }

        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(err);
}


int newcat_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    char c;
    int err;

    ENTERFUNC;

    if (!newcat_valid_command(rig, "TX"))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%c", "TX", cat_term);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);

    /* Get PTT */
    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        RETURNFUNC(err);
    }

    c = priv->ret_data[2];

    switch (c)
    {
    case '0':                 /* FT-950 "TX OFF", Original Release Firmware */
        *ptt = RIG_PTT_OFF;
        break;

    case '1' :                /* Just because,    what the CAT Manual Shows */
    case '2' :                /* FT-950 Radio:    Mic, Dataport, CW "TX ON" */
    case '3' :                /* FT-950 CAT port: Radio in "TX ON" mode     [Not what the CAT Manual Shows] */
        *ptt = RIG_PTT_ON;
        break;

    default:
        RETURNFUNC(-RIG_EPROTO);
    }

    RETURNFUNC(RIG_OK);
}


int newcat_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    ENTERFUNC;

    RETURNFUNC(-RIG_ENAVAIL);
}


int newcat_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    char c;
    char command[] = "OS";
    char main_sub_vfo = '0';

    ENTERFUNC;

    if (!newcat_valid_command(rig, command))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    /* Main or SUB vfo */
    err = newcat_set_vfo_from_alias(rig, &vfo);

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    if (rig->caps->targetable_vfo & RIG_TARGETABLE_MODE)
    {
        main_sub_vfo = (RIG_VFO_B == vfo || RIG_VFO_SUB == vfo) ? '1' : '0';
    }

    switch (rptr_shift)
    {
    case RIG_RPT_SHIFT_NONE:
        c = '0';
        break;

    case RIG_RPT_SHIFT_PLUS:
        c = '1';
        break;

    case RIG_RPT_SHIFT_MINUS:
        c = '2';
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);

    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%c%c%c", command,
             main_sub_vfo, c, cat_term);
    RETURNFUNC(newcat_set_cmd(rig));
}


int newcat_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    char c;
    char command[] = "OS";
    char main_sub_vfo = '0';

    ENTERFUNC;

    if (!newcat_valid_command(rig, command))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    /* Set Main or SUB vfo */
    err = newcat_set_vfo_from_alias(rig, &vfo);

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    if (rig->caps->targetable_vfo & RIG_TARGETABLE_MODE)
    {
        main_sub_vfo = (RIG_VFO_B == vfo || RIG_VFO_SUB == vfo) ? '1' : '0';
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%c%c", command, main_sub_vfo,
             cat_term);

    /* Get Rptr Shift */
    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        RETURNFUNC(err);
    }

    c = priv->ret_data[3];

    switch (c)
    {
    case '0':
        *rptr_shift = RIG_RPT_SHIFT_NONE;
        break;

    case '1':
        *rptr_shift = RIG_RPT_SHIFT_PLUS;
        break;

    case '2':
        *rptr_shift = RIG_RPT_SHIFT_MINUS;
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(RIG_OK);
}


int newcat_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    char command[32];
    freq_t freq = 0;

    ENTERFUNC;

    err = newcat_get_freq(rig, vfo, &freq); // Need to get freq to determine band

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    if (is_ft450)
    {
        strcpy(command, "EX050");

        // Step size is 100 kHz
        offs /= 100000;

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%03li%c", command, offs,
                 cat_term);
    }
    else if (is_ft2000)
    {
        if (freq >= 28000000 && freq <= 29700000)
        {
            strcpy(command, "EX076");
        }
        else if (freq >= 50000000 && freq <= 54000000)
        {
            strcpy(command, "EX077");
        }
        else
        {
            // only valid on 10m and 6m bands
            RETURNFUNC(-RIG_EINVAL);
        }

        // Step size is 1 kHz
        offs /= 1000;

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%04li%c", command, offs,
                 cat_term);
    }
    else if (is_ft950)
    {
        if (freq >= 28000000 && freq <= 29700000)
        {
            strcpy(command, "EX057");
        }
        else if (freq >= 50000000 && freq <= 54000000)
        {
            strcpy(command, "EX058");
        }
        else
        {
            // only valid on 10m and 6m bands
            RETURNFUNC(-RIG_EINVAL);
        }

        // Step size is 1 kHz
        offs /= 1000;

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%04li%c", command, offs,
                 cat_term);
    }
    else if (is_ft891)
    {
        if (freq >= 28000000 && freq <= 29700000)
        {
            strcpy(command, "EX0904");
        }
        else if (freq >= 50000000 && freq <= 54000000)
        {
            strcpy(command, "EX0905");
        }
        else
        {
            // only valid on 10m and 6m bands
            RETURNFUNC(-RIG_EINVAL);
        }

        // Step size is 1 kHz
        offs /= 1000;

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%04li%c", command, offs,
                 cat_term);
    }
    else if (is_ft991)
    {
        if (freq >= 28000000 && freq <= 29700000)
        {
            strcpy(command, "EX080");
        }
        else if (freq >= 50000000 && freq <= 54000000)
        {
            strcpy(command, "EX081");
        }
        else if (freq >= 144000000 && freq <= 148000000)
        {
            strcpy(command, "EX082");
        }
        else if (freq >= 430000000 && freq <= 450000000)
        {
            strcpy(command, "EX083");
        }
        else
        {
            // only valid on 10m to 70cm bands
            RETURNFUNC(-RIG_EINVAL);
        }

        // Step size is 1 kHz
        offs /= 1000;

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%04li%c", command, offs,
                 cat_term);
    }
    else if (is_ftdx1200)
    {
        if (freq >= 28000000 && freq <= 29700000)
        {
            strcpy(command, "EX087");
        }
        else if (freq >= 50000000 && freq <= 54000000)
        {
            strcpy(command, "EX088");
        }
        else
        {
            // only valid on 10m and 6m bands
            RETURNFUNC(-RIG_EINVAL);
        }

        // Step size is 1 kHz
        offs /= 1000;

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%04li%c", command, offs,
                 cat_term);
    }
    else if (is_ftdx3000 || is_ftdx3000dm)
    {
        if (freq >= 28000000 && freq <= 29700000)
        {
            strcpy(command, "EX086");
        }
        else if (freq >= 50000000 && freq <= 54000000)
        {
            strcpy(command, "EX087");
        }
        else
        {
            // only valid on 10m and 6m bands
            RETURNFUNC(-RIG_EINVAL);
        }

        // Step size is 1 kHz
        offs /= 1000;

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%04li%c", command, offs,
                 cat_term);
    }
    else if (is_ftdx5000)
    {
        if (freq >= 28000000 && freq <= 29700000)
        {
            strcpy(command, "EX081");
        }
        else if (freq >= 50000000 && freq <= 54000000)
        {
            strcpy(command, "EX082");
        }
        else
        {
            // only valid on 10m and 6m bands
            RETURNFUNC(-RIG_EINVAL);
        }

        // Step size is 1 kHz
        offs /= 1000;

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%04li%c", command, offs,
                 cat_term);
    }
    else if (is_ftdx101d || is_ftdx101mp || is_ftdx10)
    {
        if (freq >= 28000000 && freq <= 29700000)
        {
            strcpy(command, "EX010315");

            if (is_ftdx10) { strcpy(command, "EX010317"); }
        }
        else if (freq >= 50000000 && freq <= 54000000)
        {
            strcpy(command, "EX010316");

            if (is_ftdx10) { strcpy(command, "EX010318"); }
        }
        else
        {
            // only valid on 10m and 6m bands
            RETURNFUNC(-RIG_EINVAL);
        }

        // Step size is 1 kHz
        offs /= 1000;

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%04li%c", command, offs,
                 cat_term);
    }
    else
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    RETURNFUNC(newcat_set_cmd(rig));
}


int newcat_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *offs)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    int ret_data_len;
    char *retoffs;
    freq_t freq = 0;
    int step;

    ENTERFUNC;

    err = newcat_get_freq(rig, vfo, &freq); // Need to get freq to determine band

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    if (is_ft450)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX050%c", cat_term);

        // Step size is 100 kHz
        step = 100000;
    }
    else if (is_ft2000)
    {
        if (freq >= 28000000 && freq <= 29700000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX076%c", cat_term);
        }
        else if (freq >= 50000000 && freq <= 54000000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX077%c", cat_term);
        }
        else
        {
            // only valid on 10m and 6m bands
            *offs = 0;
            RETURNFUNC(RIG_OK);
        }

        // Step size is 1 kHz
        step = 1000;
    }
    else if (is_ft950)
    {
        if (freq >= 28000000 && freq <= 29700000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX057%c", cat_term);
        }
        else if (freq >= 50000000 && freq <= 54000000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX058%c", cat_term);
        }
        else
        {
            // only valid on 10m and 6m bands
            *offs = 0;
            RETURNFUNC(RIG_OK);
        }

        // Step size is 1 kHz
        step = 1000;
    }
    else if (is_ft891)
    {
        if (freq >= 28000000 && freq <= 29700000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX0904%c", cat_term);
        }
        else if (freq >= 50000000 && freq <= 54000000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX0905%c", cat_term);
        }
        else
        {
            // only valid on 10m and 6m bands
            *offs = 0;
            RETURNFUNC(RIG_OK);
        }

        // Step size is 1 kHz
        step = 1000;
    }
    else if (is_ft991)
    {
        if (freq >= 28000000 && freq <= 29700000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX080%c", cat_term);
        }
        else if (freq >= 50000000 && freq <= 54000000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX081%c", cat_term);
        }
        else if (freq >= 144000000 && freq <= 148000000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX082%c", cat_term);
        }
        else if (freq >= 430000000 && freq <= 450000000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX083%c", cat_term);
        }
        else
        {
            // only valid on 10m to 70cm bands
            *offs = 0;
            RETURNFUNC(RIG_OK);
        }

        // Step size is 1 kHz
        step = 1000;
    }
    else if (is_ftdx1200)
    {
        if (freq >= 28000000 && freq <= 29700000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX087%c", cat_term);
        }
        else if (freq >= 50000000 && freq <= 54000000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX088%c", cat_term);
        }
        else
        {
            // only valid on 10m and 6m bands
            *offs = 0;
            RETURNFUNC(RIG_OK);
        }

        // Step size is 1 kHz
        step = 1000;
    }
    else if (is_ftdx3000 || is_ftdx3000dm)
    {
        if (freq >= 28000000 && freq <= 29700000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX086%c", cat_term);
        }
        else if (freq >= 50000000 && freq <= 54000000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX087%c", cat_term);
        }
        else
        {
            // only valid on 10m and 6m bands
            *offs = 0;
            RETURNFUNC(RIG_OK);
        }

        // Step size is 1 kHz
        step = 1000;
    }
    else if (is_ftdx5000)
    {
        if (freq >= 28000000 && freq <= 29700000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX081%c", cat_term);
        }
        else if (freq >= 50000000 && freq <= 54000000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX082%c", cat_term);
        }
        else
        {
            // only valid on 10m and 6m bands
            *offs = 0;
            RETURNFUNC(RIG_OK);
        }

        // Step size is 1 kHz
        step = 1000;
    }
    else if (is_ftdx101d || is_ftdx101mp || is_ftdx10)
    {
        if (freq >= 28000000 && freq <= 29700000)
        {
            char *cmd = "EX010315%c";

            if (is_ftdx10) { cmd = "EX010317%c"; }

            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), cmd, cat_term);
        }
        else if (freq >= 50000000 && freq <= 54000000)
        {
            char *cmd = "EX010316%c";

            if (is_ftdx10) { cmd = "EX010318%c"; }

            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), cmd, cat_term);
        }
        else
        {
            // only valid on 10m and 6m bands
            *offs = 0;
            RETURNFUNC(RIG_OK);
        }

        // Step size is 1 kHz
        step = 1000;
    }
    else
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    err = newcat_get_cmd(rig);

    if (err != RIG_OK)
    {
        RETURNFUNC(err);
    }

    ret_data_len = strlen(priv->ret_data);

    /* skip command */
    retoffs = priv->ret_data + strlen(priv->cmd_str) - 1;
    /* chop term */
    priv->ret_data[ret_data_len - 1] = '\0';

    *offs = atol(retoffs) * step;

    RETURNFUNC(RIG_OK);
}


int newcat_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    ENTERFUNC;

    RETURNFUNC(-RIG_ENAVAIL);
}


int newcat_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
    ENTERFUNC;

    RETURNFUNC(-RIG_ENAVAIL);
}


int newcat_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode,
                          pbwidth_t tx_width)
{
    ENTERFUNC;
    rmode_t tmp_mode;
    pbwidth_t tmp_width;
    int err;

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s, tx_mode=%s, tx_width=%d\n", __func__,
              rig_strvfo(vfo), rig_strrmode(tx_mode), (int)tx_width);
    err = newcat_get_mode(rig, RIG_VFO_B, &tmp_mode, &tmp_width);

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    if (tmp_mode == tx_mode && (tmp_width == tx_width
                                || tmp_width == RIG_PASSBAND_NOCHANGE))
    {
        RETURNFUNC(RIG_OK);
    }

    err = rig_set_mode(rig, vfo, tx_mode, tx_width);

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    if (vfo == RIG_VFO_A || vfo == RIG_VFO_MAIN)
    {
        rig->state.cache.modeMainA = tx_mode;
    }
    else
    {
        rig->state.cache.modeMainB = tx_mode;
    }


    RETURNFUNC(-RIG_ENAVAIL);
}


int newcat_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode,
                          pbwidth_t *tx_width)
{
    int err;

    ENTERFUNC;

    err = newcat_get_mode(rig, RIG_VFO_B, tx_mode, tx_width);

    if (err < 0)
    {
        RETURNFUNC(err);
    }


    RETURNFUNC(RIG_OK);
}


int newcat_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    int err;
    vfo_t rx_vfo = RIG_VFO_NONE;

    //ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: entered, rxvfo=%s, txvfo=%s, split=%d\n",
              __func__, rig_strvfo(vfo), rig_strvfo(tx_vfo), split);

    err = newcat_set_vfo_from_alias(rig, &vfo);

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    if (is_ft991)
    {
        vfo = RIG_VFO_A;
        tx_vfo = RIG_SPLIT_ON == split ? RIG_VFO_B : RIG_VFO_A;
    }
    else if (is_ftdx101d || is_ftdx101mp)
    {
        vfo = RIG_VFO_MAIN;
        tx_vfo = RIG_SPLIT_ON == split ? RIG_VFO_SUB : RIG_VFO_MAIN;
    }
    else
    {
        err = newcat_get_vfo(rig, &rx_vfo);  /* sync to rig current vfo */

        if (err != RIG_OK)
        {
            RETURNFUNC(err);
        }
    }

    switch (split)
    {
    case RIG_SPLIT_OFF:
        err = newcat_set_tx_vfo(rig, vfo);

        if (err != RIG_OK)
        {
            RETURNFUNC(err);
        }

        if (rx_vfo != vfo && newcat_valid_command(rig, "VS"))
        {
            err = rig_set_vfo(rig, vfo);

            if (err != RIG_OK)
            {
                RETURNFUNC(err);
            }
        }

        break;

    case RIG_SPLIT_ON:
        err = newcat_set_tx_vfo(rig, tx_vfo);

        if (err != RIG_OK)
        {
            RETURNFUNC(err);
        }

        if (rx_vfo != vfo)
        {
            err = rig_set_vfo(rig, vfo);

            if (err != RIG_OK && err != -RIG_ENAVAIL)
            {
                RETURNFUNC(err);
            }
        }

        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(RIG_OK);
}


int newcat_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
    int err;

    ENTERFUNC;

    err = newcat_set_vfo_from_alias(rig, &vfo);

    if (err != RIG_OK)
    {
        RETURNFUNC(err);
    }

    err = newcat_get_tx_vfo(rig, tx_vfo);

    if (err != RIG_OK)
    {
        RETURNFUNC(err);
    }

    // we assume split is always on VFO_B
    //if (*tx_vfo == RIG_VFO_B || *tx_vfo == RIG_VFO_SUB)
    rig_debug(RIG_DEBUG_TRACE, "%s: tx_vfo=%s, curr_vfo=%s\n", __func__,
              rig_strvfo(*tx_vfo), rig_strvfo(rig->state.current_vfo));

    if (*tx_vfo != rig->state.current_vfo)
    {
        *split = RIG_SPLIT_ON;
    }
    else
    {
        *split = RIG_SPLIT_OFF;
    }

    rig_debug(RIG_DEBUG_TRACE, "SPLIT = %d, vfo = %s, TX_vfo = %s\n", *split,
              rig_strvfo(vfo),
              rig_strvfo(*tx_vfo));

    RETURNFUNC(RIG_OK);
}

int newcat_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    vfo_t oldvfo;
    int ret;

    ENTERFUNC;

    if (!newcat_valid_command(rig, "RT"))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    oldvfo = newcat_set_vfo_if_needed(rig, vfo);

    if (oldvfo < 0) { RETURNFUNC(oldvfo); }

    if (rit > rig->caps->max_rit)
    {
        rit = rig->caps->max_rit;    /* + */
    }
    else if (labs(rit) > rig->caps->max_rit)
    {
        rit = - rig->caps->max_rit;    /* - */
    }

    if (rit == 0)   // don't turn it off just because it is zero
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RC%c",
                 cat_term);
    }
    else if (rit < 0)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RC%cRD%04ld%c", cat_term,
                 labs(rit), cat_term);
    }
    else
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RC%cRU%04ld%c", cat_term,
                 labs(rit), cat_term);
    }

    ret = newcat_set_cmd(rig);

    oldvfo = newcat_set_vfo_if_needed(rig, oldvfo);

    if (oldvfo < 0) { RETURNFUNC(oldvfo); }

    RETURNFUNC(ret);
}


int newcat_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    char *retval;
    int err;
    int offset = 0;
    char *cmd = "IF";

    ENTERFUNC;

    if (vfo == RIG_VFO_B || vfo == RIG_VFO_SUB)
    {
        // OI always returns VFOB and IF always VFOA
        cmd = "OI";
    }

    if (!newcat_valid_command(rig, cmd))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    *rit = 0;

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%c", cmd, cat_term);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);

    /* Get RIT */
    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        RETURNFUNC(err);
    }

    // e.g. FT450 has 27 byte IF response, FT991 has 28 byte if response (one more byte for P2 VFO A Freq)
    // so we now check to ensure we know the length of the response

    switch (strlen(priv->ret_data))
    {
    case 27: offset = 13; break;

    case 28: offset = 14; break;

    default: offset = 0;
    }

    if (offset == 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: incorrect length of IF response, expected 27 or 28, got %du", __func__,
                  (int)strlen(priv->ret_data));
        RETURNFUNC(-RIG_EPROTO);
    }

    retval = priv->ret_data + offset;
    retval[5] = '\0';

    // return the current offset even if turned off
    *rit = (shortfreq_t) atoi(retval);

    RETURNFUNC(RIG_OK);
}


int newcat_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    vfo_t oldvfo;
    int ret;

    ENTERFUNC;

    if (!newcat_valid_command(rig, "XT"))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    oldvfo = newcat_set_vfo_if_needed(rig, vfo);

    if (oldvfo < 0) { RETURNFUNC(oldvfo); }

    if (xit > rig->caps->max_xit)
    {
        xit = rig->caps->max_xit;    /* + */
    }
    else if (labs(xit) > rig->caps->max_xit)
    {
        xit = - rig->caps->max_xit;    /* - */
    }

    if (xit == 0)
    {
        // don't turn it off just because the offset is zero
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RC%c",
                 cat_term);
    }
    else if (xit < 0)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RC%cRD%04ld%c", cat_term,
                 labs(xit), cat_term);
    }
    else
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RC%cRU%04ld%c", cat_term,
                 labs(xit), cat_term);
    }

    ret = newcat_set_cmd(rig);

    oldvfo = newcat_set_vfo_if_needed(rig, vfo);

    if (oldvfo < 0) { RETURNFUNC(oldvfo); }

    RETURNFUNC(ret);
}


int newcat_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    char *retval;
    int err;
    int offset = 0;
    char *cmd = "IF";

    ENTERFUNC;

    if (vfo == RIG_VFO_B || vfo == RIG_VFO_SUB)
    {
        // OI always returns VFOB and IF always VFOA
        cmd = "OI";
    }

    if (!newcat_valid_command(rig, cmd))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    *xit = 0;

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%c", cmd, cat_term);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);

    /* Get XIT */
    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        RETURNFUNC(err);
    }

    // e.g. FT450 has 27 byte IF response, FT991 has 28 byte if response (one more byte for P2 VFO A Freq)
    // so we now check to ensure we know the length of the response

    switch (strlen(priv->ret_data))
    {
    case 27: offset = 13; break;

    case 28: offset = 14; break;

    default: offset = 0;
    }

    if (offset == 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: incorrect length of IF response, expected 27 or 28, got %du", __func__,
                  (int)strlen(priv->ret_data));
        RETURNFUNC(-RIG_EPROTO);
    }

    retval = priv->ret_data + offset;
    retval[5] = '\0';

    // return the offset even when turned off
    *xit = (shortfreq_t) atoi(retval);

    RETURNFUNC(RIG_OK);
}


int newcat_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
    int err, i;
    pbwidth_t width;
    rmode_t mode;
    ncboolean ts_match;

    ENTERFUNC;

    err = newcat_get_mode(rig, vfo, &mode, &width);

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    /* assume 2 tuning steps per mode */
    for (i = 0, ts_match = FALSE; i < HAMLIB_TSLSTSIZ
            && rig->caps->tuning_steps[i].ts; i++)
        if (rig->caps->tuning_steps[i].modes & mode)
        {
            if (ts <= rig->caps->tuning_steps[i].ts)
            {
                err = newcat_set_faststep(rig, FALSE);
            }
            else
            {
                err = newcat_set_faststep(rig,  TRUE);
            }

            if (err != RIG_OK)
            {
                RETURNFUNC(err);
            }

            ts_match = TRUE;
            break;
        }   /* if mode */

    rig_debug(RIG_DEBUG_TRACE, "ts_match = %d, i = %d, ts = %d\n", ts_match, i,
              (int)ts);

    if (ts_match)
    {
        RETURNFUNC(RIG_OK);
    }
    else
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }
}


int newcat_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
    pbwidth_t width;
    rmode_t mode;
    int err, i;
    ncboolean ts_match;
    ncboolean fast_step = FALSE;


    ENTERFUNC;

    err = newcat_get_mode(rig, vfo, &mode, &width);

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    err = newcat_get_faststep(rig, &fast_step);

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    /* assume 2 tuning steps per mode */
    for (i = 0, ts_match = FALSE; i < HAMLIB_TSLSTSIZ
            && rig->caps->tuning_steps[i].ts; i++)
        if (rig->caps->tuning_steps[i].modes & mode)
        {
            if (fast_step == FALSE)
            {
                *ts = rig->caps->tuning_steps[i].ts;
            }
            else
            {
                *ts = rig->caps->tuning_steps[i + 1].ts;
            }

            ts_match = TRUE;
            break;
        }

    rig_debug(RIG_DEBUG_TRACE, "ts_match = %d, i = %d, i+1 = %d, *ts = %d\n",
              ts_match, i, i + 1, (int)*ts);

    if (ts_match)
    {
        RETURNFUNC(RIG_OK);
    }
    else
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }
}


int newcat_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code)
{
    ENTERFUNC;

    RETURNFUNC(-RIG_ENAVAIL);
}


int newcat_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code)
{
    ENTERFUNC;

    RETURNFUNC(-RIG_ENAVAIL);
}


int newcat_set_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    ENTERFUNC;

    RETURNFUNC(-RIG_ENAVAIL);
}


int newcat_get_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
    ENTERFUNC;

    RETURNFUNC(-RIG_ENAVAIL);
}


int newcat_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    int i;
    ncboolean tone_match;
    char main_sub_vfo = '0';

    ENTERFUNC;

    if (!newcat_valid_command(rig, "CN"))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (!newcat_valid_command(rig, "CT"))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    err = newcat_set_vfo_from_alias(rig, &vfo);

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    if (rig->caps->targetable_vfo & RIG_TARGETABLE_TONE)
    {
        main_sub_vfo = (RIG_VFO_B == vfo || RIG_VFO_SUB == vfo) ? '1' : '0';
    }

    for (i = 0, tone_match = FALSE; rig->caps->ctcss_list[i] != 0; i++)
        if (tone == rig->caps->ctcss_list[i])
        {
            tone_match = TRUE;
            break;
        }

    rig_debug(RIG_DEBUG_TRACE, "%s: tone = %u, tone_match = %d, i = %d", __func__,
              tone, tone_match, i);

    if (tone_match == FALSE && tone != 0)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (tone == 0) /* turn off ctcss */
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CT%c0%c", main_sub_vfo,
                 cat_term);
    }
    else
    {
        if (is_ft891 || is_ft991 || is_ftdx101d || is_ftdx101mp || is_ftdx10)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CN%c0%03d%cCT%c2%c",
                     main_sub_vfo, i, cat_term, main_sub_vfo, cat_term);
        }
        else
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CN%c%02d%cCT%c2%c",
                     main_sub_vfo, i, cat_term, main_sub_vfo, cat_term);
        }
    }

    RETURNFUNC(newcat_set_cmd(rig));
}


int newcat_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    int t;
    int ret_data_len;
    char *retlvl;
    char cmd[] = "CN";
    char main_sub_vfo = '0';

    ENTERFUNC;

    if (!newcat_valid_command(rig, cmd))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    err = newcat_set_vfo_from_alias(rig, &vfo);

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    if (rig->caps->targetable_vfo & RIG_TARGETABLE_TONE)
    {
        main_sub_vfo = (RIG_VFO_B == vfo || RIG_VFO_SUB == vfo) ? '1' : '0';
    }

    if (is_ft891 || is_ft991 || is_ftdx101d || is_ftdx101mp || is_ftdx10)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%c0%c", cmd, main_sub_vfo,
                 cat_term);
    }
    else
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%c%c", cmd, main_sub_vfo,
                 cat_term);
    }

    /* Get CTCSS TONE */
    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        RETURNFUNC(err);
    }

    ret_data_len = strlen(priv->ret_data);

    /* skip command */
    retlvl = priv->ret_data + strlen(priv->cmd_str) - 1;
    /* chop term */
    priv->ret_data[ret_data_len - 1] = '\0';

    t = atoi(retlvl);   /*  tone index */

    if (t < 0 || t > 49)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    *tone = rig->caps->ctcss_list[t];

    RETURNFUNC(RIG_OK);
}


int newcat_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code)
{
    ENTERFUNC;

    RETURNFUNC(-RIG_ENAVAIL);
}


int newcat_get_dcs_sql(RIG *rig, vfo_t vfo, tone_t *code)
{
    ENTERFUNC;

    RETURNFUNC(-RIG_ENAVAIL);
}


int newcat_set_tone_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
    ENTERFUNC;

    RETURNFUNC(-RIG_ENAVAIL);
}


int newcat_get_tone_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
    ENTERFUNC;

    RETURNFUNC(-RIG_ENAVAIL);
}


int newcat_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
    int err;

    ENTERFUNC;

    err = newcat_set_ctcss_tone(rig, vfo, tone);

    if (err != RIG_OK)
    {
        RETURNFUNC(err);
    }

    /* Change to sql */
    if (tone)
    {
        err = newcat_set_func(rig, vfo, RIG_FUNC_TSQL,  TRUE);

        if (err != RIG_OK)
        {
            RETURNFUNC(err);
        }
    }

    RETURNFUNC(RIG_OK);
}


int newcat_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
    int err;

    ENTERFUNC;

    err = newcat_get_ctcss_tone(rig, vfo, tone);

    RETURNFUNC(err);
}


int newcat_power2mW(RIG *rig, unsigned int *mwpower, float power, freq_t freq,
                    rmode_t mode)
{
    int rig_id;

    ENTERFUNC;

    rig_id = newcat_get_rigid(rig);

    switch (rig_id)
    {
    case NC_RIGID_FT450:
        /* 100 Watts */
        *mwpower = power * 100000;
        rig_debug(RIG_DEBUG_TRACE, "case FT450 - rig_id = %d, *mwpower = %u\n", rig_id,
                  *mwpower);
        break;

    case NC_RIGID_FT950:
        /* 100 Watts */
        *mwpower = power * 100000;      /* 0..100 Linear scale */
        rig_debug(RIG_DEBUG_TRACE,
                  "case FT950 - rig_id = %d, power = %f, *mwpower = %u\n", rig_id, power,
                  *mwpower);
        break;

    case NC_RIGID_FT2000:
        /* 100 Watts */
        *mwpower = power * 100000;
        rig_debug(RIG_DEBUG_TRACE, "case FT2000 - rig_id = %d, *mwpower = %u\n", rig_id,
                  *mwpower);
        break;

    case NC_RIGID_FT2000D:
        /* 200 Watts */
        *mwpower = power * 200000;
        rig_debug(RIG_DEBUG_TRACE, "case FT2000D - rig_id = %d, *mwpower = %u\n",
                  rig_id, *mwpower);
        break;

    case NC_RIGID_FTDX5000:
        /* 200 Watts */
        *mwpower = power * 200000;
        rig_debug(RIG_DEBUG_TRACE, "case FTDX5000 - rig_id = %d, *mwpower = %u\n",
                  rig_id, *mwpower);
        break;

    case NC_RIGID_FTDX9000D:
        /* 200 Watts */
        *mwpower = power * 200000;
        rig_debug(RIG_DEBUG_TRACE, "case FTDX9000D - rig_id = %d, *mwpower = %u\n",
                  rig_id, *mwpower);
        break;

    case NC_RIGID_FTDX9000Contest:
        /* 200 Watts */
        *mwpower = power * 200000;
        rig_debug(RIG_DEBUG_TRACE,
                  "case FTDX9000Contest - rig_id = %d, *mwpower = %u\n", rig_id, *mwpower);
        break;

    case NC_RIGID_FTDX9000MP:
        /* 400 Watts */
        *mwpower = power * 400000;
        rig_debug(RIG_DEBUG_TRACE, "case FTDX9000MP - rig_id = %d, *mwpower = %u\n",
                  rig_id, *mwpower);
        break;

    case NC_RIGID_FTDX1200:
        /* 100 Watts */
        *mwpower = power * 100000;
        rig_debug(RIG_DEBUG_TRACE, "case FTDX1200 - rig_id = %d, *mwpower = %d\n",
                  rig_id,
                  *mwpower);
        break;

    default:
        /* 100 Watts */
        *mwpower = power * 100000;
        rig_debug(RIG_DEBUG_TRACE, "default - rig_id = %d, *mwpower = %u\n", rig_id,
                  *mwpower);
    }

    RETURNFUNC(RIG_OK);
}


int newcat_mW2power(RIG *rig, float *power, unsigned int mwpower, freq_t freq,
                    rmode_t mode)
{
    int rig_id;

    ENTERFUNC;

    rig_id = newcat_get_rigid(rig);

    switch (rig_id)
    {
    case NC_RIGID_FT450:
        /* 100 Watts */
        *power = mwpower / 100000.0;
        rig_debug(RIG_DEBUG_TRACE, "case FT450 - rig_id = %d, *power = %f\n", rig_id,
                  *power);
        break;

    case NC_RIGID_FT950:
        /* 100 Watts */
        *power = mwpower / 100000.0;      /* 0..100 Linear scale */
        rig_debug(RIG_DEBUG_TRACE,
                  "case FT950 - rig_id = %d, mwpower = %u, *power = %f\n", rig_id, mwpower,
                  *power);
        break;

    case NC_RIGID_FT2000:
        /* 100 Watts */
        *power = mwpower / 100000.0;
        rig_debug(RIG_DEBUG_TRACE, "case FT2000 - rig_id = %d, *power = %f\n", rig_id,
                  *power);
        break;

    case NC_RIGID_FT2000D:
        /* 200 Watts */
        *power = mwpower / 200000.0;
        rig_debug(RIG_DEBUG_TRACE, "case FT2000D - rig_id = %d, *power = %f\n", rig_id,
                  *power);
        break;

    case NC_RIGID_FTDX5000:
        /* 200 Watts */
        *power = mwpower / 200000.0;
        rig_debug(RIG_DEBUG_TRACE, "case FTDX5000 - rig_id = %d, *power = %f\n", rig_id,
                  *power);
        break;

    case NC_RIGID_FTDX9000D:
        /* 200 Watts */
        *power = mwpower / 200000.0;
        rig_debug(RIG_DEBUG_TRACE, "case FTDX9000D - rig_id = %d, *power = %f\n",
                  rig_id, *power);
        break;

    case NC_RIGID_FTDX9000Contest:
        /* 200 Watts */
        *power = mwpower / 200000.0;
        rig_debug(RIG_DEBUG_TRACE, "case FTDX9000Contest - rig_id = %d, *power = %f\n",
                  rig_id, *power);
        break;

    case NC_RIGID_FTDX9000MP:
        /* 400 Watts */
        *power = mwpower / 400000.0;
        rig_debug(RIG_DEBUG_TRACE, "case FTDX9000MP - rig_id = %d, *power = %f\n",
                  rig_id, *power);
        break;

    case NC_RIGID_FTDX1200:
        /* 100 Watts */
        *power = mwpower / 100000.0;
        rig_debug(RIG_DEBUG_TRACE, "case FTDX1200 - rig_id = %d, *power = %f\n", rig_id,
                  *power);
        break;

    default:
        /* 100 Watts */
        *power = mwpower / 100000.0;
        rig_debug(RIG_DEBUG_TRACE, "default - rig_id = %d, *power = %f\n", rig_id,
                  *power);
    }

    RETURNFUNC(RIG_OK);
}


int newcat_set_powerstat(RIG *rig, powerstat_t status)
{
    struct rig_state *state = &rig->state;
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int retval;
    int i = 0;
    int retry_save;
    char ps;

    ENTERFUNC;

#if 0 // all Yaeus rigs have PS and calling this here interferes with power on

    if (!newcat_valid_command(rig, "PS"))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

#endif

    switch (status)
    {
    case RIG_POWER_ON:
        ps = '1';
        // when powering on need a dummy byte to wake it up
        // then sleep  from 1 to 2 seconds so we'll do 1.5 secs
        write_block(&state->rigport, (unsigned char *) "PS1;", 4);
        hl_usleep(1200000);
        break;

    case RIG_POWER_OFF:
    case RIG_POWER_STANDBY:
        ps = '0';
        write_block(&state->rigport, (unsigned char *) "PS0;", 4);
        break;

    default:
        RETURNFUNC(-RIG_ENAVAIL);
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PS%c%c", ps, cat_term);

    retval = write_block(&state->rigport, (unsigned char *) priv->cmd_str,
                         strlen(priv->cmd_str));

    retry_save = rig->state.rigport.retry;
    rig->state.rigport.retry = 0;

    if (status == RIG_POWER_ON) // wait for wakeup only
    {
        for (i = 0; i < 8; ++i) // up to ~10 seconds including the timeouts
        {
            freq_t freq;
            hl_usleep(1000000);
            rig_flush(&state->rigport);
            retval = rig_get_freq(rig, RIG_VFO_A, &freq);

            if (retval == RIG_OK)
            {
                rig->state.rigport.retry = retry_save;
                RETURNFUNC(retval);
            }

            rig_debug(RIG_DEBUG_TRACE, "%s: Wait #%d for power up\n", __func__, i + 1);
            retval = write_block(&state->rigport, (unsigned char *) priv->cmd_str,
                                 strlen(priv->cmd_str));

            if (retval != RIG_OK) { RETURNFUNC(retval); }
        }
    }

    rig->state.rigport.retry = retry_save;

    if (i == 9)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: timeout waiting for powerup, try %d\n",
                  __func__,
                  i + 1);
        retval = -RIG_ETIMEOUT;
    }

    RETURNFUNC(retval);
}


/*
 *  This functions returns an error if the rig is off,  dah
 */
int newcat_get_powerstat(RIG *rig, powerstat_t *status)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    char ps;
    char command[] = "PS";

    ENTERFUNC;

    *status = RIG_POWER_OFF;

    if (!newcat_valid_command(rig, command))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%c", command, cat_term);

    /* Get Power status */
    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        RETURNFUNC(err);
    }

    ps = priv->ret_data[2];

    switch (ps)
    {
    case '1':
        *status = RIG_POWER_ON;
        break;

    case '0':
        *status = RIG_POWER_OFF;
        break;

    default:
        RETURNFUNC(-RIG_ENAVAIL);
    }

    RETURNFUNC(RIG_OK);
}


int newcat_reset(RIG *rig, reset_t reset)
{
    ENTERFUNC;

    RETURNFUNC(-RIG_ENAVAIL);
}


int newcat_set_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t option)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    char which_ant;
    char command[] = "AN";
    char main_sub_vfo = '0';

    ENTERFUNC;

    if (!newcat_valid_command(rig, command))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    /* Main or SUB vfo */
    err = newcat_set_vfo_from_alias(rig, &vfo);

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    if ((rig->caps->targetable_vfo & RIG_TARGETABLE_ANT))
    {
        main_sub_vfo = (RIG_VFO_B == vfo || RIG_VFO_SUB == vfo) ? '1' : '0';
    }

    switch (ant)
    {
    case RIG_ANT_1:
        which_ant = '1';
        break;

    case RIG_ANT_2:
        which_ant = '2';
        break;

    case RIG_ANT_3:
        if (newcat_is_rig(rig, RIG_MODEL_FT950))
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        if (newcat_is_rig(rig, RIG_MODEL_FTDX1200))
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        which_ant = '3';
        break;

    case RIG_ANT_4:
        if (newcat_is_rig(rig, RIG_MODEL_FT950))
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        if (newcat_is_rig(rig, RIG_MODEL_FTDX1200))
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        which_ant = '4';
        break;

    case RIG_ANT_5:
        if (newcat_is_rig(rig, RIG_MODEL_FT950))
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        if (newcat_is_rig(rig, RIG_MODEL_FTDX1200))
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        /* RX only, on FT-2000/FT-5000/FT-9000 */
        which_ant = '5';
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%c%c%c", command,
             main_sub_vfo, which_ant, cat_term);
    RETURNFUNC(newcat_set_cmd(rig));
}


int newcat_get_ant(RIG *rig, vfo_t vfo, ant_t dummy, value_t *option,
                   ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    char c;
    char command[] = "AN";
    char main_sub_vfo = '0';

    ENTERFUNC;

    if (!newcat_valid_command(rig, command))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    /* Set Main or SUB vfo */
    err = newcat_set_vfo_from_alias(rig, &vfo);

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    if (rig->caps->targetable_vfo & RIG_TARGETABLE_ANT)
    {
        main_sub_vfo = (RIG_VFO_B == vfo || RIG_VFO_SUB == vfo) ? '1' : '0';
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%c%c", command, main_sub_vfo,
             cat_term);

    /* Get ANT */
    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        RETURNFUNC(err);
    }

    c = priv->ret_data[3];

    switch (c)
    {
    case '1':
        *ant_curr = RIG_ANT_1;
        break;

    case '2' :
        *ant_curr = RIG_ANT_2;
        break;

    case '3' :
        *ant_curr = RIG_ANT_3;
        break;

    case '4' :
        *ant_curr = RIG_ANT_4;
        break;

    case '5' :
        *ant_curr = RIG_ANT_5;
        break;

    default:
        RETURNFUNC(-RIG_EPROTO);
    }

    *ant_tx = * ant_rx = *ant_curr;

    RETURNFUNC(RIG_OK);
}

static int band2rig(hamlib_band_t band)
{
    int retval = 0;

    switch (band)
    {
    case RIG_BAND_160M:   retval = 0; break;

    case RIG_BAND_80M:    retval = 1; break;

    case RIG_BAND_60M:    retval = 2; break;

    case RIG_BAND_40M:    retval = 3; break;

    case RIG_BAND_30M:    retval = 4; break;

    case RIG_BAND_20M:    retval = 5; break;

    case RIG_BAND_17M:    retval = 6; break;

    case RIG_BAND_15M:    retval = 7; break;

    case RIG_BAND_12M:    retval = 8; break;

    case RIG_BAND_10M:    retval = 9; break;

    case RIG_BAND_6M:     retval = 10; break;

    case RIG_BAND_GEN:    retval = 11; break;

    case RIG_BAND_MW:     retval = 12; break;

    case RIG_BAND_AIR:    retval = 14; break;

    case RIG_BAND_144MHZ: retval = 15; break;

    case RIG_BAND_430MHZ: retval = 16; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unknown band index=%d\n", __func__, band);
        retval = -RIG_EINVAL;
        break;
    }

    return retval;
}

int newcat_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    struct rig_state *state = &rig->state;
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    int i;
    int scale;
    int fpf;
    char main_sub_vfo = '0';
    char *format;

    ENTERFUNC;

    /* Set Main or SUB vfo */
    err = newcat_set_vfo_from_alias(rig, &vfo);

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    if (rig->caps->targetable_vfo & RIG_TARGETABLE_LEVEL)
    {
        main_sub_vfo = (RIG_VFO_B == vfo || RIG_VFO_SUB == vfo) ? '1' : '0';
    }

    switch (level)
    {
    case RIG_LEVEL_RFPOWER:
        if (!newcat_valid_command(rig, "PC"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (is_ft950 || is_ftdx1200 || is_ftdx3000 || is_ft891 || is_ft991
                || is_ftdx101d || is_ftdx101mp || is_ftdx10)
        {
            scale = 100.;
        }
        else if (is_ft450 && newcat_get_rigid(rig) == NC_RIGID_FT450D)
        {
            scale = 100.;
        }
        else if (is_ftdx3000dm)
        {
            scale = 50;
        }
        else
        {
            scale = 255.;
        }

        fpf = newcat_scale_float(scale, val.f);

        if (is_ft950 || is_ft891 || is_ft991 || is_ftdx3000 || is_ftdx3000dm
                || is_ftdx101d
                || is_ftdx101mp || is_ftdx10)
        {
            // Minimum is 5 watts on these rigs
            if (fpf < 5)
            {
                fpf = 5;
            }
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PC%03d%c", fpf, cat_term);
        break;

    case RIG_LEVEL_AF:
        if (!newcat_valid_command(rig, "AG"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (val.f > 1.0) { RETURNFUNC(-RIG_EINVAL); }

        fpf = newcat_scale_float(255, val.f);

        if (is_ftdx10 || is_ft710) { main_sub_vfo = 0; }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AG%c%03d%c", main_sub_vfo, fpf,
                 cat_term);
        break;

    case RIG_LEVEL_AGC:
        if (!newcat_valid_command(rig, "GT"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        switch (val.i)
        {
        case RIG_AGC_OFF:
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "GT00;");
            break;

        case RIG_AGC_FAST:
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "GT01;");
            break;

        case RIG_AGC_MEDIUM:
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "GT02;");
            break;

        case RIG_AGC_SLOW:
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "GT03;");
            break;

        case RIG_AGC_AUTO:
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "GT04;");
            break;

        default:
            RETURNFUNC(-RIG_EINVAL);
        }

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_LEVEL && !is_ftdx10 && !is_ft710)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        break;

    case RIG_LEVEL_IF:
    {
        pbwidth_t width;
        rmode_t mode = 0;

        if (!newcat_valid_command(rig, "IS"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (is_ft991 || is_ftdx3000 || is_ftdx3000dm || is_ftdx5000 || is_ftdx101d
                || is_ftdx101mp)
        {
            newcat_get_mode(rig, vfo, &mode, &width);
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: LEVEL_IF val.i=%d\n", __func__, val.i);

        if (abs(val.i) > rig->caps->max_ifshift)
        {
            if (val.i > 0)
            {
                val.i = rig->caps->max_ifshift;
            }
            else
            {
                val.i = rig->caps->max_ifshift * -1;
            }
        }

        if (is_ftdx101d || is_ftdx101mp || is_ftdx10)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "IS%c0%+.4d%c", main_sub_vfo,
                     val.i, cat_term);
        }
        else if (is_ftdx10 || is_ft710)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "IS00%+.4d%c",
                     val.i, cat_term);
        }
        else if (is_ft891)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "IS0%d%+.4d%c",
                     val.i == 0 ? 0 : 1,
                     val.i, cat_term);
        }
        else
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "IS%c%+.4d%c", main_sub_vfo,
                     val.i, cat_term);
        }

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_LEVEL && !is_ft2000
                && !is_ftdx10 && !is_ft710)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        // Some Yaesu rigs reject this command in AM/FM modes
        if (is_ft991 || is_ftdx3000 || is_ftdx3000dm || is_ftdx5000 || is_ftdx101d
                || is_ftdx101mp)
        {
            if (mode & RIG_MODE_AM || mode & RIG_MODE_FM || mode & RIG_MODE_AMN
                    || mode & RIG_MODE_FMN)
            {
                priv->question_mark_response_means_rejected = 1;
            }
        }

        break;
    }

    case RIG_LEVEL_CWPITCH:
    {
        int kp;

        if (!newcat_valid_command(rig, "KP"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (val.i < 300)
        {
            i = 300;
        }
        else if (val.i > 1050)
        {
            i = 1050;
        }
        else
        {
            i = val.i;
        }

        if (is_ft950 || is_ft2000)
        {
            kp = (i - 300) / 50;
        }
        else
        {
            // Most Yaesu rigs seem to use range of 0-75 to represent pitch of 300..1050 Hz in 10 Hz steps
            kp = (i - 300) / 10;
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "KP%02d%c", kp, cat_term);
        break;
    }

    case RIG_LEVEL_KEYSPD:
        if (!newcat_valid_command(rig, "KS"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "KS%03d%c", val.i, cat_term);

        break;

    case RIG_LEVEL_MICGAIN:
    {
        pbwidth_t width;
        rmode_t mode = 0;

        if (!newcat_valid_command(rig, "MG"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (is_ft991 || is_ftdx3000 || is_ftdx3000dm || is_ftdx5000 || is_ftdx101d
                || is_ftdx101mp)
        {
            newcat_get_mode(rig, vfo, &mode, &width);
        }

        if (val.f > 1.0) { RETURNFUNC(-RIG_EINVAL); }

        if (is_ftdx1200 || is_ftdx3000 || is_ftdx3000dm || is_ft891 || is_ft991
                || is_ftdx101d
                || is_ftdx101mp
                || is_ftdx10)
        {
            fpf = newcat_scale_float(100, val.f);
        }
        else
        {
            fpf = newcat_scale_float(255, val.f);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MG%03d%c", fpf, cat_term);

        // Some Yaesu rigs reject this command in RTTY modes
        if (is_ft991 || is_ftdx3000 || is_ftdx3000dm || is_ftdx5000 || is_ftdx101d
                || is_ftdx101mp)
        {
            if (mode & RIG_MODE_RTTY || mode & RIG_MODE_RTTYR)
            {
                priv->question_mark_response_means_rejected = 1;
            }
        }

        break;
    }

    case RIG_LEVEL_METER:
        if (!newcat_valid_command(rig, "MS"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (is_ftdx101d
                || is_ftdx101mp) // new format for the command with VFO selection
        {
            format = "MS0%d;";

            if (vfo == RIG_VFO_SUB)
            {
                format = "MS1%d;";
            }
        }
        else if (is_ftdx10)
        {
            format = "MS%d0;";
        }
        else
        {
            format = "MS%d;";
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: format=%s\n", __func__, format);

        switch (val.i)
        {
        case RIG_METER_ALC: SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), format, 1);
            break;

        case RIG_METER_PO:
            if (newcat_is_rig(rig, RIG_MODEL_FT950))
            {
                RETURNFUNC(RIG_OK);
            }
            else
            {
                SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), format, 2);
            }

            break;

        case RIG_METER_SWR:  SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), format, 3);
            break;

        case RIG_METER_COMP: SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), format, 0);
            break;

        case RIG_METER_IC:   SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), format, 4);
            break;

        case RIG_METER_VDD:  SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), format, 5);
            break;

            rig_debug(RIG_DEBUG_ERR, "%s: unknown val.i=%d\n", __func__, val.i);

        default: RETURNFUNC(-RIG_EINVAL);
        }

        break;

    case RIG_LEVEL_PREAMP:
        if (!newcat_valid_command(rig, "PA"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (val.i == 0)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PA00%c", cat_term);

            if (rig->caps->targetable_vfo & RIG_TARGETABLE_LEVEL && !is_ft2000
                    && !is_ftdx10 && !is_ft710)
            {
                priv->cmd_str[2] = main_sub_vfo;
            }

            break;
        }

        priv->cmd_str[0] = '\0';

        for (i = 0; state->preamp[i] != RIG_DBLST_END; i++)
        {
            if (state->preamp[i] == val.i)
            {
                SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PA0%d%c", i + 1, cat_term);
                break;
            }
        }

        if (strlen(priv->cmd_str) == 0)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_LEVEL  && !is_ft2000
                && !is_ftdx10 && !is_ft710)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        break;

    case RIG_LEVEL_ATT:
        if (!newcat_valid_command(rig, "RA"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (val.i == 0)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RA00%c", cat_term);

            if (rig->caps->targetable_vfo & RIG_TARGETABLE_LEVEL && !is_ft2000
                    && !is_ftdx10 && !is_ft710)
            {
                priv->cmd_str[2] = main_sub_vfo;
            }

            break;
        }

        priv->cmd_str[0] = '\0';

        for (i = 0; state->attenuator[i] != RIG_DBLST_END; i++)
        {
            if (state->attenuator[i] == val.i)
            {
                SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RA0%d%c", i + 1, cat_term);
                break;
            }
        }

        if (strlen(priv->cmd_str) == 0)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_LEVEL && !is_ft2000
                && !is_ftdx10 && !is_ft710)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        break;

    case RIG_LEVEL_RF:
        if (!newcat_valid_command(rig, "RG"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (is_ft891)
        {
            scale = 30;
        }
        else
        {
            scale = 255;
        }

        if (is_ftdx10 || is_ft710) { main_sub_vfo = 0; }

        fpf = newcat_scale_float(scale, val.f);
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RG%c%03d%c", main_sub_vfo, fpf,
                 cat_term);
        break;

    case RIG_LEVEL_NR:
        if (!newcat_valid_command(rig, "RL"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (newcat_is_rig(rig, RIG_MODEL_FT450))
        {
            fpf = newcat_scale_float(11, val.f);

            if (fpf < 1)
            {
                fpf = 1;
            }

            if (fpf > 11)
            {
                fpf = 11;
            }

            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RL0%02d%c", fpf, cat_term);
        }
        else
        {
            if (is_ft991)
            {
                fpf = newcat_scale_float(15, val.f);

                if (fpf > 15) { fpf = 15; }

                if (fpf < 1) { fpf = 1; }
            }
            else
            {
                fpf = newcat_scale_float(15, val.f);

                if (fpf > 15) { fpf = 10; }
            }

            if (fpf < 0)
            {
                fpf = 0;
            }

            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RL0%02d%c", fpf, cat_term);

            if (rig->caps->targetable_vfo & RIG_TARGETABLE_LEVEL && !is_ft2000
                    && !is_ftdx10 && !is_ft710)
            {
                priv->cmd_str[2] = main_sub_vfo;
            }
        }

        break;

    case RIG_LEVEL_COMP:
        if (!newcat_valid_command(rig, "PL"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (is_ft2000 || is_ftdx9000 || is_ftdx5000)
        {
            scale = 255;
        }
        else
        {
            scale = 100;
        }

        fpf = newcat_scale_float(scale, val.f);
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PL%03d%c", fpf, cat_term);
        break;

    case RIG_LEVEL_BKINDL:
    {
        int millis;
        value_t keyspd;

        if (!newcat_valid_command(rig, "SD"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        // Convert 10/ths of dots to milliseconds using the current key speed
        err = newcat_get_level(rig, vfo, RIG_LEVEL_KEYSPD, &keyspd);

        if (err != RIG_OK)
        {
            RETURNFUNC(err);
        }

        millis = dot10ths_to_millis(val.i, keyspd.i);

        if (is_ftdx101d || is_ftdx101mp || is_ftdx10)
        {
            if (millis <= 30) { SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SD00;"); }
            else if (millis <= 50) { SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SD01;"); }
            else if (millis <= 100) { SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SD02;"); }
            else if (millis <= 150) { SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SD03;"); }
            else if (millis <= 200) { SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SD04;"); }
            else if (millis <= 250) { SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SD05;"); }
            else if (millis > 2900) { SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SD33;"); }
            else
            {
                // This covers 300-2900 06-32
                SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SD%02d;",
                         6 + ((millis - 300) / 100));
            }
        }
        else if (is_ftdx5000)
        {
            if (millis < 20)
            {
                millis = 20;
            }

            if (millis > 5000)
            {
                millis = 5000;
            }

            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SD%04d%c", millis, cat_term);
        }
        else if (is_ft950 || is_ft450 || is_ft891 || is_ft991 || is_ftdx1200
                 || is_ftdx3000 || is_ftdx3000dm)
        {
            if (millis < 30)
            {
                millis = 30;
            }

            if (millis > 3000)
            {
                millis = 3000;
            }

            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SD%04d%c", millis, cat_term);
        }
        else if (is_ft2000 || is_ftdx9000)
        {
            if (millis < 0)
            {
                millis = 0;
            }

            if (millis > 5000)
            {
                millis = 5000;
            }

            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SD%04d%c", millis, cat_term);
        }
        else // default
        {
            if (millis < 1)
            {
                millis = 1;
            }

            if (millis > 5000)
            {
                millis = 5000;
            }

            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SD%04d%c", millis, cat_term);
        }

        break;
    }

    case RIG_LEVEL_SQL:
        if (!newcat_valid_command(rig, "SQ"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (is_ft891 || is_ft991 || is_ftdx101d || is_ftdx101mp || is_ftdx10)
        {
            scale = 100;
        }
        else
        {
            scale = 255;
        }

        fpf = newcat_scale_float(scale, val.f);
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SQ%c%03d%c", main_sub_vfo, fpf,
                 cat_term);

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_LEVEL && !is_ftdx10 && !is_ft710)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        break;

    case RIG_LEVEL_VOXDELAY:
        if (!newcat_valid_command(rig, "VD"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        /* VOX delay, api int (tenth of seconds), ms for rig */
        val.i = val.i * 100;
        rig_debug(RIG_DEBUG_TRACE, "%s: vali=%d\n", __func__, val.i);

        if (is_ft950 || is_ft450 || is_ftdx1200)
        {
            if (val.i < 100)         /* min is 30ms but spec is 100ms Unit Intervals */
            {
                val.i = 30;
            }

            if (val.i > 3000)
            {
                val.i = 3000;
            }

            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VD%04d%c", val.i, cat_term);
        }
        else if (is_ftdx101d || is_ftdx101mp || is_ftdx10) // new lookup table argument
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: ft101 #1 val.i=%d\n", __func__, val.i);

            if (val.i == 0) { val.i = 0; }
            else if (val.i <= 100) { val.i = 2; }
            else if (val.i <= 200) { val.i = 4; }
            else if (val.i > 3000) { val.i = 33; }
            else { val.i = (val.i - 300) / 100 + 6; }

            rig_debug(RIG_DEBUG_TRACE, "%s: ftdx101/ftdx10 #1 val.i=%d\n", __func__, val.i);

            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VD%02d%c", val.i, cat_term);
        }
        else if (rig->caps->targetable_vfo & RIG_TARGETABLE_MODE)
        {
            if (val.i < 0)
            {
                val.i = 0;
            }

            if (val.i > 5000)
            {
                val.i = 5000;
            }

            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VD%04d%c", val.i, cat_term);
        }

        else
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VD%04d%c", val.i, cat_term);
        }

        break;

    case RIG_LEVEL_VOXGAIN:
        if (!newcat_valid_command(rig, "VG"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (is_ft2000 || is_ftdx9000 || is_ftdx5000 || is_ft450)
        {
            scale = 255;
        }
        else
        {
            scale = 100;
        }

        fpf = newcat_scale_float(scale, val.f);
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VG%03d%c", fpf, cat_term);
        break;

    case RIG_LEVEL_ANTIVOX:
        if (is_ftdx101d || is_ftdx101mp || is_ftdx10)
        {
            fpf = newcat_scale_float(100, val.f);
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AV%03d%c", fpf, cat_term);
        }
        else if (is_ftdx5000)
        {
            fpf = newcat_scale_float(100, val.f);
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX176%03d%c", fpf, cat_term);
        }
        else if (is_ftdx3000 || is_ftdx3000dm || is_ftdx1200)
        {
            fpf = newcat_scale_float(100, val.f);
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX183%03d%c", fpf, cat_term);
        }
        else if (is_ft991)
        {
            fpf = newcat_scale_float(100, val.f);
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX145%03d%c", fpf, cat_term);
        }
        else if (is_ft891)
        {
            fpf = newcat_scale_float(100, val.f);
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX1619%03d%c", fpf, cat_term);
        }
        else if (is_ft950)
        {
            fpf = newcat_scale_float(100, val.f);
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX117%03d%c", fpf, cat_term);
        }
        else if (is_ft2000)
        {
            fpf = newcat_scale_float(100, val.f);
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX042%03d%c", fpf, cat_term);
        }
        else
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        break;

    case RIG_LEVEL_NOTCHF:
        if (!newcat_valid_command(rig, "BP"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        val.i = val.i / 10;

        if (is_ftdx9000)
        {
            if (val.i < 0)
            {
                val.i = 0;
            }
        }
        else
        {
            if (val.i < 1)
            {
                val.i = 1;
            }
        }

        if (is_ft891 || is_ft991 || is_ftdx101d || is_ftdx101mp || is_ftdx10)
        {
            if (val.i > 320)
            {
                val.i = 320;
            }
        }

        if (is_ft950 || is_ftdx9000)
        {
            if (val.i > 300)
            {
                val.i = 300;
            }
        }
        else
        {
            if (val.i > 400)
            {
                val.i = 400;
            }
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BP01%03d%c", val.i, cat_term);

        if (is_ftdx9000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BP%03d%c", val.i, cat_term);
        }
        else if (rig->caps->targetable_vfo & RIG_TARGETABLE_LEVEL && !is_ft2000
                 && !is_ftdx10 && !is_ft710)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        break;

    case RIG_LEVEL_MONITOR_GAIN:
        if (!newcat_valid_command(rig, "ML"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (val.f > 1.0) { RETURNFUNC(-RIG_EINVAL); }

        if (is_ftdx1200 || is_ftdx3000 || is_ftdx3000dm || is_ft891 || is_ft991
                || is_ftdx101d
                || is_ftdx101mp
                || is_ftdx10)
        {
            fpf = newcat_scale_float(100, val.f);
        }
        else
        {
            fpf = newcat_scale_float(255, val.f);
        }

        if (is_ftdx9000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "ML%03d%c", fpf, cat_term);
        }
        else
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "ML1%03d%c", fpf, cat_term);
        }

        break;

    case RIG_LEVEL_BAND_SELECT:
        if (newcat_valid_command(rig, "BS"))
        {
            int band = band2rig((hamlib_band_t)val.i);

            if (band < 0)
            {
                RETURNFUNC(-RIG_EINVAL);
            }

            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BS%02d%c", band, cat_term);
        }

        break;

    case RIG_LEVEL_NB:
        if (!newcat_valid_command(rig, "NL"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        fpf = newcat_scale_float(10, val.f);

        if (fpf < 0)
        {
            fpf = 0;
        }

        if (fpf > 10)
        {
            fpf = 10;
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "NL00%02d%c", fpf, cat_term);

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_LEVEL && !is_ftdx10 && !is_ft710
                && !is_ftdx101mp)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    err = newcat_set_cmd(rig);

    // Clear flag after executing command
    priv->question_mark_response_means_rejected = 0;

    RETURNFUNC(err);
}


int newcat_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    struct rig_state *state = &rig->state;
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    int ret_data_len;
    char *retlvl;
    int retlvl_len;
    float scale;
    char main_sub_vfo = '0';
    int i;

    ENTERFUNC;

    /* Set Main or SUB vfo */
    err = newcat_set_vfo_from_alias(rig, &vfo);

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    if (rig->caps->targetable_vfo & RIG_TARGETABLE_LEVEL)
    {
        main_sub_vfo = (RIG_VFO_B == vfo || RIG_VFO_SUB == vfo) ? '1' : '0';
    }

    switch (level)
    {
    case RIG_LEVEL_RFPOWER:
        if (!newcat_valid_command(rig, "PC"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PC%c", cat_term);
        break;

    case RIG_LEVEL_PREAMP:
        if (!newcat_valid_command(rig, "PA"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PA0%c", cat_term);

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_LEVEL && !is_ft2000
                && !is_ftdx10 && !is_ft710)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        break;

    case RIG_LEVEL_AF:
        if (!newcat_valid_command(rig, "AG"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AG%c%c", main_sub_vfo,
                 cat_term);

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_LEVEL && !is_ftdx10 && !is_ft710)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        break;

    case RIG_LEVEL_AGC:
        if (!newcat_valid_command(rig, "GT"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "GT%c%c", main_sub_vfo,
                 cat_term);

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_LEVEL && !is_ftdx10 && !is_ft710)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        break;

    case RIG_LEVEL_IF:
    {
        pbwidth_t width;
        rmode_t mode = 0;

        if (!newcat_valid_command(rig, "IS"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (is_ft991 || is_ftdx3000 || is_ftdx3000dm || is_ftdx5000 || is_ftdx101d
                || is_ftdx101mp)
        {
            newcat_get_mode(rig, vfo, &mode, &width);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "IS%c%c", main_sub_vfo,
                 cat_term);

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_LEVEL && !is_ft2000
                && !is_ftdx10 && !is_ft710)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        // Some Yaesu rigs reject this command in AM/FM modes
        if (is_ft991 || is_ftdx3000 || is_ftdx3000dm || is_ftdx5000 || is_ftdx101d
                || is_ftdx101mp)
        {
            if (mode & RIG_MODE_AM || mode & RIG_MODE_FM || mode & RIG_MODE_AMN
                    || mode & RIG_MODE_FMN)
            {
                priv->question_mark_response_means_rejected = 1;
            }
        }

        break;
    }

    case RIG_LEVEL_CWPITCH:
        if (!newcat_valid_command(rig, "KP"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "KP%c", cat_term);
        break;

    case RIG_LEVEL_KEYSPD:
        if (!newcat_valid_command(rig, "KS"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "KS%c", cat_term);
        break;

    case RIG_LEVEL_MICGAIN:
    {
        pbwidth_t width;
        rmode_t mode = 0;

        if (!newcat_valid_command(rig, "MG"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (is_ft991 || is_ftdx3000 || is_ftdx3000dm || is_ftdx5000 || is_ftdx101d
                || is_ftdx101mp)
        {
            newcat_get_mode(rig, vfo, &mode, &width);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MG%c", cat_term);

        // Some Yaesu rigs reject this command in RTTY modes
        if (is_ft991 || is_ftdx3000 || is_ftdx3000dm || is_ftdx5000 || is_ftdx101d
                || is_ftdx101mp)
        {
            if (mode & RIG_MODE_RTTY || mode & RIG_MODE_RTTYR)
            {
                priv->question_mark_response_means_rejected = 1;
            }
        }

        break;
    }

    case RIG_LEVEL_METER:
        if (!newcat_valid_command(rig, "MS"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MS%c", cat_term);
        break;

    case RIG_LEVEL_ATT:
        if (!newcat_valid_command(rig, "RA"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RA0%c", cat_term);

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_LEVEL && !is_ft2000
                && !is_ftdx10 && !is_ft710)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        break;

    case RIG_LEVEL_RF:
        if (!newcat_valid_command(rig, "RG"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RG%c%c", main_sub_vfo,
                 cat_term);
        break;

    case RIG_LEVEL_COMP:
        if (!newcat_valid_command(rig, "PL"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PL%c", cat_term);
        break;

    case RIG_LEVEL_NR:
        if (!newcat_valid_command(rig, "RL"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RL0%c", cat_term);

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_LEVEL && !is_ft2000
                && !is_ftdx10 && !is_ft710)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        break;

    case RIG_LEVEL_BKINDL:
        if (!newcat_valid_command(rig, "SD"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SD%c", cat_term);
        break;

    case RIG_LEVEL_SQL:
        if (!newcat_valid_command(rig, "SQ"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SQ%c%c", main_sub_vfo,
                 cat_term);

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_LEVEL && !is_ftdx10 && !is_ft710)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        break;

    case RIG_LEVEL_VOXDELAY:

        /* VOX delay, arg int (tenth of seconds) */
        if (!newcat_valid_command(rig, "VD"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VD%c", cat_term);
        break;

    case RIG_LEVEL_VOXGAIN:
        if (!newcat_valid_command(rig, "VG"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VG%c", cat_term);
        break;

    case RIG_LEVEL_NB:
        if (!newcat_valid_command(rig, "NL"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "NL0%c", cat_term);

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_LEVEL && !is_ftdx10 && !is_ft710
                && !is_ftdx101mp)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        break;

    /*
     * Read only levels
     */
    case RIG_LEVEL_STRENGTH:
    case RIG_LEVEL_RAWSTR:
        if (!newcat_valid_command(rig, "SM"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SM%c%c", main_sub_vfo,
                 cat_term);
        break;

    case RIG_LEVEL_SWR:
        if (!newcat_valid_command(rig, "RM"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (is_ftdx9000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RM09%c", cat_term);
        }
        else if (is_ftdx3000 || is_ftdx3000dm || is_ftdx5000)
        {
            // The 3000 has to use the meter read for SWR when the tuner is on
            // We'll assume the 5000 is the same way for now
            // Also need to ensure SWR is selected for the meter
            int tuner;
            value_t meter;
            newcat_get_func(rig, RIG_VFO_A, RIG_FUNC_TUNER, &tuner);
            newcat_get_level(rig, RIG_VFO_A, RIG_LEVEL_METER, &meter);

            if (tuner && meter.i != RIG_METER_SWR)
            {
                RETURNFUNC(-RIG_ENAVAIL); // if meter not SWR can't read SWR
            }

            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RM%c%c", (tuner
                     && meter.i == RIG_METER_SWR) ? '2' : '6',
                     cat_term);
        }
        else
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RM6%c", cat_term);
        }

        break;

    case RIG_LEVEL_ALC:
        if (!newcat_valid_command(rig, "RM"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (is_ftdx9000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RM07%c", cat_term);
        }
        else
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RM4%c", cat_term);
        }

        break;

    case RIG_LEVEL_RFPOWER_METER:
    case RIG_LEVEL_RFPOWER_METER_WATTS:
        if (!newcat_valid_command(rig, "RM"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (is_ftdx9000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RM08%c", cat_term);
        }
        else
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RM5%c", cat_term);
        }

        break;

    case RIG_LEVEL_COMP_METER:
        if (!newcat_valid_command(rig, "RM"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (is_ftdx9000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RM06%c", cat_term);
        }
        else
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RM3%c", cat_term);
        }

        break;

    case RIG_LEVEL_VD_METER:
        if (!newcat_valid_command(rig, "RM"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (is_ftdx9000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RM11%c", cat_term);
        }
        else
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RM8%c", cat_term);
        }

        break;

    case RIG_LEVEL_ID_METER:
        if (!newcat_valid_command(rig, "RM"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (is_ftdx9000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RM10%c", cat_term);
        }
        else
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RM7%c", cat_term);
        }

        break;

    case RIG_LEVEL_ANTIVOX:
        if (is_ftdx101d || is_ftdx101mp || is_ftdx10)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AV%c", cat_term);
        }
        else if (is_ftdx5000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX176%c", cat_term);
        }
        else if (is_ftdx3000 || is_ftdx3000dm || is_ftdx1200)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX183%c", cat_term);
        }
        else if (is_ftdx1200)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX183%c", cat_term);
        }
        else if (is_ft991)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX147%c", cat_term);
        }
        else if (is_ft891)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX1619%c", cat_term);
        }
        else if (is_ft950)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX117%c", cat_term);
        }
        else if (is_ft2000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX042%c", cat_term);
        }
        else
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        break;

    case RIG_LEVEL_NOTCHF:
        if (!newcat_valid_command(rig, "BP"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BP01%c", cat_term);

        if (is_ftdx9000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BP%c", cat_term);
        }
        else if (rig->caps->targetable_vfo & RIG_TARGETABLE_LEVEL && !is_ft2000
                 && !is_ftdx10 && !is_ft710)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        break;

    case RIG_LEVEL_MONITOR_GAIN:
        if (!newcat_valid_command(rig, "ML"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (is_ftdx9000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "ML%c", cat_term);
        }
        else
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "ML1%c", cat_term);
        }

        break;

    case RIG_LEVEL_TEMP_METER:
        if (!newcat_valid_command(rig, "RM"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (is_ftdx9000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RM14%c", cat_term);
        }
        else
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RM9%c", cat_term);
        }

        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    err = newcat_get_cmd(rig);

    // Clear flag after executing command
    priv->question_mark_response_means_rejected = 0;

    if (err != RIG_OK)
    {
        RETURNFUNC(err);
    }

    ret_data_len = strlen(priv->ret_data);

    /* skip command */
    retlvl = priv->ret_data + strlen(priv->cmd_str) - 1;
    retlvl_len = strlen(retlvl);
    rig_debug(RIG_DEBUG_TRACE, "%s: retlvl='%s'\n", __func__, retlvl);
    /* chop term */
    priv->ret_data[ret_data_len - 1] = '\0';

    switch (level)
    {
    case RIG_LEVEL_RFPOWER:
        if (is_ft950 || is_ftdx1200 || is_ftdx3000 || is_ftdx3000dm || is_ft891
                || is_ft991
                || is_ftdx101d || is_ftdx101mp || is_ftdx10)
        {
            scale = 100.;
        }
        else if (is_ft450 && newcat_get_rigid(rig) == NC_RIGID_FT450D)
        {
            scale = 100.;
        }
        else
        {
            scale = 255.;
        }

        val->f = (float)atoi(retlvl) / scale;
        break;

    case RIG_LEVEL_VOXGAIN:
    case RIG_LEVEL_COMP:
        if (is_ft2000 || is_ftdx9000 || is_ftdx5000 || is_ft450)
        {
            scale = 255;
        }
        else
        {
            scale = 100;
        }

        val->f = (float) atoi(retlvl) / scale;
        break;

    case RIG_LEVEL_ANTIVOX:
        val->f = (float) atoi(retlvl) / 100.;
        break;

    case RIG_LEVEL_SWR:
        if (retlvl_len > 3)
        {
            // Some rigs like FTDX101 have 6-byte return so we just truncate
            retlvl[3] = 0;
        }

        if (rig->caps->swr_cal.size == 0)
        {
            val->f = rig_raw2val_float(atoi(retlvl), &yaesu_default_swr_cal);
        }
        else
        {
            val->f = rig_raw2val_float(atoi(retlvl), &rig->caps->swr_cal);
        }

        break;

    case RIG_LEVEL_ALC:
        if (retlvl_len > 3)
        {
            // Some rigs like FTDX101 have 6-byte return so we just truncate
            retlvl[3] = 0;
        }

        if (rig->caps->alc_cal.size == 0)
        {
            val->f = rig_raw2val_float(atoi(retlvl), &yaesu_default_alc_cal);
        }
        else
        {
            val->f = rig_raw2val_float(atoi(retlvl), &rig->caps->alc_cal);
        }

        break;

    case RIG_LEVEL_RFPOWER_METER:
    case RIG_LEVEL_RFPOWER_METER_WATTS:
        rig_debug(RIG_DEBUG_VERBOSE, "%s: RFPOWER_METER retlvl=%s\n", __func__, retlvl);

        if (retlvl_len > 3)
        {
            // Some rigs like FTDX101 have 6-byte return so we just truncate
            rig_debug(RIG_DEBUG_VERBOSE, "%s: retlvl of %s getting truncated\n", __func__,
                      retlvl);
            retlvl[3] = 0;
            rig_debug(RIG_DEBUG_VERBOSE, "%s: retlvl truncated to %s\n", __func__, retlvl);
        }

        if (rig->caps->rfpower_meter_cal.size == 0)
        {
            val->f = rig_raw2val_float(atoi(retlvl),
                                       &yaesu_default_rfpower_meter_cal) / (level == RIG_LEVEL_RFPOWER_METER_WATTS ?
                                               1.0 : 100.0);
        }
        else
        {
            val->f = rig_raw2val_float(atoi(retlvl),
                                       &rig->caps->rfpower_meter_cal) / (level == RIG_LEVEL_RFPOWER_METER_WATTS ? 1.0 :
                                               100.0);

            if (priv->rig_id == NC_RIGID_FT2000)
            {
                // we reuse the FT2000D table for the FT2000 so need to divide by 2
                // hopefully this works well otherwise we need a separate table
                val->f /= 2;
            }
        }

        rig_debug(RIG_DEBUG_VERBOSE, "%s: RFPOWER_METER=%s, converted to %f\n",
                  __func__, retlvl, val->f);

        if (level == RIG_LEVEL_RFPOWER_METER && val->f > 1.0)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: val->f(%f) clipped at 1.0\n", __func__,
                      val->f);
            val->f = 1.0;
        }

        break;

    case RIG_LEVEL_COMP_METER:
        if (retlvl_len > 3)
        {
            // Some rigs like FTDX101 have 6-byte return so we just truncate
            retlvl[3] = 0;
        }

        if (rig->caps->comp_meter_cal.size == 0)
        {
            val->f = rig_raw2val_float(atoi(retlvl), &yaesu_default_comp_meter_cal);
        }
        else
        {
            val->f = rig_raw2val_float(atoi(retlvl), &rig->caps->comp_meter_cal);
        }

        break;

    case RIG_LEVEL_VD_METER:
        if (retlvl_len > 3)
        {
            // Some rigs like FTDX101 have 6-byte return so we just truncate
            retlvl[3] = 0;
        }

        if (rig->caps->vd_meter_cal.size == 0)
        {
            val->f = rig_raw2val_float(atoi(retlvl), &yaesu_default_vd_meter_cal);
        }
        else
        {
            val->f = rig_raw2val_float(atoi(retlvl), &rig->caps->vd_meter_cal);
        }

        break;

    case RIG_LEVEL_ID_METER:
        if (retlvl_len > 3)
        {
            // Some rigs like FTDX101 have 6-byte return so we just truncate
            retlvl[3] = 0;
        }

        if (rig->caps->id_meter_cal.size == 0)
        {
            val->f = rig_raw2val_float(atoi(retlvl), &yaesu_default_id_meter_cal);
        }
        else
        {
            val->f = rig_raw2val_float(atoi(retlvl), &rig->caps->id_meter_cal);
        }

        break;

    case RIG_LEVEL_MICGAIN:
        if (is_ftdx1200 || is_ftdx3000 || is_ftdx3000dm || is_ft891 || is_ft991
                || is_ftdx101d
                || is_ftdx101mp
                || is_ftdx10)
        {
            scale = 100.;
        }
        else
        {
            scale = 255.;
        }

        val->f = (float)atoi(retlvl) / scale;
        break;

    case RIG_LEVEL_AF:
        val->f = (float)atoi(retlvl) / 255;
        break;

    case RIG_LEVEL_RF:
        if (is_ft891)
        {
            scale = 30.;
        }
        else
        {
            scale = 255.;
        }

        val->f = (float)atoi(retlvl) / scale;
        break;

    case RIG_LEVEL_SQL:
        if (is_ft891 || is_ft991 || is_ftdx101d || is_ftdx101mp || is_ftdx10)
        {
            scale = 100.;
        }
        else
        {
            scale = 255.;
        }

        val->f = (float)atoi(retlvl) / scale;
        break;

    case RIG_LEVEL_BKINDL:
    {
        int raw_value = atoi(retlvl);
        int millis;
        value_t keyspd;

        if (is_ftdx101d || is_ftdx101mp || is_ftdx10)
        {
            switch (raw_value)
            {
            case 0: millis = 30; break;

            case 1: millis = 50; break;

            case 2: millis = 100; break;

            case 3: millis = 150; break;

            case 4: millis = 200; break;

            case 5: millis = 250; break;

            case 6: millis = 300; break;

            default:
                millis = (raw_value - 6) * 100 + 300;
            }
        }
        else
        {
            // The rest of Yaesu rigs indicate break-in delay directly as milliseconds
            millis = raw_value;
        }

        // Convert milliseconds to 10/ths of dots using the current key speed
        err = newcat_get_level(rig, vfo, RIG_LEVEL_KEYSPD, &keyspd);

        if (err != RIG_OK)
        {
            RETURNFUNC(err);
        }

        val->i = millis_to_dot10ths(millis, keyspd.i);
        break;
    }

    case RIG_LEVEL_STRENGTH:
        if (rig->caps->str_cal.size > 0)
        {
            val->i = round(rig_raw2val(atoi(retlvl), &rig->caps->str_cal));
            break;
        }

        if (is_ftdx1200 || is_ftdx3000 || is_ftdx3000dm || is_ftdx5000 || is_ft891
                || is_ft991
                || is_ftdx101d || is_ftdx101mp || is_ftdx10)
        {
            val->i = round(rig_raw2val(atoi(retlvl), &yaesu_default_str_cal));
        }
        else
        {
            // Some Yaesu rigs return straight S-meter answers
            // Return dbS9 -- does >S9 mean 10dB increments? If not, add to rig driver
            if (val->i > 0)
            {
                val->i = (atoi(retlvl) - 9) * 10;
            }
            else
            {
                val->i = (atoi(retlvl) - 9) * 6;
            }
        }

        break;

    case RIG_LEVEL_RAWSTR:
    case RIG_LEVEL_KEYSPD:
        if (rig->caps->rig_model == RIG_MODEL_TS570D
                || rig->caps->rig_model == RIG_MODEL_TS570S)
        {
            // TS570 uses 010-~060 scale according to manual
            val->i = atoi(retlvl) / 2 + 10;
        }
        else
        {
            val->i = atoi(retlvl);
        }

        break;

    case RIG_LEVEL_IF:
        // IS00+0400
        rig_debug(RIG_DEBUG_TRACE, "%s: ret_data=%s(%d), retlvl=%s\n", __func__,
                  priv->ret_data, (int)strlen(priv->ret_data), retlvl);

        if (strlen(priv->ret_data) == 9)
        {
            int n = sscanf(priv->ret_data, "IS%*c0%d\n", &val->i);

            if (n != 1)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: unable to parse level from  %s\n", __func__,
                          priv->ret_data);
            }
        }
        else
        {
            val->i = atoi(retlvl);
        }

        break;

    case RIG_LEVEL_NR:
        if (is_ft450)
        {
            val->f = (float)(atoi(retlvl) / 11.);
        }
        else
        {
            val->f = (float)(atoi(retlvl) / 15.);
        }

        break;

    case RIG_LEVEL_VOXDELAY:
        val->i = atoi(retlvl);

        if (is_ftdx101d || is_ftdx101mp)
        {
            switch (val->i)
            {
            case 0:  val->i = 0; break; // 30ms=0 we only do tenths

            case 1:  val->i = 0; break; // 50ms=0

            case 2:  val->i = 1; break; // 100ms=1

            case 3:  val->i = 1; break; // 150ms=1

            case 4:  val->i = 2; break; // 200ms=2

            case 5:  val->i = 2; break; // 250ms=2

            default:
                val->i = (val->i - 6) + 3;
                break;
            }
        }
        else
        {
            /* VOX delay, arg int (tenth of seconds), rig in ms */
            val->i /= 10;  // Convert from ms to tenths
        }

        break;

    case RIG_LEVEL_PREAMP:
    {
        int preamp;

        if (retlvl[0] < '0' || retlvl[0] > '9')
        {
            RETURNFUNC(-RIG_EPROTO);
        }

        preamp = retlvl[0] - '0';

        val->i = 0;

        if (preamp > 0)
        {
            for (i = 0; state->preamp[i] != RIG_DBLST_END; i++)
            {
                if (i == preamp - 1)
                {
                    val->i = state->preamp[i];
                    break;
                }
            }
        }

        break;
    }

    case RIG_LEVEL_ATT:
    {
        int att;

        if (retlvl[0] < '0' || retlvl[0] > '9')
        {
            RETURNFUNC(-RIG_EPROTO);
        }

        att = retlvl[0] - '0';

        val->i = 0;

        if (att > 0)
        {
            for (i = 0; state->attenuator[i] != RIG_DBLST_END; i++)
            {
                if (i == att - 1)
                {
                    val->i = state->attenuator[i];
                    break;
                }
            }
        }

        break;
    }

    case RIG_LEVEL_AGC:
        switch (retlvl[0])
        {
        case '0':
            val->i = RIG_AGC_OFF;
            break;

        case '1':
            val->i = RIG_AGC_FAST;
            break;

        case '2':
            val->i = RIG_AGC_MEDIUM;
            break;

        case '3':
            val->i = RIG_AGC_SLOW;
            break;

        case '4':
        case '5':
        case '6':
            val->i = RIG_AGC_AUTO; break;

        default:
            RETURNFUNC(-RIG_EPROTO);
        }

        break;

    case RIG_LEVEL_CWPITCH:
        if (is_ft950 || is_ft2000)
        {
            val->i = (atoi(retlvl) * 50) + 300;
        }
        else
        {
            // Most Yaesu rigs seem to use range of 0-75 to represent pitch of 300..1050 Hz in 10 Hz steps
            val->i = (atoi(retlvl) * 10) + 300;
        }

        break;

    case RIG_LEVEL_METER:
        switch (retlvl[0])
        {
        case '0': val->i = RIG_METER_COMP; break;

        case '1': val->i = RIG_METER_ALC; break;

        case '2': val->i = RIG_METER_PO; break;

        case '3': val->i = RIG_METER_SWR; break;

        case '4': val->i = RIG_METER_IC; break;     /* ID CURRENT */

        case '5': val->i = RIG_METER_VDD; break;    /* Final Amp Voltage */

        default: RETURNFUNC(-RIG_EPROTO);
        }

        break;

    case RIG_LEVEL_NOTCHF:
        val->i = atoi(retlvl) * 10;
        break;

    case RIG_LEVEL_MONITOR_GAIN:
        if (is_ftdx1200 || is_ftdx3000 || is_ftdx3000dm || is_ft891 || is_ft991
                || is_ftdx101d
                || is_ftdx101mp)
        {
            scale = 100.;
        }
        else
        {
            scale = 255.;
        }

        val->f = (float)atoi(retlvl) / scale;
        break;

    case RIG_LEVEL_NB:
        val->f = (float)(atoi(retlvl) / 10.);
        break;

    case RIG_LEVEL_TEMP_METER:
        // return value in centigrade -- first 3 digits
        i = 0;
        sscanf(retlvl, "%3d", &i);
        val->f = i / 255. * 100.;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: retlvl=%s, i=%d, val=%g\n", __func__, retlvl,
                  i, val->f);
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(RIG_OK);
}


int newcat_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    char main_sub_vfo = '0';

    ENTERFUNC;

    /* Set Main or SUB vfo */
    err = newcat_set_vfo_from_alias(rig, &vfo);

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    if (rig->caps->targetable_vfo & RIG_TARGETABLE_FUNC)
    {
        main_sub_vfo = (RIG_VFO_B == vfo || RIG_VFO_SUB == vfo) ? '1' : '0';
    }

    switch (func)
    {
    case RIG_FUNC_ANF:
    {
        pbwidth_t width;
        rmode_t mode = 0;

        if (!newcat_valid_command(rig, "BC"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (is_ft991 || is_ftdx3000 || is_ftdx3000dm || is_ftdx5000 || is_ftdx101d
                || is_ftdx101mp)
        {
            err = newcat_get_mode(rig, vfo, &mode, &width);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BC0%d%c", status ? 1 : 0,
                 cat_term);

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_FUNC && !is_ft2000 && !is_ftdx10)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        // Some Yaesu rigs reject this command in FM mode
        if (is_ft991 || is_ftdx3000 || is_ftdx3000dm || is_ftdx5000 || is_ftdx101d
                || is_ftdx101mp)
        {
            if (mode & RIG_MODE_FM || mode & RIG_MODE_FMN)
            {
                priv->question_mark_response_means_rejected = 1;
            }
        }

        break;
    }

    case RIG_FUNC_MN:
    {
        pbwidth_t width;
        rmode_t mode = 0;

        if (!newcat_valid_command(rig, "BP"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (is_ft991 || is_ftdx3000 || is_ftdx3000dm || is_ftdx5000 || is_ftdx101d
                || is_ftdx101mp)
        {
            newcat_get_mode(rig, vfo, &mode, &width);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BP00%03d%c", status ? 1 : 0,
                 cat_term);

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_FUNC && !is_ft2000 && !is_ftdx10)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        // Some Yaesu rigs reject this command in FM mode
        if (is_ft991 || is_ftdx3000 || is_ftdx3000dm || is_ftdx5000 || is_ftdx101d
                || is_ftdx101mp)
        {
            if (mode & RIG_MODE_FM || mode & RIG_MODE_FMN)
            {
                priv->question_mark_response_means_rejected = 1;
            }
        }

        break;
    }

    case RIG_FUNC_FBKIN:
        if (!newcat_valid_command(rig, "BI"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BI%d%c", status ? 1 : 0,
                 cat_term);
        break;

    case RIG_FUNC_TONE:
        if (!newcat_valid_command(rig, "CT"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CT0%d%c", status ? 2 : 0,
                 cat_term);

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_TONE)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        break;

    case RIG_FUNC_TSQL:
        if (!newcat_valid_command(rig, "CT"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CT0%d%c", status ? 1 : 0,
                 cat_term);

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_TONE)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        break;

    case RIG_FUNC_CSQL:
        if (!newcat_valid_command(rig, "CT"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CT0%d%c", status ? 3 : 0,
                 cat_term);

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_TONE)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        break;

    case RIG_FUNC_LOCK:
        if (!newcat_valid_command(rig, "LK"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (is_ftdx1200 || is_ftdx3000 || is_ftdx3000dm || is_ftdx5000 || is_ftdx101d
                || is_ftdx101mp)
        {
            // These rigs can lock Main/Sub VFO dials individually
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "LK%d%c", status ? 7 : 4,
                     cat_term);
        }
        else
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "LK%d%c", status ? 1 : 0,
                     cat_term);
        }

        break;

    case RIG_FUNC_MON:
        if (!newcat_valid_command(rig, "ML"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "ML0%03d%c", status ? 1 : 0,
                 cat_term);
        break;

    case RIG_FUNC_NB:
        if (!newcat_valid_command(rig, "NB"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "NB0%d%c", status ? 1 : 0,
                 cat_term);

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_MODE)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        break;

    case RIG_FUNC_NB2:
        if (!newcat_valid_command(rig, "NB"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "NB0%d%c", status ? 2 : 0,
                 cat_term);

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_MODE)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        break;

    case RIG_FUNC_NR:
    {
        pbwidth_t width;
        rmode_t mode = 0;

        if (!newcat_valid_command(rig, "NR"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (is_ft991 || is_ftdx3000 || is_ftdx3000dm || is_ftdx5000 || is_ftdx101d
                || is_ftdx101mp)
        {
            newcat_get_mode(rig, vfo, &mode, &width);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "NR0%d%c", status ? 1 : 0,
                 cat_term);

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_MODE)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        // Some Yaesu rigs reject this command in FM mode
        if (is_ft991 || is_ftdx3000 || is_ftdx3000dm || is_ftdx5000 || is_ftdx101d
                || is_ftdx101mp)
        {
            if (mode & RIG_MODE_FM || mode & RIG_MODE_FMN)
            {
                priv->question_mark_response_means_rejected = 1;
            }
        }

        break;
    }

    case RIG_FUNC_COMP:
    {
        pbwidth_t width;
        rmode_t mode = 0;

        if (!newcat_valid_command(rig, "PR"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (is_ft991 || is_ftdx3000 || is_ftdx3000dm || is_ftdx5000 || is_ftdx101d
                || is_ftdx101mp)
        {
            newcat_get_mode(rig, vfo, &mode, &width);
        }

        if (is_ft891 || is_ft991 || is_ftdx1200 || is_ftdx3000 || is_ftdx3000dm
                || is_ftdx101d
                || is_ftdx101mp)
        {
            // There seems to be an error in the manuals for some of these rigs stating that values should be 1 = OFF and 2 = ON, but they are 0 = OFF and 1 = ON instead
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PR0%d%c", status ? 1 : 0,
                     cat_term);
        }
        else
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PR%d%c", status ? 1 : 0,
                     cat_term);
        }

        // Some Yaesu rigs reject this command in AM/FM/RTTY modes
        if (is_ft991 || is_ftdx3000 || is_ftdx3000dm || is_ftdx5000 || is_ftdx101d
                || is_ftdx101mp)
        {
            if (mode & RIG_MODE_AM || mode & RIG_MODE_FM || mode & RIG_MODE_AMN
                    || mode & RIG_MODE_FMN ||
                    mode & RIG_MODE_RTTY || mode & RIG_MODE_RTTYR)
            {
                priv->question_mark_response_means_rejected = 1;
            }
        }

        break;
    }

    case RIG_FUNC_VOX:
        if (!newcat_valid_command(rig, "VX"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VX%d%c", status ? 1 : 0,
                 cat_term);
        break;

    case RIG_FUNC_TUNER:
        if (!newcat_valid_command(rig, "AC"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        priv->question_mark_response_means_rejected = 1;
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AC00%d%c",
                 status == 0 ? 0 : status,
                 cat_term);
        break;

    case RIG_FUNC_RIT:
        if (!newcat_valid_command(rig, "RT"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RT%d%c", status ? 1 : 0,
                 cat_term);
        break;

    case RIG_FUNC_XIT:
        if (!newcat_valid_command(rig, "XT"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "XT%d%c", status ? 1 : 0,
                 cat_term);
        break;

    case RIG_FUNC_APF:
        if (!newcat_valid_command(rig, "CO"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }


        if (is_ftdx101d || is_ftdx101mp)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%c2%04d%c", main_sub_vfo,
                     status ? 1 : 0, cat_term);
        }
        else if (is_ftdx10 || is_ft991 || is_ft891)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO02%04d%c", status ? 1 : 0,
                     cat_term);
        }
        else if (is_ftdx5000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%c0%02d%c", main_sub_vfo,
                     status ? 2 : 0, cat_term);
        }
        else if (is_ftdx3000 || is_ftdx3000dm || is_ftdx1200 || is_ft2000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO00%02d%c", status ? 2 : 0,
                     cat_term);
        }
        else
        {
            RETURNFUNC(-RIG_ENIMPL);
        }

        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    err = newcat_set_cmd(rig);

    // Clear flag after executing command
    priv->question_mark_response_means_rejected = 0;

    RETURNFUNC(err);
}


int newcat_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    int ret_data_len;
    int last_char_index;
    char *retfunc;
    char main_sub_vfo = '0';

    ENTERFUNC;

    if (rig->caps->targetable_vfo & RIG_TARGETABLE_FUNC)
    {
        main_sub_vfo = (RIG_VFO_B == vfo || RIG_VFO_SUB == vfo) ? '1' : '0';
    }

    switch (func)
    {
    case RIG_FUNC_ANF:
    {
        pbwidth_t width;
        rmode_t mode = 0;

        if (!newcat_valid_command(rig, "BC"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (is_ft991 || is_ftdx3000 || is_ftdx3000dm || is_ftdx5000 || is_ftdx101d
                || is_ftdx101mp)
        {
            err = newcat_get_mode(rig, vfo, &mode, &width);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BC0%c", cat_term);

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_MODE)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        // Some Yaesu rigs reject this command in FM mode
        if (is_ft991 || is_ftdx3000 || is_ftdx3000dm || is_ftdx5000 || is_ftdx101d
                || is_ftdx101mp)
        {
            if (mode & RIG_MODE_FM || mode & RIG_MODE_FMN)
            {
                priv->question_mark_response_means_rejected = 1;
            }
        }

        break;
    }

    case RIG_FUNC_MN:
        if (!newcat_valid_command(rig, "BP"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BP00%c", cat_term);

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_MODE)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        break;

    case RIG_FUNC_FBKIN:
        if (!newcat_valid_command(rig, "BI"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BI%c", cat_term);
        break;

    case RIG_FUNC_TONE:
        if (!newcat_valid_command(rig, "CT"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CT0%c", cat_term);

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_TONE)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        break;

    case RIG_FUNC_TSQL:
        if (!newcat_valid_command(rig, "CT"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CT0%c", cat_term);

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_TONE)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        break;

    case RIG_FUNC_CSQL:
        if (!newcat_valid_command(rig, "CT"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CT0%c", cat_term);

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_TONE)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        break;

    case RIG_FUNC_LOCK:
        if (!newcat_valid_command(rig, "LK"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "LK%c", cat_term);
        break;

    case RIG_FUNC_MON:
        if (!newcat_valid_command(rig, "ML"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "ML0%c", cat_term);
        break;

    case RIG_FUNC_NB:
    case RIG_FUNC_NB2:
        if (!newcat_valid_command(rig, "NB"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "NB0%c", cat_term);

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_FUNC)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        break;

    case RIG_FUNC_NR:
        if (!newcat_valid_command(rig, "NR"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "NR0%c", cat_term);

        if (rig->caps->targetable_vfo & RIG_TARGETABLE_FUNC)
        {
            priv->cmd_str[2] = main_sub_vfo;
        }

        break;

    case RIG_FUNC_COMP:
        if (!newcat_valid_command(rig, "PR"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (is_ftdx1200 || is_ftdx3000 || is_ftdx3000dm || is_ft891 || is_ft991
                || is_ftdx101d
                || is_ftdx101mp)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PR0%c", cat_term);
        }
        else
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PR%c", cat_term);
        }

        break;

    case RIG_FUNC_VOX:
        if (!newcat_valid_command(rig, "VX"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VX%c", cat_term);
        break;

    case RIG_FUNC_TUNER:
        if (!newcat_valid_command(rig, "AC"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AC%c", cat_term);
        break;

    case RIG_FUNC_RIT:
        if (!newcat_valid_command(rig, "RT"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RT%c", cat_term);
        break;

    case RIG_FUNC_XIT:
        if (!newcat_valid_command(rig, "XT"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "XT%c", cat_term);
        break;

    case RIG_FUNC_APF:
        if (!newcat_valid_command(rig, "CO"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (is_ftdx101d || is_ftdx101mp)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%c2%c", main_sub_vfo,
                     cat_term);
        }
        else if (is_ftdx10 || is_ft991 || is_ft891)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO02%c", cat_term);
        }
        else if (is_ftdx5000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%c0%c", main_sub_vfo,
                     cat_term);
        }
        else if (is_ftdx3000 || is_ftdx3000dm || is_ftdx1200 || is_ft2000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO00%c", cat_term);
        }
        else
        {
            RETURNFUNC(-RIG_ENIMPL);
        }

        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    err = newcat_get_cmd(rig);

    // Clear flag after executing command
    priv->question_mark_response_means_rejected = 0;

    if (err != RIG_OK)
    {
        RETURNFUNC(err);
    }

    ret_data_len = strlen(priv->ret_data);

    /* skip command */
    retfunc = priv->ret_data + strlen(priv->cmd_str) - 1;
    /* chop term */
    priv->ret_data[ret_data_len - 1] = '\0';

    last_char_index = strlen(retfunc) - 1;

    rig_debug(RIG_DEBUG_TRACE, "%s: retfunc='%s'\n", __func__, retfunc);

    switch (func)
    {
    case RIG_FUNC_MN:
        *status = (retfunc[2] == '0') ? 0 : 1;
        break;

    case RIG_FUNC_COMP:
        *status = (retfunc[0] == '0') ? 0 : 1;
        break;

    case RIG_FUNC_MON:
        // The number of digits varies by rig, but the last digit indicates the status always
        *status = (retfunc[last_char_index] == '0') ? 0 : 1;
        break;

    case RIG_FUNC_LOCK:
        if (is_ftdx1200 || is_ftdx3000 || is_ftdx3000dm || is_ftdx5000 || is_ftdx101d
                || is_ftdx101mp)
        {
            // These rigs can lock Main/Sub VFO dials individually
            *status = (retfunc[0] == '0' || retfunc[0] == '4') ? 0 : 1;
        }
        else
        {
            *status = (retfunc[0] == '0') ? 0 : 1;
        }

        break;

    case RIG_FUNC_ANF:
    case RIG_FUNC_FBKIN:
    case RIG_FUNC_NR:
    case RIG_FUNC_VOX:
        *status = (retfunc[0] == '0') ? 0 : 1;
        break;

    case RIG_FUNC_NB:
        *status = (retfunc[0] == '1') ? 1 : 0;
        break;

    case RIG_FUNC_NB2:
        *status = (retfunc[0] == '2') ? 1 : 0;
        break;

    case RIG_FUNC_TONE:
        *status = (retfunc[0] == '2') ? 1 : 0;
        break;

    case RIG_FUNC_TSQL:
        *status = (retfunc[0] == '1') ? 1 : 0;
        break;

    case RIG_FUNC_CSQL:
        *status = (retfunc[0] == '3') ? 1 : 0;
        break;

    case RIG_FUNC_TUNER:
        *status = (retfunc[2] == '1') ? 1 : 0;
        break;

    case RIG_FUNC_RIT:
        *status = (retfunc[0] == '1') ? 1 : 0;
        break;

    case RIG_FUNC_XIT:
        *status = (retfunc[0] == '1') ? 1 : 0;
        break;

    case RIG_FUNC_APF:
        if (is_ftdx101d || is_ftdx101mp)
        {
            *status = (retfunc[last_char_index] == '1') ? 1 : 0;
        }
        else if (is_ftdx10 || is_ft991 || is_ft891)
        {
            *status = (retfunc[last_char_index] == '1') ? 1 : 0;
        }
        else if (is_ftdx5000)
        {
            *status = (retfunc[last_char_index] == '2') ? 1 : 0;
        }
        else if (is_ftdx3000 || is_ftdx3000dm || is_ftdx1200 || is_ft2000)
        {
            *status = (retfunc[last_char_index] == '2') ? 1 : 0;
        }
        else
        {
            RETURNFUNC(-RIG_ENIMPL);
        }

        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(RIG_OK);
}


int newcat_set_parm(RIG *rig, setting_t parm, value_t val)
{
    ENTERFUNC;

    RETURNFUNC(-RIG_ENAVAIL);
}


int newcat_get_parm(RIG *rig, setting_t parm, value_t *val)
{
    ENTERFUNC;

    RETURNFUNC(-RIG_ENAVAIL);
}


int newcat_set_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t val)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;

    ENTERFUNC;

    switch (token)
    {
    case TOK_ROOFING_FILTER:
        RETURNFUNC(set_roofing_filter(rig, vfo, val.i));

    case TOK_KEYER:
        if (!newcat_valid_command(rig, "ML"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "KR%d%c", val.i ? 1 : 0,
                 cat_term);

        RETURNFUNC(newcat_set_cmd(rig));

    case TOK_APF_FREQ:
        RETURNFUNC(newcat_set_apf_frequency(rig, vfo, val.f));

    case TOK_APF_WIDTH:
        RETURNFUNC(newcat_set_apf_width(rig, vfo, val.i));

    case TOK_CONTOUR:
        RETURNFUNC(newcat_set_contour(rig, vfo, val.i));

    case TOK_CONTOUR_FREQ:
        RETURNFUNC(newcat_set_contour_frequency(rig, vfo, val.f));

    case TOK_CONTOUR_LEVEL:
        RETURNFUNC(newcat_set_contour_level(rig, vfo, val.f));

    case TOK_CONTOUR_WIDTH:
        RETURNFUNC(newcat_set_contour_width(rig, vfo, val.f));

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported ext level %s\n", __func__,
                  rig_strlevel(token));
        RETURNFUNC(-RIG_EINVAL);
    }
}

int newcat_get_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t *val)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    char *result;
    int retval;
    int value;

    ENTERFUNC;

    switch (token)
    {
    case TOK_ROOFING_FILTER:
    {
        struct newcat_roofing_filter *roofing_filter;
        retval = get_roofing_filter(rig, vfo, &roofing_filter);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        val->i = roofing_filter->index;
        break;
    }

    case TOK_KEYER:
        if (!newcat_valid_command(rig, "KR"))
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "KR%c", cat_term);

        retval = newcat_get_cmd(rig);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        /* skip command */
        result = priv->ret_data + strlen(priv->cmd_str) - 1;
        /* chop term */
        priv->ret_data[strlen(priv->ret_data) - 1] = '\0';

        val->i = result[0] == '0' ? 0 : 1;
        break;

    case TOK_APF_FREQ:
        retval = newcat_get_apf_frequency(rig, vfo, &value);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        val->f = value;
        break;

    case TOK_APF_WIDTH:
        retval = newcat_get_apf_width(rig, vfo, &value);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        val->i = value;
        break;

    case TOK_CONTOUR:
        retval = newcat_get_contour(rig, vfo, &value);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        val->i = value;
        break;

    case TOK_CONTOUR_WIDTH:
        retval = newcat_get_contour_width(rig, vfo, &value);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        val->f = value;
        break;

    case TOK_CONTOUR_FREQ:
        retval = newcat_get_contour_frequency(rig, vfo, &value);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        val->f = value;
        break;

    case TOK_CONTOUR_LEVEL:
        retval = newcat_get_contour_level(rig, vfo, &value);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        val->f = value;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported ext level %s\n", __func__,
                  rig_strlevel(token));
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(RIG_OK);
}

int newcat_set_ext_parm(RIG *rig, token_t token, value_t val)
{
    ENTERFUNC;

    RETURNFUNC(-RIG_ENAVAIL);
}


int newcat_get_ext_parm(RIG *rig, token_t token, value_t *val)
{
    ENTERFUNC;

    RETURNFUNC(-RIG_ENAVAIL);
}


int newcat_send_dtmf(RIG *rig, vfo_t vfo, const char *digits)
{
    ENTERFUNC;

    RETURNFUNC(-RIG_ENAVAIL);
}


int newcat_recv_dtmf(RIG *rig, vfo_t vfo, char *digits, int *length)
{
    ENTERFUNC;

    RETURNFUNC(-RIG_ENAVAIL);
}


int newcat_send_morse(RIG *rig, vfo_t vfo, const char *msg)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int rc;
    char *s = strdup(msg);
    ENTERFUNC;

    if (newcat_is_rig(rig, RIG_MODEL_FT450))
    {
        // 450 manual says 1/2/3 playback needs P1=6/7/8
        s[0] += 5;
    }
    else
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "KY%c%c", s[0], cat_term);
    }

    rc = newcat_set_cmd(rig);
    free(s);
    RETURNFUNC(rc);
}


int newcat_set_bank(RIG *rig, vfo_t vfo, int bank)
{
    ENTERFUNC;

    RETURNFUNC(-RIG_ENAVAIL);
}


int newcat_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err, i;
    ncboolean restore_vfo;
    chan_t *chan_list;
    channel_t valid_chan;
    channel_cap_t *mem_caps = NULL;

    ENTERFUNC;

    if (!newcat_valid_command(rig, "MC"))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    chan_list = rig->caps->chan_list;

    for (i = 0; i < HAMLIB_CHANLSTSIZ && !RIG_IS_CHAN_END(chan_list[i]); i++)
    {
        if (ch >= chan_list[i].startc &&
                ch <= chan_list[i].endc)
        {
            mem_caps = &chan_list[i].mem_caps;
            break;
        }
    }

    /* Test for valid usable channel, skip if empty */
    memset(&valid_chan, 0, sizeof(channel_t));
    valid_chan.channel_num = ch;
    err = newcat_get_channel(rig, vfo, &valid_chan, 1);

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    if (valid_chan.freq <= 1.0)
    {
        mem_caps = NULL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: valChan Freq = %f\n", __func__,
              valid_chan.freq);

    /* Out of Range, or empty */
    if (!mem_caps)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    /* set to usable vfo if needed */
    err = newcat_set_vfo_from_alias(rig, &vfo);

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    /* Restore to VFO mode or leave in Memory Mode */
    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_MAIN:
        /* Jump back from memory channel */
        restore_vfo = TRUE;
        break;

    case RIG_VFO_MEM:
        /* Jump from channel to channel in memory mode */
        restore_vfo = FALSE;
        break;

    case RIG_VFO_B:
    case RIG_VFO_SUB:
    default:
        /* Only works with VFO A */
        RETURNFUNC(-RIG_ENTARGET);
    }

    /* Set Memory Channel Number ************** */
    rig_debug(RIG_DEBUG_TRACE, "channel_num = %d, vfo = %s\n", ch, rig_strvfo(vfo));

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MC%03d%c", ch, cat_term);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);

    priv->question_mark_response_means_rejected = 1;
    err = newcat_set_cmd(rig);
    priv->question_mark_response_means_rejected = 0;

    if (err != RIG_OK)
    {
        RETURNFUNC(err);
    }

    /* Restore VFO even if setting to blank memory channel */
    if (restore_vfo)
    {
        err = newcat_vfomem_toggle(rig);

        if (err != RIG_OK)
        {
            RETURNFUNC(err);
        }
    }

    RETURNFUNC(RIG_OK);
}


int newcat_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;

    ENTERFUNC;

    if (!newcat_valid_command(rig, "MC"))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MC%c", cat_term);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);

    /* Get Memory Channel Number */
    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        RETURNFUNC(err);
    }

    *ch = atoi(priv->ret_data + 2);

    RETURNFUNC(RIG_OK);
}

int newcat_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    char main_sub_vfo = '0';

    ENTERFUNC;

    /* Set Main or SUB vfo */
    err = newcat_set_vfo_from_alias(rig, &vfo);

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    if (rig->caps->targetable_vfo & RIG_TARGETABLE_MODE)
    {
        main_sub_vfo = (RIG_VFO_B == vfo || RIG_VFO_SUB == vfo) ? '1' : '0';
    }

    switch (op)
    {
    case RIG_OP_TUNE:
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AC002%c", cat_term);
        break;

    case RIG_OP_CPY:
        if (newcat_is_rig(rig, RIG_MODEL_FT450))
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VV%c", cat_term);
        }
        else
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AB%c", cat_term);
        }

        break;

    case RIG_OP_XCHG:
    case RIG_OP_TOGGLE:
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SV%c", cat_term);
        break;

    case RIG_OP_UP:
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "UP%c", cat_term);
        break;

    case RIG_OP_DOWN:
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DN%c", cat_term);
        break;

    case RIG_OP_BAND_UP:
        if (main_sub_vfo == 1)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BU1%c", cat_term);
        }
        else
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BU0%c", cat_term);
        }

        break;

    case RIG_OP_BAND_DOWN:
        if (main_sub_vfo == 1)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BD1%c", cat_term);
        }
        else
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BD0%c", cat_term);
        }

        break;

    case RIG_OP_FROM_VFO:
        /* VFOA ! */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AM%c", cat_term);
        break;

    case RIG_OP_TO_VFO:
        /* VFOA ! */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MA%c", cat_term);
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(newcat_set_cmd(rig));
}


int newcat_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int retval;

    ENTERFUNC;

    if (scan != RIG_SCAN_VFO) { RETURNFUNC2(-RIG_EINVAL); }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SC%d%c",
             scan == RIG_SCAN_STOP ? 0 : ch, cat_term);

    if (RIG_OK != (retval = newcat_set_cmd(rig)))
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s:%d command err = %d\n", __func__, __LINE__,
                  retval);
        RETURNFUNC2(retval);
    }

    RETURNFUNC2(retval);
}


int newcat_set_trn(RIG *rig, int trn)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    char c;

    ENTERFUNC;

    if (!newcat_valid_command(rig, "AI"))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (trn  == RIG_TRN_OFF)
    {
        c = '0';
    }
    else
    {
        c = '1';
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AI%c%c", c, cat_term);

    rig_debug(RIG_DEBUG_TRACE, "cmd_str = %s\n", priv->cmd_str);

    RETURNFUNC(newcat_set_cmd(rig));
}


int newcat_get_trn(RIG *rig, int *trn)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    char c;
    char command[] = "AI";

    ENTERFUNC;

    if (!newcat_valid_command(rig, command))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%c", command, cat_term);

    /* Get Auto Information */
    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        // if we failed to get AI we turn it off and try again
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s0%c", command, cat_term);
        hl_usleep(500 * 1000); // is 500ms enough for the rig to stop sending info?
        newcat_set_cmd(rig); // don't care about any errors here
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%c", command, cat_term);
        err = newcat_get_cmd(rig);
        RETURNFUNC(err);
    }

    c = priv->ret_data[2];

    if (c == '0')
    {
        *trn = RIG_TRN_OFF;
    }
    else
    {
        *trn = RIG_TRN_RIG;
    }

    RETURNFUNC(RIG_OK);
}


int newcat_decode_event(RIG *rig)
{
    ENTERFUNC;

    RETURNFUNC(-RIG_ENAVAIL);
}


int newcat_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    struct rig_state *state = &rig->state;
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err, i;
    int rxit;
    char c_rit, c_xit, c_mode, c_vfo, c_tone, c_rptr_shift;
    tone_t tone;
    ncboolean restore_vfo;
    chan_t *chan_list;
    channel_cap_t *mem_caps = NULL;

    ENTERFUNC;


    if (!newcat_valid_command(rig, "MW"))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    chan_list = rig->caps->chan_list;

    for (i = 0; i < HAMLIB_CHANLSTSIZ && !RIG_IS_CHAN_END(chan_list[i]); i++)
    {
        if (chan->channel_num >= chan_list[i].startc &&
                chan->channel_num <= chan_list[i].endc &&
                // writable memory types... NOT 60-METERS or READ-ONLY channels
                (chan_list[i].type == RIG_MTYPE_MEM ||
                 chan_list[i].type == RIG_MTYPE_EDGE))
        {
            mem_caps = &chan_list[i].mem_caps;
            break;
        }
    }

    /* Out of Range */
    if (!mem_caps)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    /* Set Restore to VFO or leave in memory mode */
    switch (state->current_vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_B:
        /* Jump back from memory channel */
        restore_vfo = TRUE;
        break;

    case RIG_VFO_MEM:
        /* Jump from channel to channel in memory mode */
        restore_vfo = FALSE;
        break;

    case RIG_VFO_SUB:
    default:
        /* Only works with VFO Main */
        RETURNFUNC(-RIG_ENTARGET);
    }

    /* Write Memory Channel ************************* */
    /*  Clarifier TX, RX */
    if (chan->rit)
    {
        rxit = chan->rit;
        c_rit = '1';
        c_xit = '0';
    }
    else if (chan->xit)
    {
        rxit = chan->xit;
        c_rit = '0';
        c_xit = '1';
    }
    else
    {
        rxit  =  0;
        c_rit = '0';
        c_xit = '0';
    }

    /* MODE */
    c_mode = newcat_modechar(chan->mode);

    /* VFO Fixed */
    c_vfo = '0';

    /* CTCSS Tone / Sql */
    if (chan->ctcss_tone)
    {
        c_tone = '2';
        tone = chan->ctcss_tone;
    }
    else if (chan->ctcss_sql)
    {
        c_tone = '1';
        tone = chan->ctcss_sql;
    }
    else
    {
        c_tone = '0';
        tone = 0;
    }

    for (i = 0; rig->caps->ctcss_list[i] != 0; i++)
        if (tone == rig->caps->ctcss_list[i])
        {
            tone = i;

            if (tone > 49)
            {
                tone = 0;
            }

            break;
        }

    /* Repeater Shift */
    switch (chan->rptr_shift)
    {
    case RIG_RPT_SHIFT_NONE:  c_rptr_shift = '0'; break;

    case RIG_RPT_SHIFT_PLUS:  c_rptr_shift = '1'; break;

    case RIG_RPT_SHIFT_MINUS: c_rptr_shift = '2'; break;

    default: c_rptr_shift = '0';
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str),
             "MW%03d%08d%+.4d%c%c%c%c%c%02u%c%c",
             chan->channel_num, (int)chan->freq, rxit, c_rit, c_xit, c_mode, c_vfo,
             c_tone, tone, c_rptr_shift, cat_term);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);

    /* Set Memory Channel */
    priv->question_mark_response_means_rejected = 1;
    err = newcat_set_cmd(rig);
    priv->question_mark_response_means_rejected = 0;

    if (err != RIG_OK)
    {
        RETURNFUNC(err);
    }

    /* Restore VFO ********************************** */
    if (restore_vfo)
    {
        err = newcat_vfomem_toggle(rig);
        RETURNFUNC(err);
    }

    RETURNFUNC(RIG_OK);
}


int newcat_get_channel(RIG *rig, vfo_t vfo, channel_t *chan, int read_only)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    char *retval;
    char c, c2;
    int err, i;
    chan_t *chan_list;
    channel_cap_t *mem_caps = NULL;

    ENTERFUNC;

    if (!newcat_valid_command(rig, "MR"))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    chan_list = rig->caps->chan_list;

    for (i = 0; i < HAMLIB_CHANLSTSIZ && !RIG_IS_CHAN_END(chan_list[i]); i++)
    {
        if (chan->channel_num >= chan_list[i].startc &&
                chan->channel_num <= chan_list[i].endc)
        {
            mem_caps = &chan_list[i].mem_caps;
            break;
        }
    }

    /* Out of Range */
    if (!mem_caps)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    rig_debug(RIG_DEBUG_TRACE, "sizeof(channel_t) = %d\n", (int)sizeof(channel_t));
    rig_debug(RIG_DEBUG_TRACE, "sizeof(priv->cmd_str) = %d\n",
              (int)sizeof(priv->cmd_str));

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MR%03d%c", chan->channel_num,
             cat_term);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);


    /* Get Memory Channel */
    priv->question_mark_response_means_rejected = 1;
    err = newcat_get_cmd(rig);
    priv->question_mark_response_means_rejected = 0;

    if (RIG_OK != err)
    {
        if (-RIG_ERJCTED == err)
        {
            /* Invalid channel, has not been set up, make sure freq is
               0 to indicate empty channel */
            chan->freq = 0.;
            RETURNFUNC(RIG_OK);
        }

        RETURNFUNC(err);
    }

    /* ret_data string to channel_t struct :: this will destroy ret_data */

    /* rptr_shift P10 ************************ */
    retval = priv->ret_data + 25;

    switch (*retval)
    {
    case '0': chan->rptr_shift = RIG_RPT_SHIFT_NONE;  break;

    case '1': chan->rptr_shift = RIG_RPT_SHIFT_PLUS;  break;

    case '2': chan->rptr_shift = RIG_RPT_SHIFT_MINUS; break;

    default:  chan->rptr_shift = RIG_RPT_SHIFT_NONE;
    }

    *retval = '\0';

    /* CTCSS Encoding P8 ********************* */
    retval = priv->ret_data + 22;
    c = *retval;

    /* CTCSS Tone P9 ************************* */
    chan->ctcss_tone = 0;
    chan->ctcss_sql  = 0;
    retval = priv->ret_data + 23;
    i = atoi(retval);

    if (c == '1')
    {
        chan->ctcss_sql = rig->caps->ctcss_list[i];
    }
    else if (c == '2')
    {
        chan->ctcss_tone = rig->caps->ctcss_list[i];
    }

    /* vfo, mem, P7 ************************** */
    retval = priv->ret_data + 21;

    if (*retval == '1')
    {
        chan->vfo = RIG_VFO_MEM;
    }
    else
    {
        chan->vfo = RIG_VFO_CURR;
    }

    /* MODE P6 ******************************* */
    chan->width = 0;

    retval = priv->ret_data + 20;
    chan->mode = newcat_rmode(*retval);

    if (chan->mode == RIG_MODE_NONE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unknown mode=%c\n", __func__, *retval);
        chan->mode = RIG_MODE_LSB;
    }

    /* Clarifier TX P5 *********************** */
    retval = priv->ret_data + 19;
    c2 = *retval;

    /* Clarifier RX P4 *********************** */
    retval = priv->ret_data + 18;
    c = *retval;
    *retval = '\0';

    /* Clarifier Offset P3 ******************* */
    chan->rit = 0;
    chan->xit = 0;
    retval = priv->ret_data + 13;

    if (c == '1')
    {
        chan->rit = atoi(retval);
    }
    else if (c2 == '1')
    {
        chan->xit = atoi(retval);
    }

    *retval = '\0';

    /* Frequency P2 ************************** */
    retval = priv->ret_data + 5;
    chan->freq = atof(retval);

    if (!read_only)
    {
        // Set rig to channel values
        rig_debug(RIG_DEBUG_ERR,
                  "%s: please contact hamlib mailing list to implement this\n", __func__);
        rig_debug(RIG_DEBUG_ERR,
                  "%s: need to know if rig updates when channel read or not\n", __func__);
        RETURNFUNC(-RIG_ENIMPL);
    }

    RETURNFUNC(RIG_OK);
}


const char *newcat_get_info(RIG *rig)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    static char idbuf[129]; /* extra large static string array */

    /* Build the command string */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "ID;");

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);

    /* Get Identification Channel */
    if (RIG_OK != newcat_get_cmd(rig))
    {
        return (NULL);
    }

    priv->ret_data[6] = '\0';
    SNPRINTF(idbuf, sizeof(idbuf), "%s", priv->ret_data);

    return (idbuf);
}


/*
 * newcat_valid_command
 *
 * Determine whether or not the command is valid for the specified
 * rig.  This function should be called before sending the command
 * to the rig to make it easier to differentiate invalid and illegal
 * commands (for a rig).
 */

ncboolean newcat_valid_command(RIG *rig, char const *const command)
{
    const struct rig_caps *caps;
    int search_high;
    int search_low;

    rig_debug(RIG_DEBUG_TRACE, "%s %s\n", __func__, command);

    caps = rig->caps;

    if (!caps)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Rig capabilities not valid\n", __func__);
        RETURNFUNC2(FALSE);
    }

    if (!is_ft450 && !is_ft950 && !is_ft891 && !is_ft991 && !is_ft2000
            && !is_ftdx5000 && !is_ftdx9000 && !is_ftdx1200 && !is_ftdx3000 && !is_ftdx101d
            && !is_ftdx101mp && !is_ftdx10 && !is_ft710)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: '%s' is unknown\n", __func__, caps->model_name);
        RETURNFUNC2(FALSE);
    }

    /*
     * Make sure the command is known, and then check to make sure
     * is it valid for the rig.
     */

    search_low = 0;
    search_high = valid_commands_count;

    while (search_low <= search_high)
    {
        int search_test;
        int search_index;

        search_index = (search_low + search_high) / 2;
        search_test = strcmp(valid_commands[search_index].command, command);

        if (search_test > 0)
        {
            search_high = search_index - 1;
        }
        else if (search_test < 0)
        {
            search_low = search_index + 1;
        }
        else
        {
            /*
             * The command is valid.  Now make sure it is supported by the rig.
             */
            if (is_ft450 && valid_commands[search_index].ft450)
            {
                RETURNFUNC2(TRUE);
            }
            else if (is_ft891 && valid_commands[search_index].ft891)
            {
                RETURNFUNC2(TRUE);
            }
            else if (is_ft950 && valid_commands[search_index].ft950)
            {
                RETURNFUNC2(TRUE);
            }
            else if (is_ft991 && valid_commands[search_index].ft991)
            {
                RETURNFUNC2(TRUE);
            }
            else if (is_ft2000 && valid_commands[search_index].ft2000)
            {
                RETURNFUNC2(TRUE);
            }
            else if (is_ftdx5000 && valid_commands[search_index].ft5000)
            {
                RETURNFUNC2(TRUE);
            }
            else if (is_ftdx9000 && valid_commands[search_index].ft9000)
            {
                RETURNFUNC2(TRUE);
            }
            else if (is_ftdx1200 && valid_commands[search_index].ft1200)
            {
                RETURNFUNC2(TRUE);
            }
            else if (is_ftdx3000 && valid_commands[search_index].ft3000)
            {
                RETURNFUNC2(TRUE);
            }
            else if (is_ftdx3000dm && valid_commands[search_index].ft3000)
            {
                RETURNFUNC2(TRUE);
            }
            else if (is_ftdx101d && valid_commands[search_index].ft101d)
            {
                RETURNFUNC2(TRUE);
            }
            else if (is_ftdx101mp && valid_commands[search_index].ft101mp)
            {
                RETURNFUNC2(TRUE);
            }
            else if (is_ftdx10 && valid_commands[search_index].ftdx10)
            {
                RETURNFUNC2(TRUE);
            }
            else if (is_ft710 && valid_commands[search_index].ft710)
            {
                RETURNFUNC2(TRUE);
            }
            else
            {
                rig_debug(RIG_DEBUG_TRACE, "%s: '%s' command '%s' not supported\n",
                          __func__, caps->model_name, command);
                RETURNFUNC2(FALSE);
            }
        }
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: '%s' command '%s' not valid\n",
              __func__, caps->model_name, command);
    RETURNFUNC2(FALSE);
}


ncboolean newcat_is_rig(RIG *rig, rig_model_t model)
{
    ncboolean is_rig;

    //a bit too verbose so disable this unless needed
    //rig_debug(RIG_DEBUG_TRACE, "%s(%d):%s called\n", __FILE__, __LINE__, __func__);
    is_rig = (model == rig->caps->rig_model) ? TRUE : FALSE;

    return (is_rig); // RETURN is too verbose here
}


/*
 * newcat_set_tx_vfo does not set priv->curr_vfo
 * does set rig->state.tx_vfo
 */
int newcat_set_tx_vfo(RIG *rig, vfo_t tx_vfo)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    char p1;
    char *command = "FT";

    ENTERFUNC;

    if (!newcat_valid_command(rig, "FT"))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    err = newcat_set_vfo_from_alias(rig, &tx_vfo);

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    switch (tx_vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_MAIN:
        p1 = '0';
        break;

    case RIG_VFO_B:
    case RIG_VFO_SUB:
        p1 = '1';
        break;

    case RIG_VFO_MEM:

        /* VFO A */
        if (priv->current_mem == NC_MEM_CHANNEL_NONE)
        {
            RETURNFUNC(RIG_OK);
        }
        else    /* Memory Channel mode */
        {
            p1 = '0';
        }

        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    /* TODO: G4WJS - FT-450 only has toggle command so not sure how to
       definitively set the TX VFO (VS; doesn't seem to help
       either) */
    if (newcat_is_rig(rig, RIG_MODEL_FT950) ||
            newcat_is_rig(rig, RIG_MODEL_FT2000) ||
            newcat_is_rig(rig, RIG_MODEL_FTDX5000) ||
            newcat_is_rig(rig, RIG_MODEL_FTDX1200) ||
            newcat_is_rig(rig, RIG_MODEL_FT991) ||
            newcat_is_rig(rig, RIG_MODEL_FTDX10) ||
            newcat_is_rig(rig, RIG_MODEL_FTDX3000))
    {
        HAMLIB_TRACE;
        p1 = p1 + 2;    /* use non-Toggle commands */

        // If VFOB is active then we change VFOB with FT3 instead of VFOA
        if (rig->state.current_vfo == RIG_VFO_B) { p1++; }
    }

    if (is_ftdx101d || is_ftdx101mp)
    {
        // what other Yaeus rigs should be using this?
        // The DX101D returns FT0 when in split and not transmitting
        command = "ST";
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%c%c", command, p1, cat_term);

    rig_debug(RIG_DEBUG_TRACE, "cmd_str = %s, vfo=%s\n", priv->cmd_str,
              rig_strvfo(tx_vfo));

    rig->state.tx_vfo = tx_vfo;

    RETURNFUNC(newcat_set_cmd(rig));
}


/*
 * newcat_get_tx_vfo does not set priv->curr_vfo
 */
int newcat_get_tx_vfo(RIG *rig, vfo_t *tx_vfo)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    char c;
    rmode_t vfo_mode;
    char const *command = "FT";

    ENTERFUNC;

    if (is_ftdx101d || is_ftdx101mp)
    {
        // what other Yaeus rigs should be using this?
        // The DX101D returns FT0 when in split and not transmitting
        command = "ST";
    }

    if (!newcat_valid_command(rig, command))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%c", command, cat_term);

    /* Get TX VFO */
    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        RETURNFUNC(err);
    }

    c = priv->ret_data[2];

    switch (c)
    {
    case '0':
        if (rig->state.vfo_list & RIG_VFO_MAIN) { *tx_vfo = RIG_VFO_MAIN; }
        else { *tx_vfo = RIG_VFO_A; }

        rig->state.cache.split = 0;
        break;

    case '1' :
        if (rig->state.vfo_list & RIG_VFO_SUB) { *tx_vfo = RIG_VFO_SUB; }
        else { *tx_vfo = RIG_VFO_B; }

        rig->state.cache.split = 1;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unknown tx_vfo=%c from index 2 of %s\n", __func__,
                  c, priv->ret_data);
        RETURNFUNC(-RIG_EPROTO);
    }

    /* Check to see if RIG is in MEM mode */
    err = newcat_get_vfo_mode(rig, RIG_VFO_A, &vfo_mode);

    if (err != RIG_OK)
    {
        RETURNFUNC(err);
    }

    if (vfo_mode == RIG_VFO_MEM && *tx_vfo == RIG_VFO_A)
    {
        *tx_vfo = RIG_VFO_MEM;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: tx_vfo = %s\n", __func__, rig_strvfo(*tx_vfo));

    RETURNFUNC(RIG_OK);
}


int newcat_set_vfo_from_alias(RIG *rig, vfo_t *vfo)
{

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: alias vfo = %s\n", __func__, rig_strvfo(*vfo));

    if (*vfo == RIG_VFO_NONE)
    {
        int rc = rig_get_vfo(rig, vfo);

        if (rc != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: rig_get_vfo failed: %s\n", __func__,
                      rig_strvfo(*vfo));
            RETURNFUNC(rc);
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: vfo==None so get vfo=%s\n", __func__,
                  rig_strvfo(*vfo));
    }

    switch (*vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_B:
    case RIG_VFO_MEM:
        /* passes through */
        break;

    case RIG_VFO_CURR:  /* RIG_VFO_RX == RIG_VFO_CURR */
    case RIG_VFO_VFO:
        *vfo = rig->state.current_vfo;
        break;

    case RIG_VFO_TX:

        /* set to another vfo for split or uplink */
        if (rig->state.vfo_list & RIG_VFO_MAIN)
        {
            *vfo = (rig->state.current_vfo == RIG_VFO_SUB) ? RIG_VFO_MAIN : RIG_VFO_SUB;
        }
        else
        {
            *vfo = (rig->state.current_vfo == RIG_VFO_B) ? RIG_VFO_A : RIG_VFO_B;
        }

        break;

    case RIG_VFO_MAIN:
        *vfo = RIG_VFO_MAIN;
        break;

    case RIG_VFO_SUB:
        *vfo = RIG_VFO_SUB;
        break;

    default:
        rig_debug(RIG_DEBUG_TRACE, "Unrecognized.  vfo= %s\n", rig_strvfo(*vfo));
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(RIG_OK);
}

/*
 *  Found newcat_set_level() floating point math problem
 *  Using rigctl on FT950 I was trying to set RIG_LEVEL_COMP to 12
 *  I kept setting it to 11.  I wrote some test software and
 *  found out that 0.12 * 100 = 11 with my setup.
 *  Compiler is gcc 4.2.4, CPU is AMD X2
 *  This works somewhat but Find a better way.
 *  The newcat_get_level() seems to work correctly.
 *  Terry KJ4EED
 *
 */
int newcat_scale_float(int scale, float fval)
{
    float f;
    float fudge = 0.003;

    if ((fval + fudge) > 1.0)
    {
        f = scale * fval;
    }
    else
    {
        f = scale * (fval + fudge);
    }

    return (int) f; // RETURN is too verbose here
}


int newcat_set_narrow(RIG *rig, vfo_t vfo, ncboolean narrow)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    char c;
    char main_sub_vfo = '0';

    ENTERFUNC;

    if (!newcat_valid_command(rig, "NA"))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    err = newcat_set_vfo_from_alias(rig, &vfo);

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    if (rig->caps->targetable_vfo & RIG_TARGETABLE_MODE)
    {
        main_sub_vfo = (RIG_VFO_B == vfo || RIG_VFO_SUB == vfo) ? '1' : '0';
    }

    if (narrow == TRUE)
    {
        c = '1';
    }
    else
    {
        c = '0';
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "NA%c%c%c", main_sub_vfo, c,
             cat_term);

    rig_debug(RIG_DEBUG_TRACE, "cmd_str = %s\n", priv->cmd_str);

    RETURNFUNC(newcat_set_cmd(rig));
}


int newcat_get_narrow(RIG *rig, vfo_t vfo, ncboolean *narrow)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    char c;
    char command[] = "NA";
    char main_sub_vfo = '0';

    ENTERFUNC;

    if (!newcat_valid_command(rig, command))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    err = newcat_set_vfo_from_alias(rig, &vfo);

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    if (rig->caps->targetable_vfo & RIG_TARGETABLE_MODE)
    {
        main_sub_vfo = (RIG_VFO_B == vfo || RIG_VFO_SUB == vfo) ? '1' : '0';
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%c%c", command, main_sub_vfo,
             cat_term);

    /* Get NAR */
    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        RETURNFUNC(err);
    }

    c = priv->ret_data[3];

    if (c == '1')
    {
        *narrow = TRUE;
    }
    else
    {
        *narrow = FALSE;
    }

    RETURNFUNC(RIG_OK);
}

// returns 1 if in narrow mode 0 if not, < 0 if error
// if vfo != RIG_VFO_NONE then will use NA0 or NA1 depending on vfo Main or Sub
static int get_narrow(RIG *rig, vfo_t vfo)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int narrow = 0;
    int err;

    ENTERFUNC;
    // find out if we're in narrow or wide mode

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "NA%c%c",
             vfo == RIG_VFO_SUB ? '1' : '0', cat_term);

    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        RETURNFUNC(err);
    }

    if (sscanf(priv->ret_data, "NA%*1d%3d", &narrow) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unable to parse width from '%s'\n", __func__,
                  priv->ret_data);
        RETURNFUNC(-RIG_EPROTO);
    }

    RETURNFUNC(narrow);
}

int newcat_set_rx_bandwidth(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    int w = 0;
    char main_sub_vfo = '0';

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s vfo=%s, mode=%s, width=%d\n", __func__,
              rig_strvfo(vfo), rig_strrmode(mode), (int)width);

    if (!newcat_valid_command(rig, "SH"))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    err = newcat_set_vfo_from_alias(rig, &vfo);

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    if (rig->caps->targetable_vfo & RIG_TARGETABLE_MODE)
    {
        main_sub_vfo = (RIG_VFO_SUB == vfo) ? '1' : '0';
    }

    // NOTE: RIG_PASSBAND_NORMAL (0) should select the default filter width (SH00)

    if (is_ft950)
    {
        switch (mode)
        {
        case RIG_MODE_PKTUSB:
        case RIG_MODE_PKTLSB:
        case RIG_MODE_RTTY:
        case RIG_MODE_RTTYR:
        case RIG_MODE_CW:
        case RIG_MODE_CWR:
            // Narrow mode must be chosen correctly before filter width
            err = newcat_set_narrow(rig, vfo, width <= 500 ? TRUE : FALSE);

            if (err != RIG_OK)
            {
                RETURNFUNC(err);
            }

            if (width == RIG_PASSBAND_NORMAL) { w = 0; }
            else if (width <= 100) { w = 3; }
            else if (width <= 200) { w = 4; }
            else if (width <= 300) { w = 5; }
            else if (width <= 400) { w = 6; }
            else if (width <= 500) { w = 7; }
            else if (width <= 800) { w = 8; }
            else if (width <= 1200) { w = 9; }
            else if (width <= 1400) { w = 10; }
            else if (width <= 1700) { w = 11; }
            else if (width <= 2000) { w = 12; }
            else { w = 13; } // 2400 Hz

            break;

        case RIG_MODE_LSB:
        case RIG_MODE_USB:
            // Narrow mode must be chosen correctly before filter width
            err = newcat_set_narrow(rig, vfo, width <= 1800 ? TRUE : FALSE);

            if (err != RIG_OK)
            {
                RETURNFUNC(err);
            }

            if (width == RIG_PASSBAND_NORMAL) { w = 0; }
            else if (width <= 200) { w = 1; }
            else if (width <= 400) { w = 2; }
            else if (width <= 600) { w = 3; }
            else if (width <= 850) { w = 4; }
            else if (width <= 1100) { w = 5; }
            else if (width <= 1350) { w = 6; }
            else if (width <= 1500) { w = 7; }
            else if (width <= 1650) { w = 8; }
            else if (width <= 1800) { w = 9; }
            else if (width <= 1950) { w = 10; }
            else if (width <= 2100) { w = 11; }
            else if (width <= 2250) { w = 12; }
            else if (width <= 2400) { w = 13; }
            else if (width <= 2450) { w = 14; }
            else if (width <= 2500) { w = 15; }
            else if (width <= 2600) { w = 16; }
            else if (width <= 2700) { w = 17; }
            else if (width <= 2800) { w = 18; }
            else if (width <= 2900) { w = 19; }
            else { w = 20; } // 3000 Hz

            break;

        case RIG_MODE_AM:
        case RIG_MODE_FM:
        case RIG_MODE_PKTFM:
        case RIG_MODE_FMN:
            // Set roofing filter and narrow mode
            break;

        default:
            RETURNFUNC(-RIG_EINVAL);
        } // end switch(mode)

        if ((err = set_roofing_filter_for_width(rig, vfo, width)) != RIG_OK)
        {
            RETURNFUNC(err);
        }

        switch (mode)
        {
        case RIG_MODE_AM:
        case RIG_MODE_FM:
        case RIG_MODE_PKTFM:
            if (width > 0 && width < rig_passband_normal(rig, mode))
            {
                err = newcat_set_narrow(rig, vfo,  TRUE);
            }
            else
            {
                err = newcat_set_narrow(rig, vfo, FALSE);
            }

            RETURNFUNC(err);

        case RIG_MODE_FMN:
            RETURNFUNC(RIG_OK);
        }
    } // end is_ft950 */
    else if (is_ft891)
    {
        switch (mode)
        {
        case RIG_MODE_PKTUSB:
        case RIG_MODE_PKTLSB:
        case RIG_MODE_RTTY:
        case RIG_MODE_RTTYR:
        case RIG_MODE_CW:
        case RIG_MODE_CWR:
            // Narrow mode must be chosen correctly before filter width
            err = newcat_set_narrow(rig, vfo, width <= 500 ? TRUE : FALSE);

            if (err != RIG_OK)
            {
                RETURNFUNC(err);
            }

            if (width == RIG_PASSBAND_NORMAL) { w = 0; }
            else if (width <= 50) { w = 1; }
            else if (width <= 100) { w = 2; }
            else if (width <= 150) { w = 3; }
            else if (width <= 200) { w = 4; }
            else if (width <= 250) { w = 5; }
            else if (width <= 300) { w = 6; }
            else if (width <= 350) { w = 7; }
            else if (width <= 400) { w = 8; }
            else if (width <= 450) { w = 9; }
            else if (width <= 500) { w = 10; }
            else if (width <= 800) { w = 11; }
            else if (width <= 1200) { w = 12; }
            else if (width <= 1400) { w = 13; }
            else if (width <= 1700) { w = 14; }
            else if (width <= 2000) { w = 15; }
            else if (width <= 2400) { w = 16; }
            else { w = 17; } // 3000 Hz

            break;

        case RIG_MODE_LSB:
        case RIG_MODE_USB:
            // Narrow mode must be chosen correctly before filter width
            err = newcat_set_narrow(rig, vfo, width <= 1800 ? TRUE : FALSE);

            if (err != RIG_OK)
            {
                RETURNFUNC(err);
            }

            if (width == RIG_PASSBAND_NORMAL) { w = 0; }
            else if (width <= 200) { w = 1; }
            else if (width <= 400) { w = 2; }
            else if (width <= 600) { w = 3; }
            else if (width <= 850) { w = 4; }
            else if (width <= 1100) { w = 5; }
            else if (width <= 1350) { w = 6; }
            else if (width <= 1500) { w = 7; }
            else if (width <= 1650) { w = 8; }
            else if (width <= 1800) { w = 9; }
            else if (width <= 1950) { w = 10; }
            else if (width <= 2100) { w = 11; }
            else if (width <= 2200) { w = 12; }
            else if (width <= 2300) { w = 13; }
            else if (width <= 2400) { w = 14; }
            else if (width <= 2500) { w = 15; }
            else if (width <= 2600) { w = 16; }
            else if (width <= 2700) { w = 17; }
            else if (width <= 2800) { w = 18; }
            else if (width <= 2900) { w = 19; }
            else if (width <= 3000) { w = 20; }
            else { w = 21; } // 3000 Hz

            break;

        case RIG_MODE_AM:
        case RIG_MODE_FM:
        case RIG_MODE_PKTFM:
            if (width > 0 && width < rig_passband_normal(rig, mode))
            {
                err = newcat_set_narrow(rig, vfo,  TRUE);
            }
            else
            {
                err = newcat_set_narrow(rig, vfo, FALSE);
            }

            RETURNFUNC(err);

        case RIG_MODE_FMN:
            break;

        default:
            RETURNFUNC(-RIG_EINVAL);
        } // end switch(mode)
    } // end is_ft891
    else if (is_ft991)
    {
        switch (mode)
        {
        case RIG_MODE_PKTUSB:
        case RIG_MODE_PKTLSB:
        case RIG_MODE_RTTY:
        case RIG_MODE_RTTYR:
        case RIG_MODE_CW:
        case RIG_MODE_CWR:
            // Narrow mode must be chosen correctly before filter width
            err = newcat_set_narrow(rig, vfo, width <= 500 ? TRUE : FALSE);

            if (err != RIG_OK)
            {
                RETURNFUNC(err);
            }

            if (width == RIG_PASSBAND_NORMAL) { w = 0; }
            else if (width <= 50) { w = 1; }
            else if (width <= 100) { w = 2; }
            else if (width <= 150) { w = 3; }
            else if (width <= 200) { w = 4; }
            else if (width <= 250) { w = 5; }
            else if (width <= 300) { w = 6; }
            else if (width <= 350) { w = 7; }
            else if (width <= 400) { w = 8; }
            else if (width <= 450) { w = 9; }
            else if (width <= 500) { w = 10; }
            else if (width <= 800) { w = 11; }
            else if (width <= 1200) { w = 12; }
            else if (width <= 1400) { w = 13; }
            else if (width <= 1700) { w = 14; }
            else if (width <= 2000) { w = 15; }
            else if (width <= 2400) { w = 16; }
            else { w = 17; } // 3000 Hz

            break;

        case RIG_MODE_LSB:
        case RIG_MODE_USB:
            // Narrow mode must be chosen correctly before filter width
            err = newcat_set_narrow(rig, vfo, width <= 1800 ? TRUE : FALSE);

            if (err != RIG_OK)
            {
                RETURNFUNC(err);
            }

            if (width == RIG_PASSBAND_NORMAL) { w = 0; }
            else if (width <= 200) { w = 1; }
            else if (width <= 400) { w = 2; }
            else if (width <= 600) { w = 3; }
            else if (width <= 850) { w = 4; }
            else if (width <= 1100) { w = 5; }
            else if (width <= 1350) { w = 6; }
            else if (width <= 1500) { w = 7; }
            else if (width <= 1650) { w = 8; }
            else if (width <= 1800) { w = 9; }
            else if (width <= 1950) { w = 10; }
            else if (width <= 2100) { w = 11; }
            else if (width <= 2200) { w = 12; }
            else if (width <= 2300) { w = 13; }
            else if (width <= 2400) { w = 14; }
            else if (width <= 2500) { w = 15; }
            else if (width <= 2600) { w = 16; }
            else if (width <= 2700) { w = 17; }
            else if (width <= 2800) { w = 18; }
            else if (width <= 2900) { w = 19; }
            else if (width <= 3000) { w = 20; }
            else { w = 21; } // 3200 Hz

            break;

        case RIG_MODE_AM: // Only 1 passband each for AM or AMN
            if (width == RIG_PASSBAND_NORMAL || width == 9000)
            {
                err = newcat_set_narrow(rig, vfo, FALSE);
            }

            RETURNFUNC(err);

        case RIG_MODE_AMN:
            if (width == RIG_PASSBAND_NORMAL || width == 6000)
            {
                err = newcat_set_narrow(rig, vfo,  TRUE);
            }

            RETURNFUNC(err);

        case RIG_MODE_FM: // Only 1 passband each for FM or FMN
            if (width == RIG_PASSBAND_NORMAL || width == 16000)
            {
                err = newcat_set_narrow(rig, vfo, FALSE);
            }

            RETURNFUNC(err);

        case RIG_MODE_FMN:
            if (width == RIG_PASSBAND_NORMAL || width == 9000)
            {
                err = newcat_set_narrow(rig, vfo,  TRUE);
            }

            RETURNFUNC(err);

        case RIG_MODE_C4FM:
            if (width == RIG_PASSBAND_NORMAL || width == 16000)
            {
                err = newcat_set_narrow(rig, vfo,  TRUE);
            }
            else if (width == 9000)
            {
                err = newcat_set_narrow(rig, vfo, FALSE);
            }
            else
            {
                RETURNFUNC(-RIG_EINVAL);
            }

            RETURNFUNC(err);

        case RIG_MODE_PKTFM:
            if (width > 0 && width < rig_passband_normal(rig, mode))
            {
                err = newcat_set_narrow(rig, vfo,  TRUE);
            }
            else
            {
                err = newcat_set_narrow(rig, vfo, FALSE);
            }

            RETURNFUNC(err);

        default:
            RETURNFUNC(-RIG_EINVAL);
        } // end switch(mode)
    } // end is_ft991
    else if (is_ftdx1200 || is_ftdx3000)
    {
        // FTDX 1200 and FTDX 3000 have the same set of filter choices
        switch (mode)
        {
        case RIG_MODE_PKTUSB:
        case RIG_MODE_PKTLSB:
        case RIG_MODE_RTTY:
        case RIG_MODE_RTTYR:
        case RIG_MODE_CW:
        case RIG_MODE_CWR:
            // Narrow mode must be chosen correctly before filter width
            err = newcat_set_narrow(rig, vfo, width <= 500 ? TRUE : FALSE);

            if (err != RIG_OK)
            {
                RETURNFUNC(err);
            }

            if (width == RIG_PASSBAND_NORMAL) { w = 0; }
            else if (width <= 50) { w = 1; }
            else if (width <= 100) { w = 2; }
            else if (width <= 150) { w = 3; }
            else if (width <= 200) { w = 4; }
            else if (width <= 250) { w = 5; }
            else if (width <= 300) { w = 6; }
            else if (width <= 350) { w = 7; }
            else if (width <= 400) { w = 8; }
            else if (width <= 450) { w = 9; }
            else if (width <= 500) { w = 10; }
            else if (width <= 800) { w = 11; }
            else if (width <= 1200) { w = 12; }
            else if (width <= 1400) { w = 13; }
            else if (width <= 1700) { w = 14; }
            else if (width <= 2000) { w = 15; }
            else { w = 16; } // 2400 Hz

            break;

        case RIG_MODE_LSB:
        case RIG_MODE_USB:
            // Narrow mode must be chosen correctly before filter width
            err = newcat_set_narrow(rig, vfo, width <= 1800 ? TRUE : FALSE);

            if (err != RIG_OK)
            {
                RETURNFUNC(err);
            }

            if (width == RIG_PASSBAND_NORMAL) { w = 0; }
            else if (width <= 200) {  w = 1; }
            else if (width <= 400) {  w = 2; }
            else if (width <= 600) {  w = 3; }
            else if (width <= 850) {  w = 4; }
            else if (width <= 1100) {  w = 5; }
            else if (width <= 1350) {  w = 6; }
            else if (width <= 1500) {  w = 7; }
            else if (width <= 1650) {  w = 8; }
            else if (width <= 1800) {  w = 9; }
            else if (width <= 1950) {  w = 10; }
            else if (width <= 2100) {  w = 11; }
            else if (width <= 2200) {  w = 12; }
            else if (width <= 2300) {  w = 13; }
            else if (width <= 2400) {  w = 14; }
            else if (width <= 2500) {  w = 15; }
            else if (width <= 2600) {  w = 16; }
            else if (width <= 2700) {  w = 17; }
            else if (width <= 2800) {  w = 18; }
            else if (width <= 2900) {  w = 19; }
            else if (width <= 3000) {  w = 20; }
            else if (width <= 3200) {  w = 21; }
            else if (width <= 3400) {  w = 22; }
            else if (width <= 3600) {  w = 23; }
            else if (width <= 3800) {  w = 24; }
            else { w = 25; } // 4000 Hz

            break;

        case RIG_MODE_AM:
        case RIG_MODE_AMN:
        case RIG_MODE_FM:
        case RIG_MODE_PKTFM:
        case RIG_MODE_FMN:
            // Set roofing filter and narrow mode
            break;

        default:
            RETURNFUNC(-RIG_EINVAL);
        } // end switch(mode)

        if ((err = set_roofing_filter_for_width(rig, vfo, width)) != RIG_OK)
        {
            RETURNFUNC(err);
        }

        switch (mode)
        {
        case RIG_MODE_AM:
        case RIG_MODE_AMN:
        case RIG_MODE_FM:
        case RIG_MODE_PKTFM:
        case RIG_MODE_FMN:
            if (width > 0 && width < rig_passband_normal(rig, mode))
            {
                err = newcat_set_narrow(rig, vfo,  TRUE);
            }
            else
            {
                err = newcat_set_narrow(rig, vfo, FALSE);
            }

            RETURNFUNC(err);
        }
    } // end is_ftdx1200 and is_ftdx3000
    else if (is_ftdx5000)
    {
        switch (mode)
        {
        case RIG_MODE_PKTUSB:
        case RIG_MODE_PKTLSB:
        case RIG_MODE_RTTY:
        case RIG_MODE_RTTYR:
        case RIG_MODE_CW:
        case RIG_MODE_CWR:
            // Narrow mode must be chosen correctly before filter width
            err = newcat_set_narrow(rig, vfo, width <= 500 ? TRUE : FALSE);

            if (err != RIG_OK)
            {
                RETURNFUNC(err);
            }

            if (width == RIG_PASSBAND_NORMAL) { w = 0; }
            else if (width <= 50) { w = 1; }
            else if (width <= 100) { w = 2; }
            else if (width <= 150) { w = 3; }
            else if (width <= 200) { w = 4; }
            else if (width <= 250) { w = 5; }
            else if (width <= 300) { w = 6; }
            else if (width <= 350) { w = 7; }
            else if (width <= 400) { w = 8; }
            else if (width <= 450) { w = 9; }
            else if (width <= 500) { w = 10; }
            else if (width <= 800) { w = 11; }
            else if (width <= 1200) { w = 12; }
            else if (width <= 1400) { w = 13; }
            else if (width <= 1700) { w = 14; }
            else if (width <= 2000) { w = 15; }
            else { w = 16; } // 2400 Hz

            break;

        case RIG_MODE_LSB:
        case RIG_MODE_USB:
            // Narrow mode must be chosen correctly before filter width
            err = newcat_set_narrow(rig, vfo, width <= 1800 ? TRUE : FALSE);

            if (err != RIG_OK)
            {
                RETURNFUNC(err);
            }

            if (width == RIG_PASSBAND_NORMAL) { w = 0; }
            else if (width <= 200) {  w = 1; }
            else if (width <= 400) {  w = 2; }
            else if (width <= 600) {  w = 3; }
            else if (width <= 850) {  w = 4; }
            else if (width <= 1100) {  w = 5; }
            else if (width <= 1350) {  w = 6; }
            else if (width <= 1500) {  w = 7; }
            else if (width <= 1650) {  w = 8; }
            else if (width <= 1800) {  w = 9; }
            else if (width <= 1950) {  w = 10; }
            else if (width <= 2100) {  w = 11; }
            else if (width <= 2250) {  w = 12; }
            else if (width <= 2400) {  w = 13; }
            else if (width <= 2500) {  w = 15; }
            else if (width <= 2600) {  w = 16; }
            else if (width <= 2700) {  w = 17; }
            else if (width <= 2800) {  w = 18; }
            else if (width <= 2900) {  w = 19; }
            else if (width <= 3000) {  w = 20; }
            else if (width <= 3200) {  w = 21; }
            else if (width <= 3400) {  w = 22; }
            else if (width <= 3600) {  w = 23; }
            else if (width <= 3800) {  w = 24; }
            else { w = 25; } // 4000 Hz

            break;

        case RIG_MODE_AM:
        case RIG_MODE_AMN:
        case RIG_MODE_FM:
        case RIG_MODE_PKTFM:
        case RIG_MODE_FMN:
            // Set roofing filter and narrow mode
            break;

        default:
            RETURNFUNC(-RIG_EINVAL);
        } // end switch(mode)

        if ((err = set_roofing_filter_for_width(rig, vfo, width)) != RIG_OK)
        {
            RETURNFUNC(err);
        }

        switch (mode)
        {
        case RIG_MODE_AM:
        case RIG_MODE_AMN:
        case RIG_MODE_FM:
        case RIG_MODE_PKTFM:
        case RIG_MODE_FMN:
            if (width > 0 && width < rig_passband_normal(rig, mode))
            {
                err = newcat_set_narrow(rig, vfo,  TRUE);
            }
            else
            {
                err = newcat_set_narrow(rig, vfo, FALSE);
            }

            RETURNFUNC(err);
        }
    } // end is_ftdx5000
    else if (is_ftdx101d || is_ftdx101mp || is_ftdx10)
    {
        switch (mode)
        {
        case RIG_MODE_PKTUSB:
        case RIG_MODE_PKTLSB:
        case RIG_MODE_RTTY:
        case RIG_MODE_RTTYR:
        case RIG_MODE_CW:
        case RIG_MODE_CWR:
            if (width == RIG_PASSBAND_NORMAL) { w = 0; }
            else if (width <= 50) { w = 1; }
            else if (width <= 100) { w = 2; }
            else if (width <= 150) { w = 3; }
            else if (width <= 200) { w = 4; }
            else if (width <= 250) { w = 5; }
            else if (width <= 300) { w = 6; }
            else if (width <= 350) { w = 7; }
            else if (width <= 400) { w = 8; }
            else if (width <= 450) { w = 9; }
            else if (width <= 500) { w = 10; }
            else if (width <= 600) { w = 11; }
            else if (width <= 800) { w = 12; }
            else if (width <= 1200) { w = 13; }
            else if (width <= 1400) { w = 14; }
            else if (width <= 1700) { w = 15; }
            else if (width <= 2000) { w = 16; }
            else if (width <= 2400) { w = 17; }
            else { w = 18; } // 3000Hz

            break;

        case RIG_MODE_LSB:
        case RIG_MODE_USB:
            if (width == RIG_PASSBAND_NORMAL) { w = 0; }
            else if (width <= 300) {  w = 1; }
            else if (width <= 400) {  w = 2; }
            else if (width <= 600) {  w = 3; }
            else if (width <= 850) {  w = 4; }
            else if (width <= 1100) {  w = 5; }
            else if (width <= 1200) {  w = 6; }
            else if (width <= 1500) {  w = 7; }
            else if (width <= 1650) {  w = 8; }
            else if (width <= 1800) {  w = 9; }
            else if (width <= 1950) {  w = 10; }
            else if (width <= 2100) {  w = 11; }
            else if (width <= 2200) {  w = 12; }
            else if (width <= 2300) {  w = 13; }
            else if (width <= 2400) {  w = 14; }
            else if (width <= 2500) {  w = 15; }
            else if (width <= 2600) {  w = 16; }
            else if (width <= 2700) {  w = 17; }
            else if (width <= 2800) {  w = 18; }
            else if (width <= 2900) {  w = 19; }
            else if (width <= 3000) {  w = 20; }
            else if (width <= 3200) {  w = 21; }
            else if (width <= 3500) {  w = 22; }
            else { w = 23; } // 4000Hz

            break;

        case RIG_MODE_AM:
        case RIG_MODE_AMN:
        case RIG_MODE_FM:
        case RIG_MODE_PKTFM:
        case RIG_MODE_FMN:
            // Set roofing filter and narrow mode
            break;

        default:
            RETURNFUNC(-RIG_EINVAL);
        } // end switch(mode)

        if ((err = set_roofing_filter_for_width(rig, vfo, width)) != RIG_OK)
        {
            RETURNFUNC(err);
        }

        switch (mode)
        {
        case RIG_MODE_AM:
        case RIG_MODE_FM:
        case RIG_MODE_PKTFM:
            if (width > 0 && width < rig_passband_normal(rig, mode))
            {
                err = newcat_set_narrow(rig, vfo,  TRUE);
            }
            else
            {
                err = newcat_set_narrow(rig, vfo, FALSE);
            }

            RETURNFUNC(err);

        case RIG_MODE_AMN:
        case RIG_MODE_FMN:
            RETURNFUNC(RIG_OK);
        }
    } // end is_ftdx101
    else if (is_ft2000)
    {
        // We need details on the widths here, manuals lack information.
        switch (mode)
        {
        case RIG_MODE_CW:
        case RIG_MODE_CWR:
            // Narrow mode overrides DSP filter width on FT-2000
            newcat_set_narrow(rig, vfo, FALSE);

            // CW bandwidth is 2400 Hz at value 16
            if (width == RIG_PASSBAND_NORMAL) { w = 16; }
            else if (width <= 200) { w = 4; }
            else if (width <= 500) { w = 6; }
            else if (width <= 2400) { w = 16; }
            else { w = 31; } // No effect?

            break;

        case RIG_MODE_PKTUSB:
        case RIG_MODE_PKTLSB:
            // Narrow mode overrides DSP filter width on FT-2000
            newcat_set_narrow(rig, vfo, FALSE);

            // Packet SSB bandwidth is 2400 Hz at value 31
            if (width == RIG_PASSBAND_NORMAL) { w = 31; }
            else if (width <= 200) { w = 8; }
            else if (width <= 500) { w = 16; }
            else { w = 31; } // 2400

            break;

        case RIG_MODE_RTTY:
        case RIG_MODE_RTTYR:
            // Narrow mode overrides DSP filter width on FT-2000
            newcat_set_narrow(rig, vfo, FALSE);

            if (width == RIG_PASSBAND_NORMAL) { w = 16; }
            else if (width <= 300) { w = 8; }
            else if (width <= 500) { w = 16; }
            else { w = 31; } // 2400

            break;

        case RIG_MODE_LSB:
        case RIG_MODE_USB:
            // Narrow mode overrides DSP filter width on FT-2000
            newcat_set_narrow(rig, vfo, FALSE);

            if (width == RIG_PASSBAND_NORMAL) { w = 16; }
            else if (width <= 1800) { w = 8; }
            else if (width <= 2400) { w = 16; }
            else if (width <= 3000) { w = 25; }
            else { w = 31; } // 4000

            break;

        case RIG_MODE_AM:
        case RIG_MODE_FM:
        case RIG_MODE_PKTFM:
        case RIG_MODE_AMN:
        case RIG_MODE_FMN:
            RETURNFUNC(RIG_OK);

        default:
            RETURNFUNC(-RIG_EINVAL);
        }

        if ((err = set_roofing_filter_for_width(rig, vfo, width)) != RIG_OK)
        {
            RETURNFUNC(err);
        }

        switch (mode)
        {
        case RIG_MODE_AM:
        case RIG_MODE_AMN:
        case RIG_MODE_FM:
        case RIG_MODE_PKTFM:
        case RIG_MODE_FMN:
            if (width > 0 && width < rig_passband_normal(rig, mode))
            {
                err = newcat_set_narrow(rig, vfo,  TRUE);
            }
            else
            {
                err = newcat_set_narrow(rig, vfo, FALSE);
            }

            RETURNFUNC(err);
        }
    }
    else
    {
        // FT-450, FTDX 9000
        // We need details on the widths here, manuals lack information.
        switch (mode)
        {
        case RIG_MODE_PKTUSB:
        case RIG_MODE_PKTLSB:
        case RIG_MODE_RTTY:
        case RIG_MODE_RTTYR:
        case RIG_MODE_CW:
        case RIG_MODE_CWR:
            if (width == RIG_PASSBAND_NORMAL) { w = 16; }
            else if (width <= 500) { w = 6; }
            else if (width <= 1800) { w = 16; }
            else { w = 24; }

            break;

        case RIG_MODE_LSB:
        case RIG_MODE_USB:
            if (width == RIG_PASSBAND_NORMAL) { w = 16; }
            else if (width <= 1800) { w = 8; }
            else if (width <= 2400) { w = 16; }
            else { w = 25; } // 3000

            break;

        case RIG_MODE_AM:
        case RIG_MODE_FM:
        case RIG_MODE_PKTFM:
            if (width > 0 && width < rig_passband_normal(rig, mode))
            {
                err = newcat_set_narrow(rig, vfo,  TRUE);
            }
            else
            {
                err = newcat_set_narrow(rig, vfo, FALSE);
            }

            RETURNFUNC(err);

        case RIG_MODE_FMN:
            RETURNFUNC(RIG_OK);

        default:
            RETURNFUNC(-RIG_EINVAL);
        } /* end switch(mode) */

    } /* end else */

    if (is_ftdx101d || is_ftdx101mp || is_ft891)
    {
        // some rigs now require the bandwidth be turned "on"
        int on = is_ft891;
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SH%c%d%02d;", main_sub_vfo, on,
                 w);
    }
    else if (is_ft2000 || is_ftdx3000)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SH0%02d;", w);
    }
    else if (is_ftdx10)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SH00%02d;", w);
    }
    else
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SH%c%02d;", main_sub_vfo, w);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);

    /* Set RX Bandwidth */
    RETURNFUNC(newcat_set_cmd(rig));
}

static int set_roofing_filter(RIG *rig, vfo_t vfo, int index)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    struct newcat_priv_caps *priv_caps = (struct newcat_priv_caps *)rig->caps->priv;
    struct newcat_roofing_filter *roofing_filters;
    char main_sub_vfo = '0';
    char roofing_filter_choice = 0;
    int err;
    int i;

    ENTERFUNC;

    if (priv_caps == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    roofing_filters = priv_caps->roofing_filters;

    if (rig->caps->targetable_vfo & RIG_TARGETABLE_ROOFING)
    {
        main_sub_vfo = (RIG_VFO_B == vfo || RIG_VFO_SUB == vfo) ? '1' : '0';
    }

    if (!newcat_valid_command(rig, "RF"))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    for (i = 0; roofing_filters[i].index >= 0; i++)
    {
        struct newcat_roofing_filter *current_filter = &roofing_filters[i];
        char set_value = current_filter->set_value;

        if (set_value == 0)
        {
            continue;
        }

        roofing_filter_choice = set_value;

        if (current_filter->index == index)
        {
            break;
        }
    }

    if (roofing_filter_choice == 0)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RF%c%c%c", main_sub_vfo,
             roofing_filter_choice, cat_term);

    priv->question_mark_response_means_rejected = 1;
    err = newcat_set_cmd(rig);
    priv->question_mark_response_means_rejected = 0;

    if (RIG_OK != err)
    {
        RETURNFUNC(err);
    }

    RETURNFUNC(RIG_OK);
}

static int set_roofing_filter_for_width(RIG *rig, vfo_t vfo, int width)
{
    struct newcat_priv_caps *priv_caps = (struct newcat_priv_caps *)rig->caps->priv;
    int index = 0;
    int i;

    ENTERFUNC;

    if (priv_caps == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    for (i = 0; i < priv_caps->roofing_filter_count; i++)
    {
        struct newcat_roofing_filter *current_filter = &priv_caps->roofing_filters[i];
        char set_value = current_filter->set_value;

        // Skip get-only values and optional filters
        if (set_value == 0 || current_filter->optional)
        {
            continue;
        }

        // The last filter is always the narrowest
        if (current_filter->width < width)
        {
            break;
        }

        index = current_filter->index;
    }

    RETURNFUNC(set_roofing_filter(rig, vfo, index));
}

static int get_roofing_filter(RIG *rig, vfo_t vfo,
                              struct newcat_roofing_filter **roofing_filter)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    struct newcat_priv_caps *priv_caps = (struct newcat_priv_caps *)rig->caps->priv;
    struct newcat_roofing_filter *roofing_filters;
    char roofing_filter_choice;
    char main_sub_vfo = '0';
    char rf_vfo = 'X';
    int err;
    int n;
    int i;

    ENTERFUNC;

    if (priv_caps == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    roofing_filters = priv_caps->roofing_filters;

    if (rig->caps->targetable_vfo & RIG_TARGETABLE_ROOFING)
    {
        main_sub_vfo = (RIG_VFO_B == vfo || RIG_VFO_SUB == vfo) ? '1' : '0';
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RF%c%c", main_sub_vfo,
             cat_term);

    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        RETURNFUNC(err);
    }

    n = sscanf(priv->ret_data, "RF%c%c", &rf_vfo, &roofing_filter_choice);

    if (n != 2)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: error parsing '%s' for vfo and roofing filter, got %d parsed\n", __func__,
                  priv->ret_data, n);
        RETURNFUNC(-RIG_EPROTO);
    }

    for (i = 0; i < priv_caps->roofing_filter_count; i++)
    {
        struct newcat_roofing_filter *current_filter = &roofing_filters[i];

        if (current_filter->get_value == roofing_filter_choice)
        {
            *roofing_filter = current_filter;
            RETURNFUNC(RIG_OK);
        }
    }

    rig_debug(RIG_DEBUG_ERR,
              "%s: Expected a valid roofing filter but got %c from '%s'\n", __func__,
              roofing_filter_choice, priv->ret_data);

    RETURNFUNC(RIG_EPROTO);
}

int newcat_get_rx_bandwidth(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t *width)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    int w;
    int sh_command_valid = 1;
    int narrow = 0;
    char cmd[] = "SH";
    char main_sub_vfo = '0';

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s vfo=%s, mode=%s\n", __func__,
              rig_strvfo(vfo), rig_strrmode(mode));

    if (!newcat_valid_command(rig, cmd))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    err = newcat_set_vfo_from_alias(rig, &vfo);

    if (err < 0)
    {
        RETURNFUNC(err);
    }

    if (is_ft950 || is_ftdx5000 || is_ftdx3000)
    {
        // Some Yaesu rigs cannot query SH in modes such as AM/FM
        switch (mode)
        {
        case RIG_MODE_FM:
        case RIG_MODE_FMN:
        case RIG_MODE_PKTFM:
        case RIG_MODE_AM:
        case RIG_MODE_AMN:
        case RIG_MODE_PKTAM:
            sh_command_valid = 0;
            break;
        }
    }

    if (rig->caps->targetable_vfo & RIG_TARGETABLE_MODE)
    {
        main_sub_vfo = (RIG_VFO_B == vfo || RIG_VFO_SUB == vfo) ? '1' : '0';
    }

    if (sh_command_valid)
    {
        if (is_ft2000 || is_ftdx10 || is_ftdx3000)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s0%c", cmd, cat_term);
        }
        else
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%c%c", cmd, main_sub_vfo,
                     cat_term);
        }

        err = newcat_get_cmd(rig);

        if (err != RIG_OK)
        {
            RETURNFUNC(err);
        }


        w = 0; // use default in case of error

        if (strlen(priv->ret_data) == 7)
        {
            int on;
            // do we need to pay attention to the Main/Sub here?
            int n = sscanf(priv->ret_data, "SH%*1d%1d%3d", &on, &w);

            if (n != 2)
            {
                err = -RIG_EPROTO;
            }

#if 0 // this may apply to another Yaesu rig

            if (n == 2)
            {
                if (!on) { w = 0; }
            }
            else
            {
                err = -RIG_EPROTO;
            }

#endif
        }
        else if (strlen(priv->ret_data) == 6)
        {
            int n = sscanf(priv->ret_data, "SH%3d", &w);

            if (n != 1) { err = -RIG_EPROTO; }
        }
        else
        {
            err = -RIG_EPROTO;
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: w=%d\n", __func__, w);

        if (err != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unable to parse width from '%s'\n", __func__,
                      priv->ret_data);
            RETURNFUNC(-RIG_EPROTO);
        }
    }
    else
    {
        // Some Yaesu rigs cannot query filter width using SH command in modes such as AM/FM
        w = 0;
    }

    if (is_ft950)
    {
        if ((narrow = get_narrow(rig, RIG_VFO_MAIN)) < 0)
        {
            RETURNFUNC(-RIG_EPROTO);
        }

        switch (mode)
        {
        case RIG_MODE_PKTUSB:
        case RIG_MODE_PKTLSB:
        case RIG_MODE_RTTY:
        case RIG_MODE_RTTYR:
        case RIG_MODE_CW:
        case RIG_MODE_CWR:
            switch (w)
            {
            case 0:
                *width = narrow ? 300 : 500;
                break;

            case 3: *width = 100; break;

            case 4: *width = 200; break;

            case 5: *width = 300; break;

            case 6: *width = 400; break;

            case 7: *width = 5000; break;

            case 8: *width = 800; break;

            case 9: *width = 1200; break;

            case 10: *width = 1400; break;

            case 11: *width = 1700; break;

            case 12: *width = 2000; break;

            case 13: *width = 2400; break;

            default: RETURNFUNC(-RIG_EINVAL);
            }

            break;

        case RIG_MODE_LSB:
        case RIG_MODE_USB:
            switch (w)
            {
            case 0:
                *width = narrow ? 1800 : 2400;
                break;

            case  1: *width =  200; break;

            case  2: *width =  400; break;

            case  3: *width =  600; break;

            case  4: *width =  850; break;

            case  5: *width = 1100; break;

            case  6: *width = 1350; break;

            case  7: *width = 1500; break;

            case  8: *width = 1650; break;

            case  9: *width = 1800; break;

            case 10: *width = 1950; break;

            case 11: *width = 2100; break;

            case 12: *width = 2250; break;

            case 13: *width = 2400; break;

            case 14: *width = 2450; break;

            case 15: *width = 2500; break;

            case 16: *width = 2600; break;

            case 17: *width = 2700; break;

            case 18: *width = 2800; break;

            case 19: *width = 2900; break;

            case 20: *width = 3000; break;

            default:
                RETURNFUNC(-RIG_EINVAL);
            }

            break;

        case RIG_MODE_AM:
            *width = narrow ? 6000 : 9000;
            break;

        case RIG_MODE_PKTFM:
        case RIG_MODE_FM:
            *width = narrow ? 9000 : 16000;
            break;

        case RIG_MODE_FMN:
            *width = 9000;
            break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unknown mode=%s\n", __func__, rig_strrmode(mode));
            RETURNFUNC(-RIG_EINVAL);
        }   /* end switch(mode) */

    } /* end if is_ft950 */
    else if (is_ft891)
    {
        if ((narrow = get_narrow(rig, vfo)) < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: error narrow < 0, narrow=%d\n", __func__, narrow);
            RETURNFUNC(-RIG_EPROTO);
        }

        switch (mode)
        {
        case RIG_MODE_PKTUSB:
        case RIG_MODE_PKTLSB:
        case RIG_MODE_RTTY:
        case RIG_MODE_RTTYR:
        case RIG_MODE_CW:
        case RIG_MODE_CWR:
            switch (w)
            {
            case 0:
                if (mode == RIG_MODE_CW || mode == RIG_MODE_CWR)
                {
                    *width = narrow ? 500 : 2400;
                }
                else
                {
                    *width = narrow ? 300 : 500;
                }

                break;

            case 1: *width = 50; break;

            case 2: *width = 100; break;

            case 3: *width = 150; break;

            case 4: *width = 200; break;

            case 5: *width = 250; break;

            case 6: *width = 300; break;

            case 7: *width = 350; break;

            case 8: *width = 400; break;

            case 9: *width = 450; break;

            case 10: *width = 500; break;

            case 11: *width = 800; break;

            case 12: *width = 1200; break;

            case 13: *width = 1400; break;

            case 14: *width = 1700; break;

            case 15: *width = 2000; break;

            case 16: *width = 2400; break;

            case 17: *width = 3000; break;

            default:
                rig_debug(RIG_DEBUG_ERR, "%s: unknown w=%d\n", __func__, w);
                RETURNFUNC(-RIG_EINVAL);
            }

            break;

        case RIG_MODE_LSB:
        case RIG_MODE_USB:
            switch (w)
            {
            case 0: *width = narrow ? 1500 : 2400; break;

            case  1: *width =  200; break;

            case  2: *width =  400; break;

            case  3: *width =  600; break;

            case  4: *width =  850; break;

            case  5: *width = 1100; break;

            case  6: *width = 1350; break;

            case  7: *width = 1500; break;

            case  8: *width = 1650; break;

            case  9: *width = 1800; break;

            case 10: *width = 1950; break;

            case 11: *width = 2100; break;

            case 12: *width = 2200; break;

            case 13: *width = 2300; break;

            case 14: *width = 2400; break;

            case 15: *width = 2500; break;

            case 16: *width = 2600; break;

            case 17: *width = 2700; break;

            case 18: *width = 2800; break;

            case 19: *width = 2900; break;

            case 20: *width = 3000; break;

            case 21: *width = 3200; break;

            default:
                rig_debug(RIG_DEBUG_ERR, "%s: unknown mode=%s\n", __func__, rig_strrmode(mode));
                RETURNFUNC(-RIG_EINVAL);
            }

            break;

        case RIG_MODE_AM:
        case RIG_MODE_FMN:
            *width =  9000;
            break;

        case RIG_MODE_AMN:
            *width =  6000;
            break;

        case RIG_MODE_FM:
        case RIG_MODE_PKTFM:
            *width = 16000;
            break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unknown mode=%s\n", __func__, rig_strrmode(mode));
            RETURNFUNC(-RIG_EINVAL);
        }   /* end switch(mode) */

    } /* end if is_ft891 */
    else if (is_ft991)
    {
        // some modes are fixed and can't be queried with "NA0"
        if (mode != RIG_MODE_C4FM && mode != RIG_MODE_PKTFM && mode != RIG_MODE_PKTFMN
                && (narrow = get_narrow(rig, vfo)) < 0)
        {
            RETURNFUNC(-RIG_EPROTO);
        }

        narrow = 0;

        switch (mode)
        {
        case RIG_MODE_PKTUSB:
        case RIG_MODE_PKTLSB:
        case RIG_MODE_RTTY:
        case RIG_MODE_RTTYR:
        case RIG_MODE_CW:
        case RIG_MODE_CWR:
            switch (w)
            {
            case 0:
                if (mode == RIG_MODE_CW || mode == RIG_MODE_CWR)
                {
                    *width = narrow ? 500 : 2400;
                }
                else
                {
                    *width = narrow ? 300 : 500;
                }

                break;

            case 1: *width = 50; break;

            case 2: *width = 100; break;

            case 3: *width = 150; break;

            case 4: *width = 200; break;

            case 5: *width = 250; break;

            case 6: *width = 300; break;

            case 7: *width = 350; break;

            case 8: *width = 400; break;

            case 9: *width = 450; break;

            case 10: *width = 500; break;

            case 11: *width = 800; break;

            case 12: *width = 1200; break;

            case 13: *width = 1400; break;

            case 14: *width = 1700; break;

            case 15: *width = 2000; break;

            case 16: *width = 2400; break;

            case 17: *width = 3000; break;

            default: RETURNFUNC(-RIG_EINVAL);
            }

            break;

        case RIG_MODE_LSB:
        case RIG_MODE_USB:
            switch (w)
            {
            case 0: *width = narrow ? 1500 : 2400; break;

            case  1: *width =  200; break;

            case  2: *width =  400; break;

            case  3: *width =  600; break;

            case  4: *width =  850; break;

            case  5: *width = 1100; break;

            case  6: *width = 1350; break;

            case  7: *width = 1500; break;

            case  8: *width = 1650; break;

            case  9: *width = 1800; break;

            case 10: *width = 1950; break;

            case 11: *width = 2100; break;

            case 12: *width = 2200; break;

            case 13: *width = 2300; break;

            case 14: *width = 2400; break;

            case 15: *width = 2500; break;

            case 16: *width = 2600; break;

            case 17: *width = 2700; break;

            case 18: *width = 2800; break;

            case 19: *width = 2900; break;

            case 20: *width = 3000; break;

            case 21: *width = 3200; break;

            default: RETURNFUNC(-RIG_EINVAL);
            }

            break;

        case RIG_MODE_AM:
        case RIG_MODE_FMN:
            *width =  9000;
            break;

        case RIG_MODE_AMN:
            *width =  6000;
            break;

        case RIG_MODE_FM:
        case RIG_MODE_C4FM:
        case RIG_MODE_PKTFM:
            *width = 16000;
            break;

        default:
            RETURNFUNC(-RIG_EINVAL);
        }   /* end switch(mode) */

    } /* end if is_ft991 */
    else if (is_ftdx1200 || is_ftdx3000)
    {
        if ((narrow = get_narrow(rig, RIG_VFO_MAIN)) < 0)
        {
            RETURNFUNC(-RIG_EPROTO);
        }

        switch (mode)
        {
        case RIG_MODE_PKTUSB:
        case RIG_MODE_PKTLSB:
        case RIG_MODE_RTTY:
        case RIG_MODE_RTTYR:
        case RIG_MODE_CW:
        case RIG_MODE_CWR:
            switch (w)
            {
            case 0:
                *width = narrow ? 500 : 2400;
                break;

            case 1: *width = 50; break;

            case 2: *width = 100; break;

            case 3: *width = 150; break;

            case 4: *width = 200; break;

            case 5: *width = 250; break;

            case 6: *width = 300; break;

            case 7: *width = 350; break;

            case 8: *width = 400; break;

            case 9: *width = 450; break;

            case 10: *width = 500; break;

            case 11: *width = 800; break;

            case 12: *width = 1200; break;

            case 13: *width = 1400; break;

            case 14: *width = 1700; break;

            case 15: *width = 2000; break;

            case 16: *width = 2400; break;

            default: RETURNFUNC(-RIG_EINVAL);
            }

            break;

        case RIG_MODE_LSB:
        case RIG_MODE_USB:
            switch (w)
            {
            case 0:
                *width = narrow ? 1500 : 2400;
                break;

            case  1: *width =  200; break;

            case  2: *width =  400; break;

            case  3: *width =  600; break;

            case  4: *width =  850; break;

            case  5: *width = 1100; break;

            case  6: *width = 1350; break;

            case  7: *width = 1500; break;

            case  8: *width = 1650; break;

            case  9: *width = 1800; break;

            case 10: *width = 1950; break;

            case 11: *width = 2100; break;

            case 12: *width = 2250; break;

            case 13: *width = 2400; break;

            case 14: *width = 2450; break;

            case 15: *width = 2500; break;

            case 16: *width = 2600; break;

            case 17: *width = 2700; break;

            case 18: *width = 2800; break;

            case 19: *width = 2900; break;

            case 20: *width = 3000; break;

            case 21: *width = 3200; break;

            case 22: *width = 3400; break;

            case 23: *width = 3600; break;

            case 24: *width = 3800; break;

            case 25: *width = 4000; break;

            default: RETURNFUNC(-RIG_EINVAL);
            }

            break;

        case RIG_MODE_AM:
            *width = narrow ? 6000 : 9000;
            break;

        case RIG_MODE_PKTFM:
        case RIG_MODE_FM:
            *width = narrow ? 9000 : 16000;
            break;

        case RIG_MODE_FMN:
            *width = 9000;
            break;

        case RIG_MODE_AMN:
            *width = 6000;
            break;

        default:
            RETURNFUNC(-RIG_EINVAL);
        }   /* end switch(mode) */

    } /* end if is_ftdx1200 or is_ftdx3000 */
    else if (is_ftdx5000)
    {
        if ((narrow = get_narrow(rig, RIG_VFO_MAIN)) < 0)
        {
            RETURNFUNC(-RIG_EPROTO);
        }

        switch (mode)
        {
        case RIG_MODE_PKTUSB:
        case RIG_MODE_PKTLSB:
        case RIG_MODE_RTTY:
        case RIG_MODE_RTTYR:
        case RIG_MODE_CW:
        case RIG_MODE_CWR:
            switch (w)
            {
            case 0:
                *width = narrow ? 500 : 2400;
                break;

            case 1: *width = 50; break;

            case 2: *width = 100; break;

            case 3: *width = 150; break;

            case 4: *width = 200; break;

            case 5: *width = 250; break;

            case 6: *width = 300; break;

            case 7: *width = 350; break;

            case 8: *width = 400; break;

            case 9: *width = 450; break;

            case 10: *width = 500; break;

            case 11: *width = 800; break;

            case 12: *width = 1200; break;

            case 13: *width = 1400; break;

            case 14: *width = 1700; break;

            case 15: *width = 2000; break;

            case 16: *width = 2400; break;

            default: RETURNFUNC(-RIG_EINVAL);
            }

            break;

        case RIG_MODE_LSB:
        case RIG_MODE_USB:
            switch (w)
            {
            case 0:
                *width = narrow ? 1500 : 2400;
                break;

            case  1: *width =  200; break;

            case  2: *width =  400; break;

            case  3: *width =  600; break;

            case  4: *width =  850; break;

            case  5: *width = 1100; break;

            case  6: *width = 1350; break;

            case  7: *width = 1500; break;

            case  8: *width = 1650; break;

            case  9: *width = 1800; break;

            case 10: *width = 1950; break;

            case 11: *width = 2100; break;

            case 12: *width = 2250; break;

            case 13: *width = 2400; break;

            // 14 is not defined for FTDX 5000, but leaving here for completeness
            case 14: *width = 2400; break;

            case 15: *width = 2500; break;

            case 16: *width = 2600; break;

            case 17: *width = 2700; break;

            case 18: *width = 2800; break;

            case 19: *width = 2900; break;

            case 20: *width = 3000; break;

            case 21: *width = 3200; break;

            case 22: *width = 3400; break;

            case 23: *width = 3600; break;

            case 24: *width = 3800; break;

            case 25: *width = 4000; break;

            default: RETURNFUNC(-RIG_EINVAL);
            }

            break;

        case RIG_MODE_AM:
            *width = narrow ? 6000 : 9000;
            break;

        case RIG_MODE_PKTFM:
        case RIG_MODE_FM:
            *width = narrow ? 9000 : 16000;
            break;

        case RIG_MODE_FMN:
            *width = 9000;
            break;

        case RIG_MODE_AMN:
            *width = 6000;
            break;

        default:
            RETURNFUNC(-RIG_EINVAL);
        }   /* end switch(mode) */

    } /* end if is_ftdx5000 */
    else if (is_ftdx101d || is_ftdx101mp || is_ftdx10)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: is_ftdx101 w=%d, mode=%s\n", __func__, w,
                  rig_strrmode(mode));

        if (w == 0) // then we need to know the roofing filter
        {
            struct newcat_roofing_filter *roofing_filter;
            int err = get_roofing_filter(rig, vfo, &roofing_filter);

            if (err == RIG_OK)
            {
                *width = roofing_filter->width;
            }
        }

        switch (mode)
        {
        case RIG_MODE_PKTUSB:
        case RIG_MODE_PKTLSB:
        case RIG_MODE_RTTY:
        case RIG_MODE_RTTYR:
        case RIG_MODE_CW:
        case RIG_MODE_CWR:
            switch (w)
            {
            case 0: break; /* use roofing filter width */

            case 1: *width = 50; break;

            case 2: *width = 100; break;

            case 3: *width = 150; break;

            case 4: *width = 200; break;

            case 5: *width = 250; break;

            case 6: *width = 300; break;

            case 7: *width = 350; break;

            case 8: *width = 400; break;

            case 9: *width = 450;  break;

            case 10: *width = 500;  break;

            case 11: *width = 600;  break;

            case 12: *width = 800;  break;

            case 13: *width = 1200;  break;

            case 14: *width = 1400;  break;

            case 15: *width = 1700;  break;

            case 16: *width = 2000;  break;

            case 17: *width = 2400;  break;

            case 18: *width = 3000;  break;

            default: RETURNFUNC(-RIG_EINVAL);
            }

            break;

        case RIG_MODE_LSB:
        case RIG_MODE_USB:
            switch (w)
            {
            case 0: break; /* use roofing filter width */

            case 1: *width = 300; break;

            case 2: *width = 400; break;

            case 3: *width = 600; break;

            case 4: *width = 850; break;

            case 5: *width = 1100; break;

            case 6: *width = 1200; break;

            case 7: *width = 1500; break;

            case 8: *width = 1650; break;

            case 9: *width = 1800; break;

            case 10: *width = 1950;  break;

            case 11: *width = 2100;  break;

            case 12: *width = 2200;  break;

            case 13: *width = 2300;  break;

            case 14: *width = 2400;  break;

            case 15: *width = 2500;  break;

            case 16: *width = 2600;  break;

            case 17: *width = 2700;  break;

            case 18: *width = 2800;  break;

            case 19: *width = 2900;  break;

            case 20: *width = 3000;  break;

            case 21: *width = 3200;  break;

            case 22: *width = 3500;  break;

            case 23: *width = 4000;  break;

            default:
                rig_debug(RIG_DEBUG_ERR, "%s: unknown width=%d\n", __func__, w);
                RETURNFUNC(-RIG_EINVAL);
            }

            break;

        case RIG_MODE_AM:
        case RIG_MODE_FMN:
        case RIG_MODE_PKTFMN:
            *width = 9000;
            break;

        case RIG_MODE_AMN:
            *width = 6000;
            break;

        case RIG_MODE_FM:
        case RIG_MODE_PKTFM:
            *width = 16000;
            break;

        default:
            rig_debug(RIG_DEBUG_TRACE, "%s: bad mode\n", __func__);
            RETURNFUNC(-RIG_EINVAL);
        }   /* end switch(mode) */

        rig_debug(RIG_DEBUG_TRACE, "%s: end if FTDX101D\n", __func__);
    } /* end if is_ftdx101 */
    else if (is_ft2000)
    {
        if ((narrow = get_narrow(rig, RIG_VFO_MAIN)) < 0)
        {
            RETURNFUNC(-RIG_EPROTO);
        }

        switch (mode)
        {
        case RIG_MODE_CW:
        case RIG_MODE_CWR:
            if (w <= 4)
            {
                *width = 200;
            }
            else if (w <= 6)
            {
                *width = 500;
            }
            else if (w <= 16)
            {
                *width = 2400;
            }
            else
            {
                *width = 3000;
            }

            break;

        case RIG_MODE_PKTUSB:
        case RIG_MODE_PKTLSB:
            if (w <= 8)
            {
                *width = 200;
            }
            else if (w <= 16)
            {
                *width = 500;
            }
            else
            {
                *width = 2400;
            }

            break;

        case RIG_MODE_RTTY:
        case RIG_MODE_RTTYR:
            if (w <= 8)
            {
                *width = 300;
            }
            else if (w <= 16)
            {
                *width = 500;
            }
            else
            {
                *width = 2400;
            }

            break;

        case RIG_MODE_LSB:
        case RIG_MODE_USB:
            if (w <= 8)
            {
                *width = 1800;
            }
            else if (w <= 16)
            {
                *width = 2400;
            }
            else if (w <= 25)
            {
                *width = 3000;
            }
            else
            {
                *width = 4000;
            }

            break;

        case RIG_MODE_AM:
            *width = narrow ? 6000 : 9000;
            break;

        case RIG_MODE_PKTFM:
        case RIG_MODE_FM:
            *width = narrow ? 9000 : 16000;
            break;

        case RIG_MODE_FMN:
            *width = 9000;
            break;

        case RIG_MODE_AMN:
            *width = 6000;
            break;

        default:
            RETURNFUNC(-RIG_EINVAL);
        } /* end switch (mode) */
    } /* end if is_ft2000 */
    else
    {
        /* FT450, FT9000 */
        switch (mode)
        {
        case RIG_MODE_PKTUSB:
        case RIG_MODE_PKTLSB:
        case RIG_MODE_RTTY:
        case RIG_MODE_RTTYR:
        case RIG_MODE_CW:
        case RIG_MODE_CWR:
        case RIG_MODE_LSB:
        case RIG_MODE_USB:
            if (w < 16)
            {
                *width = rig_passband_narrow(rig, mode);
            }
            else if (w > 16)
            {
                *width = rig_passband_wide(rig, mode);
            }
            else
            {
                *width = rig_passband_normal(rig, mode);
            }

            break;

        case RIG_MODE_AM:
            *width = narrow ? 6000 : 9000;
            break;

        case RIG_MODE_PKTFM:
        case RIG_MODE_FM:
            *width = narrow ? 9000 : 16000;
            break;

        case RIG_MODE_FMN:
            *width = 9000;
            break;

        case RIG_MODE_AMN:
            *width = 6000;
            break;

        default:
            RETURNFUNC(-RIG_EINVAL);
        }   /* end switch (mode) */
    } /* end else */

    rig_debug(RIG_DEBUG_TRACE, "%s: RETURNFUNC(RIG_OK)\n", __func__);
    RETURNFUNC(RIG_OK);
}


int newcat_set_faststep(RIG *rig, ncboolean fast_step)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    char c;

    ENTERFUNC;

    if (!newcat_valid_command(rig, "FS"))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (fast_step == TRUE)
    {
        c = '1';
    }
    else
    {
        c = '0';
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "FS%c%c", c, cat_term);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);

    RETURNFUNC(newcat_set_cmd(rig));
}


int newcat_get_faststep(RIG *rig, ncboolean *fast_step)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    char c;
    char command[] = "FS";

    ENTERFUNC;

    if (!newcat_valid_command(rig, command))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%c", command, cat_term);

    /* Get Fast Step */
    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        RETURNFUNC(err);
    }

    c = priv->ret_data[2];

    if (c == '1')
    {
        *fast_step = TRUE;
    }
    else
    {
        *fast_step = FALSE;
    }

    RETURNFUNC(RIG_OK);
}


int newcat_get_rigid(RIG *rig)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    const char *s = NULL;

    ENTERFUNC;

    /* if first valid get */
    if (priv->rig_id == NC_RIGID_NONE)
    {
        s = newcat_get_info(rig);

        if (s != NULL)
        {
            s += 2;     /* ID0310, jump past ID */
            priv->rig_id = atoi(s);

            is_ftdx3000dm = priv->rig_id == NC_RIGID_FTDX3000DM;
        }

        rig_debug(RIG_DEBUG_TRACE, "rig_id = %d, idstr = %s\n", priv->rig_id,
                  s == NULL ? "NULL" : s);
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "rig_id = %d\n", priv->rig_id);
    }

    RETURNFUNC(priv->rig_id);
}


/*
 * input:   RIG *, vfo_t *
 * output:  VFO mode: RIG_VFO_VFO for VFO A or B
 *                    RIG_VFO_MEM for VFO MEM
 * return: RIG_OK or error
 */
int newcat_get_vfo_mode(RIG *rig, vfo_t vfo, rmode_t *vfo_mode)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    int offset = 0;
    char *cmd = "IF";

    ENTERFUNC;

    if (vfo == RIG_VFO_B || vfo == RIG_VFO_SUB)
    {
        // OI always returns VFOB and IF always VFOA
        cmd = "OI";
    }

    if (!newcat_valid_command(rig, cmd))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    /* Get VFO Information ****************** */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%c", cmd, cat_term);

    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        RETURNFUNC(err);
    }

    /* vfo, mem, P7 ************************** */
    // e.g. FT450 has 27 byte IF response, FT991 has 28 byte if response (one more byte for P2 VFO A Freq)
    // so we now check to ensure we know the length of the response
    switch (strlen(priv->ret_data))
    {
    case 27: offset = 21; priv->width_frequency = 8; break;

    case 28: offset = 22; priv->width_frequency = 9; break;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: incorrect length of IF response, expected 27 or 28, got %d", __func__,
                  (int)strlen(priv->ret_data));
        RETURNFUNC(-RIG_EPROTO);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: offset=%d, width_frequency=%d\n", __func__,
              offset, priv->width_frequency);

    switch (priv->ret_data[offset])
    {
    case '0': *vfo_mode = RIG_VFO_VFO; break;

    case '1':   /* Memory */
    case '2':   /* Memory Tune */
    case '3':   /* Quick Memory Bank */
    case '4':   /* Quick Memory Bank Tune */
    default:
        *vfo_mode = RIG_VFO_MEM;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo mode = %s\n", __func__,
              rig_strvfo(*vfo_mode));

    RETURNFUNC(err);
}


/*
 * Writes data and waits for response
 * input:  complete CAT command string including termination in cmd_str
 * output: complete CAT command answer string in ret_data
 * return: RIG_OK or error
 */


int newcat_vfomem_toggle(RIG *rig)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    char command[] = "VM";

    ENTERFUNC;

    if (!newcat_valid_command(rig, command))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    /* copy set command */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s%c", command, cat_term);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);

    RETURNFUNC(newcat_set_cmd(rig));
}

/*
 * Writes a null  terminated command string from  priv->cmd_str to the
 * CAT  port and  returns a  response from  the rig  in priv->ret_data
 * which is also null terminated.
 *
 * Honors the 'retry'  capabilities field by resending  the command up
 * to 'retry' times until a valid response is received. In the special
 * cases of receiving  a valid response to a different  command or the
 * "?;" busy please wait response; the command is not resent but up to
 * 'retry' retries to receive a valid response are made.
 */
int newcat_get_cmd(RIG *rig)
{
    struct rig_state *state = &rig->state;
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int retry_count = 0;
    int rc = -RIG_EPROTO;
    int is_read_cmd = 0;

    ENTERFUNC;

    // try to cache rapid repeats of the IF command
    // this is for WSJT-X/JTDX sequence of v/f/m/t
    // should allow rapid repeat of any call using the IF; cmd
    // Any call that changes something in the IF response should invalidate the cache
    if (strcmp(priv->cmd_str, "IF;") == 0 && priv->cache_start.tv_sec != 0)
    {
        int cache_age_ms;

        cache_age_ms = elapsed_ms(&priv->cache_start, 0);

        if (cache_age_ms < 500) // 500ms cache time
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: cache hit, age=%dms\n", __func__, cache_age_ms);
            strcpy(priv->ret_data, priv->last_if_response);
            RETURNFUNC(RIG_OK);
        }

        // we drop through and do the real IF command
    }

    // any command that is read only should not expire cache
    is_read_cmd =
        strcmp(priv->cmd_str, "AG0;") == 0
        || strcmp(priv->cmd_str, "AG1;") == 0
        || strcmp(priv->cmd_str, "AN0;") == 0
        || strcmp(priv->cmd_str, "AN1;") == 0
        || strcmp(priv->cmd_str, "BP00;") == 0
        || strcmp(priv->cmd_str, "BP01;") == 0
        || strcmp(priv->cmd_str, "BP10;") == 0
        || strcmp(priv->cmd_str, "BP11;") == 0
        || strcmp(priv->cmd_str, "CN00;") == 0
        || strcmp(priv->cmd_str, "CN10;") == 0
        || strcmp(priv->cmd_str, "CO00;") == 0
        || strcmp(priv->cmd_str, "CO01;") == 0
        || strcmp(priv->cmd_str, "CO02;") == 0
        || strcmp(priv->cmd_str, "CO03;") == 0
        || strcmp(priv->cmd_str, "CO10;") == 0
        || strcmp(priv->cmd_str, "CO11;") == 0
        || strcmp(priv->cmd_str, "CO12;") == 0
        || strcmp(priv->cmd_str, "CO13;") == 0
        || strcmp(priv->cmd_str, "IS1;") == 0
        || strcmp(priv->cmd_str, "IS0;") == 0
        || strcmp(priv->cmd_str, "IS1;") == 0
        || strcmp(priv->cmd_str, "MD0;") == 0
        || strcmp(priv->cmd_str, "MD1;") == 0
        || strcmp(priv->cmd_str, "NA0;") == 0
        || strcmp(priv->cmd_str, "NA1;") == 0
        || strcmp(priv->cmd_str, "NB0;") == 0
        || strcmp(priv->cmd_str, "NB1;") == 0
        || strcmp(priv->cmd_str, "NL0;") == 0
        || strcmp(priv->cmd_str, "NL1;") == 0
        || strcmp(priv->cmd_str, "NR0;") == 0
        || strcmp(priv->cmd_str, "NR1;") == 0
        || strcmp(priv->cmd_str, "NR0;") == 0
        || strcmp(priv->cmd_str, "NR1;") == 0
        || strcmp(priv->cmd_str, "OI;") == 0
        || strcmp(priv->cmd_str, "OS0;") == 0
        || strcmp(priv->cmd_str, "OS0;") == 0
        || strcmp(priv->cmd_str, "OS1;") == 0
        || strcmp(priv->cmd_str, "PA0;") == 0
        || strcmp(priv->cmd_str, "PA1;") == 0
        || strcmp(priv->cmd_str, "RA0;") == 0
        || strcmp(priv->cmd_str, "RA1;") == 0
        || strcmp(priv->cmd_str, "RF0;") == 0
        || strcmp(priv->cmd_str, "RF1;") == 0
        || strcmp(priv->cmd_str, "RL0;") == 0
        || strcmp(priv->cmd_str, "RL1;") == 0
        || strncmp(priv->cmd_str, "RM", 2) == 0
        || strcmp(priv->cmd_str, "SM0;") == 0
        || strcmp(priv->cmd_str, "SM1;") == 0
        || strcmp(priv->cmd_str, "SQ0;") == 0
        || strcmp(priv->cmd_str, "SQ1;") == 0
        || strcmp(priv->cmd_str, "VT0;") == 0
        || strcmp(priv->cmd_str, "VT1;") == 0;

    if (priv->cmd_str[2] !=
            ';' && !is_read_cmd) // then we must be setting something so we'll invalidate the cache
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: cache invalidated\n", __func__);
        priv->cache_start.tv_sec = 0;
    }


    while (rc != RIG_OK && retry_count++ <= state->rigport.retry)
    {
        rig_flush(&state->rigport);  /* discard any unsolicited data */

        if (rc != -RIG_BUSBUSY)
        {
            /* send the command */
            rig_debug(RIG_DEBUG_TRACE, "cmd_str = %s\n", priv->cmd_str);

            if (RIG_OK != (rc = write_block(&state->rigport,
                                            (unsigned char *) priv->cmd_str,
                                            strlen(priv->cmd_str))))
            {
                RETURNFUNC(rc);
            }
        }

        /* read the reply */
        if ((rc = read_string(&state->rigport, (unsigned char *) priv->ret_data,
                              sizeof(priv->ret_data),
                              &cat_term, sizeof(cat_term), 0, 1)) <= 0)
        {
            continue;             /* usually a timeout - retry */
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: read count = %d, ret_data = %s\n",
                  __func__, rc, priv->ret_data);
        rc = RIG_OK;              /* received something */

        /* Check that command termination is correct - alternative is
           response is longer than the buffer */
        if (cat_term  != priv->ret_data[strlen(priv->ret_data) - 1])
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n",
                      __func__, priv->ret_data);
            rc = -RIG_EPROTO; /* retry */
            /* we could decrement retry_count
               here but there is a danger of
               infinite looping so we just use up
               a retry for safety's sake */
            continue;
        }

        /* check for error codes */
        if (2 == strlen(priv->ret_data))
        {
            /* The following error responses  are documented for Kenwood
               but not for  Yaesu, but at least one of  them is known to
               occur  in that  the  FT-450 certainly  responds to  "IF;"
               occasionally with  "?;". The others are  harmless even of
               they do not occur as they are unambiguous. */
            switch (priv->ret_data[0])
            {
            case 'N':
                /* Command recognized by rig but invalid data entered. */
                rig_debug(RIG_DEBUG_VERBOSE, "%s: NegAck for '%s'\n", __func__, priv->cmd_str);
                RETURNFUNC(-RIG_ENAVAIL);

            case 'O':
                /* Too many characters sent without a carriage return */
                rig_debug(RIG_DEBUG_VERBOSE, "%s: Overflow for '%s'\n", __func__,
                          priv->cmd_str);
                rc = -RIG_EPROTO;
                break;            /* retry */

            case 'E':
                /* Communication error */
                rig_debug(RIG_DEBUG_VERBOSE, "%s: Communication error for '%s'\n", __func__,
                          priv->cmd_str);
                rc = -RIG_EIO;
                break;            /* retry */

            case '?':

                /* The ? response is ambiguous and undocumented by Yaesu, but for get commands it seems to
                 * indicate that the rig rejected the command because the state of the rig is not valid for the command
                 * or that the command parameter is invalid. Retrying the command does not fix the issue,
                 * as the error is caused by the an invalid combination of rig state.
                 *
                 * For example, the following cases have been observed:
                 * - MR and MC commands are rejected when referring to an _empty_ memory channel even
                 *   if the channel number is in a valid range
                 * - BC (ANF) and RL (NR) commands fail in AM/FM modes, because they are
                 *   supported only in SSB/CW/RTTY modes
                 * - MG (MICGAIN) command fails in RTTY mode, as it's a digital mode
                 *
                 * There are many more cases like these and they vary by rig model.
                 *
                 * So far, "rig busy" type situations with the ? response have not been observed for get commands.
                 * Followup 20201213 FTDX3000 FB; command returning ?; so do NOT abort
                 * see https://github.com/Hamlib/Hamlib/issues/464
                 */
                if (priv->question_mark_response_means_rejected)
                {
                    rig_debug(RIG_DEBUG_ERR, "%s: Command rejected by the rig (get): '%s'\n",
                              __func__,
                              priv->cmd_str);
                    RETURNFUNC(-RIG_ERJCTED);
                }

                rig_debug(RIG_DEBUG_WARN, "%s: Rig busy - retrying %d of %d: '%s'\n", __func__,
                          retry_count, state->rigport.retry, priv->cmd_str);
                // DX3000 was taking 1.6 seconds in certain command sequences
                hl_usleep(600 * 1000); // 600ms wait should cover most cases hopefully

                rc = -RIG_ERJCTED; /* retry */
                break;
            }

            continue;
        }

        /* verify that reply was to the command we sent */
        if ((priv->ret_data[0] != priv->cmd_str[0]
                || priv->ret_data[1] != priv->cmd_str[1]))
        {
            /*
             * TODO: When RIG_TRN is enabled, we can pass the string
             * to the decoder for callback. That way we don't ignore
             * any commands.
             */
            rig_debug(RIG_DEBUG_ERR, "%s: wrong reply %.2s for command %.2s\n",
                      __func__, priv->ret_data, priv->cmd_str);
            // we were using BUSBUSY but microham devices need retries
            // this should be OK under all other circumstances too
            //rc = -RIG_BUSBUSY;    /* retry read only */
            rc = -RIG_EPROTO;
        }
    }

    // update the cache
    if (strncmp(priv->cmd_str, "IF;", 3) == 0)
    {
        elapsed_ms(&priv->cache_start, 1);
        strcpy(priv->last_if_response, priv->ret_data);
    }

    RETURNFUNC(rc);
}

/*
 * This tries to set and read to validate the set command actually worked
 * returns RIG_OK if set, -RIG_EIMPL if not implemented yet, or -RIG_EPROTO if unsuccessful
 */
int newcat_set_cmd_validate(RIG *rig)
{
    struct rig_state *state = &rig->state;
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    char valcmd[16];
    int retries = 8;
    int retry = 0;
    int sleepms = 50;
    int rc = -RIG_EPROTO;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: priv->cmd_str=%s\n", __func__, priv->cmd_str);

    // For FA and FB rig.c now tries to verify the set_freq actually works
    // For example the FT-2000 can't do a FA set followed by an immediate read
    // We were using "ID" to verify the command but rig.c now does
    // a verification of frequency and retries if it doesn't match
    if ((strncmp(priv->cmd_str, "FA", 2) == 0) && (strlen(priv->cmd_str) > 3))
    {
        strcpy(valcmd, "FA;");

        if (priv->rig_id == NC_RIGID_FTDX3000 || priv->rig_id == NC_RIGID_FTDX5000
                || priv->rig_id == NC_RIGID_FTDX3000DM)
        {
            strcpy(valcmd, "");
        }
    }
    else if ((strncmp(priv->cmd_str, "FB", 2) == 0) && (strlen(priv->cmd_str) > 3))
    {
        strcpy(valcmd, "FB;");

        if (priv->rig_id == NC_RIGID_FTDX3000 || priv->rig_id == NC_RIGID_FTDX5000
                || priv->rig_id == NC_RIGID_FTDX3000DM)
        {
            strcpy(valcmd, "");
        }
    }
    else if ((strncmp(priv->cmd_str, "MD", 2) == 0) && (strlen(priv->cmd_str) > 3))
    {
        strcpy(valcmd, priv->cmd_str); // pull the needed part of the cmd
        valcmd[3] = ';';
        valcmd[4] = 0;
    }
    else if ((strncmp(priv->cmd_str, "TX", 2) == 0) && (strlen(priv->cmd_str) > 3))
    {
        strcpy(valcmd, "TX;");
    }
    else if ((strncmp(priv->cmd_str, "FT", 2) == 0) && (strlen(priv->cmd_str) > 3))
    {
        strcpy(valcmd, "FT;");
    }
    else if ((strncmp(priv->cmd_str, "AI", 2) == 0) && (strlen(priv->cmd_str) > 3))
    {
        strcpy(valcmd, "AI;");
    }
    else if ((strncmp(priv->cmd_str, "VS", 2) == 0) && (strlen(priv->cmd_str) > 3))
    {
        strcpy(valcmd, "VS;");

        // Some models treat the 2nd VS as a mute request
        if (is_ftdx3000 || is_ftdx9000)
        {
            strcpy(valcmd, "");
        }
    }
    else if (strncmp(priv->cmd_str, "SV", 2) == 0)
    {
        strcpy(valcmd, ""); // nothing to validate
    }
    else if (strncmp(priv->cmd_str, "BA", 2) == 0)
    {
        strcpy(valcmd, ""); // nothing to validate
    }
    else if (strncmp(priv->cmd_str, "AB", 2) == 0)
    {
        strcpy(valcmd, ""); // nothing to validate
    }
    else if (strncmp(priv->cmd_str, "BS", 2) == 0)
    {
        strcpy(valcmd, ""); // nothing to validate
    }
    else if (strncmp(priv->cmd_str, "MS", 2) == 0)
    {
        strcpy(valcmd, ""); // nothing to validate
    }
    else if (strncmp(priv->cmd_str, "SH", 2) == 0)
    {
        // could validate with SH but different formats need to be handled
        strcpy(valcmd, ""); // nothing to validate
    }
    else if (strncmp(priv->cmd_str, "RF", 2) == 0)
    {
        // could validate with RF but different formats need to be handled
        strcpy(valcmd, ""); // nothing to validate
    }
    else if (strncmp(priv->cmd_str, "BU", 2) == 0)
    {
        strcpy(valcmd, "FA;"); // nothing to validate
    }
    else if (strncmp(priv->cmd_str, "BD", 2) == 0)
    {
        strcpy(valcmd, "FA;"); // nothing to validate
    }
    else if (strncmp(priv->cmd_str, "EX", 2) == 0)
    {
        strcpy(valcmd, "");
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: %s not implemented\n", __func__, priv->cmd_str);
        RETURNFUNC(-RIG_ENIMPL);
    }

    while (rc != RIG_OK && retry++ < retries)
    {
        int bytes;
        char cmd[256]; // big enough
        rig_flush(&state->rigport);  /* discard any unsolicited data */
        SNPRINTF(cmd, sizeof(cmd), "%s%s", priv->cmd_str, valcmd);
        rc = write_block(&state->rigport, (unsigned char *) cmd, strlen(cmd));

        if (rc != RIG_OK) { RETURNFUNC(-RIG_EIO); }

        if (strlen(valcmd) == 0) { RETURNFUNC(RIG_OK); }

        bytes = read_string(&state->rigport, (unsigned char *) priv->ret_data,
                            sizeof(priv->ret_data),
                            &cat_term, sizeof(cat_term), 0, 1);

        // FA and FB success is now verified in rig.c with a followup query
        // so no validation is needed
        if (strncmp(priv->cmd_str, "FA", 2) == 0 || strncmp(priv->cmd_str, "FB", 2))
        {
            return RIG_OK;
        }

        if (strncmp(priv->cmd_str, "FT", 2) == 0
                && strncmp(priv->ret_data, "FT", 2) == 0)
        {
            // FT command does not echo what's sent so we just check the basic command
            RETURNFUNC(RIG_OK);
        }

        if (strncmp(priv->cmd_str, "TX", 2) == 0
                && strncmp(priv->ret_data, "TX", 2) == 0)
        {
            // TX command does not echo what's sent so we just check the basic command
            RETURNFUNC(RIG_OK);
        }

        if (bytes > 0)
        {
            // for the BS command we can only run it once
            // so we'll assume it worked
            // maybe Yaeus will make this command more intelligent
            if (strstr(priv->cmd_str, "BS")) { RETURNFUNC(RIG_OK); }

            // if the first two chars match we are validated
            if (strncmp(priv->cmd_str, "VS", 2) == 0
                    && strncmp(priv->cmd_str, priv->ret_data, 2) == 0) { RETURNFUNC(RIG_OK); }
            else if (strcmp(priv->cmd_str, priv->ret_data) == 0) { RETURNFUNC(RIG_OK); }
            else { rc = -RIG_EPROTO; }
        }

        rig_debug(RIG_DEBUG_WARN, "%s: cmd validation failed, '%s'!='%s', try#%d\n",
                  __func__, priv->cmd_str, priv->ret_data, retry);
        hl_usleep(sleepms * 1000);
    }

    RETURNFUNC(-RIG_EPROTO);
}
/*
 * Writes a null  terminated command string from  priv->cmd_str to the
 * CAT  port that is not expected to have a response.
 *
 * Honors the 'retry'  capabilities field by resending  the command up
 * to 'retry' times until a valid response is received. In the special
 * cases of receiving  a valid response to a different  command or the
 * "?;" busy please wait response; the command is not resent but up to
 * 'retry' retries to receive a valid response are made.
 */
int newcat_set_cmd(RIG *rig)
{
    struct rig_state *state = &rig->state;
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int retry_count = 0;
    int rc = -RIG_EPROTO;

    ENTERFUNC;
    /* pick a basic quick query command for verification */
    char const *const verify_cmd = RIG_MODEL_FT9000 == rig->caps->rig_model ?
                                   "AI;" : "ID;";

    while (rc != RIG_OK && retry_count++ <= state->rigport.retry)
    {
        rig_flush(&state->rigport);  /* discard any unsolicited data */
        /* send the command */
        rig_debug(RIG_DEBUG_TRACE, "cmd_str = %s\n", priv->cmd_str);

        rc =  newcat_set_cmd_validate(rig);

        if (rc == RIG_OK)
        {
            // if we were able to set and and validate we're done
            rig_debug(RIG_DEBUG_TRACE, "%s: cmd_validate OK\n", __func__);
            RETURNFUNC(RIG_OK);
        }
        else if (rc == -RIG_EPROTO)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: set_cmd_validate failed\n", __func__);
            RETURNFUNC(rc);
        }

        rig_debug(RIG_DEBUG_TRACE,
                  "%s: newcat_set_cmd_validate not implemented...continuing\n", __func__);

        if (RIG_OK != (rc = write_block(&state->rigport,
                                        (unsigned char *) priv->cmd_str,
                                        strlen(priv->cmd_str))))
        {
            RETURNFUNC(rc);
        }

        /* skip validation if high throughput is needed */
        if (priv->fast_set_commands == TRUE)
        {
            RETURNFUNC(RIG_OK);
        }

        // freq set and ptt are now verified in rig.c
        if (strncmp(priv->cmd_str, "FA", 2) == 0
                || strncmp(priv->cmd_str, "FB", 2) == 0
                || strncmp(priv->cmd_str, "TX", 2) == 0)
        {
            RETURNFUNC(RIG_OK);
        }


        if (strncmp(priv->cmd_str, "BS", 2) == 0)
        {
            // the BS command needs time to do it's thing
            hl_usleep(500 * 1000);
            priv->cache_start.tv_sec = 0; // invalidate the cache
        }

        /* send the verification command */
        rig_debug(RIG_DEBUG_TRACE, "cmd_str = %s\n", verify_cmd);

        if (RIG_OK != (rc = write_block(&state->rigport, (unsigned char *) verify_cmd,
                                        strlen(verify_cmd))))
        {
            RETURNFUNC(rc);
        }

        /* read the reply */
        if ((rc = read_string(&state->rigport, (unsigned char *) priv->ret_data,
                              sizeof(priv->ret_data),
                              &cat_term, sizeof(cat_term), 0, 1)) <= 0)
        {
            continue;             /* usually a timeout - retry */
        }

        rig_debug(RIG_DEBUG_TRACE, "%s(%d): read count = %d, ret_data = %s\n",
                  __func__, __LINE__, rc, priv->ret_data);
        rc = RIG_OK;              /* received something */

        /* check for error codes */
        if (2 == strlen(priv->ret_data))
        {
            /* The following error responses  are documented for Kenwood
               but not for  Yaesu, but at least one of  them is known to
               occur  in that  the  FT-450 certainly  responds to  "IF;"
               occasionally with  "?;". The others are  harmless even of
               they do not occur as they are unambiguous. */
            switch (priv->ret_data[0])
            {
            case 'N':
                /* Command recognized by rig but invalid data entered. */
                rig_debug(RIG_DEBUG_VERBOSE, "%s: NegAck for '%s'\n", __func__, priv->cmd_str);
                RETURNFUNC(-RIG_ENAVAIL);

            case 'O':
                /* Too many characters sent without a carriage return */
                rig_debug(RIG_DEBUG_VERBOSE, "%s: Overflow for '%s'\n", __func__,
                          priv->cmd_str);
                rc = -RIG_EPROTO;
                break;            /* retry */

            case 'E':
                /* Communication error */
                rig_debug(RIG_DEBUG_VERBOSE, "%s: Communication error for '%s'\n", __func__,
                          priv->cmd_str);
                rc = -RIG_EIO;
                break;            /* retry */

            case '?':

                /* The ? response is ambiguous and undocumented by Yaesu. For set commands it seems to indicate:
                 * 1) either that the rig is busy and the command needs to be retried
                 * 2) or that the rig rejected the command because the state of the rig is not valid for the command
                 *    or that the command parameter is invalid. Retrying the command does not fix the issue
                 *    in this case, as the error is caused by the an invalid combination of rig state.
                 *    The latter case is consistent with behaviour of get commands.
                 *
                 * For example, the following cases have been observed:
                 * - MR and MC commands are rejected when referring to an _empty_ memory channel even
                 *   if the channel number is in a valid range
                 * - BC (ANF) and RL (NR) commands fail in AM/FM modes, because they are
                 *   supported only in SSB/CW/RTTY modes
                 * - MG (MICGAIN) command fails in RTTY mode, as it's a digital mode
                 *
                 * There are many more cases like these and they vary by rig model.
                 */
                if (priv->question_mark_response_means_rejected)
                {
                    rig_debug(RIG_DEBUG_ERR, "%s: Command rejected by the rig (set): '%s'\n",
                              __func__,
                              priv->cmd_str);
                    RETURNFUNC(-RIG_ERJCTED);
                }

                /* Rig busy wait please */
                rig_debug(RIG_DEBUG_WARN, "%s: Rig busy - retrying: '%s'\n", __func__,
                          priv->cmd_str);

                /* read/flush the verify command reply which should still be there */
                if ((rc = read_string(&state->rigport, (unsigned char *) priv->ret_data,
                                      sizeof(priv->ret_data),
                                      &cat_term, sizeof(cat_term), 0, 1)) > 0)
                {
                    rig_debug(RIG_DEBUG_TRACE, "%s(%d): read count = %d, ret_data = %s\n",
                              __func__, __LINE__, rc, priv->ret_data);
                    rc = -RIG_BUSBUSY; /* retry */
                }

                break;
            }
        }

        if (RIG_OK == rc)
        {
            /* Check that response prefix and response termination is
               correct - alternative is response is longer that the
               buffer */
            if (strncmp(verify_cmd, priv->ret_data, strlen(verify_cmd) - 1)
                    || (cat_term != priv->ret_data[strlen(priv->ret_data) - 1]))
            {
                rig_debug(RIG_DEBUG_ERR, "%s: Unexpected verify command response '%s'\n",
                          __func__, priv->ret_data);
                rc = -RIG_BUSBUSY;
                continue;             /* retry */
            }
        }
    }

    RETURNFUNC(rc);
}

struct
{
    rmode_t mode;
    char modechar;
    ncboolean chk_width;
} static const newcat_mode_conv[] =
{
    { RIG_MODE_LSB,    '1', FALSE },
    { RIG_MODE_USB,    '2', FALSE },
    { RIG_MODE_CW,     '3', FALSE },
    { RIG_MODE_FM,     '4',  TRUE},
    { RIG_MODE_AM,     '5',  TRUE},
    { RIG_MODE_RTTY,   '6', FALSE },
    { RIG_MODE_CWR,    '7', FALSE },
    { RIG_MODE_PKTLSB, '8', FALSE },
    { RIG_MODE_RTTYR,  '9', FALSE },
    { RIG_MODE_PKTFM,  'A',  TRUE},
    { RIG_MODE_FMN,    'B',  TRUE},
    { RIG_MODE_PKTUSB, 'C', FALSE },
    { RIG_MODE_AMN,    'D',  TRUE},
    { RIG_MODE_C4FM,   'E',  TRUE},
    { RIG_MODE_PKTFMN, 'F',  TRUE}
};

rmode_t newcat_rmode(char mode)
{
    int i;

    for (i = 0; i < sizeof(newcat_mode_conv) / sizeof(newcat_mode_conv[0]); i++)
    {
        if (newcat_mode_conv[i].modechar == mode)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: %s for %c\n", __func__,
                      rig_strrmode(newcat_mode_conv[i].mode), mode);
            return (newcat_mode_conv[i].mode);
        }
    }

    return (RIG_MODE_NONE);
}

char newcat_modechar(rmode_t rmode)
{
    int i;

    for (i = 0; i < sizeof(newcat_mode_conv) / sizeof(newcat_mode_conv[0]); i++)
    {
        if (newcat_mode_conv[i].mode == rmode)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: return %c for %s\n", __func__,
                      newcat_mode_conv[i].modechar, rig_strrmode(rmode));
            return (newcat_mode_conv[i].modechar);
        }
    }

    return ('0');
}

rmode_t newcat_rmode_width(RIG *rig, vfo_t vfo, char mode, pbwidth_t *width)
{
    ncboolean narrow;
    int i;

    ENTERFUNC;

    *width = RIG_PASSBAND_NORMAL;

    for (i = 0; i < sizeof(newcat_mode_conv) / sizeof(newcat_mode_conv[0]); i++)
    {
        if (newcat_mode_conv[i].modechar == mode)
        {
            if (newcat_mode_conv[i].chk_width == TRUE)
            {
                // crude fix because 991 hangs on NA0; command while in C4FM
                if (newcat_is_rig(rig, RIG_MODEL_FT991))
                {
                    if (mode == 'E')
                    {
                        *width = 16000;
                    }
                    else if (mode == 'F')
                    {
                        *width = 9000;
                    }

                    rig_debug(RIG_DEBUG_TRACE, "991A & C4FM Skip newcat_get_narrow in %s\n",
                              __func__);
                }
                else
                {
                    if (newcat_get_narrow(rig, vfo, &narrow) != RIG_OK)
                    {
                        RETURNFUNC(newcat_mode_conv[i].mode);
                    }

                    if (narrow == TRUE)
                    {
                        *width = rig_passband_narrow(rig, mode);
                    }
                    else
                    {
                        *width = rig_passband_normal(rig, mode);
                    }
                }
            }

            // don't use RETURNFUNC here as that macros expects an int for the return code
            return (newcat_mode_conv[i].mode);
        }
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s fell out the bottom %c %s\n", __func__,
              mode, rig_strrmode(mode));

    RETURNFUNC('0');
}

int newcat_send_voice_mem(RIG *rig, vfo_t vfo, int ch)
{
    char *p1 = "0";  // newer rigs have 2 bytes where is fixed at zero e.g. FT991
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;

    if (!newcat_valid_command(rig, "PB"))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    // we don't do any channel checking -- varies by rig -- could do it but not critical

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PB%s%d%c", p1, ch, cat_term);
    RETURNFUNC2(newcat_set_cmd(rig));
}

static int newcat_set_apf_frequency(RIG *rig, vfo_t vfo, int freq)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    char main_sub_vfo = (RIG_VFO_B == vfo || RIG_VFO_SUB == vfo) ? '1' : '0';

    if (!newcat_valid_command(rig, "CO"))
    {
        RETURNFUNC2(-RIG_ENAVAIL);
    }

    // Range seems to be -250..250 Hz in 10 Hz steps
    if (is_ftdx101d || is_ftdx101mp)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%c3%04d%c", main_sub_vfo,
                 (freq + 250) / 10, cat_term);
    }
    else if (is_ftdx10 || is_ft991 || is_ft891)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO03%04d%c", (freq + 250) / 10,
                 cat_term);
    }
    else if (is_ftdx3000 || is_ftdx1200)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO02%02d%c", (freq + 250) / 10,
                 cat_term);
    }
    else
    {
        RETURNFUNC2(-RIG_ENIMPL);
    }

    RETURNFUNC2(newcat_set_cmd(rig));
}

static int newcat_get_apf_frequency(RIG *rig, vfo_t vfo, int *freq)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    char main_sub_vfo = (RIG_VFO_B == vfo || RIG_VFO_SUB == vfo) ? '1' : '0';
    int err;
    int ret_data_len;
    char *ret_data;

    if (!newcat_valid_command(rig, "CO"))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (is_ftdx101d || is_ftdx101mp)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%c3%c", main_sub_vfo,
                 cat_term);
    }
    else if (is_ftdx10 || is_ft991 || is_ft891)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO03%c", cat_term);
    }
    else if (is_ftdx3000 || is_ftdx1200)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO02%c", cat_term);
    }
    else
    {
        RETURNFUNC(-RIG_ENIMPL);
    }

    if ((err = newcat_get_cmd(rig)) != RIG_OK)
    {
        RETURNFUNC(err);
    }

    ret_data_len = strlen(priv->ret_data);

    /* skip command */
    ret_data = priv->ret_data + strlen(priv->cmd_str) - 1;
    rig_debug(RIG_DEBUG_TRACE, "%s: ret_data='%s'\n", __func__, ret_data);
    /* chop term */
    priv->ret_data[ret_data_len - 1] = '\0';

    int raw_value = atoi(ret_data);

    // Range seems to be -250..250 Hz in 10 Hz steps
    *freq = raw_value * 10 - 250;

    RETURNFUNC2(RIG_OK);
}

static int newcat_set_apf_width(RIG *rig, vfo_t vfo, int choice)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;

    if (!newcat_valid_command(rig, "EX"))
    {
        RETURNFUNC2(-RIG_ENAVAIL);
    }

    if (is_ftdx101d || is_ftdx101mp || is_ftdx10)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX030201%d%c", choice,
                 cat_term);
    }
    else if (is_ft991)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX111%d%c", choice, cat_term);
    }
    else if (is_ft891)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX1201%d%c", choice, cat_term);
    }
    else if (is_ftdx5000)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX112%d%c", choice, cat_term);
    }
    else if (is_ftdx3000 || is_ftdx1200)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX107%d%c", choice, cat_term);
    }
    else
    {
        RETURNFUNC2(-RIG_ENIMPL);
    }

    RETURNFUNC2(newcat_set_cmd(rig));
}

static int newcat_get_apf_width(RIG *rig, vfo_t vfo, int *choice)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    int ret_data_len;
    char *ret_data;

    if (!newcat_valid_command(rig, "EX"))
    {
        RETURNFUNC2(-RIG_ENAVAIL);
    }

    if (is_ftdx101d || is_ftdx101mp || is_ftdx10)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX030201%c", cat_term);
    }
    else if (is_ft991)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX111%c", cat_term);
    }
    else if (is_ft891)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX1201%c", cat_term);
    }
    else if (is_ftdx5000)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX112%c", cat_term);
    }
    else if (is_ftdx3000 || is_ftdx1200)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX107%c", cat_term);
    }
    else
    {
        RETURNFUNC2(-RIG_ENIMPL);
    }

    if ((err = newcat_get_cmd(rig)) != RIG_OK)
    {
        RETURNFUNC2(err);
    }

    ret_data_len = strlen(priv->ret_data);

    /* skip command */
    ret_data = priv->ret_data + strlen(priv->cmd_str) - 1;
    rig_debug(RIG_DEBUG_TRACE, "%s: ret_data='%s'\n", __func__, ret_data);
    /* chop term */
    priv->ret_data[ret_data_len - 1] = '\0';

    *choice = atoi(ret_data);

    RETURNFUNC2(RIG_OK);
}

static int newcat_set_contour(RIG *rig, vfo_t vfo, int status)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    char main_sub_vfo = (RIG_VFO_B == vfo || RIG_VFO_SUB == vfo) ? '1' : '0';

    if (!newcat_valid_command(rig, "CO"))
    {
        RETURNFUNC2(-RIG_ENAVAIL);
    }

    if (is_ftdx101d || is_ftdx101mp)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%c0%04d%c", main_sub_vfo,
                 status ? 1 : 0, cat_term);
    }
    else if (is_ftdx10 || is_ft991 || is_ft891)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO00%04d%c", status ? 1 : 0,
                 cat_term);
    }
    else if (is_ftdx5000)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%c0%02d%c", main_sub_vfo,
                 status ? 1 : 0, cat_term);
    }
    else if (is_ftdx3000 || is_ftdx1200 || is_ft2000)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO00%02d%c", status ? 1 : 0,
                 cat_term);
    }
    else
    {
        RETURNFUNC2(-RIG_ENIMPL);
    }

    RETURNFUNC2(newcat_set_cmd(rig));
}

static int newcat_get_contour(RIG *rig, vfo_t vfo, int *status)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    char main_sub_vfo = (RIG_VFO_B == vfo || RIG_VFO_SUB == vfo) ? '1' : '0';
    int err;
    int ret_data_len;
    char *ret_data;
    int last_char_index;

    if (!newcat_valid_command(rig, "CO"))
    {
        RETURNFUNC2(-RIG_ENAVAIL);
    }

    if (is_ftdx101d || is_ftdx101mp)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%c0%c", main_sub_vfo,
                 cat_term);
    }
    else if (is_ftdx10 || is_ft991 || is_ft891)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO00%c", cat_term);
    }
    else if (is_ftdx5000)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%c0%c", main_sub_vfo,
                 cat_term);
    }
    else if (is_ftdx3000 || is_ftdx1200 || is_ft2000)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO00%c", cat_term);
    }
    else
    {
        RETURNFUNC2(-RIG_ENIMPL);
    }

    if ((err = newcat_get_cmd(rig)) != RIG_OK)
    {
        RETURNFUNC2(err);
    }

    ret_data_len = strlen(priv->ret_data);

    /* skip command */
    ret_data = priv->ret_data + strlen(priv->cmd_str) - 1;
    rig_debug(RIG_DEBUG_TRACE, "%s: ret_data='%s'\n", __func__, ret_data);
    /* chop term */
    priv->ret_data[ret_data_len - 1] = '\0';

    last_char_index = strlen(ret_data) - 1;

    *status = (ret_data[last_char_index] == '1') ? 1 : 0;

    RETURNFUNC2(RIG_OK);
}

static int newcat_set_contour_frequency(RIG *rig, vfo_t vfo, int freq)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    char main_sub_vfo = (RIG_VFO_B == vfo || RIG_VFO_SUB == vfo) ? '1' : '0';

    if (!newcat_valid_command(rig, "CO"))
    {
        RETURNFUNC2(-RIG_ENAVAIL);
    }

    if (is_ftdx101d || is_ftdx101mp)
    {
        // Range is 10..3200 Hz
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%c1%04d%c", main_sub_vfo,
                 freq, cat_term);
    }
    else if (is_ftdx10 || is_ft991 || is_ft891)
    {
        // Range is 10..3200 Hz
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO01%04d%c", freq, cat_term);
    }
    else if (is_ftdx5000)
    {
        // Range is 100..4000 Hz in 100 Hz steps
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%c1%01d%c", main_sub_vfo,
                 freq / 100, cat_term);
    }
    else if (is_ftdx3000 || is_ftdx1200 || is_ft2000)
    {
        // Range is 100..4000 Hz in 100 Hz steps
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO01%02d%c", freq / 100,
                 cat_term);
    }
    else
    {
        RETURNFUNC2(-RIG_ENIMPL);
    }

    RETURNFUNC2(newcat_set_cmd(rig));
}

static int newcat_get_contour_frequency(RIG *rig, vfo_t vfo, int *freq)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    char main_sub_vfo = (RIG_VFO_B == vfo || RIG_VFO_SUB == vfo) ? '1' : '0';
    int err;
    int ret_data_len;
    char *ret_data;

    if (!newcat_valid_command(rig, "CO"))
    {
        RETURNFUNC2(-RIG_ENAVAIL);
    }

    if (is_ftdx101d || is_ftdx101mp)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%c1%c", main_sub_vfo,
                 cat_term);
    }
    else if (is_ftdx10 || is_ft991 || is_ft891)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO01%c", cat_term);
    }
    else if (is_ftdx5000)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%c1%c", main_sub_vfo,
                 cat_term);
    }
    else if (is_ftdx3000 || is_ftdx1200 || is_ft2000)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO01%c", cat_term);
    }
    else
    {
        RETURNFUNC2(-RIG_ENIMPL);
    }

    if ((err = newcat_get_cmd(rig)) != RIG_OK)
    {
        RETURNFUNC2(err);
    }

    ret_data_len = strlen(priv->ret_data);

    /* skip command */
    ret_data = priv->ret_data + strlen(priv->cmd_str) - 1;
    rig_debug(RIG_DEBUG_TRACE, "%s: ret_data='%s'\n", __func__, ret_data);
    /* chop term */
    priv->ret_data[ret_data_len - 1] = '\0';

    int raw_value = atoi(ret_data);

    if (is_ftdx101d || is_ftdx101mp || is_ftdx10 || is_ft991 || is_ft891)
    {
        *freq = raw_value;
    }
    else if (is_ftdx5000 || is_ftdx3000 || is_ftdx1200 || is_ft2000)
    {
        *freq = raw_value * 100;
    }
    else
    {
        RETURNFUNC2(-RIG_ENIMPL);
    }

    RETURNFUNC2(RIG_OK);
}

static int newcat_set_contour_level(RIG *rig, vfo_t vfo, int level)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;

    if (!newcat_valid_command(rig, "EX"))
    {
        RETURNFUNC2(-RIG_ENAVAIL);
    }

    if (is_ftdx101d || is_ftdx101mp || is_ftdx10)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX030202%+03d%c", level,
                 cat_term);
    }
    else if (is_ft991)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX112%+03d%c", level, cat_term);
    }
    else if (is_ft891)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX1202%+03d%c", level,
                 cat_term);
    }
    else if (is_ftdx5000)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX113%+03d%c", level, cat_term);
    }
    else if (is_ftdx3000 || is_ftdx1200)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX108%+03d%c", level, cat_term);
    }
    else
    {
        RETURNFUNC2(-RIG_ENIMPL);
    }

    RETURNFUNC2(newcat_set_cmd(rig));
}

static int newcat_get_contour_level(RIG *rig, vfo_t vfo, int *level)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    int ret_data_len;
    char *ret_data;

    if (!newcat_valid_command(rig, "EX"))
    {
        RETURNFUNC2(-RIG_ENAVAIL);
    }

    if (is_ftdx101d || is_ftdx101mp || is_ftdx10)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX030202%c", cat_term);
    }
    else if (is_ft991)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX112%c", cat_term);
    }
    else if (is_ft891)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX1202%c", cat_term);
    }
    else if (is_ftdx5000)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX113%c", cat_term);
    }
    else if (is_ftdx3000 || is_ftdx1200)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX108%c", cat_term);
    }
    else
    {
        RETURNFUNC2(-RIG_ENIMPL);
    }

    if ((err = newcat_get_cmd(rig)) != RIG_OK)
    {
        RETURNFUNC2(err);
    }

    ret_data_len = strlen(priv->ret_data);

    /* skip command */
    ret_data = priv->ret_data + strlen(priv->cmd_str) - 1;
    rig_debug(RIG_DEBUG_TRACE, "%s: ret_data='%s'\n", __func__, ret_data);
    /* chop term */
    priv->ret_data[ret_data_len - 1] = '\0';

    *level = atoi(ret_data);

    RETURNFUNC2(RIG_OK);
}

static int newcat_set_contour_width(RIG *rig, vfo_t vfo, int width)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;

    if (!newcat_valid_command(rig, "EX"))
    {
        RETURNFUNC2(-RIG_ENAVAIL);
    }

    if (is_ftdx101d || is_ftdx101mp || is_ftdx10)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX030203%02d%c", width,
                 cat_term);
    }
    else if (is_ft991)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX113%02d%c", width, cat_term);
    }
    else if (is_ft891)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX1203%02d%c", width, cat_term);
    }
    else if (is_ftdx5000)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX114%02d%c", width, cat_term);
    }
    else if (is_ftdx3000 || is_ftdx1200)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX109%02d%c", width, cat_term);
    }
    else
    {
        RETURNFUNC2(-RIG_ENIMPL);
    }

    RETURNFUNC2(newcat_set_cmd(rig));
}

static int newcat_get_contour_width(RIG *rig, vfo_t vfo, int *width)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;
    int err;
    int ret_data_len;
    char *ret_data;

    if (!newcat_valid_command(rig, "EX"))
    {
        RETURNFUNC2(-RIG_ENAVAIL);
    }

    if (is_ftdx101d || is_ftdx101mp || is_ftdx10)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX030203%c", cat_term);
    }
    else if (is_ft991)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX113%c", cat_term);
    }
    else if (is_ft891)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX1203%c", cat_term);
    }
    else if (is_ftdx5000)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX114%c", cat_term);
    }
    else if (is_ftdx3000 || is_ftdx1200)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX109%c", cat_term);
    }
    else
    {
        RETURNFUNC2(-RIG_ENIMPL);
    }

    if ((err = newcat_get_cmd(rig)) != RIG_OK)
    {
        RETURNFUNC2(err);
    }

    ret_data_len = strlen(priv->ret_data);

    /* skip command */
    ret_data = priv->ret_data + strlen(priv->cmd_str) - 1;
    rig_debug(RIG_DEBUG_TRACE, "%s: ret_data='%s'\n", __func__, ret_data);
    /* chop term */
    priv->ret_data[ret_data_len - 1] = '\0';

    *width = atoi(ret_data);

    RETURNFUNC2(RIG_OK);
}

int newcat_set_clock(RIG *rig, int year, int month, int day, int hour, int min,
                     int sec, double msec, int utc_offset)
{
    int retval = RIG_OK;
    int err;
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;

    if (!newcat_valid_command(rig, "DT"))
    {
        RETURNFUNC2(-RIG_ENAVAIL);
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DT0%04d%02d%02d%c", year, month,
             day, cat_term);

    if (RIG_OK != (err = newcat_set_cmd(rig)))
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s:%d command err = %d\n", __func__, __LINE__,
                  err);
        RETURNFUNC2(err);
    }

    if (hour < 0) { RETURNFUNC2(RIG_OK); }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DT1%02d%02d%02d%c", hour, min,
             sec, cat_term);

    if (RIG_OK != (err = newcat_set_cmd(rig)))
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s:%d command err = %d\n", __func__, __LINE__,
                  err);
        RETURNFUNC2(err);
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DT2%c%04d%c",
             utc_offset >= 0 ? '+' : '-', utc_offset, cat_term);

    if (RIG_OK != (err = newcat_set_cmd(rig)))
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s:%d command err = %d\n", __func__, __LINE__,
                  err);
        RETURNFUNC2(err);
    }

    RETURNFUNC2(retval);
}

int newcat_get_clock(RIG *rig, int *year, int *month, int *day, int *hour,
                     int *min, int *sec, double *msec, int *utc_offset)
{
    int retval = RIG_OK;
    int err;
    int n;
    struct newcat_priv_data *priv = (struct newcat_priv_data *)rig->state.priv;

    if (!newcat_valid_command(rig, "DT"))
    {
        RETURNFUNC2(-RIG_ENAVAIL);
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DT0%c", cat_term);

    if ((err = newcat_get_cmd(rig)) != RIG_OK)
    {
        RETURNFUNC2(err);
    }

    n = sscanf(priv->ret_data, "DT0%04d%02d%02d", year, month, day);

    if (n != 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: DT0 unable to parse '%s'\n", __func__,
                  priv->ret_data);
        RETURNFUNC2(-RIG_EPROTO);
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DT1%c", cat_term);

    if ((err = newcat_get_cmd(rig)) != RIG_OK)
    {
        RETURNFUNC2(err);
    }

    n = sscanf(priv->ret_data, "DT1%02d%02d%02d", hour, min, sec);

    if (n != 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: DT1 unable to parse '%s'\n", __func__,
                  priv->ret_data);
        RETURNFUNC2(-RIG_EPROTO);
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DT2%c", cat_term);

    if ((err = newcat_get_cmd(rig)) != RIG_OK)
    {
        RETURNFUNC2(err);
    }

    // we keep utc_offset in HHMM format rather than converting
    n = sscanf(priv->ret_data, "DT2%d", utc_offset);

    if (n != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: DT2 unable to parse '%s'\n", __func__,
                  priv->ret_data);
        RETURNFUNC2(-RIG_EPROTO);
    }

    RETURNFUNC2(retval);
}
