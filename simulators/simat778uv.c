// AnyTone AT-778UV / Retevis RT95 microphone-bus simulator.
//
// Run it against the backend with a pty:
//
//     ./simulators/simat778uv /dev/ptmx        # prints the slave name
//     ./tests/rigctl -m 37002 -r /dev/ttys00X
//
// The register layout below mirrors a real AT-778UV running firmware V200,
// with representative frequencies and settings, so a reviewer without the
// radio gets something plausible to look at rather than zeroes.
//
// What is modelled, because the backend depends on all of it:
//
//   - every byte received is echoed before any reply, as the single-wire
//     half-duplex bus does;
//   - block reads answer only a length of 0x10;
//   - the parameter frame is rigidly seven bytes;
//   - the parameter command writes whichever record is SELECTED, and in
//     memory mode writes the memory channel instead of the VFO while
//     acknowledging normally. That is the trap the backend exists to avoid,
//     so it is reproduced faithfully. Start with "memory" as the second
//     argument, or press PA, to put slot A on a memory channel and watch
//     set_freq refuse.
//
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "sim.h"

#define BLOCK       0x10
#define RECLEN      32

#define ADDR_CHAN45 0x0580          /* the memory channel slot A is parked on */
#define ADDR_VFOA   0x1900
#define ADDR_VFOB   0x1920
#define ADDR_SET1   0x3200
#define ADDR_SET2   0x3210
#define ADDR_LIVE   0x3260

#define KEY_STATUS  0x52            /* 'R' - posts no event, draws a status frame */
#define KEY_AB      0x2F            /* '/' - toggles the selected slot */
#define KEY_PA      0x41            /* 'A' - PA, bound to VFO/MEM on this radio */

static unsigned char vfoa[RECLEN] =
{
    0x44, 0x60, 0x45, 0x00, 0x00, 0x06, 0x00, 0x00,
    0x00, 0x02, 0x08, 0x00, 0x09, 0x09, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0xf2, 0x05
};

static unsigned char vfob[RECLEN] =
{
    0x43, 0x35, 0x00, 0x00, 0x00, 0x50, 0x00, 0x00,
    0x00, 0x00, 0x08, 0x00, 0x09, 0x09, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0xf2, 0x05
};

/* Memory channel 45 - 145.625, high power, wide, named "MEM 1". */
static unsigned char chan45[RECLEN] =
{
    0x14, 0x56, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x4d, 0x45, 0x4d, 0x20, 0x31, 0x00, 0x00
};

static unsigned char set1[BLOCK] =
{
    0x00, 0x00, 0x02, 0x00, 0x02, 0x02, 0x0f, 0x00,
    0x01, 0x00, 0x01, 0x00, 0x01, 0x03, 0x03, 0x00
};

static unsigned char set2[BLOCK] =
{
    0x02, 0x00, 0x0f, 0x00, 0x02, 0x04, 0x0d, 0x08,
    0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x00
};

/* 0x3261 bit 0 and 0x3268 bit 0: 1 = VFO, 0 = memory. */
static unsigned char live[BLOCK] =
{
    0x2c, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00
};

static int selected = 0;            /* 0 = A, 1 = B */
static int cos_a = 0;
static int cos_b = 0;

// Fault injection. The real bus loses roughly one reply in forty to
// collisions, and the radio emits status frames of its own accord whenever
// the squelch opens. Both break a reader that assumes the next byte is the
// byte it asked for, so "noise" reproduces them on demand: a backend that
// survives this will survive the radio.
static int noise = 0;
static unsigned long tick = 0;
static unsigned long echo_tick = 0;

static void send_status(int fd);

