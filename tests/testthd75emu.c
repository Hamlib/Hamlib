#if defined(_WIN32) || defined(WIN32)

int main(void)
{
    return 77;
}

#else

#define _XOPEN_SOURCE 600

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "hamlib/rig.h"

#define MEMORY_RECORD_MAX 160

enum reply_fault
{
    FAULT_NONE,
    FAULT_MALFORMED,
    FAULT_WRONG_CHANNEL,
    FAULT_REJECT_ONCE,
};

static const char fo_zero[] =
    "FO 0,0146550000,0000600000,0,0,0,0,1,0,0,0,0,0,0,08,08,000,0,CQCQCQ,0,00";

static int write_all(int fd, const char *data, size_t length)
{
    while (length > 0)
    {
        ssize_t written = write(fd, data, length);

        if (written < 0)
        {
            return -1;
        }

        data += written;
        length -= (size_t)written;
    }

    return 0;
}

static int send_reply(int fd, const char *reply)
{
    return write_all(fd, reply, strlen(reply)) < 0
           || write_all(fd, "\r", 1) < 0 ? -1 : 0;
}

static int read_command(int fd, char *command, size_t capacity)
{
    size_t used = 0;

    while (used + 1 < capacity)
    {
        char byte;
        ssize_t count = read(fd, &byte, 1);

        if (count <= 0)
        {
            return -1;
        }

        if (byte == '\r')
        {
            command[used] = '\0';
            return 0;
        }

        command[used++] = byte;
    }

    return -1;
}

static int emulate(int master)
{
    char command[MEMORY_RECORD_MAX];
    char memory[MEMORY_RECORD_MAX] = "";
    enum reply_fault next_fault = FAULT_NONE;
    unsigned int memory_queries = 0;
    int canonicalize_next_write = 0;
    int mode[2] = { 0, 0 };
    int stress = 0;

    while (read_command(master, command, sizeof(command)) == 0)
    {
        if (strcmp(command, "ID") == 0)
        {
            send_reply(master, "ID TH-D75");
        }
        else if (strcmp(command, "BC") == 0)
        {
            send_reply(master, "BC 0");
        }
        else if (strcmp(command, "AI") == 0)
        {
            send_reply(master, "AI 0");
        }
        else if (strcmp(command, "FO 0") == 0)
        {
            send_reply(master, fo_zero);
        }
        else if (strcmp(command, "MD 0") == 0 || strcmp(command, "MD 1") == 0)
        {
            int band = command[3] - '0';
            snprintf(reply, sizeof(reply), "MD %d,%d", band, mode[band]);
            send_reply(master, reply);
        }
        else if (strlen(command) == 6 && strncmp(command, "MD ", 3) == 0
                 && (command[3] == '0' || command[3] == '1')
                 && command[4] == ',' && command[5] >= '0' && command[5] <= '9')
        {
            int band = command[3] - '0';
            mode[band] = command[5] - '0';
            send_reply(master, command);
        }
        else if (strcmp(command, "ZZ STRESS") == 0)
        {
            stress = 1;
            send_reply(master, command);
        }
        else if (strcmp(command, "ZZ MALFORMED") == 0)
        {
            next_fault = FAULT_MALFORMED;
            send_reply(master, command);
        }
        else if (strcmp(command, "ZZ WRONG_CHANNEL") == 0)
        {
            next_fault = FAULT_WRONG_CHANNEL;
            send_reply(master, command);
        }
        else if (strcmp(command, "ZZ REJECT_ONCE") == 0)
        {
            next_fault = FAULT_REJECT_ONCE;
            send_reply(master, command);
        }
        else if (strcmp(command, "ZZ CANONICALIZE") == 0)
        {
            canonicalize_next_write = 1;
            send_reply(master, command);
        }
        else if (strcmp(command, "ME 999") == 0)
        {
            memory_queries++;

            if (next_fault == FAULT_MALFORMED)
            {
                next_fault = FAULT_NONE;
                send_reply(master, "ME 999,bad");
            }
            else if (next_fault == FAULT_WRONG_CHANNEL)
            {
                next_fault = FAULT_NONE;
                send_reply(master,
                           "ME 998,0145000000,0000000000,5,5,0,0,0,0,0,0,0,0,0,0,08,08,000,0,CQCQCQ,0,00,0");
            }
            else if (next_fault == FAULT_REJECT_ONCE)
            {
                next_fault = FAULT_NONE;
                send_reply(master, "?");
            }
            else
            {
                if (stress && memory_queries % 17 == 0)
                {
                    send_reply(master, "BC 0");
                }

                send_reply(master, memory[0] == '\0' ? "N" : memory);
            }
        }
        else if (strcmp(command, "ME 999,") == 0)
        {
            memory[0] = '\0';
            send_reply(master, "ME 999");
        }
        else if (strncmp(command, "ME 999,", 7) == 0)
        {
            char *tone_indexes;

            snprintf(memory, sizeof(memory), "%s", command);

            if (canonicalize_next_write
                    && (tone_indexes = strstr(memory, ",08,08,000,0,")) != NULL)
            {
                tone_indexes[2] = '9';
                canonicalize_next_write = 0;
            }

            send_reply(master, memory);
        }
        else
        {
            send_reply(master, "?");
        }
    }

    close(master);
    return 0;
}

