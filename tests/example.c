/*  This is a elementary program calling Hamlib to do some useful things.
 *
 *  Edit to specify your rig model and serial port, and baud rate
 *  before compiling.
 *  To compile:
 *      gcc -L/usr/local/lib -lhamlib -o example example.c
 *      if hamlib is installed in /usr/local/...
 *
 */

#include <stdio.h>
#include <string.h>
#include <hamlib/rig.h>
#include <hamlib/riglist.h>


int main()
{
    RIG *my_rig;
    char *rig_file, *info_buf, *mm;
    freq_t freq;
    value_t rawstrength, power, strength;
    float s_meter, rig_raw2val();
    int status, retcode, isz, mwpower;
    rmode_t mode;
    pbwidth_t width;

    /* Set verbosity level */
    rig_set_debug(RIG_DEBUG_ERR);       // errors only

    /* Instantiate a rig */
    my_rig = rig_init(RIG_MODEL_TT565); // your rig model.

    /* Set up serial port, baud rate */
    rig_file = "/dev/ttyUSB0";        // your serial device

    strncpy(my_rig->state.rigport.pathname, rig_file, FILPATHLEN - 1);

    my_rig->state.rigport.parm.serial.rate = 57600; // your baud rate

    /* Open my rig */
    retcode = rig_open(my_rig);

    /* Give me ID info, e.g., firmware version. */
    info_buf = (char *)rig_get_info(my_rig);

    printf("Rig_info: '%s'\n", info_buf);

    /* Note: As a general practice, we should check to see if a given
     * function is within the rig's capabilities before calling it, but
     * we are simplifying here. Also, we should check each call's returned
     * status in case of error.  (That's an inelegant way to catch an unsupported
     * operation.)
     */

    /* Main VFO frequency */
    status = rig_get_freq(my_rig, RIG_VFO_CURR, &freq);

    printf("VFO freq. = %.1f Hz\n", freq);

    /* Current mode */
    status = rig_get_mode(my_rig, RIG_VFO_CURR, &mode, &width);

    switch (mode)
    {
    case RIG_MODE_USB:
        mm = "USB";
        break;

    case RIG_MODE_LSB:
        mm = "LSB";
        break;

    case RIG_MODE_CW:
        mm = "CW";
        break;

    case RIG_MODE_CWR:
        mm = "CWR";
        break;

    case RIG_MODE_AM:
        mm = "AM";
        break;

    case RIG_MODE_FM:
        mm = "FM";
        break;

    case RIG_MODE_WFM:
        mm = "WFM";
        break;

    case RIG_MODE_RTTY:
        mm = "RTTY";
        break;

    default:
        mm = "unrecognized";
        break; /* there are more possibilities! */
    }

    printf("Current mode = 0x%X = %s, width = %d\n", mode, mm, width);

    /* rig power output */
    status = rig_get_level(my_rig, RIG_VFO_CURR, RIG_LEVEL_RFPOWER, &power);

    printf("RF Power relative setting = %.3f (0.0 - 1.0)\n", power.f);

    /* Convert power reading to watts */
    status = rig_power2mW(my_rig, &mwpower, power.f, freq, mode);

    printf("RF Power calibrated = %.1f Watts\n", mwpower / 1000.);

    /* Raw and calibrated S-meter values */
    status = rig_get_level(my_rig, RIG_VFO_CURR, RIG_LEVEL_RAWSTR, &rawstrength);

    printf("Raw receive strength = %d\n", rawstrength.i);

    isz = my_rig->caps->str_cal.size;

    s_meter = rig_raw2val(rawstrength.i, &my_rig->caps->str_cal);

    printf("S-meter value = %.2f dB relative to S9\n", s_meter);

    /* now try using RIG_LEVEL_STRENGTH itself */
    status = rig_get_strength(my_rig, RIG_VFO_CURR, &strength);

    printf("LEVEL_STRENGTH returns %d\n", strength.i);
};
