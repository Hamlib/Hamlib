#if defined(_WIN32) || defined(WIN32)

int main(void)
{
    return 77;
}

#else

#define _XOPEN_SOURCE 600

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "hamlib/rig.h"

/*
 * TM-D750 emulator on a pty. Front-panel changes reach it through a control
 * pipe, one line per change.
 */

#define RECORD_MAX 160
#define MEMORIES 100

static const char *fo_initial[2] =
{
    "FO 0,0146850000,0000600000,2,2,0,0,0,0,0,0,0,08,08,000,0,CQCQCQ,0,00",
    "FO 1,0444150000,0005000000,9,9,0,0,0,0,1,0,1,18,18,000,3,CQCQCQ,0,00",
};

static const char me_054[] =
    "ME 054,0446475000,0005000000,9,9,0,1,0,0,0,0,0,0,18,18,000,3,CQCQCQ,0,00,1";

/*
 * A cross-band split channel as read from the radio: RX 446.475 on a 25 kHz
 * step, TX 145.970 in the offset field on a 5 kHz TX step, split flag set.
 */
static const char me_055[] =
    "ME 055,0446475000,0145970000,9,2,0,0,0,0,1,0,1,0,18,18,000,3,CQCQCQ,0,00,0";

static int send_reply(int fd, const char *reply)
{
    size_t length = strlen(reply);

    return write(fd, reply, length) != (ssize_t)length
           || write(fd, "\r", 1) != 1 ? -1 : 0;
}

static int read_line(int fd, char *line, size_t capacity, char end)
{
    size_t used = 0;

    while (used + 1 < capacity)
    {
        char byte;

        if (read(fd, &byte, 1) != 1)
        {
            return -1;
        }

        if (byte == end)
        {
            line[used] = '\0';
            return 0;
        }

        line[used++] = byte;
    }

    return -1;
}

static int band_of(const char *command)
{
    return command[3] == '0' || command[3] == '1' ? command[3] - '0' : -1;
}

/* "XX b" reads, "XX b,v" sets and echoes, within 0..maximum. */
static int band_value(int fd, const char *command, const char *name,
                      int values[2], int maximum, const char *format)
{
    char reply[RECORD_MAX];
    int band = band_of(command);

    if (strncmp(command, name, 2) != 0 || command[2] != ' ' || band < 0)
    {
        return 0;
    }

    if (command[4] == '\0')
    {
        int length = snprintf(reply, sizeof(reply), "%s %d,", name, band);
        snprintf(reply + length, sizeof(reply) - length, format, values[band]);
        send_reply(fd, reply);
    }
    else if (command[4] == ',' && atoi(command + 5) <= maximum)
    {
        values[band] = atoi(command + 5);
        send_reply(fd, command);
    }
    else
    {
        send_reply(fd, "N");
    }

    return 1;
}