static int raw_command(RIG *rig, const char *command, char *reply,
                       size_t reply_size)
{
    unsigned char send[MEMORY_RECORD_MAX];
    unsigned char terminator = '\r';
    int send_length = snprintf((char *)send, sizeof(send), "%s\r", command);
    int result;

    if (send_length < 0 || (size_t)send_length >= sizeof(send))
    {
        return -RIG_EINVAL;
    }

    result = rig_send_raw(rig, send, send_length, (unsigned char *)reply,
                          (int)reply_size - 1, &terminator);

    if (result < 0)
    {
        return result;
    }

    if ((size_t)result >= reply_size)
    {
        return -RIG_ETRUNC;
    }

    reply[result] = '\0';

    if (result > 0 && reply[result - 1] == '\r')
    {
        reply[result - 1] = '\0';
    }

    return RIG_OK;
}

static channel_t base_channel(void)
{
    channel_t channel;

    memset(&channel, 0, sizeof(channel));
    channel.channel_num = 999;
    channel.vfo = RIG_VFO_MEM;
    channel.freq = 145000000;
    channel.mode = RIG_MODE_FM;
    channel.width = 14000;
    channel.rptr_shift = RIG_RPT_SHIFT_NONE;
    channel.tuning_step = 12500;
    return channel;
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

int main(void)
{
    const char *slave_name;
    channel_t channel, received;
    char reply[MEMORY_RECORD_MAX];
    int master, guard, status;
    pid_t child;
    pbwidth_t width;
    RIG *rig;
    rmode_t mode;
    int failures = 0;

    master = posix_openpt(O_RDWR | O_NOCTTY);

    if (master < 0 || grantpt(master) < 0 || unlockpt(master) < 0)
    {
        perror("posix_openpt");
        return 1;
    }

    slave_name = ptsname(master);

    if (slave_name == NULL || (guard = open(slave_name, O_RDWR | O_NOCTTY)) < 0)
    {
        perror("open pty slave");
        close(master);
        return 1;
    }

    child = fork();

    if (child < 0)
    {
        perror("fork");
        close(guard);
        close(master);
        return 1;
    }

    if (child == 0)
    {
        close(guard);
        _exit(emulate(master));
    }

    close(master);
    rig_set_debug_level(RIG_DEBUG_NONE);
    rig_load_backend("kenwood");
    rig = rig_init(RIG_MODEL_THD75);
    failures += expect(rig != NULL, "initialize TH-D75 backend");

    if (rig == NULL)
    {
        kill(child, SIGTERM);
        waitpid(child, NULL, 0);
        close(guard);
        return 1;
    }

    rig_set_conf(rig, rig_token_lookup(rig, "rig_pathname"), slave_name);
    status = rig_open(rig);
    failures += expect(status == RIG_OK, "open TH-D75 emulator");
    close(guard);

    if (status != RIG_OK)
    {
        rig_cleanup(rig);
        kill(child, SIGTERM);
        waitpid(child, NULL, 0);
        return 1;
    }

    failures += expect(rig_set_mode(rig, RIG_VFO_A, RIG_MODE_DSTAR,
                                    RIG_PASSBAND_NOCHANGE) == RIG_OK,
                       "set supported D-STAR mode");
    failures += expect(rig_get_mode(rig, RIG_VFO_A, &mode, &width) == RIG_OK
                       && mode == RIG_MODE_DSTAR,
                       "read supported D-STAR mode");
    failures += expect(rig_set_mode(rig, RIG_VFO_A, RIG_MODE_RTTY,
                                    RIG_PASSBAND_NOCHANGE) == -RIG_EINVAL,
                       "reject unsupported mode");
    failures += expect(rig_get_mode(rig, RIG_VFO_A, &mode, &width) == RIG_OK
                       && mode == RIG_MODE_DSTAR,
                       "preserve mode after rejected mode request");
    failures += expect(rig_set_mode(rig, RIG_VFO_A, RIG_MODE_FM,
                                    RIG_PASSBAND_NOCHANGE) == RIG_OK,
                       "restore supported FM mode");

    channel = base_channel();
    channel.width = 6000;
    failures += expect(rig_set_channel(rig, RIG_VFO_NONE, &channel)
                       == -RIG_EINVAL,
                       "reject width inconsistent with mode");
    channel.width = 14000;
    failures += expect(rig_set_channel(rig, RIG_VFO_NONE, &channel) == RIG_OK,
                       "write FM simplex memory");
    failures += expect(raw_command(rig, "ME 999", reply, sizeof(reply)) == RIG_OK,
                       "query serialized FM memory");
    failures += expect(strcmp(reply,
                              "ME 999,0145000000,0000000000,5,5,0,0,0,0,0,0,0,0,0,0,08,08,000,0,CQCQCQ,0,00,0") == 0,
                       "serialize deterministic FM defaults");
    failures += expect(raw_command(rig, "ZZ CANONICALIZE", reply,
                                   sizeof(reply)) == RIG_OK,
                       "arm canonicalized write acknowledgement");
    failures += expect(rig_set_channel(rig, RIG_VFO_NONE, &channel) == RIG_OK,
                       "accept valid canonicalized acknowledgement");
    failures += expect(raw_command(rig, "ME 999", reply, sizeof(reply)) == RIG_OK
                       && strstr(reply, ",09,08,000,0,") != NULL,
                       "retain radio-canonicalized memory record");

    channel.ctcss_tone = 1000;
    channel.ctcss_sql = 1230;
    failures += expect(rig_set_channel(rig, RIG_VFO_NONE, &channel) == RIG_OK,
                       "write Cross Tone/CTCSS memory");
    failures += expect(raw_command(rig, "ME 999", reply, sizeof(reply)) == RIG_OK,
                       "query Cross Tone/CTCSS memory");
    failures += expect(strstr(reply, ",0,0,0,1,0,0,0,12,18,000,3,") != NULL,
                       "serialize Cross selector 3");

    failures += expect(raw_command(rig,
                                   "ME 999,0145670000,0000000000,5,5,1,0,0,0,0,0,0,0,0,0,12,18,000,0,N0CALL,1,12,0",
                                   reply, sizeof(reply)) == RIG_OK,
                       "seed non-default DV fields");
    channel = base_channel();
    channel.freq = 145680000;
    channel.mode = RIG_MODE_DSTAR;
    channel.width = 6000;
    failures += expect(rig_set_channel(rig, RIG_VFO_NONE, &channel) == RIG_OK,
                       "update seeded DV memory");
    failures += expect(raw_command(rig, "ME 999", reply, sizeof(reply)) == RIG_OK,
                       "query updated DV memory");
    failures += expect(strcmp(reply,
                              "ME 999,0145680000,0000000000,5,5,1,0,0,0,0,0,0,0,0,0,12,18,000,0,N0CALL,1,12,0") == 0,
                       "preserve unrepresented DV fields");

    channel = base_channel();
    channel.split = RIG_SPLIT_ON;
    channel.tx_freq = 145700000;
    channel.funcs = RIG_FUNC_REV;
    failures += expect(rig_set_channel(rig, RIG_VFO_NONE, &channel)
                       == -RIG_EINVAL,
                       "reject Reverse on odd-split memory");
    channel.funcs = 0;
    failures += expect(rig_set_channel(rig, RIG_VFO_NONE, &channel) == RIG_OK,
                       "write odd-split memory");
    failures += expect(raw_command(rig, "ME 999", reply, sizeof(reply)) == RIG_OK,
                       "query odd-split memory");
    failures += expect(strstr(reply,
                              "ME 999,0145000000,0145700000,5,5,0,0,0,0,0,0,0,0,1,0,") == reply,
                       "serialize odd split with zero shift");

    memset(&received, 0, sizeof(received));
    received.channel_num = 999;
    received.vfo = RIG_VFO_MEM;
    failures += expect(rig_get_channel(rig, RIG_VFO_NONE, &received, 1) == RIG_OK,
                       "read odd-split memory");
    failures += expect(received.split == RIG_SPLIT_ON
                       && received.freq == 145000000
                       && received.tx_freq == 145700000
                       && received.funcs == 0,
                       "decode odd-split memory");

    failures += expect(raw_command(rig, "ZZ STRESS", reply, sizeof(reply)) == RIG_OK,
                       "enable unsolicited stress replies");

    for (int i = 0; i < 1000; i++)
    {
        memset(&received, 0, sizeof(received));
        received.channel_num = 999;
        received.vfo = RIG_VFO_MEM;

        if (rig_get_channel(rig, RIG_VFO_NONE, &received, 1) != RIG_OK)
        {
            fprintf(stderr, "FAIL: stress read %d\n", i);
            failures++;
            break;
        }
    }

    failures += expect(raw_command(rig, "ZZ MALFORMED", reply, sizeof(reply)) == RIG_OK,
                       "arm malformed reply");
    failures += expect(rig_get_channel(rig, RIG_VFO_NONE, &received, 1)
                       == -RIG_EPROTO,
                       "map malformed ME record to protocol error");
    failures += expect(raw_command(rig, "ZZ WRONG_CHANNEL", reply,
                                   sizeof(reply)) == RIG_OK,
                       "arm wrong-channel reply");
    failures += expect(rig_get_channel(rig, RIG_VFO_NONE, &received, 1)
                       == -RIG_EPROTO,
                       "reject same-command reply for another channel");
    failures += expect(raw_command(rig, "ZZ REJECT_ONCE", reply, sizeof(reply))
                       == RIG_OK,
                       "arm one rejected transaction");
    failures += expect(rig_get_channel(rig, RIG_VFO_NONE, &received, 1) == RIG_OK,
                       "recover after one question-mark rejection");

    channel = base_channel();
    channel.freq = RIG_FREQ_NONE;
    failures += expect(rig_set_channel(rig, RIG_VFO_NONE, &channel) == RIG_OK,
                       "erase memory through RIG_FREQ_NONE");
    failures += expect(rig_get_channel(rig, RIG_VFO_NONE, &received, 1)
                       == -RIG_ENAVAIL,
                       "report erased memory unavailable");

    rig_close(rig);
    rig_cleanup(rig);
    waitpid(child, &status, 0);
    failures += expect(WIFEXITED(status) && WEXITSTATUS(status) == 0,
                       "emulator exited cleanly");

    if (failures == 0)
    {
        printf("TH-D75 emulator integration tests passed\n");
    }

    return failures == 0 ? 0 : 1;
}

#endif
