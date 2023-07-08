/*
 * Hamlib sample program to test transceive mode (async event)
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <hamlib/rig.h>

#include <hamlib/config.h>

int nrigs = 0;

int callback(const struct rig_caps *caps, rig_ptr_t rigp)
{
    RIG *rig = (RIG *) rigp;

    switch (caps->rig_model)
    {
    case RIG_MODEL_NETRIGCTL:
        return 1;
        break;
    }

    rig = rig_init(caps->rig_model);

    if (!rig)
    {
        fprintf(stderr, "Unknown rig num: %u\n", caps->rig_model);
        fprintf(stderr, "Please check riglist.h\n");
        exit(1); /* whoops! something went wrong (mem alloc?) */
    }

    if (caps->mW2power)
    {
        nrigs++;
        printf("%20s:", caps->model_name);
        fflush(stdout);
        float fpow;
        unsigned int mwpower = 1 * 1000;
        freq_t freq = MHz(14);
        int retcode = rig_mW2power(rig, &fpow, mwpower, freq, RIG_MODE_CW);

        if (retcode != RIG_OK)
        {
            printf("rig_mW2power: error = %s \n", rigerror(retcode));
            return 1;
        }

        if (fpow < 0.009 || fpow > .11)
        {
//            printf("rig=%d, fpow=%g, min=%d, max=%d\n", caps->rig_model, fpow, caps->);
            printf("rig=%d, fpow=%g\n", caps->rig_model, fpow);
            // we call again to make debugging this section easier
            rig_mW2power(rig, &fpow, mwpower, freq, RIG_MODE_CW);
        }
        else { printf("\n"); }
    }

    rig_cleanup(rig); /* if you care about memory */
    return 1;
}

int main(int argc, char *argv[])
{
    RIG *rig;
    int i;

    rig_set_debug(RIG_DEBUG_NONE);

    rig_load_all_backends();
    rig_list_foreach(callback, &rig);
    printf("Done testing %d rigs\n", nrigs);

    return 0;
}
