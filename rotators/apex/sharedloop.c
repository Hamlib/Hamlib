/* Apex Shared Loop Controller */

#include <math.h>
#include "hamlib/rotator.h"
#include "iofunc.h"
#include "apex.h"

int apex_shared_loop_get_position(ROT *rot, float *az, float *el)
{
    int loop = 10;

    while (--loop > 0 && apex_azimuth < 0)
    {
        hl_usleep(250 * 1000);
    };

    *az = apex_azimuth;

    *el = 0;

    return RIG_OK;
}

int apex_shared_loop_set_position(ROT *rot, float az, float dummy)
{
    char cmdstr[16];
    int retval;
    hamlib_port_t *rotp = ROTPORT(rot);
    int remainder = lround(az + 22.5) % 45;
    int apex_az = lround(az + 22.5) - remainder;

    // default to 0 degrees
    snprintf(cmdstr, sizeof(cmdstr), "[R99T4AM10]\r\n");

    switch (apex_az)
    {
    case  45: cmdstr[9] = '1'; break;

    case  90: cmdstr[9] = '2'; break;

    case 135: cmdstr[9] = '3'; break;

    case 180: cmdstr[9] = '4'; break;

    case 225: cmdstr[9] = '5'; break;

    case 270: cmdstr[9] = '6'; break;

    case 315: cmdstr[9] = '7'; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unknown az=%d\n", __func__, apex_az);
        return -RIG_EINTERNAL;
    }

    rig_flush(rotp);
    apex_azimuth = -1;
    retval = write_block(rotp, (unsigned char *) cmdstr, strlen(cmdstr));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: write_block error - %s\n", __func__,
                  rigerror(retval));
        return retval;
    }

    return RIG_OK;
}

const struct rot_caps apex_shared_loop_rot_caps =
{
    ROT_MODEL(ROT_MODEL_APEX_SHARED_LOOP),
    .model_name =     "Shared Loop",
    .mfg_name =       "Apex",
    .version =        "20231013.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rot_type =       ROT_TYPE_AZIMUTH,
    .port_type =      RIG_PORT_SERIAL,
    .serial_rate_min  = 57600,
    .serial_rate_max  = 57600,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity    = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay      = 0,
    .post_write_delay = 0,
    .timeout          = 4000,
    .retry            = 2,

    .min_az =     0.0,
    .max_az =     360.0,

    .rot_open = apex_open,
    .get_info = apex_get_info,
    .get_position = apex_shared_loop_get_position,
    .set_position = apex_shared_loop_set_position,
};

