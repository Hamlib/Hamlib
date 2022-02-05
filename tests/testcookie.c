#include <hamlib/rig.h>

// GET tests
static int test1()
{
    int retcode;
    // Normal get
    char cookie[HAMLIB_COOKIE_SIZE];
    retcode = rig_cookie(NULL, RIG_COOKIE_GET, cookie, sizeof(cookie));

    if (retcode == RIG_OK) { printf("Test#1a OK\n"); }
    else {printf("Test#1a Failed\n"); return 1;}

    // Should be able to release and get it right back
    rig_cookie(NULL, RIG_COOKIE_RELEASE, cookie, sizeof(cookie));
    retcode = rig_cookie(NULL, RIG_COOKIE_GET, cookie, sizeof(cookie));

    if (retcode == RIG_OK) { printf("Test#1b OK\n"); }
    else {printf("Test#1b Failed\n"); return 1;}


    // Doing a get when another cookie is active should fail
    char cookie2[HAMLIB_COOKIE_SIZE];
    cookie2[0] = 0;
    retcode = rig_cookie(NULL, RIG_COOKIE_GET, cookie2, sizeof(cookie2));

    if (retcode == RIG_OK) { printf("Test#1c OK\n"); }
    else {printf("Test#1c Failed\n"); return 1;}

#if 0
    // after 1 second we should be able to get a coookie
    // this means the cookie holder did not renew within 1 second
    hl_usleep(1500 * 1000); // after 1 second we should be able to get a coookie

    retcode = rig_cookie(NULL, RIG_COOKIE_GET, cookie2, sizeof(cookie2));

    if (retcode == RIG_OK) { printf("Test#1d OK\n"); }
    else {printf("Test#1d Failed\n"); return 1;}

#endif

    retcode = rig_cookie(NULL, RIG_COOKIE_RELEASE, cookie2, sizeof(cookie2));

    if (retcode == RIG_OK) { printf("Test#1e OK\n"); }
    else {printf("Test#1e Failed\n"); return 1;}

    return 0;
}

// RENEW tests
static int test2()
{
    int retcode;
    char cookie[HAMLIB_COOKIE_SIZE];
    retcode = rig_cookie(NULL, RIG_COOKIE_GET, cookie, sizeof(cookie));

    if (retcode == RIG_OK) { printf("Test#2a OK   %s\n", cookie); }
    else {printf("Test#2a Failed\n"); return 1;}

    retcode = rig_cookie(NULL, RIG_COOKIE_RELEASE, cookie, sizeof(cookie));

    if (retcode == RIG_OK) { printf("Test#2b OK\n"); }
    else {printf("Test#2b Failed\n"); return 1;}

// get another cookie should work
    char cookie2[HAMLIB_COOKIE_SIZE];
    retcode = rig_cookie(NULL, RIG_COOKIE_GET, cookie2, sizeof(cookie2));

    if (retcode == RIG_OK) { printf("Test#2c OK   %s\n", cookie2); }
    else {printf("Test#2c Failed\n"); return 1;}

// should not be able to renew 1st cookie
    retcode = rig_cookie(NULL, RIG_COOKIE_RENEW, cookie, sizeof(cookie));

    if (retcode != RIG_OK) { printf("Test#2d OK\n"); }
    else {printf("Test#2d Failed cookie=%s\n", cookie); return 1;}

// release cookie2 again to clean up test
    retcode = rig_cookie(NULL, RIG_COOKIE_RELEASE, cookie2, sizeof(cookie2));

    if (retcode == RIG_OK) { printf("Test#2e OK\n"); }
    else {printf("Test#2e Failed\n"); return 1;}

    return 0;
}

// Input sanity checks
static int test3_invalid_input()
{
    int retcode;
    char cookie[HAMLIB_COOKIE_SIZE];
    int n = 0;

    /* Make sure any value smaller then HAMLIB_COOKIE_SIZE is rejected */
    for (unsigned int i = 0; i < HAMLIB_COOKIE_SIZE; i++)
    {
        retcode = rig_cookie(NULL, RIG_COOKIE_GET, cookie, i);

        if (retcode != -RIG_EINVAL) { n++; printf("Test#3a failed at %d bytes\n", i); }
    }

    if (n == 0) { printf("Test#3a OK\n"); }

    /* Make sure a NULL cookie is ignored */
    retcode = rig_cookie(NULL, RIG_COOKIE_GET, NULL, sizeof(cookie));

    if (retcode == -RIG_EINVAL) { printf("Test#3b OK\n"); }
    else {printf("Test#3b Failed\n"); return 1;}

    /* Make sure an invalid command is dropped with proto error */
    retcode = rig_cookie(NULL, RIG_COOKIE_RENEW + 1, cookie, sizeof(cookie));

    if (retcode == -RIG_EPROTO) { printf("Test#3c OK\n"); }
    else {printf("Test#3c Failed\n"); return 1;}

    return 0;
}

static int test4_large_cookie_size()
{
    int retcode;
    char cookie[HAMLIB_COOKIE_SIZE * 2];

    /* Using a larger cookie should also work */
    retcode = rig_cookie(NULL, RIG_COOKIE_GET, cookie, sizeof(cookie));

    if (retcode == RIG_OK) { printf("Test#4a OK\n"); }
    else {printf("Test#4a Failed\n"); return 1;}

    /* Cookie should be smaller the maximum specified by lib */
    //if (strlen(cookie) < HAMLIB_COOKIE_SIZE) { printf("Test#4b OK\n"); }
    //else {printf("Test#4b Failed\n"); return 1;}

    /* Release the cookie again to clean up */
    retcode = rig_cookie(NULL, RIG_COOKIE_RELEASE, cookie, sizeof(cookie));

    if (retcode == RIG_OK) { printf("Test#4c OK\n"); }
    else {printf("Test#4c Failed\n"); return 1;}

    return 0;
}

int main()
{
    rig_set_debug(RIG_DEBUG_VERBOSE);

    if (test1()) { return 1; }

    if (test2()) { return 1; }

    if (test3_invalid_input()) { return 1; }

    if (test4_large_cookie_size()) { return 1; }

    return 0;
}