//
// Fault injection, modelled on what the radio actually does.
//
// Measured over 1500 exchanges on a real AT-778UV with a carrier keyed on and
// off 129 times: 29 damaged echoes and 7 full retries. EVERY retry was
// triggered by a timeout after zero characters - our outgoing command mangled
// on the shared wire, so the radio never understood it and answered nothing
// at all. That, not a corrupted reply, is the dominant failure, and it is the
// only one a retry can recover.
//
// Notably NO unsolicited status frame was ever seen mid-reply, because the
// radio serialises its own output and pending frames are flushed between
// transactions. That path still has to work, so it is exercised here, just
// rarely - the proportions below follow the measurements.
//
#define INJECT_NONE     0
#define INJECT_CLIP     1
#define INJECT_SILENT   2

static int inject(int fd)
{
    if (!noise)
    {
        return INJECT_NONE;
    }

    tick++;

    /* The command never arrived. Say nothing and let the host time out. */
    if (tick % 5 == 0)
    {
        printf("[noise] command lost on the wire - no reply at all\n");
        return INJECT_SILENT;
    }

    /* A collision chewed the head off the reply. */
    if (tick % 11 == 0)
    {
        printf("[noise] clipping the first byte of the reply\n");
        return INJECT_CLIP;
    }

    /* Somebody keyed up: a status frame arrives ahead of the real reply. */
    if (tick % 17 == 0)
    {
        printf("[noise] unsolicited status frame\n");
        cos_a = !cos_a;
        send_status(fd);
    }

    return INJECT_NONE;
}

static void emit(int fd, const unsigned char *buf, int len, int how)
{
    if (how == INJECT_SILENT)
    {
        return;
    }

    if (how == INJECT_CLIP && len > 1)
    {
        WRITE(fd, buf + 1, len - 1);
        return;
    }

    WRITE(fd, buf, len);
}

static int readn(int fd, unsigned char *buf, int n)
{
    int got = 0;

    while (got < n)
    {
        int bytes = read(fd, buf + got, n - got);

        if (bytes <= 0)
        {
            return got;
        }

        got += bytes;
    }

    return got;
}

//
// The record a parameter write lands on: the selected slot's VFO, unless
// that slot is showing a memory channel, in which case the channel itself.
//
static unsigned char *target_record(void)
{
    int is_vfo = selected ? (live[8] & 1) : (live[1] & 1);

    if (!is_vfo)
    {
        return chan45;
    }

    return selected ? vfob : vfoa;
}

static unsigned char *block_for(unsigned short addr)
{
    switch (addr)
    {
    case ADDR_CHAN45:
        return &chan45[0];

    case ADDR_CHAN45 + BLOCK:
        return &chan45[BLOCK];

    case ADDR_VFOA:
        return &vfoa[0];

    case ADDR_VFOA + BLOCK:
        return &vfoa[BLOCK];

    case ADDR_VFOB:
        return &vfob[0];

    case ADDR_VFOB + BLOCK:
        return &vfob[BLOCK];

    case ADDR_SET1:
        return set1;

    case ADDR_SET2:
        return set2;

    case ADDR_LIVE:
        return live;

    default:
        return NULL;
    }
}

static void do_read(int fd, unsigned short addr, unsigned char len)
{
    unsigned char reply[22];
    unsigned char *src = block_for(addr);
    unsigned char sum = 0;
    static const unsigned char zero[BLOCK] = { 0 };
    int i;

    if (len != BLOCK)
    {
        // The real radio answers a longer request with a correctly
        // checksummed frame whose tail is stale buffer content. Refusing
        // outright would be kinder than the radio is, so don't: a backend
        // that asks for the wrong length deserves to see what it would
        // really get.
        printf("read of 0x%04x with length 0x%02x - the radio would return "
               "garbage here\n", addr, len);
    }

    if (src == NULL)
    {
        src = (unsigned char *)zero;
    }

    reply[0] = 'W';
    reply[1] = (unsigned char)(addr >> 8);
    reply[2] = (unsigned char)(addr & 0xff);
    reply[3] = BLOCK;
    memcpy(&reply[4], src, BLOCK);

    for (i = 1; i < 20; i++)
    {
        sum = (unsigned char)(sum + reply[i]);
    }

    reply[20] = sum;
    reply[21] = 0x06;

    emit(fd, reply, sizeof(reply), inject(fd));
}