static int emulate(int master, int control)
{
    char command[RECORD_MAX], reply[RECORD_MAX];
    char fo[2][RECORD_MAX];
    static char memory[MEMORIES][RECORD_MAX];
    int mode[2] = { 0, 0 }, power[2] = { 0, 0 }, squelch[2] = { 5, 5 };
    int gain[2] = { 100, 100 }, busy[2] = { 0, 1 }, signal[2] = { 0, 7 };
    int vm[2] = { 0, 1 }, mr[2] = { 0, 54 };
    char att[2] = { 'F', 'F' };  /* F as observed on the radio while off */
    int ctrl = 1, ptt = 0, keyed = 0, vox = 0, vox_gain = 4, vox_delay = 1;
    int fo_in_memory_mode = 0;

    strcpy(memory[54], me_054);
    strcpy(memory[55], me_055);
    strcpy(fo[0], fo_initial[0]);
    strcpy(fo[1], fo_initial[1]);

    for (;;)
    {
        fd_set ready;

        FD_ZERO(&ready);
        FD_SET(master, &ready);
        FD_SET(control, &ready);

        if (select((master > control ? master : control) + 1, &ready, NULL,
                   NULL, NULL) < 0)
        {
            return 1;
        }

        if (FD_ISSET(control, &ready))
        {
            char change[32];

            if (read_line(control, change, sizeof(change), '\n') != 0)
            {
                break;
            }

            if (strncmp(change, "BC ", 3) == 0)
            {
                ctrl = change[3] - '0';
                ptt = change[5] - '0';
            }
            else if (strncmp(change, "DR ", 3) == 0)
            {
                mode[change[3] - '0'] = 4;
            }

            continue;
        }

        if (read_line(master, command, sizeof(command), '\r') != 0)
        {
            break;
        }

        if (strcmp(command, "ID") == 0)
        {
            send_reply(master, "ID TM-D750");
        }
        else if (strcmp(command, "AI") == 0 || strcmp(command, "AI 0") == 0)
        {
            send_reply(master, "AI 0");
        }
        else if (strcmp(command, "BC") == 0)
        {
            snprintf(reply, sizeof(reply), "BC %d,%d", ctrl, ptt);
            send_reply(master, reply);
        }
        else if (strncmp(command, "FO ", 3) == 0 && band_of(command) >= 0)
        {
            int band = band_of(command);

            if (command[4] == ',')
            {
                /* The radio rounds the frequency down to the record's step. */
                static const long steps[13] = { 1000, 2500, 5000, 6250, 8330, 10000,
                                                12500, 15000, 20000, 25000, 30000,
                                                50000, 100000 };
                char code = command[27];
                long step = steps[code >= 'A' ? 10 + code - 'A' : code - '0'];
                long long hz = atoll(command + 5);

                /* The backend must leave memory mode before writing FO. */
                fo_in_memory_mode += vm[band];
                snprintf(fo[band], sizeof(fo[band]), "FO %d,%010lld%s", band,
                         hz - hz % step, command + 15);
            }

            send_reply(master, fo[band]);
        }
        else if (strncmp(command, "MD ", 3) == 0 && band_of(command) >= 0)
        {
            int band = band_of(command);

            if (command[4] == '\0')
            {
                snprintf(reply, sizeof(reply), "MD %d,%d", band, mode[band]);
                send_reply(master, reply);
            }
            else if (command[4] == ',' && command[5] >= '0' && command[5] <= '3'
                     && command[6] == '\0' && mode[band] != 4)
            {
                mode[band] = command[5] - '0';
                send_reply(master, command);
            }
            else
            {
                send_reply(master, "N");
            }
        }
        else if (strncmp(command, "AG ", 3) == 0 && command[4] == ','
                 && strlen(command) != 8)
        {
            /* AG wants three digits; "AG 1,75" is not understood. */
            send_reply(master, "?");
        }
        else if (band_value(master, command, "PC", power, 2, "%d")
                 || band_value(master, command, "SQ", squelch, 31, "%d")
                 || band_value(master, command, "AG", gain, 200, "%03d")
                 || band_value(master, command, "VM", vm, 1, "%d"))
        {
        }
        else if (strncmp(command, "RA ", 3) == 0 && band_of(command) >= 0)
        {
            int band = band_of(command);

            if (command[4] == '\0')
            {
                snprintf(reply, sizeof(reply), "RA %d,%c", band, att[band]);
                send_reply(master, reply);
            }
            else if (command[4] == ',' && (command[5] == '0' || command[5] == '1')
                     && command[6] == '\0')
            {
                att[band] = command[5];
                send_reply(master, command);
            }
            else
            {
                send_reply(master, "N");
            }
        }
        else if ((strncmp(command, "BY ", 3) == 0 || strncmp(command, "SM ", 3) == 0)
                 && band_of(command) >= 0 && command[4] == '\0')
        {
            int band = band_of(command);

            snprintf(reply, sizeof(reply), "%.2s %d,%d", command, band,
                     command[0] == 'B' ? busy[band] : signal[band]);
            send_reply(master, reply);
        }
        else if (strncmp(command, "MR ", 3) == 0 && band_of(command) >= 0)
        {
            int band = band_of(command);

            if (command[4] == ',' && !vm[band])
            {
                send_reply(master, "N");
            }
            else if (command[4] == ',')
            {
                mr[band] = atoi(command + 5);
                send_reply(master, command);
            }
            else
            {
                snprintf(reply, sizeof(reply), "MR %03d", mr[band]);
                send_reply(master, reply);
            }
        }
        else if (strncmp(command, "ME ", 3) == 0 && strlen(command) >= 6)
        {
            int channel = atoi(command + 3);

            if (channel < 0 || channel >= MEMORIES)
            {
                send_reply(master, "N");
            }
            else if (command[6] == ',')
            {
                /* A write echoes the record; an erase answers "ME ccc", as the radio does. */
                snprintf(memory[channel], RECORD_MAX, "%s",
                         command[7] == '\0' ? "" : command);
                snprintf(reply, sizeof(reply), "ME %03d", channel);
                send_reply(master, command[7] == '\0' ? reply : command);
            }
            else
            {
                send_reply(master, memory[channel][0] ? memory[channel] : "N");
            }
        }
        else if (strcmp(command, "TX") == 0)
        {
            keyed = 1;
            snprintf(reply, sizeof(reply), "TX %d", ptt);
            send_reply(master, reply);
        }
        else if (strcmp(command, "RX") == 0)
        {
            keyed = 0;
            send_reply(master, "RX");
        }
        else if (strcmp(command, "VX") == 0 || strcmp(command, "VG") == 0
                 || strcmp(command, "VD") == 0)
        {
            snprintf(reply, sizeof(reply), "%s %d", command,
                     command[1] == 'X' ? vox : (command[1] == 'G' ? vox_gain : vox_delay));
            send_reply(master, reply);
        }
        else if (strlen(command) == 4 && command[0] == 'V' && command[2] == ' '
                 && (command[1] == 'X' || command[1] == 'G' || command[1] == 'D'))
        {
            int value = command[3] - '0';

            if (command[1] == 'X') { vox = value; }
            else if (command[1] == 'G') { vox_gain = value; }
            else { vox_delay = value; }

            send_reply(master, command);
        }
        else
        {
            send_reply(master, "?");
        }
    }

    close(master);
    return keyed ? 2 : (fo_in_memory_mode ? 3 : 0);
}

