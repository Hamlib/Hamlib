#include <stdio.h>
#include <string.h>

#include "hamlib/rig.h"
#include "hamlib/riglist.h"


static int check_string_parm(RIG *rig, setting_t parm, const char *first,
                             const char *second)
{
    char input[32];
    value_t value;
    int status;

    snprintf(input, sizeof(input), "%s", first);
    value.cs = input;
    status = rig_set_parm(rig, parm, value);

    if (status != RIG_OK)
    {
        fprintf(stderr, "setting %s failed: %s\n", rig_strparm(parm),
                rigerror(status));
        return 1;
    }

    memset(input, 0, sizeof(input));
    status = rig_get_parm(rig, parm, &value);

    if (status != RIG_OK || value.cs == NULL || strcmp(value.cs, first) != 0)
    {
        fprintf(stderr, "%s did not retain its first string value\n",
                rig_strparm(parm));
        return 1;
    }

    snprintf(input, sizeof(input), "%s", second);
    value.cs = input;
    status = rig_set_parm(rig, parm, value);

    if (status != RIG_OK)
    {
        fprintf(stderr, "replacing %s failed: %s\n", rig_strparm(parm),
                rigerror(status));
        return 1;
    }

    memset(input, 0, sizeof(input));
    status = rig_get_parm(rig, parm, &value);

    if (status != RIG_OK || value.cs == NULL || strcmp(value.cs, second) != 0)
    {
        fprintf(stderr, "%s did not retain its replacement string value\n",
                rig_strparm(parm));
        return 1;
    }

    return 0;
}


int main(void)
{
    RIG *rig = rig_init(RIG_MODEL_DUMMY);
    int failed = 0;

    if (rig == NULL)
    {
        fprintf(stderr, "failed to initialize Dummy rig\n");
        return 1;
    }

    if (rig_open(rig) != RIG_OK)
    {
        fprintf(stderr, "failed to open Dummy rig\n");
        rig_cleanup(rig);
        return 1;
    }

    failed |= check_string_parm(rig, RIG_PARM_BANDSELECT, "BAND70CM", "BAND33CM");
    failed |= check_string_parm(rig, RIG_PARM_KEYERTYPE, "BUG", "PADDLE");
    rig_close(rig);
    rig_cleanup(rig);
    return failed;
}