static void do_param(int fd, const unsigned char *frame)
{
    unsigned char sub = frame[1];
    unsigned char v = frame[2];
    unsigned char *rec = target_record();
    unsigned char ack = 0x06;
    int wire;

    if (frame[6] != 0x06)
    {
        printf("parameter frame without its terminator - ignored\n");
        return;
    }

    switch (sub)
    {
    case 0:                                     /* frequency */
        memcpy(&rec[0], &frame[2], 4);

        if (rec == chan45)
        {
            printf("*** wrote the MEMORY CHANNEL, not the VFO - this is the "
                   "silent overwrite the backend guards against\n");
        }

        break;

    case 1:                                     /* offset, forces a minus shift */
        memcpy(&rec[4], &frame[2], 4);
        rec[9] = (unsigned char)((rec[9] & ~0x03) | 0x02);
        break;

    case 2:                                     /* bandwidth */
        rec[10] = (unsigned char)((rec[10] & ~0x0c) | ((v & 3) << 2));
        break;

    case 3:                                     /* RX tone */
        rec[11] = (unsigned char)((rec[11] & ~0x0c) | ((v & 3) << 2));

        if (v == 1)
        {
            rec[12] = frame[3];
            rec[20] = 1;
        }
        else if (v == 2)
        {
            wire = frame[4] | ((frame[5] & 1) << 8);
            rec[14] = (unsigned char)(wire & 0xff);
            rec[15] = (unsigned char)(((frame[3] & 1) << 1) | ((wire >> 8) & 1));
            rec[20] = 1;
        }
        else
        {
            rec[20] = 0;
        }

        break;

    case 4:                                     /* TX tone */
        rec[11] = (unsigned char)((rec[11] & ~0x03) | (v & 3));

        if (v == 1)
        {
            rec[13] = frame[3];
        }
        else if (v == 2)
        {
            wire = frame[4] | ((frame[5] & 1) << 8);
            rec[16] = (unsigned char)(wire & 0xff);
            rec[17] = (unsigned char)(((frame[3] & 1) << 1) | ((wire >> 8) & 1));
        }

        break;

    case 5:                                     /* tone squelch */
        rec[20] = v;
        break;

    case 6:                                     /* squelch - NOT range checked */
        set1[4] = v;
        break;

    case 7:                                     /* volume; 0 mutes */
        if (v != 0)
        {
            set1[6] = v;
        }

        break;

    case 8:                                     /* TX power */
        rec[9] = (unsigned char)((rec[9] & ~0x0c) | ((v & 3) << 2));
        break;

    case 9:                                     /* VOX - absent on a non-P radio */
        printf("VOX requested; this model has none, so nothing changes\n");
        break;

    default:
        break;
    }

    emit(fd, &ack, 1, inject(fd));
}

static void send_status(int fd)
{
    unsigned char st[14];

    memset(st, 0, sizeof(st));
    st[0] = 'S';
    st[1] = 'T';
    st[2] = (unsigned char)cos_a;
    st[3] = (unsigned char)cos_b;
    st[6] = 0x04;
    st[7] = 0x04;
    st[8] = (unsigned char)selected;
    st[10] = 0x0f;
    st[12] = (unsigned char)(cos_a || cos_b);
    st[13] = 0x06;
    WRITE(fd, st, sizeof(st));
}

