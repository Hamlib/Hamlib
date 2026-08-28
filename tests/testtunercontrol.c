/*
 * Hamlib external tuner control tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

/*
 * The tuner_control_pathname option names a program that stands in for
 * the rig's antenna tuner: "a program to control a tuner with 1 argument
 * of 0/1 for Tuner Off/On".  It has to take over RIG_FUNC_TUNER and
 * leave every other function to the backend.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hamlib/rig.h"
#include "hamlib/riglist.h"

#ifdef _WIN32

int main(void)
{
    /* The stand-in program is a shell script; 77 is automake's SKIP. */
    return 77;
}

#else

#include <sys/stat.h>
#include <unistd.h>

#define STUB_PATH "./testtunercontrol_stub.sh"
#define MARKER_PATH "./testtunercontrol_marker"

static int write_stub(void)
{
    FILE *f = fopen(STUB_PATH, "w");

    if (f == NULL) { return -1; }

    fprintf(f, "#!/bin/sh\nprintf '%%s' \"$1\" > %s\n", MARKER_PATH);
    fclose(f);
    return chmod(STUB_PATH, 0755);
}

static void clear_marker(void)
{
    remove(MARKER_PATH);
}

/* Returns the argument the stub was called with, or NULL if it never ran */
static const char *marker_arg(char *buf, size_t len)
{
    FILE *f = fopen(MARKER_PATH, "r");

    if (f == NULL) { return NULL; }

    if (fgets(buf, (int) len, f) == NULL) { buf[0] = '\0'; }

    fclose(f);
    return buf;
}

int main(void)
{
    const struct confparams *cfp;
    char buf[32];
    const char *arg;
    RIG *rig;
    int status = 0;
    int failed = 0;
    int retcode;

    if (write_stub() != 0)
    {
        fprintf(stderr, "could not create the stand-in tuner program\n");
        return 1;
    }

    rig = rig_init(RIG_MODEL_DUMMY);

    if (rig == NULL)
    {
        fprintf(stderr, "failed to initialize Dummy rig\n");
        return 1;
    }

    cfp = rig_confparam_lookup(rig, "tuner_control_pathname");

    if (cfp == NULL)
    {
        fprintf(stderr, "tuner_control_pathname is not a known setting\n");
        rig_cleanup(rig);
        return 1;
    }

    if (rig_set_conf(rig, cfp->token, STUB_PATH) != RIG_OK
            || rig_open(rig) != RIG_OK)
    {
        fprintf(stderr, "failed to set up the Dummy rig\n");
        rig_cleanup(rig);
        return 1;
    }

    /* A function other than the tuner belongs to the backend */
    clear_marker();
    retcode = rig_set_func(rig, RIG_VFO_CURR, RIG_FUNC_NB, 1);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "setting NB failed: %s\n", rigerror(retcode));
        failed = 1;
    }

    arg = marker_arg(buf, sizeof(buf));

    if (arg != NULL)
    {
        fprintf(stderr, "the tuner program ran for NB, with argument '%s'\n",
                arg);
        failed = 1;
    }

    retcode = rig_get_func(rig, RIG_VFO_CURR, RIG_FUNC_NB, &status);

    if (retcode != RIG_OK || status != 1)
    {
        fprintf(stderr, "NB did not reach the rig: %s, status %d\n",
                rigerror(retcode), status);
        failed = 1;
    }

    /* The tuner itself does go to the program */
    clear_marker();
    retcode = rig_set_func(rig, RIG_VFO_CURR, RIG_FUNC_TUNER, 1);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "setting TUNER failed: %s\n", rigerror(retcode));
        failed = 1;
    }

    arg = marker_arg(buf, sizeof(buf));

    if (arg == NULL || strcmp(arg, "1") != 0)
    {
        fprintf(stderr, "the tuner program was not called with 1, got '%s'\n",
                arg == NULL ? "(not called)" : arg);
        failed = 1;
    }

    clear_marker();
    remove(STUB_PATH);
    rig_close(rig);
    rig_cleanup(rig);
    return failed;
}

#endif
