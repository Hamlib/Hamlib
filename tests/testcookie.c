#include <hamlib/rig.h>

// GET tests
int test1()
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

    // after 1 second we should be able to get a coookie
    // this means the cookie holder did not renew within 1 second
    hl_usleep(1500 * 1000); // after 1 second we should be able to get a coookie

    retcode = rig_cookie(NULL, RIG_COOKIE_GET, cookie2, sizeof(cookie2));

    if (retcode == RIG_OK) { printf("Test#1d OK\n"); }
    else {printf("Test#1d Failed\n"); return 1;}

    retcode = rig_cookie(NULL, RIG_COOKIE_RELEASE, cookie2, sizeof(cookie2));

    if (retcode == RIG_OK) { printf("Test#1e OK\n"); }
    else {printf("Test#1e Failed\n"); return 1;}

    return 0;
}

// RENEW tests
int test2()
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

    return 0;
}


int main()
{
    rig_set_debug(RIG_DEBUG_VERBOSE);

    if (test1()) { return 1; }

    if (test2()) { return 1; }

    return 0;
}
