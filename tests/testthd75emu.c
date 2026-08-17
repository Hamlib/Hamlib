#if defined(_WIN32) || defined(WIN32)

int main(void)
{
    return 77;
}

#else

#define _XOPEN_SOURCE 600

#include <errno.h>
#include <fcntl.h>
#include <math.h>
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
    FAULT_SM_WRONG_BAND,
    FAULT_SM_OUT_OF_RANGE,
    FAULT_BY_WRONG_BAND,
    FAULT_BY_OUT_OF_RANGE,
    FAULT_AG_OUT_OF_RANGE,
    FAULT_VG_MALFORMED,
    FAULT_VD_OUT_OF_RANGE,
    FAULT_VX_OUT_OF_RANGE,
    FAULT_VX_MISMATCH_ACK,
    FAULT_RT_INVALID_DATE,
    FAULT_RT_NON_DECIMAL,
    FAULT_RT_TRUNCATED,
    FAULT_RT_WRONG_PREFIX,
    FAULT_RT_MISMATCH_ACK,
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
    char reply[MEMORY_RECORD_MAX];
    char clock_record[MEMORY_RECORD_MAX] = "RT 260729095824";
    enum reply_fault next_fault = FAULT_NONE;
    unsigned int memory_queries = 0;
    int audio_gain = 46;
    int busy[2] = { 0, 1 };
    int canonicalize_next_write = 0;
    int mode[2] = { 0, 0 };
    int power[2] = { 0, 3 };
    int selected_band = 0;
    int signal[2] = { 0, 5 };
    int squelch[2] = { 1, 2 };
    int stress = 0;
    int vox_delay = 1;
    int vox_enabled = 0;
    int vox_gain = 4;

    while (read_command(master, command, sizeof(command)) == 0)
    {
        if (strcmp(command, "ID") == 0)
        {
            send_reply(master, "ID TH-D75");
        }
        else if (strcmp(command, "BC") == 0)
        {
            snprintf(reply, sizeof(reply), "BC %d", selected_band);
            send_reply(master, reply);
        }
        else if (strcmp(command, "BC 0") == 0 || strcmp(command, "BC 1") == 0)
        {
            selected_band = command[3] - '0';
            send_reply(master, command);
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
        else if (strcmp(command, "SM 0") == 0 || strcmp(command, "SM 1") == 0)
        {
            int band = command[3] - '0';

            if (next_fault == FAULT_SM_OUT_OF_RANGE)
            {
                next_fault = FAULT_NONE;
                snprintf(reply, sizeof(reply), "SM %d,6", band);
                send_reply(master, reply);
                continue;
            }
            else if (next_fault == FAULT_SM_WRONG_BAND)
            {
                next_fault = FAULT_NONE;
                band = 1 - band;
            }

            snprintf(reply, sizeof(reply), "SM %d,%d", band, signal[band]);
            send_reply(master, reply);
        }
        else if (strcmp(command, "BY 0") == 0 || strcmp(command, "BY 1") == 0)
        {
            int band = command[3] - '0';

            if (next_fault == FAULT_BY_OUT_OF_RANGE)
            {
                next_fault = FAULT_NONE;
                snprintf(reply, sizeof(reply), "BY %d,2", band);
                send_reply(master, reply);
                continue;
            }
            else if (next_fault == FAULT_BY_WRONG_BAND)
            {
                next_fault = FAULT_NONE;
                band = 1 - band;
            }

            snprintf(reply, sizeof(reply), "BY %d,%d", band, busy[band]);
            send_reply(master, reply);
        }
        else if (strcmp(command, "PC 0") == 0 || strcmp(command, "PC 1") == 0)
        {
            int band = command[3] - '0';
            snprintf(reply, sizeof(reply), "PC %d,%d", band, power[band]);
            send_reply(master, reply);
        }
        else if (strlen(command) == 6 && strncmp(command, "PC ", 3) == 0
                 && (command[3] == '0' || command[3] == '1')
                 && command[4] == ',' && command[5] >= '0' && command[5] <= '3')
        {
            power[command[3] - '0'] = command[5] - '0';
            send_reply(master, command);
        }
        else if (strcmp(command, "SQ 0") == 0 || strcmp(command, "SQ 1") == 0)
        {
            int band = command[3] - '0';
            snprintf(reply, sizeof(reply), "SQ %d,%d", band, squelch[band]);
            send_reply(master, reply);
        }
        else if (strlen(command) == 6 && strncmp(command, "SQ ", 3) == 0
                 && (command[3] == '0' || command[3] == '1')
                 && command[4] == ',' && command[5] >= '0' && command[5] <= '5')
        {
            squelch[command[3] - '0'] = command[5] - '0';
            send_reply(master, command);
        }
        else if (strcmp(command, "AG") == 0)
        {
            if (next_fault == FAULT_AG_OUT_OF_RANGE)
            {
                next_fault = FAULT_NONE;
                send_reply(master, "AG 201");
            }
            else
            {
                snprintf(reply, sizeof(reply), "AG %03d", audio_gain);
                send_reply(master, reply);
            }
        }
        else if (strlen(command) == 6 && strncmp(command, "AG ", 3) == 0
                 && command[3] >= '0' && command[3] <= '9'
                 && command[4] >= '0' && command[4] <= '9'
                 && command[5] >= '0' && command[5] <= '9')
        {
            int value = 100 * (command[3] - '0') + 10 * (command[4] - '0')
                        + command[5] - '0';

            if (value <= 200)
            {
                audio_gain = value;
                send_reply(master, command);
            }
            else
            {
                send_reply(master, "N");
            }
        }
        else if (strcmp(command, "VG") == 0)
        {
            if (next_fault == FAULT_VG_MALFORMED)
            {
                next_fault = FAULT_NONE;
                send_reply(master, "VG 10");
            }
            else
            {
                snprintf(reply, sizeof(reply), "VG %d", vox_gain);
                send_reply(master, reply);
            }
        }
        else if (strlen(command) == 4 && strncmp(command, "VG ", 3) == 0
                 && command[3] >= '0' && command[3] <= '9')
        {
            vox_gain = command[3] - '0';
            send_reply(master, command);
        }
        else if (strcmp(command, "VD") == 0)
        {
            if (next_fault == FAULT_VD_OUT_OF_RANGE)
            {
                next_fault = FAULT_NONE;
                send_reply(master, "VD 7");
            }
            else
            {
                snprintf(reply, sizeof(reply), "VD %d", vox_delay);
                send_reply(master, reply);
            }
        }
        else if (strlen(command) == 4 && strncmp(command, "VD ", 3) == 0
                 && command[3] >= '0' && command[3] <= '6')
        {
            vox_delay = command[3] - '0';
            send_reply(master, command);
        }
        else if (strcmp(command, "VX") == 0)
        {
            if (next_fault == FAULT_VX_OUT_OF_RANGE)
            {
                next_fault = FAULT_NONE;
                send_reply(master, "VX 2");
            }
            else
            {
                snprintf(reply, sizeof(reply), "VX %d", vox_enabled);
                send_reply(master, reply);
            }
        }
        else if (strlen(command) == 4 && strncmp(command, "VX ", 3) == 0
                 && (command[3] == '0' || command[3] == '1'))
        {
            if (next_fault == FAULT_VX_MISMATCH_ACK)
            {
                next_fault = FAULT_NONE;
                send_reply(master, command[3] == '0' ? "VX 1" : "VX 0");
            }
            else
            {
                vox_enabled = command[3] - '0';
                send_reply(master, command);
            }
        }
        else if (strcmp(command, "RT") == 0)
        {
            if (next_fault == FAULT_RT_INVALID_DATE)
            {
                next_fault = FAULT_NONE;
                send_reply(master, "RT 260230246060");
            }
            else if (next_fault == FAULT_RT_NON_DECIMAL)
            {
                next_fault = FAULT_NONE;
                send_reply(master, "RT 26072909X824");
            }
            else if (next_fault == FAULT_RT_TRUNCATED)
            {
                next_fault = FAULT_NONE;
                send_reply(master, "RT 26072909582");
            }
            else if (next_fault == FAULT_RT_WRONG_PREFIX)
            {
                send_reply(master, "RX 260729095824");
            }
            else
            {
                send_reply(master, clock_record);
            }
        }
        else if (strlen(command) == 15 && strncmp(command, "RT ", 3) == 0)
        {
            if (next_fault == FAULT_RT_MISMATCH_ACK)
            {
                next_fault = FAULT_NONE;
                send_reply(master, "RT 240229235958");
            }
            else
            {
                snprintf(clock_record, sizeof(clock_record), "%s", command);
                send_reply(master, clock_record);
            }
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
        else if (strcmp(command, "ZZ SM_WRONG_BAND") == 0)
        {
            next_fault = FAULT_SM_WRONG_BAND;
            send_reply(master, command);
        }
        else if (strcmp(command, "ZZ SM_OUT_OF_RANGE") == 0)
        {
            next_fault = FAULT_SM_OUT_OF_RANGE;
            send_reply(master, command);
        }
        else if (strcmp(command, "ZZ BY_WRONG_BAND") == 0)
        {
            next_fault = FAULT_BY_WRONG_BAND;
            send_reply(master, command);
        }
        else if (strcmp(command, "ZZ BY_OUT_OF_RANGE") == 0)
        {
            next_fault = FAULT_BY_OUT_OF_RANGE;
            send_reply(master, command);
        }
        else if (strcmp(command, "ZZ AG_OUT_OF_RANGE") == 0)
        {
            next_fault = FAULT_AG_OUT_OF_RANGE;
            send_reply(master, command);
        }
        else if (strcmp(command, "ZZ VG_MALFORMED") == 0)
        {
            next_fault = FAULT_VG_MALFORMED;
            send_reply(master, command);
        }
        else if (strcmp(command, "ZZ VD_OUT_OF_RANGE") == 0)
        {
            next_fault = FAULT_VD_OUT_OF_RANGE;
            send_reply(master, command);
        }
        else if (strcmp(command, "ZZ VX_OUT_OF_RANGE") == 0)
        {
            next_fault = FAULT_VX_OUT_OF_RANGE;
            send_reply(master, command);
        }
        else if (strcmp(command, "ZZ VX_MISMATCH_ACK") == 0)
        {
            next_fault = FAULT_VX_MISMATCH_ACK;
            send_reply(master, command);
        }
        else if (strcmp(command, "ZZ RT_INVALID_DATE") == 0)
        {
            next_fault = FAULT_RT_INVALID_DATE;
            send_reply(master, command);
        }
        else if (strcmp(command, "ZZ RT_TRUNCATED") == 0)
        {
            next_fault = FAULT_RT_TRUNCATED;
            send_reply(master, command);
        }
        else if (strcmp(command, "ZZ RT_NON_DECIMAL") == 0)
        {
            next_fault = FAULT_RT_NON_DECIMAL;
            send_reply(master, command);
        }
        else if (strcmp(command, "ZZ RT_WRONG_PREFIX") == 0)
        {
            next_fault = FAULT_RT_WRONG_PREFIX;
            send_reply(master, command);
        }
        else if (strcmp(command, "ZZ RT_MISMATCH_ACK") == 0)
        {
            next_fault = FAULT_RT_MISMATCH_ACK;
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
    dcd_t dcd;
    double msec;
    int year, month, day, hour, minute, second, utc_offset;
    int master, guard, status;
    pid_t child;
    pbwidth_t width;
    RIG *rig;
    rmode_t mode;
    value_t value;
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

    failures += expect((rig->caps->has_get_level & RIG_LEVEL_RAWSTR) != 0,
                       "advertise raw signal strength");
    failures += expect(rig_has_get_level(rig, RIG_LEVEL_STRENGTH) == 0,
                       "do not synthesize uncalibrated signal strength");
    failures += expect((rig->caps->has_get_level & RIG_LEVEL_AF) != 0
                       && (rig->caps->has_get_level & RIG_LEVEL_VOXGAIN) != 0
                       && (rig->caps->has_get_level & RIG_LEVEL_VOXDELAY) != 0,
                       "advertise receiver and VOX levels");
    failures += expect((rig->caps->has_get_level & RIG_LEVEL_ATT) == 0,
                       "leave boolean attenuator unadvertised");
    failures += expect(rig->caps->dcd_type == RIG_DCD_RIG
                       && rig->caps->get_dcd != NULL,
                       "advertise radio carrier detect");
    failures += expect((rig->caps->has_get_parm & RIG_PARM_TIME) == 0
                       && (rig->caps->has_set_parm & RIG_PARM_TIME) == 0,
                       "omit time-only clock parameter");
    failures += expect(rig->caps->get_clock != NULL
                       && rig->caps->set_clock != NULL,
                       "advertise full clock access");
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
    failures += expect(rig_has_get_func(rig, RIG_FUNC_VOX) != 0
                       && rig_has_set_func(rig, RIG_FUNC_VOX) != 0,
                       "advertise VOX status and control");
    failures += expect(rig_get_func(rig, RIG_VFO_A, RIG_FUNC_VOX,
                                    &status) == RIG_OK && status == 0,
                       "read disabled VOX status");
    failures += expect(raw_command(rig, "ZZ VX_OUT_OF_RANGE", reply,
                                   sizeof(reply)) == RIG_OK,
                       "arm out-of-range VOX status reply");
    failures += expect(rig_get_func(rig, RIG_VFO_A, RIG_FUNC_VOX,
                                    &status) == -RIG_EPROTO,
                       "reject out-of-range VOX status reply");
    failures += expect(rig_set_func(rig, RIG_VFO_A, RIG_FUNC_VOX, 1)
                       == RIG_OK,
                       "enable VOX");
    failures += expect(rig_get_func(rig, RIG_VFO_A, RIG_FUNC_VOX,
                                    &status) == RIG_OK && status == 1,
                       "read enabled VOX status");
    failures += expect(rig_set_func(rig, RIG_VFO_A, RIG_FUNC_VOX, 0)
                       == RIG_OK,
                       "disable VOX");
    failures += expect(rig_set_func(rig, RIG_VFO_A, RIG_FUNC_VOX, 2)
                       == -RIG_EINVAL,
                       "reject invalid VOX state");
    failures += expect(raw_command(rig, "ZZ VX_MISMATCH_ACK", reply,
                                   sizeof(reply)) == RIG_OK,
                       "arm mismatched VOX acknowledgment");
    failures += expect(rig_set_func(rig, RIG_VFO_A, RIG_FUNC_VOX, 1)
                       == -RIG_EPROTO,
                       "reject mismatched VOX acknowledgment");
    failures += expect(rig_get_func(rig, RIG_VFO_A, RIG_FUNC_VOX,
                                    &status) == RIG_OK && status == 0,
                       "preserve VOX state after rejected acknowledgment");

    failures += expect(rig_get_level(rig, RIG_VFO_A, RIG_LEVEL_RAWSTR,
                                     &value) == RIG_OK && value.i == 0,
                       "read Band A raw signal strength");
    failures += expect(rig_get_level(rig, RIG_VFO_B, RIG_LEVEL_RAWSTR,
                                     &value) == RIG_OK && value.i == 5,
                       "read Band B raw signal strength");
    failures += expect(rig_get_dcd(rig, RIG_VFO_A, &dcd) == RIG_OK
                       && dcd == RIG_DCD_OFF,
                       "read Band A carrier detect off");
    failures += expect(rig_get_dcd(rig, RIG_VFO_B, &dcd) == RIG_OK
                       && dcd == RIG_DCD_ON,
                       "read Band B carrier detect on");

    failures += expect(raw_command(rig, "ZZ SM_WRONG_BAND", reply,
                                   sizeof(reply)) == RIG_OK,
                       "arm wrong-band signal reply");
    failures += expect(rig_get_level(rig, RIG_VFO_A, RIG_LEVEL_RAWSTR,
                                     &value) == -RIG_EPROTO,
                       "reject wrong-band signal reply");
    failures += expect(raw_command(rig, "ZZ SM_OUT_OF_RANGE", reply,
                                   sizeof(reply)) == RIG_OK,
                       "arm out-of-range signal reply");
    failures += expect(rig_get_level(rig, RIG_VFO_A, RIG_LEVEL_RAWSTR,
                                     &value) == -RIG_EPROTO,
                       "reject out-of-range signal reply");
    failures += expect(raw_command(rig, "ZZ BY_WRONG_BAND", reply,
                                   sizeof(reply)) == RIG_OK,
                       "arm wrong-band carrier reply");
    failures += expect(rig_get_dcd(rig, RIG_VFO_A, &dcd) == -RIG_EPROTO,
                       "reject wrong-band carrier reply");
    failures += expect(raw_command(rig, "ZZ BY_OUT_OF_RANGE", reply,
                                   sizeof(reply)) == RIG_OK,
                       "arm out-of-range carrier reply");
    failures += expect(rig_get_dcd(rig, RIG_VFO_A, &dcd) == -RIG_EPROTO,
                       "reject out-of-range carrier reply");

    failures += expect(rig_get_level(rig, RIG_VFO_A, RIG_LEVEL_SQL,
                                     &value) == RIG_OK
                       && fabsf(value.f - 0.2f) < 0.001f,
                       "read normalized squelch level");
    value.f = 0.6f;
    failures += expect(rig_set_level(rig, RIG_VFO_A, RIG_LEVEL_SQL,
                                     value) == RIG_OK,
                       "set normalized squelch level");
    failures += expect(rig_get_level(rig, RIG_VFO_A, RIG_LEVEL_SQL,
                                     &value) == RIG_OK
                       && fabsf(value.f - 0.6f) < 0.001f,
                       "read canonical squelch level");

    failures += expect(rig_get_level(rig, RIG_VFO_A, RIG_LEVEL_AF,
                                     &value) == RIG_OK
                       && fabsf(value.f - 0.23f) < 0.001f,
                       "read normalized audio gain");
    value.f = 0.5f;
    failures += expect(rig_set_level(rig, RIG_VFO_A, RIG_LEVEL_AF,
                                     value) == RIG_OK,
                       "set normalized audio gain");
    failures += expect(rig_get_level(rig, RIG_VFO_A, RIG_LEVEL_AF,
                                     &value) == RIG_OK
                       && fabsf(value.f - 0.5f) < 0.001f,
                       "read canonical audio gain");
    failures += expect(raw_command(rig, "ZZ AG_OUT_OF_RANGE", reply,
                                   sizeof(reply)) == RIG_OK,
                       "arm out-of-range audio gain reply");
    failures += expect(rig_get_level(rig, RIG_VFO_A, RIG_LEVEL_AF,
                                     &value) == -RIG_EPROTO,
                       "reject out-of-range audio gain reply");

    failures += expect(rig_get_level(rig, RIG_VFO_A, RIG_LEVEL_VOXGAIN,
                                     &value) == RIG_OK
                       && fabsf(value.f - 4.0f / 9.0f) < 0.001f,
                       "read normalized VOX gain");
    value.f = 5.0f / 9.0f;
    failures += expect(rig_set_level(rig, RIG_VFO_A, RIG_LEVEL_VOXGAIN,
                                     value) == RIG_OK,
                       "set normalized VOX gain");
    failures += expect(rig_get_level(rig, RIG_VFO_A, RIG_LEVEL_VOXGAIN,
                                     &value) == RIG_OK
                       && fabsf(value.f - 5.0f / 9.0f) < 0.001f,
                       "read canonical VOX gain");
    failures += expect(raw_command(rig, "ZZ VG_MALFORMED", reply,
                                   sizeof(reply)) == RIG_OK,
                       "arm malformed VOX gain reply");
    failures += expect(rig_get_level(rig, RIG_VFO_A, RIG_LEVEL_VOXGAIN,
                                     &value) == -RIG_EPROTO,
                       "reject malformed VOX gain reply");

    failures += expect(rig_get_level(rig, RIG_VFO_A, RIG_LEVEL_VOXDELAY,
                                     &value) == RIG_OK && value.i == 5,
                       "read VOX delay in tenths of seconds");
    value.i = 8;
    failures += expect(rig_set_level(rig, RIG_VFO_A, RIG_LEVEL_VOXDELAY,
                                     value) == RIG_OK,
                       "set nearest VOX delay");
    failures += expect(rig_get_level(rig, RIG_VFO_A, RIG_LEVEL_VOXDELAY,
                                     &value) == RIG_OK && value.i == 8,
                       "read canonical VOX delay");
    failures += expect(raw_command(rig, "ZZ VD_OUT_OF_RANGE", reply,
                                   sizeof(reply)) == RIG_OK,
                       "arm out-of-range VOX delay reply");
    failures += expect(rig_get_level(rig, RIG_VFO_A, RIG_LEVEL_VOXDELAY,
                                     &value) == -RIG_EPROTO,
                       "reject out-of-range VOX delay reply");

    value.f = NAN;
    failures += expect(rig_set_level(rig, RIG_VFO_A, RIG_LEVEL_AF,
                                     value) == -RIG_EINVAL,
                       "reject non-finite normalized level");
    value.i = 31;
    failures += expect(rig_set_level(rig, RIG_VFO_A, RIG_LEVEL_VOXDELAY,
                                     value) == -RIG_EINVAL,
                       "reject out-of-range VOX delay");

    failures += expect(rig_get_clock(rig, &year, &month, &day, &hour, &minute,
                                     &second, &msec, &utc_offset) == RIG_OK
                       && year == 2026 && month == 7 && day == 29
                       && hour == 9 && minute == 58 && second == 24
                       && msec == 0.0 && utc_offset == 0,
                       "read full clock record");
    failures += expect(rig_set_clock(rig, 2024, 2, 29, 23, 59, 59, 0.0, 0)
                       == RIG_OK,
                       "write leap-day clock record");
    failures += expect(rig_get_clock(rig, &year, &month, &day, &hour, &minute,
                                     &second, &msec, &utc_offset) == RIG_OK
                       && year == 2024 && month == 2 && day == 29
                       && hour == 23 && minute == 59 && second == 59,
                       "read updated full clock record");
    failures += expect(rig_set_clock(rig, 2024, 2, 29, 23, 59, 58, -1.0, 0)
                       == RIG_OK,
                       "accept omitted-milliseconds sentinel");
    failures += expect(rig_set_clock(rig, 2024, 2, 29, 23, 59, 58,
                                     0.123456, 0) == RIG_OK,
                       "accept rigctl-style fractional seconds");
    failures += expect(rig_set_clock(rig, 2024, 2, 29, 23, 59, 58,
                                     999.999, 0) == RIG_OK,
                       "accept millisecond upper boundary");
    failures += expect(rig_get_clock(rig, &year, &month, &day, &hour, &minute,
                                     &second, &msec, &utc_offset) == RIG_OK
                       && year == 2024 && month == 2 && day == 29
                       && hour == 23 && minute == 59 && second == 58
                       && msec == 0.0,
                       "discard unsupported subsecond precision");

    failures += expect(rig_set_clock(rig, 2025, 2, 29, 12, 0, 0, 0.0, 0)
                       == -RIG_EINVAL,
                       "reject non-leap February 29");
    failures += expect(rig_set_clock(rig, 2100, 1, 1, 12, 0, 0, 0.0, 0)
                       == -RIG_EINVAL,
                       "reject year outside two-digit protocol range");
    failures += expect(rig_set_clock(rig, 2026, 12, 31, 24, 0, 0, 0.0, 0)
                       == -RIG_EINVAL,
                       "reject out-of-range clock time");
    failures += expect(rig_set_clock(rig, 2026, 12, 31, 23, 59, 59, -0.5, 0)
                       == -RIG_EINVAL,
                       "reject negative milliseconds");
    failures += expect(rig_set_clock(rig, 2026, 12, 31, 23, 59, 59, 1000.0, 0)
                       == -RIG_EINVAL,
                       "reject out-of-range milliseconds");
    failures += expect(rig_set_clock(rig, 2026, 12, 31, 23, 59, 59, NAN, 0)
                       == -RIG_EINVAL,
                       "reject non-finite milliseconds");
    failures += expect(rig_set_clock(rig, 2026, 12, 31, 23, 59, 59, 0.0, -500)
                       == -RIG_ENAVAIL,
                       "reject unavailable UTC-offset update");
    failures += expect(rig_get_clock(rig, &year, &month, &day, &hour, &minute,
                                     &second, &msec, &utc_offset) == RIG_OK
                       && year == 2024 && month == 2 && day == 29
                       && hour == 23 && minute == 59 && second == 58,
                       "preserve clock after invalid clock requests");

    failures += expect(raw_command(rig, "ZZ RT_INVALID_DATE", reply,
                                   sizeof(reply)) == RIG_OK,
                       "arm invalid calendar reply");
    failures += expect(rig_get_clock(rig, &year, &month, &day, &hour, &minute,
                                     &second, &msec, &utc_offset)
                       == -RIG_EPROTO,
                       "reject invalid calendar reply");
    failures += expect(raw_command(rig, "ZZ RT_NON_DECIMAL", reply,
                                   sizeof(reply)) == RIG_OK,
                       "arm non-decimal clock reply");
    failures += expect(rig_get_clock(rig, &year, &month, &day, &hour, &minute,
                                     &second, &msec, &utc_offset)
                       == -RIG_EPROTO,
                       "reject non-decimal clock reply");
    failures += expect(raw_command(rig, "ZZ RT_TRUNCATED", reply,
                                   sizeof(reply)) == RIG_OK,
                       "arm truncated clock reply");
    failures += expect(rig_get_clock(rig, &year, &month, &day, &hour, &minute,
                                     &second, &msec, &utc_offset)
                       == -RIG_EPROTO,
                       "reject truncated clock reply");
    failures += expect(raw_command(rig, "ZZ RT_WRONG_PREFIX", reply,
                                   sizeof(reply)) == RIG_OK,
                       "arm wrong-prefix clock reply");
    status = rig_get_clock(rig, &year, &month, &day, &hour, &minute,
                           &second, &msec, &utc_offset);
    failures += expect(status == -RIG_ETIMEOUT,
                       "reject wrong-prefix clock reply");

    failures += expect(raw_command(rig, "ZZ RT_MISMATCH_ACK", reply,
                                   sizeof(reply)) == RIG_OK,
                       "arm mismatched clock acknowledgment");
    failures += expect(rig_set_clock(rig, 2026, 7, 29, 10, 0, 0, 0.0, 0)
                       == -RIG_EPROTO,
                       "reject mismatched clock acknowledgment");
    failures += expect(rig_get_clock(rig, &year, &month, &day, &hour, &minute,
                                     &second, &msec, &utc_offset) == RIG_OK
                       && year == 2024 && month == 2 && day == 29
                       && hour == 23 && minute == 59 && second == 58,
                       "preserve clock state after rejected acknowledgment");

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
