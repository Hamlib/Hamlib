/* Exercise the FT-600 binary CAT status and mode commands. */

#ifdef _WIN32

int main(void)
{
    return 77;
}

#else

#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "hamlib/rig.h"
#include "hamlib/port.h"

enum operation
{
    GET_FREQ,
    GET_MODE,
    SET_MODE
};

struct exchange
{
    unsigned char command[5];
    const unsigned char *reply;
    size_t reply_size;
};

struct cat_case
{
    const char *name;
    enum operation operation;
    struct exchange exchanges[2];
    size_t exchange_count;
    rmode_t mode;
    pbwidth_t width;
    int result;
};

struct peer
{
    int fd;
    const struct cat_case *test;
    int result;
};

static const unsigned char split_status[] =
{
    0x20, 0x07, 0x20, 0x0b, 0x20, 0, 0, 0x05, 0, 0,
    0x05, 0x15, 0x5c, 0xc0, 0, 0, 0x01, 0, 0
};
static const unsigned char j2b_lsb_flags[] = {0x80, 0x08, 0, 0x06, 0x04};
static const unsigned char j2b_usb_flags[] = {0x80, 0x20, 0, 0x06, 0x04};
static const unsigned char cw_narrow_flags[] = {0x80, 0x10, 0, 0x06, 0x04};
static const unsigned char malformed_flags[] = {0x20, 0x07, 0x20, 0x0b, 0x20};

#define STATUS_REPLY(data) {{0, 0, 0, 2, 0x10}, data, sizeof(data)}
#define FLAGS_REPLY(data) {{0, 0, 0, 0, 0xfa}, data, sizeof(data)}
#define MODE_COMMAND(mode) {{0, 0, 0, mode, 0x0c}, NULL, 0}

static const struct cat_case cases[] =
{
    {
        "split RX frequency", GET_FREQ, {STATUS_REPLY(split_status)}, 1,
        RIG_MODE_NONE, 0, RIG_OK
    },
    {
        "J2B LSB narrow", GET_MODE,
        {STATUS_REPLY(split_status), FLAGS_REPLY(j2b_lsb_flags)}, 2,
        RIG_MODE_PKTLSB, 500, RIG_OK
    },
    {
        "J2B USB normal", GET_MODE,
        {STATUS_REPLY(split_status), FLAGS_REPLY(j2b_usb_flags)}, 2,
        RIG_MODE_PKTUSB, RIG_PASSBAND_NORMAL, RIG_OK
    },
    {
        "preserve stored narrow CW", SET_MODE,
        {FLAGS_REPLY(cw_narrow_flags), MODE_COMMAND(0x03)}, 2,
        RIG_MODE_CW, RIG_PASSBAND_NOCHANGE, RIG_OK
    },
    {
        "select narrow J2B LSB", SET_MODE, {MODE_COMMAND(0x0e)}, 1,
        RIG_MODE_PKTLSB, 500, RIG_OK
    },
    {
        "reject malformed flags before changing mode", SET_MODE,
        {FLAGS_REPLY(malformed_flags)}, 1,
        RIG_MODE_CW, RIG_PASSBAND_NOCHANGE, -RIG_EPROTO
    },
    {
        "propagate flags timeout before changing mode", SET_MODE,
        {{{0, 0, 0, 0, 0xfa}, NULL, 0}}, 1,
        RIG_MODE_CW, RIG_PASSBAND_NOCHANGE, -RIG_ETIMEOUT
    }
};

static int read_peer(int fd, unsigned char *data, size_t length)
{
    struct pollfd descriptor = { .fd = fd, .events = POLLIN };

    return poll(&descriptor, 1, 5000) == 1 ? read(fd, data, length) : -1;
}

static void *run_peer(void *arg)
{
    struct peer *peer = arg;
    unsigned char command[5];

    for (size_t i = 0; i < peer->test->exchange_count; ++i)
    {
        const struct exchange *exchange = &peer->test->exchanges[i];
        size_t used = 0;

        while (used < sizeof(command))
        {
            int count = read_peer(peer->fd, command + used, sizeof(command) - used);

            if (count <= 0) { return NULL; }

            used += count;
        }

        if (memcmp(command, exchange->command, sizeof(command)) != 0
                || (exchange->reply_size != 0
                    && write(peer->fd, exchange->reply, exchange->reply_size)
                    != (ssize_t)exchange->reply_size))
        {
            return NULL;
        }
    }

    /* The caller shuts down writes after the operation returns. */
    peer->result = read_peer(peer->fd, command, sizeof(command)) == 0 ? 0 : 1;
    return NULL;
}

static int run_case(RIG *rig, const struct cat_case *test)
{
    int sockets[2];
    pthread_t thread;
    int result = -RIG_EINVAL;
    int output_matches = 1;
    rmode_t mode;
    pbwidth_t width;
    freq_t freq;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) { return 1; }

    struct peer peer = { .fd = sockets[1], .test = test, .result = 1 };

    if (pthread_create(&thread, NULL, run_peer, &peer) != 0)
    {
        close(sockets[0]);
        close(sockets[1]);
        return 1;
    }

    RIGPORT(rig)->fd = sockets[0];

    switch (test->operation)
    {
    case GET_FREQ:
        result = rig->caps->get_freq(rig, RIG_VFO_A, &freq);
        output_matches = result == RIG_OK && freq == 21000000;
        break;

    case GET_MODE:
        result = rig->caps->get_mode(rig, RIG_VFO_A, &mode, &width);
        output_matches = result == RIG_OK && mode == test->mode && width == test->width;
        break;

    case SET_MODE:
        result = rig->caps->set_mode(rig, RIG_VFO_A, test->mode, test->width);
        break;
    }

    shutdown(sockets[0], SHUT_WR);
    pthread_join(thread, NULL);
    RIGPORT(rig)->fd = -1;
    close(sockets[0]);
    close(sockets[1]);

    if (result != test->result || !output_matches || peer.result != 0)
    {
        fprintf(stderr, "%s failed (result %d, peer %d)\n",
                test->name, result, peer.result);
        return 1;
    }

    return 0;
}

int main(void)
{
    rig_set_debug_level(RIG_DEBUG_NONE);

    RIG *rig = rig_init(RIG_MODEL_FT600);
    int result = 0;

    if (!rig) { return 1; }

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        result |= run_case(rig, &cases[i]);
    }

    if (rig_passband_normal(rig, RIG_MODE_AM) != 6000
            || rig_passband_narrow(rig, RIG_MODE_PKTLSB) != 500)
    {
        fprintf(stderr, "Unexpected FT-600 passband capabilities\n");
        result = 1;
    }

    rig_cleanup(rig);
    return result;
}

#endif