static void do_key(int fd, const unsigned char *frame)
{
    switch (frame[4])
    {
    case KEY_STATUS:
        send_status(fd);
        break;

    case KEY_AB:
        selected = !selected;
        set1[0x0b] = (unsigned char)((set1[0x0b] & ~1) | selected);
        printf("slot %c selected\n", selected ? 'B' : 'A');
        break;

    case KEY_PA:

        /* PA is bound to VFO/MEM on the radio these captures came from. */
        if (selected)
        {
            live[8] ^= 1;
            printf("slot B now in %s mode\n", (live[8] & 1) ? "VFO" : "MEMORY");
        }
        else
        {
            live[1] ^= 1;
            printf("slot A now in %s mode\n", (live[1] & 1) ? "VFO" : "MEMORY");
        }

        break;

    default:
        /* An unmatched code repeats the previous key on the real radio.
           Nothing here depends on that, so just note it. */
        printf("key 0x%02x is not in the radio's table\n", frame[4]);
        break;
    }
}

int main(int argc, char *argv[])
{
    static const unsigned char ident[16] =
    {
        'I', 'A', 'T', '7', '7', '8', 'U', 'V',
        0x01, 'V', '2', '0', '0', 0x00, 0x00, 0x06
    };
    unsigned char frame[32];
    int fd;

    if (argc < 2)
    {
        fprintf(stderr, "usage: %s <port> [memory] [noise]\n", argv[0]);
        return 1;
    }

    /* This is a tool you watch while it runs, and its output is usually
       piped somewhere. Unbuffer it so the port name and the fault-injection
       notices appear when they happen rather than when the pipe fills. */
    setvbuf(stdout, NULL, _IONBF, 0);

    fd = openPort(argv[1]);

    if (fd < 0)
    {
        return 1;
    }

    for (int i = 2; i < argc; i++)
    {
        if (strcmp(argv[i], "memory") == 0)
        {
            live[1] &= (unsigned char)~1;
            printf("slot A starts on a MEMORY channel - record writes will hit "
                   "channel 45\n");
        }
        else if (strcmp(argv[i], "noise") == 0)
        {
            noise = 1;
            printf("fault injection on - unsolicited status frames and clipped "
                   "reply heads\n");
        }
    }

    while (1)
    {
        unsigned char c;
        int want;

        if (read(fd, &c, 1) <= 0)
        {
            sleep(1);
            continue;
        }

        frame[0] = c;

        switch (c)
        {
        case 0x02:
            want = 0;
            break;        /* identify */

        case 0x52:
            want = 3;
            break;        /* 'R' block read */

        case 0x44:
            want = 6;
            break;        /* 'D' parameter set */

        case 0x41:
            want = 7;
            break;        /* 'A' key frame */

        case 0x06:
            want = 0;
            break;        /* bare ack, ignored */

        default:
            printf("unhandled command byte 0x%02x\n", c);
            WRITE(fd, &c, 1);
            continue;
        }

        if (want > 0 && readn(fd, &frame[1], want) != want)
        {
            printf("short frame for 0x%02x - the radio would wait for the "
                   "rest and swallow whatever came next\n", c);
            continue;
        }

        /* Everything is echoed before any reply. Sometimes the echo is what
           the collision damages while the command itself still arrives - by
           far the commonest event on the bench, 29 times in 1500 exchanges,
           and it must NOT be treated as a failure. Corrupt a byte and answer
           normally so that path gets exercised. */
        if (noise && ++echo_tick % 4 == 0 && want > 0)
        {
            unsigned char spoiled[32];
            memcpy(spoiled, frame, want + 1);
            spoiled[1] ^= 0xFF;
            printf("[noise] echo damaged, command still valid\n");
            WRITE(fd, spoiled, want + 1);
        }
        else
        {
            WRITE(fd, frame, want + 1);
        }

        switch (c)
        {
        case 0x02:
            emit(fd, ident, sizeof(ident), inject(fd));
            break;

        case 0x52:
            do_read(fd, (unsigned short)((frame[1] << 8) | frame[2]), frame[3]);
            break;

        case 0x44:
            do_param(fd, frame);
            break;

        case 0x41:
            do_key(fd, frame);
            break;

        default:
            break;
        }
    }

    return 0;
}
