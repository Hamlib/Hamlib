#include <hamlib/rotator.h>
#include <pthread.h>
#include "iofunc.h"
#include "register.h"
#include "apex.h"

float apex_azimuth;

char apex_info[65];

static pthread_t apex_read_thread;

// We only have two strings to get
// one is 18 chars and the other may be variable
// [T4BRFA99H00M010#] Unidirectional
// <VER> xxxxxxxxxx
// So we'll read 5 chars and use the to determine how to read the rest

static int apex_get_string(ROT *rot, char *s, int maxlen)
{
    int retval = 0;
    struct rot_state *rs = &rot->state;
    char buf[64];

    memset(s, 0, maxlen);

    retval = read_string(&rs->rotport, (unsigned char *)buf,
                         sizeof(buf),
                         "\n", strlen("\n"), sizeof(buf), 1);
    strncpy(s, buf, 64);
    strtok(s, "\r\n"); // truncate any CR/LF
    rig_debug(RIG_DEBUG_VERBOSE, "%s: %d bytes '%s'\n", __func__, retval, s);

    if (retval <= 0) { return -RIG_EPROTO; }

    return RIG_OK;
}


// Expecting # from 0-7
// [T4BRFA99H00M010#] Unidirectional
// Or
// [T4BRFA99H00M020#] Bidirectional
static void *apex_read(void *arg)
{
    ROT *rot = arg;
    int retval = 0;
    char data[64];
    int expected_return_length = 63;

    while (1)
    {
        apex_get_string(rot, data, expected_return_length);

        if (strstr(data, "<VER>"))
        {
            strncpy(apex_info, data, sizeof(apex_info));
            rig_debug(RIG_DEBUG_TRACE, "%s: apex_info=%s\n", __func__, apex_info);
            continue;
        }

        if (retval != 0 || strstr(data, "[T4BRFA99") == NULL)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unknown apex status message=%s\n", __func__,
                      data);
            continue;
        }

        rig_debug(RIG_DEBUG_VERBOSE, "%s: data='%s'\n", __func__, data);

        switch (data[16])
        {
        case '0': apex_azimuth =  45; break;

        case '1': apex_azimuth =  90; break;

        case '2': apex_azimuth = 135; break;

        case '3': apex_azimuth = 180; break;

        case '4': apex_azimuth = 225; break;

        case '5': apex_azimuth = 270; break;

        case '6': apex_azimuth = 315; break;

        case '7': apex_azimuth =   0; break;
        }

//            printf("az=%f\n", apex_azimuth);
    }

    return NULL;
}


int apex_open(ROT *rot)
{
    int retval;
    char *cmdstr = "[GETVER]\r"; // does this work on all Apex controllers?
    struct rot_state *rs = &rot->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: entered\n", __func__);

    apex_azimuth = -1;  // we check to see if we've seen azimuth at least one time
    rig_flush(&rs->rotport);
    retval = write_block(&rs->rotport, (unsigned char *) cmdstr, strlen(cmdstr));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: write_block failed - %s\n", __func__,
                  rigerror(retval));
        return retval;
    }

    pthread_create(&apex_read_thread, NULL, apex_read, rot);
    return RIG_OK;
}

const char *apex_get_info(ROT *rot)
{
    return apex_info;
}

DECLARE_INITROT_BACKEND(apex)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rot_register(&apex_shared_loop_rot_caps);

    return RIG_OK;
}