/* A command the backend does not cover yet: reported, never a failure. */
static void placeholder(const char *what)
{
    printf("SKIP: %s\n", what);
}

static int expect(int condition, const char *message)
{
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }

    return 0;
}

static void radio_side(int control, const char *change)
{
    if (write(control, change, strlen(change)) < 0)
    {
        perror("control");
    }

    usleep(100 * 1000);
}

int main(void)
{
    const char *slave_name;
    channel_t channel;
    dcd_t dcd;
    freq_t freq;
    int master, guard, status, pipefd[2], ch;
    int failures = 0;
    pbwidth_t width;
    pid_t child;
    rmode_t mode;
    rptr_shift_t shift;
    shortfreq_t step, offset;
    tone_t tone;
    value_t value;
    vfo_t vfo;
    RIG *rig;

    master = posix_openpt(O_RDWR | O_NOCTTY);

    if (master < 0 || grantpt(master) < 0 || unlockpt(master) < 0
            || pipe(pipefd) < 0)
    {
        perror("setup");
        return 1;
    }

    slave_name = ptsname(master);

    if (slave_name == NULL || (guard = open(slave_name, O_RDWR | O_NOCTTY)) < 0)
    {
        perror("open pty slave");
        return 1;
    }

    child = fork();

    if (child < 0)
    {
        perror("fork");
        return 1;
    }

    if (child == 0)
    {
        close(guard);
        close(pipefd[1]);
        _exit(emulate(master, pipefd[0]));
    }

    close(master);
    close(pipefd[0]);
    rig_set_debug_level(RIG_DEBUG_NONE);
    rig_load_backend("kenwood");
    rig = rig_init(RIG_MODEL_TMD750);
    failures += expect(rig != NULL, "initialize TM-D750 backend");

    if (rig == NULL)
    {
        kill(child, SIGTERM);
        waitpid(child, NULL, 0);
        return 1;
    }

    rig_set_conf(rig, rig_token_lookup(rig, "rig_pathname"), slave_name);
    /* Front-panel changes must be seen at once. */
    rig_set_cache_timeout_ms(rig, HAMLIB_CACHE_ALL, 0);
    status = rig_open(rig);
    failures += expect(status == RIG_OK, "open TM-D750 emulator");
    close(guard);

    if (status != RIG_OK)
    {
        rig_cleanup(rig);
        kill(child, SIGTERM);
        waitpid(child, NULL, 0);
        return 1;
    }

    /* BC 1,0: CTRL on B, PTT on A; the backend starts on the PTT band. */
    failures += expect(rig_get_vfo(rig, &vfo) == RIG_OK && vfo == RIG_VFO_A,
                       "start on the PTT band");

    failures += expect(rig_get_freq(rig, RIG_VFO_A, &freq) == RIG_OK
                       && freq == 146850000,
                       "read band A frequency");
    failures += expect(rig_set_freq(rig, RIG_VFO_B, 445000000) == RIG_OK
                       && rig_get_freq(rig, RIG_VFO_B, &freq) == RIG_OK
                       && freq == 445000000,
                       "set band B frequency");

    failures += expect(rig_get_ts(rig, RIG_VFO_A, &step) == RIG_OK
                       && step == 5000,
                       "read step code 2 as 5 kHz");
    failures += expect(rig_set_freq(rig, RIG_VFO_A, 146524995) == RIG_OK
                       && rig_get_freq(rig, RIG_VFO_A, &freq) == RIG_OK
                       && freq == 146520000,
                       "report the frequency the radio tuned off the step");
    failures += expect(rig_get_ts(rig, RIG_VFO_B, &step) == RIG_OK
                       && step == 25000,
                       "read step code 9 as 25 kHz");
    failures += expect(rig_set_ts(rig, RIG_VFO_A, 12500) == RIG_OK
                       && rig_get_ts(rig, RIG_VFO_A, &step) == RIG_OK
                       && step == 12500,
                       "set 12.5 kHz step");
    failures += expect(rig_set_ts(rig, RIG_VFO_A, 7000) == -RIG_EINVAL,
                       "reject a step the radio does not have");

    failures += expect(rig_get_rptr_shift(rig, RIG_VFO_B, &shift) == RIG_OK
                       && shift == RIG_RPT_SHIFT_PLUS
                       && rig_get_rptr_offs(rig, RIG_VFO_B, &offset) == RIG_OK
                       && offset == 5000000,
                       "read band B shift and offset");
    failures += expect(rig_set_rptr_shift(rig, RIG_VFO_A, RIG_RPT_SHIFT_MINUS) == RIG_OK
                       && rig_get_rptr_shift(rig, RIG_VFO_A, &shift) == RIG_OK
                       && shift == RIG_RPT_SHIFT_MINUS,
                       "set minus shift");

    failures += expect(rig_set_ctcss_tone(rig, RIG_VFO_A, 1000) == RIG_OK
                       && rig_get_ctcss_tone(rig, RIG_VFO_A, &tone) == RIG_OK
                       && tone == 1000,
                       "set 100.0 Hz tone");
    failures += expect(rig_set_ctcss_sql(rig, RIG_VFO_A, 1230) == RIG_OK
                       && rig_get_ctcss_sql(rig, RIG_VFO_A, &tone) == RIG_OK
                       && tone == 1230,
                       "set 123.0 Hz tone squelch");
    failures += expect(rig_set_dcs_code(rig, RIG_VFO_A, 754) == RIG_OK
                       && rig_get_dcs_code(rig, RIG_VFO_A, &tone) == RIG_OK
                       && tone == 754,
                       "set DCS 754");
    failures += expect(rig_set_ctcss_tone(rig, RIG_VFO_A, 1001) == -RIG_EINVAL,
                       "reject a tone the radio does not have");

    failures += expect(rig_set_mode(rig, RIG_VFO_B, RIG_MODE_FMN,
                                    RIG_PASSBAND_NOCHANGE) == RIG_OK
                       && rig_get_mode(rig, RIG_VFO_B, &mode, &width) == RIG_OK
                       && mode == RIG_MODE_FMN,
                       "set narrow FM");
    failures += expect(rig_set_mode(rig, RIG_VFO_B, RIG_MODE_USB,
                                    RIG_PASSBAND_NOCHANGE) == -RIG_EINVAL,
                       "reject a mode the radio does not have");
    radio_side(pipefd[1], "DR 1\n");
    failures += expect(rig_get_mode(rig, RIG_VFO_B, &mode, &width) == RIG_OK
                       && mode == RIG_MODE_DSTAR,
                       "read DR as D-STAR");
    failures += expect(rig_set_mode(rig, RIG_VFO_B, RIG_MODE_FM,
                                    RIG_PASSBAND_NOCHANGE) == -RIG_ERJCTED,
                       "refuse to leave DR");

    failures += expect(rig_set_level(rig, RIG_VFO_A, RIG_LEVEL_RFPOWER,
                                     (value_t){ .f = 0.2f }) == RIG_OK
                       && rig_get_level(rig, RIG_VFO_A, RIG_LEVEL_RFPOWER,
                                        &value) == RIG_OK
                       && value.f == 0.2f,
                       "set medium power");
    failures += expect(rig_set_level(rig, RIG_VFO_A, RIG_LEVEL_SQL,
                                     (value_t){ .f = 1.0f }) == RIG_OK
                       && rig_get_level(rig, RIG_VFO_A, RIG_LEVEL_SQL,
                                        &value) == RIG_OK
                       && value.f == 1.0f,
                       "set squelch to 31");
    failures += expect(rig_set_level(rig, RIG_VFO_B, RIG_LEVEL_AF,
                                     (value_t){ .f = 0.5f }) == RIG_OK
                       && rig_get_level(rig, RIG_VFO_B, RIG_LEVEL_AF,
                                        &value) == RIG_OK
                       && value.f == 0.5f,
                       "set band B volume");
    failures += expect(rig_get_level(rig, RIG_VFO_B, RIG_LEVEL_ATT, &value) == RIG_OK
                       && value.i == 0,
                       "read F as attenuator off");
    failures += expect(rig_set_level(rig, RIG_VFO_B, RIG_LEVEL_ATT,
                                     (value_t){ .i = 3 }) == RIG_OK
                       && rig_get_level(rig, RIG_VFO_B, RIG_LEVEL_ATT, &value) == RIG_OK
                       && value.i == 10,
                       "any nonzero attenuator level switches it on");
    failures += expect(rig_set_level(rig, RIG_VFO_B, RIG_LEVEL_ATT,
                                     (value_t){ .i = 0 }) == RIG_OK
                       && rig_get_level(rig, RIG_VFO_B, RIG_LEVEL_ATT, &value) == RIG_OK
                       && value.i == 0,
                       "switch the attenuator off");
    failures += expect(rig_get_level(rig, RIG_VFO_B, RIG_LEVEL_RAWSTR,
                                     &value) == RIG_OK && value.i == 7,
                       "read band B S-meter");
    failures += expect(rig_get_dcd(rig, RIG_VFO_B, &dcd) == RIG_OK
                       && dcd == RIG_DCD_ON
                       && rig_get_dcd(rig, RIG_VFO_A, &dcd) == RIG_OK
                       && dcd == RIG_DCD_OFF,
                       "read busy per band");
    failures += expect(rig_get_func(rig, RIG_VFO_A, RIG_FUNC_VOX,
                                    &status) == RIG_OK && status == 0,
                       "read VOX off");

    failures += expect(rig_get_mem(rig, RIG_VFO_B, &ch) == RIG_OK && ch == 54,
                       "read memory channel without a band in the reply");
    failures += expect(rig_set_mem(rig, RIG_VFO_B, 47) == RIG_OK
                       && rig_get_mem(rig, RIG_VFO_B, &ch) == RIG_OK && ch == 47,
                       "select memory channel 047");
    failures += expect(rig_vfo_op(rig, RIG_VFO_B, RIG_OP_TO_VFO) == RIG_OK,
                       "switch band B to VFO mode");
    failures += expect(rig_set_mem(rig, RIG_VFO_B, 54) == RIG_OK
                       && rig_get_mem(rig, RIG_VFO_B, &ch) == RIG_OK && ch == 54,
                       "select a memory from VFO mode");

    memset(&channel, 0, sizeof(channel));
    channel.vfo = RIG_VFO_MEM;
    channel.channel_num = 54;
    failures += expect(rig_get_channel(rig, RIG_VFO_NONE, &channel, 1) == RIG_OK
                       && channel.freq == 446475000
                       && channel.tuning_step == 25000
                       && channel.ctcss_tone == 1230
                       && channel.rptr_shift == RIG_RPT_SHIFT_NONE
                       && channel.flags == RIG_CHFLAG_SKIP,
                       "read memory 054");
    channel.channel_num = 55;
    failures += expect(rig_get_channel(rig, RIG_VFO_NONE, &channel, 1) == RIG_OK
                       && channel.split == RIG_SPLIT_ON
                       && channel.freq == 446475000
                       && channel.tx_freq == 145970000
                       && channel.tuning_step == 25000
                       && channel.rptr_shift == RIG_RPT_SHIFT_NONE,
                       "read a cross-band split memory");
    channel.channel_num = 0;
    failures += expect(rig_get_channel(rig, RIG_VFO_NONE, &channel, 1)
                       == -RIG_ENAVAIL,
                       "report an empty memory unavailable");

    memset(&channel, 0, sizeof(channel));
    channel.vfo = RIG_VFO_MEM;
    channel.channel_num = 60;
    channel.freq = 446500000;
    channel.mode = RIG_MODE_FM;
    channel.split = RIG_SPLIT_ON;
    channel.tx_freq = 146520000;
    channel.ctcss_tone = 1000;
    failures += expect(rig_set_channel(rig, RIG_VFO_NONE, &channel) == RIG_OK,
                       "store a new cross-band split memory");
    memset(&channel, 0, sizeof(channel));
    channel.vfo = RIG_VFO_MEM;
    channel.channel_num = 60;
    failures += expect(rig_get_channel(rig, RIG_VFO_NONE, &channel, 1) == RIG_OK
                       && channel.freq == 446500000 && channel.split == RIG_SPLIT_ON
                       && channel.tx_freq == 146520000 && channel.ctcss_tone == 1000
                       && channel.tuning_step == 25000,
                       "read the stored split memory back");

    channel.channel_num = 54;
    failures += expect(rig_get_channel(rig, RIG_VFO_NONE, &channel, 1) == RIG_OK,
                       "read memory 054 before changing it");
    channel.funcs = 0;
    channel.flags = 0;
    failures += expect(rig_set_channel(rig, RIG_VFO_NONE, &channel) == RIG_OK
                       && rig_get_channel(rig, RIG_VFO_NONE, &channel, 1) == RIG_OK
                       && channel.flags == 0 && channel.ctcss_tone == 1230
                       && channel.freq == 446475000,
                       "clear lockout on 054 and keep the rest");

    memset(&channel, 0, sizeof(channel));
    channel.vfo = RIG_VFO_MEM;
    channel.channel_num = 60;
    channel.freq = RIG_FREQ_NONE;
    failures += expect(rig_set_channel(rig, RIG_VFO_NONE, &channel) == RIG_OK,
                       "erase memory 060");
    channel.freq = 0;
    failures += expect(rig_get_channel(rig, RIG_VFO_NONE, &channel, 1)
                       == -RIG_ENAVAIL,
                       "memory 060 is empty after the erase");

    failures += expect(rig_set_ptt(rig, RIG_VFO_B, RIG_PTT_ON) == -RIG_ENTARGET,
                       "refuse PTT on a band that is not the PTT band");
    failures += expect(rig_set_ptt(rig, RIG_VFO_A, RIG_PTT_ON) == RIG_OK
                       && rig_set_ptt(rig, RIG_VFO_A, RIG_PTT_OFF) == RIG_OK,
                       "key and unkey the PTT band");
    radio_side(pipefd[1], "BC 1,1\n");
    failures += expect(rig_set_ptt(rig, RIG_VFO_A, RIG_PTT_ON) == -RIG_ENTARGET,
                       "follow a PTT band change made on the radio");

    /* Band B is in memory mode here; set_freq must switch it to VFO first. */
    failures += expect(rig_set_mem(rig, RIG_VFO_B, 54) == RIG_OK
                       && rig_set_freq(rig, RIG_VFO_B, 445500000) == RIG_OK,
                       "tune a band that was in memory mode");

    placeholder("memory channel names");
    placeholder("split VFO operation (set_split_vfo)");

    rig_close(rig);
    rig_cleanup(rig);
    close(pipefd[1]);
    waitpid(child, &status, 0);
    failures += expect(WIFEXITED(status) && WEXITSTATUS(status) == 0,
                       "emulator exited cleanly, unkeyed, and no FO was written in memory mode");

    if (failures == 0)
    {
        printf("TM-D750 emulator integration tests passed\n");
    }

    return failures == 0 ? 0 : 1;
}

#endif
