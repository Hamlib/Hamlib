/* Exercise the documented truSDX CAT subset over a pseudo terminal. */
#if defined(_WIN32) || defined(WIN32)
int main(void) { return 77; }
#else
#define _XOPEN_SOURCE 600
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "hamlib/rig.h"

static int emulate(int fd)
{
    char cmd[64], reply[80];
    size_t used = 0;
    char byte;
    long freq = 14075000;
    int mode = 2, failures = 0;

    while (read(fd, &byte, 1) == 1)
    {
        if (byte != ';')
        {
            if (used + 1 >= sizeof(cmd)) { return 1; }
            cmd[used++] = byte;
            continue;
        }

        cmd[used] = 0;
        used = 0;
        reply[0] = 0;

        if (!strcmp(cmd, "ID")) { strcpy(reply, "ID020;"); }
        else if (!strcmp(cmd, "FA")) { snprintf(reply, sizeof(reply), "FA%011ld;", freq); }
        else if (strlen(cmd) == 13 && !strncmp(cmd, "FA", 2)
                 && strspn(cmd + 2, "0123456789") == 11) { freq = atol(cmd + 2); }
        else if (!strcmp(cmd, "MD")) { snprintf(reply, sizeof(reply), "MD%d;", mode); }
        else if (strlen(cmd) == 3 && !strncmp(cmd, "MD", 2)
                 && cmd[2] >= '1' && cmd[2] <= '5') { mode = cmd[2] - '0'; }
        else if (!strcmp(cmd, "PS")) { strcpy(reply, "PS1;"); }
        else if (!strcmp(cmd, "TX") || !strcmp(cmd, "TX0")
                 || !strcmp(cmd, "TX1") || !strcmp(cmd, "RX")
                 || !strcmp(cmd, "PS1")) { /* No reply to setters. */ }
        else
        {
            fprintf(stderr, "Undocumented/unexpected command: %s;\n", cmd);
            failures++;
            strcpy(reply, "?;");
        }

        if (reply[0] && write(fd, reply, strlen(reply)) < 0) { return 1; }
    }

    return failures ? 1 : 0;
}

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr); failures++; } } while (0)

int main(void)
{
    int master, guard, status, failures = 0;
    pid_t child;
    const char *path;
    RIG *rig;
    freq_t freq;
    rmode_t mode;
    pbwidth_t width;
    powerstat_t power;
    ptt_t ptt;
    value_t value;
    vfo_t vfo;
    split_t split;
    shortfreq_t rit;
    dcd_t dcd;
    const rmode_t modes[] = {RIG_MODE_LSB, RIG_MODE_USB, RIG_MODE_CW, RIG_MODE_FM, RIG_MODE_AM};

    alarm(30);
    master = posix_openpt(O_RDWR | O_NOCTTY);
    if (master < 0 || grantpt(master) < 0 || unlockpt(master) < 0) { return 1; }
    path = ptsname(master);
    if (!path || (guard = open(path, O_RDWR | O_NOCTTY)) < 0) { return 1; }
    child = fork();
    if (child < 0) { return 1; }
    if (child == 0) { close(guard); _exit(emulate(master)); }
    close(master);

    rig_set_debug_level(RIG_DEBUG_NONE);
    rig = rig_init(RIG_MODEL_TRUSDX);
    if (!rig) { return 1; }
    rig_set_conf(rig, rig_token_lookup(rig, "rig_pathname"), path);
    rig_set_conf(rig, rig_token_lookup(rig, "cache_timeout"), "0");
    CHECK(rig_open(rig) == RIG_OK);
    close(guard);
    CHECK(rig_get_freq(rig, RIG_VFO_CURR, &freq) == RIG_OK && freq == 14075000);
    CHECK(rig_get_mode(rig, RIG_VFO_CURR, &mode, &width) == RIG_OK && mode == RIG_MODE_USB);
    CHECK(rig_set_freq(rig, RIG_VFO_A, 7100000) == RIG_OK);
    CHECK(rig_get_freq(rig, RIG_VFO_A, &freq) == RIG_OK && freq == 7100000);

    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++)
    {
        CHECK(rig_set_mode(rig, RIG_VFO_A, modes[i], RIG_PASSBAND_NORMAL) == RIG_OK);
        CHECK(rig_get_mode(rig, RIG_VFO_A, &mode, &width) == RIG_OK && mode == modes[i]);
    }

    CHECK(rig_set_ptt(rig, RIG_VFO_A, RIG_PTT_ON) == RIG_OK);
    CHECK(rig_set_ptt(rig, RIG_VFO_A, RIG_PTT_ON_MIC) == RIG_OK);
    CHECK(rig_set_ptt(rig, RIG_VFO_A, RIG_PTT_ON_DATA) == RIG_OK);
    CHECK(rig_set_ptt(rig, RIG_VFO_A, RIG_PTT_OFF) == RIG_OK);
    CHECK(rig_get_powerstat(rig, &power) == RIG_OK && power == RIG_POWER_ON);
    CHECK(rig_set_powerstat(rig, RIG_POWER_ON) == RIG_OK);
    CHECK(rig_set_powerstat(rig, RIG_POWER_OFF) < 0);
    CHECK(rig_set_vfo(rig, RIG_VFO_B) < 0);
    CHECK(rig_set_freq(rig, RIG_VFO_B, 7200000) < 0);
    CHECK(rig_set_mode(rig, RIG_VFO_A, RIG_MODE_RTTY, RIG_PASSBAND_NORMAL) < 0);
    CHECK(rig_set_mode(rig, RIG_VFO_A, RIG_MODE_CWR, RIG_PASSBAND_NORMAL) < 0);
    CHECK(rig_set_mode(rig, RIG_VFO_A, RIG_MODE_USB, 500) < 0);
    CHECK(rig_get_level(rig, RIG_VFO_A, RIG_LEVEL_STRENGTH, &value) < 0);
    CHECK(rig_set_split_vfo(rig, RIG_VFO_A, RIG_SPLIT_ON, RIG_VFO_B) < 0);
    /* The frontend synthesizes split-off for single-VFO rigs. */
    CHECK(rig_get_split_vfo(rig, RIG_VFO_A, &split, &vfo) == RIG_OK
          && split == RIG_SPLIT_OFF);
    CHECK(rig_get_rit(rig, RIG_VFO_A, &rit) < 0);
    CHECK(rig_get_dcd(rig, RIG_VFO_A, &dcd) < 0);
    CHECK(rig->caps->get_ptt == NULL);
    /* The frontend may return cached PTT; the backend must not query IF for it. */
    rig_get_ptt(rig, RIG_VFO_A, &ptt);
    CHECK(rig_send_morse(rig, RIG_VFO_A, "TEST") < 0);
    CHECK(rig->caps->has_get_level == 0 && rig->caps->has_set_level == 0);
    CHECK(rig->caps->has_get_func == 0 && rig->caps->has_set_func == 0);
    CHECK(rig_close(rig) == RIG_OK);
    rig_cleanup(rig);
    waitpid(child, &status, 0);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    return failures ? 1 : 0;
}
#endif
