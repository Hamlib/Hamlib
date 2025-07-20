#include <string.h>
#include "hamlib/rig.h"
#include "misc.h"


// cppcheck-suppress unusedFunction
int rig_test_cw(RIG *rig)
{
    char *s = "SOS SOS SOS SOS SOS SOS SOS SOS SOS SOS SOS SOS SOS";
    //char *s = "TEST TEST TEST TEST TEST TEST TEST TEST TEST TEST TEST TEST TEST TEST";
    int i;
    ELAPSED1;
    ENTERFUNC;

    for (i = 0; i < strlen(s); ++i)
    {
        char cw[2];
        cw[0] = s[i];
        cw[1] = '\0';

        int retval = rig_send_morse(rig, RIG_VFO_CURR, cw);
        hl_usleep(100 * 1000);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: rig_send_morse error: %s\n", __func__,
                      rigerror(retval));
        }
    }

    ELAPSED2;
    RETURNFUNC(RIG_OK);
}
