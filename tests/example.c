/*  This is a elementary program calling Hamlib to do some useful things.
 *
 *  Edit to specify your rig model and serial port, and baud rate
 *  before compiling.
 *  To compile:
 *      gcc -I../src -I../include -g -o example example.c sprintflst.c -lhamlib
 *      if hamlib is installed in /usr/local/...
 *
 */

#include <stdio.h>
#include <string.h>
#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"
#include "hamlib/riglist.h"
#include "sprintflst.h"
#include "hamlib/rotator.h"

#if 1
#define MODEL RIG_MODEL_DUMMY
#define PATH "/dev/ttyUSB0"
#define BAUD 19200
#else
#define MODEL RIG_MODEL_NETRIGCTL
#define PATH "127.0.0.1:4532"
#define BAUD 0
#endif

int main()
{
    RIG *my_rig;
    char *info_buf;
    freq_t freq;
    value_t rawstrength, power, strength;
    float s_meter, rig_raw2val(int i, cal_table_t *t);
    int status, retcode;
    unsigned int mwpower;
    rmode_t mode;
    pbwidth_t width;

    /* Set verbosity level */
    rig_set_debug(RIG_DEBUG_TRACE);       // Lots of output

    /* Instantiate a rig */
    my_rig = rig_init(MODEL); // your rig model.

    rig_set_conf(my_rig, rig_token_lookup(my_rig, "rig_pathname"), PATH);

    HAMLIB_RIGPORT(my_rig)->parm.serial.rate = BAUD; // your baud rate

    /* Open my rig */
    retcode = rig_open(my_rig);

    if (retcode != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: rig_open failed %s\n", __func__,
                  rigerror(retcode));
        return 1;
    }

    /* Give me ID info, e.g., firmware version. */
    info_buf = (char *)rig_get_info(my_rig);

    printf("Rig_info: '%s'\n", info_buf);

    /* Note: As a general practice, we should check to see if a given
     * function is within the rig's capabilities before calling it, but
     * we are simplifying here. Also, we should check each call's returned
     * status in case of error.  (That's an inelegant way to catch an unsupported
     * operation.)
     */

    if (my_rig->caps->rig_model == RIG_MODEL_NETRIGCTL)
    {
        status = rig_set_vfo_opt(my_rig, 1);

        if (status != RIG_OK) { printf("set_vfo_opt failed?? Err=%s\n", rigerror(status)); }
    }

    /* Main VFO frequency */
    status = rig_get_freq(my_rig, RIG_VFO_CURR, &freq);

    if (status != RIG_OK) { printf("Get freq failed?? Err=%s\n", rigerror(status)); }

    printf("VFO freq. = %.1f Hz\n", freq);

    /* Current mode */
    status = rig_get_mode(my_rig, RIG_VFO_CURR, &mode, &width);

    if (status != RIG_OK) { printf("Get mode failed?? Err=%s\n", rigerror(status)); }

    printf("Current mode = 0x%llX = %s, width = %ld\n", (unsigned long long)mode, rig_strrmode(mode),
           width);

    /* rig power output */
    status = rig_get_level(my_rig, RIG_VFO_CURR, RIG_LEVEL_RFPOWER, &power);

    if (status != RIG_OK) { rig_debug(RIG_DEBUG_ERR, "%s: error rig_get_level: %s\n", __func__, rigerror(status)); }

    printf("RF Power relative setting = %.3f (0.0 - 1.0)\n", power.f);

    /* Convert power reading to watts */
    status = rig_power2mW(my_rig, &mwpower, power.f, freq, mode);

    if (status != RIG_OK) { rig_debug(RIG_DEBUG_ERR, "%s: error rig_get_level: %s\n", __func__, rigerror(status)); }

    printf("RF Power calibrated = %.1f Watts\n", mwpower / 1000.);

    /* Raw and calibrated S-meter values */
    status = rig_get_level(my_rig, RIG_VFO_CURR, RIG_LEVEL_RAWSTR, &rawstrength);

    if (status != RIG_OK) { rig_debug(RIG_DEBUG_ERR, "%s: error rig_get_level: %s\n", __func__, rigerror(status)); }

    printf("Raw receive strength = %d\n", rawstrength.i);

    s_meter = rig_raw2val(rawstrength.i, &my_rig->caps->str_cal);

    printf("S-meter value = %.2f dB relative to S9\n", s_meter);

    /* now try using RIG_LEVEL_STRENGTH itself */
    status = rig_get_strength(my_rig, RIG_VFO_CURR, &strength);

    if (status != RIG_OK) { rig_debug(RIG_DEBUG_ERR, "%s: error rig_get_level: %s\n", __func__, rigerror(status)); }

    printf("LEVEL_STRENGTH returns %d\n", strength.i);

    const struct rig_state *my_rs = HAMLIB_STATE(my_rig);
    const freq_range_t *range = rig_get_range(&my_rs->rx_range_list[0],
                                14074000, RIG_MODE_USB);

    if (status != RIG_OK) { rig_debug(RIG_DEBUG_ERR, "%s: error rig_get_range: %s\n", __func__, rigerror(status)); }

    if (range)
    {
        char vfolist[256];
        rig_sprintf_vfo(vfolist, sizeof(vfolist), my_rs->vfo_list);
        printf("Range start=%"PRIfreq", end=%"PRIfreq", low_power=%d, high_power=%d, vfos=%s\n",
               range->startf, range->endf, range->low_power, range->high_power, vfolist);
    }
    else
    {
        printf("Not rx range list found\n");
    }

    printf("Closing and reopening rig\n");
    rig_close(my_rig);

    int loops = 1;

    while (loops--)
    {
        retcode = rig_open(my_rig);

        if (retcode != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: rig_open failed %s\n", __func__,
                      rigerror(retcode));
            return 1;
        }

#if 0
        status = rig_get_freq(my_rig, RIG_VFO_CURR, &freq);

        if (status != RIG_OK) { printf("Get freq failed?? Err=%s\n", rigerror(status)); }

        printf("VFO freq. = %.1f Hz\n", freq);
        printf("rig reopen is OK\n");
#endif
        rig_close(my_rig);
    }
};
