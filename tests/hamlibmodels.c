/* Example showing host to list all models */
#include <stdio.h>
#include <stdlib.h>
#include <hamlib/rig.h>

char *list[1000]; // as of 2023-01-17 we have 275 rigs so this should cover us for long time

int nmodels = 0;

static int hash_model_list(const struct rig_caps *caps, void *data)
{
    char s[256];
    sprintf(s, "%s %s", caps->mfg_name, caps->model_name);
    list[nmodels] = strdup(s);
    ++nmodels;
    return 1;  /* !=0, we want them all ! */
}

int mycmp(const void *p1, const void *p2)
{
    const  char **s1 = (const char **)p1;
    const  char **s2 = (const char **)p2;
    return strcasecmp(*s1, *s2);
}

int main()
{
    rig_set_debug_level(RIG_DEBUG_NONE);
    rig_load_all_backends();
    rig_list_foreach(hash_model_list, NULL);
    qsort(list, nmodels, sizeof(list) / 1000, mycmp);

    for (int i = 0; i < nmodels; ++i) { printf("%s\n", list[i]); }

    printf("%d models\n", nmodels);

    return 0;
}
