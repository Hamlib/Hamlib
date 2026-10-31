/*
 *  Hamlib tests - "usb:" ports (src/usb_serial.c) against a fake adapter
 *  Copyright (c) 2026 by Rafael Diniz
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*
 * No hardware: src/usb_serial.c is compiled here with its usbfs, sysfs and
 * thread calls going to a fake adapter.  The pseudo-terminals and the pump
 * threads are the real ones.  The fake's bulk IN behaves as usbfs does: a
 * transfer that times out before it fills, with no short packet, returns
 * nothing of what it received.
 */

#include "hamlib/config.h"

#if defined(__linux__) && defined(HAVE_LINUX_USBDEVICE_FS_H)

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <linux/usbdevice_fs.h>

static int fake_ioctl(int fd, unsigned long req, void *arg);
static ssize_t fake_read(int fd, void *buf, size_t len);
static off_t fake_lseek(int fd, off_t off, int whence);
static int fake_open(const char *node, int flags);
static int fake_close(int fd);
static int fake_pthread_create(pthread_t *t, const pthread_attr_t *a,
                               void *(*fn)(void *), void *arg);
static char sysfs_root[256];

#define USBSER_IOCTL fake_ioctl
#define USBSER_READ fake_read
#define USBSER_LSEEK fake_lseek
#define USBSER_OPEN fake_open
#define USBSER_CLOSE fake_close
#define USBSER_SYSFS sysfs_root
#define USBSER_PTHREAD_CREATE fake_pthread_create

#include "../src/usb_serial.c"
#include "misc.h"
#include "serial.h"

/* ---- the fake adapter ---- */

#define MPS 64
#define RXQ 4096

static struct
{
    pthread_mutex_t lock;
    uint8_t desc[8192];
    size_t desc_len;
    char node[64];                      /* the usbfs node it answers at */
    int claim_fail;                     /* interface whose claim fails, or -1 */
    int thread_fail;                    /* fail the Nth thread start (1, 2), or 0 */
    int threads;
    volatile int gone;                  /* unplugged: everything ENODEV */
    int slow_out_ms;                    /* each bulk OUT takes this long */
    int ctl_fail;                       /* control transfers fail */
    uint8_t rx[RXQ][MPS];
    int rx_len[RXQ];
    int rx_head, rx_tail;
    uint8_t tx[1 << 16];
    size_t tx_len;
    int claimed[8];
    int reconnected[8];
    int opened, closed;
    struct { uint8_t type, req; uint16_t value, index; uint8_t data[8]; } ctl[64];
    int nctl;
} F;

static void fake_reset(void)
{
    pthread_mutex_destroy(&F.lock);
    memset(&F, 0, sizeof(F));
    pthread_mutex_init(&F.lock, NULL);
    F.claim_fail = -1;
}

static void rx_queue(const uint8_t *p, int len)
{
    pthread_mutex_lock(&F.lock);
    memcpy(F.rx[F.rx_tail % RXQ], p, (size_t)len);
    F.rx_len[F.rx_tail % RXQ] = len;
    F.rx_tail++;
    pthread_mutex_unlock(&F.lock);
}

static int fake_ioctl(int fd, unsigned long req, void *arg)
{
    (void)fd;

    if (F.gone)
    {
        errno = ENODEV;
        return -1;
    }

    if (req == USBDEVFS_CONTROL)
    {
        struct usbdevfs_ctrltransfer *c = arg;

        if (F.ctl_fail)
        {
            errno = EPIPE;
            return -1;
        }

        pthread_mutex_lock(&F.lock);

        if (F.nctl < 64)
        {
            F.ctl[F.nctl].type = c->bRequestType;
            F.ctl[F.nctl].req = c->bRequest;
            F.ctl[F.nctl].value = c->wValue;
            F.ctl[F.nctl].index = c->wIndex;

            if (!(c->bRequestType & 0x80) && c->data)
            {
                memcpy(F.ctl[F.nctl].data, c->data, c->wLength < 8 ? c->wLength : 8);
            }

            F.nctl++;
        }

        pthread_mutex_unlock(&F.lock);

        if (c->bRequestType & 0x80)
        {
            memset(c->data, 0, c->wLength);

            if (c->wLength) { ((uint8_t *)c->data)[0] = 0x10; }    /* CP210x: CTS */
        }

        return c->wLength;
    }

    if (req == USBDEVFS_BULK)
    {
        struct usbdevfs_bulktransfer *b = arg;
        unsigned int got = 0;
        int ms;

        if (!(b->ep & 0x80))
        {
            if (F.slow_out_ms) { usleep((useconds_t)F.slow_out_ms * 1000); }

            pthread_mutex_lock(&F.lock);

            if (F.tx_len + b->len <= sizeof(F.tx))
            {
                memcpy(F.tx + F.tx_len, b->data, b->len);
                F.tx_len += b->len;
            }

            pthread_mutex_unlock(&F.lock);
            return (int)b->len;
        }

        /* IN: packets until the buffer is full or one is short */
        for (ms = 0; ms <= (int)b->timeout; ms++)
        {
            pthread_mutex_lock(&F.lock);

            while (F.rx_head < F.rx_tail && got + (unsigned)F.rx_len[F.rx_head % RXQ] <= b->len)
            {
                int n = F.rx_len[F.rx_head % RXQ];

                memcpy((uint8_t *)b->data + got, F.rx[F.rx_head % RXQ], (size_t)n);
                got += (unsigned)n;
                F.rx_head++;

                if (n < MPS || got == b->len)
                {
                    pthread_mutex_unlock(&F.lock);
                    return (int)got;
                }
            }

            pthread_mutex_unlock(&F.lock);

            if (F.gone)
            {
                errno = ENODEV;
                return -1;
            }

            usleep(1000);
        }

        errno = ETIMEDOUT;              /* and what arrived is lost, as in usbfs */
        return -1;
    }

    if (req == USBDEVFS_DISCONNECT_CLAIM || req == USBDEVFS_CLAIMINTERFACE)
    {
        int ifno = req == USBDEVFS_CLAIMINTERFACE ? (int) * (unsigned int *)arg
                   : (int)((struct usbdevfs_disconnect_claim *)arg)->interface;

        if (ifno == F.claim_fail)
        {
            errno = EBUSY;
            return -1;
        }

        F.claimed[ifno & 7] = 1;
        return 0;
    }

    if (req == USBDEVFS_RELEASEINTERFACE)
    {
        F.claimed[*(unsigned int *)arg & 7] = 0;
        return 0;
    }

    if (req == USBDEVFS_IOCTL)
    {
        F.reconnected[((struct usbdevfs_ioctl *)arg)->ifno & 7] = 1;
        return 0;
    }

    errno = ENOTTY;
    return -1;
}

static ssize_t fake_read(int fd, void *buf, size_t len)
{
    (void)fd;

    if (len > F.desc_len) { len = F.desc_len; }

    memcpy(buf, F.desc, len);
    return (ssize_t)len;
}

static off_t fake_lseek(int fd, off_t off, int whence)
{
    (void)fd; (void)whence;
    return off;
}

static int fake_open(const char *node, int flags)
{
    (void)flags;

    if (strcmp(node, F.node))
    {
        errno = ENOENT;
        return -1;
    }

    F.opened++;
    return open("/dev/null", O_RDWR | O_CLOEXEC);
}

static int fake_close(int fd)
{
    F.closed++;
    return close(fd);
}

static int fake_pthread_create(pthread_t *t, const pthread_attr_t *a,
                               void *(*fn)(void *), void *arg)
{
    if (++F.threads == F.thread_fail)
    {
        return EAGAIN;
    }

    return pthread_create(t, a, fn, arg);
}

/* ---- descriptors ---- */

static size_t put(uint8_t *d, size_t at, const uint8_t *p, size_t n)
{
    memcpy(d + at, p, n);
    return at + n;
}

static size_t device(uint8_t *d, unsigned vid, unsigned pid, uint8_t cls)
{
    const uint8_t dev[18] = { 18, 1, 0x00, 0x02, cls, 0, 0, 64,
                              (uint8_t)vid, (uint8_t)(vid >> 8), (uint8_t)pid, (uint8_t)(pid >> 8),
                              0x00, 0x01, 1, 2, 3, 1
                            };
    const uint8_t cfg[9] = { 9, 2, 0, 0, 2, 1, 0, 0x80, 50 };
    return put(d, put(d, 0, dev, 18), cfg, 9);
}

static size_t interface(uint8_t *d, size_t at, uint8_t ifno, uint8_t neps,
                        uint8_t cls, uint8_t sub)
{
    const uint8_t i[9] = { 9, 4, ifno, 0, neps, cls, sub, 0, 0 };
    return put(d, at, i, 9);
}

static size_t endpoint(uint8_t *d, size_t at, uint8_t addr, uint8_t type)
{
    const uint8_t e[7] = { 7, 5, addr, type, MPS, 0, 0 };
    return put(d, at, e, 7);
}

/* a CP2105: two vendor interfaces, bulk 0x81/0x01 and 0x82/0x02 */
static void cp2105(void)
{
    size_t n = device(F.desc, 0x10c4, 0xea70, 0);

    n = interface(F.desc, n, 0, 2, 0xff, 0);
    n = endpoint(F.desc, n, 0x81, 2);
    n = endpoint(F.desc, n, 0x01, 2);
    n = interface(F.desc, n, 1, 2, 0xff, 0);
    n = endpoint(F.desc, n, 0x82, 2);
    n = endpoint(F.desc, n, 0x02, 2);
    F.desc_len = n;
}

/* an FT232R */
static void ft232r(void)
{
    size_t n = device(F.desc, 0x0403, 0x6001, 0);

    F.desc[12] = 0x00;                  /* bcdDevice 0x0600 */
    F.desc[13] = 0x06;
    n = interface(F.desc, n, 0, 2, 0xff, 0xff);
    n = endpoint(F.desc, n, 0x81, 2);
    n = endpoint(F.desc, n, 0x02, 2);
    F.desc_len = n;
}

/* a CDC-ACM device of no listed vendor: control interface 0 (with its
 * notification endpoint and a Union naming interface 1), data interface 1 */
static void cdc_acm_dev(void)
{
    const uint8_t uni[5] = { 5, 0x24, 0x06, 0, 1 };
    size_t n = device(F.desc, 0x1209, 0x4321, 2);

    n = interface(F.desc, n, 0, 1, 2, 2);
    n = put(F.desc, n, uni, 5);
    n = endpoint(F.desc, n, 0x83, 3);
    n = interface(F.desc, n, 1, 2, 0x0a, 0);
    n = endpoint(F.desc, n, 0x81, 2);
    n = endpoint(F.desc, n, 0x01, 2);
    F.desc_len = n;
}

/* sysfs for one device, 1-1: vid:pid, serial ABC123, at /dev/bus/usb/001/005 */
static void sysfs(unsigned vid, unsigned pid)
{
    char p[512];
    const char *files[][2] = { { "idVendor", NULL }, { "idProduct", NULL },
        { "serial", "ABC123" }, { "busnum", "1" }, { "devnum", "5" }
    };
    char v[8], q[8];
    size_t i;

    snprintf(v, sizeof(v), "%04x", vid);
    snprintf(q, sizeof(q), "%04x", pid);
    files[0][1] = v;
    files[1][1] = q;
    snprintf(p, sizeof(p), "%s/1-1", sysfs_root);
    mkdir(p, 0755);

    for (i = 0; i < 5; i++)
    {
        FILE *f;

        snprintf(p, sizeof(p), "%s/1-1/%s", sysfs_root, files[i][0]);
        f = fopen(p, "w");

        if (f)
        {
            fprintf(f, "%s\n", files[i][1]);
            fclose(f);
        }
    }

    snprintf(F.node, sizeof(F.node), "/dev/bus/usb/001/005");
}

/* ---- checks ---- */

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; \
        fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

static void port(hamlib_port_t *rp, const char *path)
{
    memset(rp, 0, sizeof(*rp));
    snprintf(rp->pathname, sizeof(rp->pathname), "%s", path);
    rp->parm.serial.rate = 38400;
    rp->parm.serial.data_bits = 8;
    rp->parm.serial.stop_bits = 1;
    rp->parm.serial.parity = RIG_PARITY_NONE;
}

static int open_port(const char *path)
{
    hamlib_port_t rp;

    port(&rp, path);
    return usb_serial_open(&rp);
}

/* a test that hangs fails instead */
static const char *running;

static void watchdog(int sig)
{
    (void)sig;
    fprintf(stderr, "FAIL %s: hung\n", running);
    _exit(1);
}

#define RUN(t) do { running = #t; alarm(20); t(); alarm(0); } while (0)

static double now(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

/* read exactly n bytes from fd within secs */
static int read_all(int fd, uint8_t *b, size_t n, double secs)
{
    size_t got = 0;
    double end = now() + secs;

    while (got < n && now() < end)
    {
        ssize_t r = read(fd, b + got, n - got);

        if (r > 0) { got += (size_t)r; }
        else { usleep(1000); }
    }

    return (int)got;
}

static int slots_free(void)
{
    int i, n = 0;

    for (i = 0; i < USBSER_MAX; i++) { n += !usbser[i].used; }

    return n;
}

static void test_path_forms(void)
{
    const char *ok[] = { "usb:", "usb:10c4:ea70", "usb:10c4:ea70:ABC123",
                         "usb:/dev/bus/usb/001/005"
                       };
    size_t i;
    int fd, raw;
    char path[64];

    for (i = 0; i < sizeof(ok) / sizeof(ok[0]); i++)
    {
        fake_reset();
        cp2105();
        sysfs(0x10c4, 0xea70);
        fd = open_port(ok[i]);
        CHECK(fd >= 0, "%s: open = %d", ok[i], fd);
        CHECK(F.claimed[0] && !F.claimed[1], "%s: interface 0 not the one claimed", ok[i]);
        usb_serial_close(fd);
        CHECK(!F.claimed[0] && F.reconnected[0], "%s: interface 0 not given back", ok[i]);
        CHECK(F.opened == F.closed, "%s: usbfs node left open", ok[i]);
    }

    fake_reset();
    cp2105();
    sysfs(0x10c4, 0xea70);
    CHECK(open_port("usb:10c4:ea70:WRONG") < 0, "a wrong serial number opened");
    CHECK(open_port("usb:0403:6001") < 0, "an absent vid:pid opened");
    CHECK(open_port("usb:xyz") < 0, "a malformed path opened");

    /* @1: the second UART */
    fd = open_port("usb:10c4:ea70@1");
    CHECK(fd >= 0 && F.claimed[1] && !F.claimed[0], "@1 did not take interface 1");
    usb_serial_close(fd);

    /* usbfd:N: the caller's descriptor, which close leaves open */
    raw = open("/dev/null", O_RDWR);
    snprintf(path, sizeof(path), "usbfd:%d", raw);
    fd = open_port(path);
    CHECK(fd >= 0, "%s: open = %d", path, fd);
    usb_serial_close(fd);
    CHECK(fcntl(raw, F_GETFD) != -1, "close closed the caller's usbfd");
    close(raw);
}

/* the CAT port and a PTT port on the same chip at once */
static void test_two_interfaces(void)
{
    int cat, ptt;

    fake_reset();
    cp2105();
    sysfs(0x10c4, 0xea70);
    cat = open_port("usb:10c4:ea70@0");
    ptt = open_port("usb:10c4:ea70@1");
    CHECK(cat >= 0 && ptt >= 0 && F.claimed[0] && F.claimed[1], "both interfaces not open");
    CHECK(usb_serial_set_rts(ptt, 1) == RIG_OK, "RTS on the PTT port");
    usb_serial_close(ptt);
    CHECK(F.claimed[0] && !F.claimed[1], "closing the PTT port released the CAT one");
    usb_serial_close(cat);
    CHECK(!F.claimed[0], "the CAT port not released");
}

static void test_failed_opens(void)
{
    int fd, t;

    /* a failed open leaves stdin alone and frees what it took */
    if (fcntl(0, F_GETFD) == -1) { open("/dev/null", O_RDONLY); }

    fake_reset();
    cp2105();
    sysfs(0x10c4, 0xea70);
    F.claim_fail = 0;
    fd = open_port("usb:10c4:ea70");
    CHECK(fd < 0, "an unclaimable interface opened");
    CHECK(F.opened == F.closed, "claim failure: usbfs node left open");
    CHECK(slots_free() == USBSER_MAX, "claim failure: slot not freed");
    CHECK(fcntl(0, F_GETFD) != -1, "claim failure closed stdin");

    for (t = 1; t <= 2; t++)
    {
        fake_reset();
        cp2105();
        sysfs(0x10c4, 0xea70);
        F.thread_fail = t;
        fd = open_port("usb:10c4:ea70");
        CHECK(fd < 0, "thread %d failing: opened", t);
        CHECK(F.opened == F.closed && !F.claimed[0], "thread %d failing: not released", t);
        CHECK(slots_free() == USBSER_MAX, "thread %d failing: slot not freed", t);
        CHECK(fcntl(0, F_GETFD) != -1, "thread %d failing: stdin closed", t);
    }

    /* truncated descriptors: the device's, then mid-endpoint */
    for (t = 0; t < 2; t++)
    {
        fake_reset();
        cp2105();
        sysfs(0x10c4, 0xea70);
        F.desc_len = t == 0 ? 10 : 18 + 9 + 9 + 4;
        fd = open_port("usb:10c4:ea70");
        CHECK(fd < 0, "truncated descriptors (%d): opened", t);
        CHECK(F.opened == F.closed && slots_free() == USBSER_MAX,
              "truncated descriptors (%d): not released", t);
    }

    /* descriptors that lie about their length: one too short to be an
     * interface, and the last one cut off in the middle */
    {
        const uint8_t bad[] = { 3, 4, 0 };

        fake_reset();
        cp2105();
        sysfs(0x10c4, 0xea70);
        F.desc_len = put(F.desc, 18 + 9, bad, 3);
        F.desc_len = put(F.desc, F.desc_len, (const uint8_t[]) { 9, 4, 0, 2, 0xff, 0 }, 6);
        CHECK(open_port("usb:10c4:ea70") < 0, "malformed descriptors: opened");
        CHECK(slots_free() == USBSER_MAX, "malformed descriptors: not released");
    }

    /* descriptors filling usbfs' read, the last one cut off at its end:
     * nothing may be read past it */
    {
        const uint8_t pad[2] = { 255, 0x30 };   /* a vendor descriptor */
        size_t n;

        fake_reset();
        cp2105();
        sysfs(0x10c4, 0xea70);
        n = 18 + 9;

        while (n + 255 <= 4094)
        {
            put(F.desc, n, pad, 2);
            memset(F.desc + n + 2, 0, 253);
            n += 255;
        }

        while (n < 4094) { F.desc[n++] = 2; F.desc[n++] = 0x30; }   /* 2-byte fillers */

        F.desc[n++] = 9;                        /* an interface, 2 bytes of it */
        F.desc[n++] = 4;
        F.desc_len = n;                         /* = 4096, the read's size */
        CHECK(F.desc_len == 4096, "test setup: %zu bytes", F.desc_len);
        CHECK(open_port("usb:10c4:ea70") < 0, "descriptors cut at the read's end: opened");
    }

    /* a framing the chip cannot do: refused, not opened as 8N1 */
    {
        hamlib_port_t rp;

        fake_reset();
        cp2105();
        sysfs(0x10c4, 0xea70);
        port(&rp, "usb:10c4:ea70");
        rp.parm.serial.data_bits = 9;
        CHECK(usb_serial_open(&rp) < 0, "9 data bits opened");
        port(&rp, "usb:10c4:ea70");
        rp.parm.serial.stop_bits = 3;
        CHECK(usb_serial_open(&rp) < 0, "3 stop bits opened");
        CHECK(slots_free() == USBSER_MAX, "framing refusal: slot not freed");
    }
}

static void test_no_lost_bytes(void)
{
    uint8_t p[MPS], b[3 * MPS], out[3000];
    int fd, i;
    size_t sent = 0;

    fake_reset();
    cp2105();
    sysfs(0x10c4, 0xea70);
    fd = open_port("usb:10c4:ea70");

    /* full packets, then nothing: no short packet ever ends the transfer */
    for (i = 0; i < 3; i++)
    {
        memset(p, 'a' + i, sizeof(p));
        rx_queue(p, MPS);
    }

    CHECK(read_all(fd, b, sizeof(b), 2) == (int)sizeof(b),
          "full packets then a timeout: bytes lost");
    CHECK(b[0] == 'a' && b[MPS] == 'b' && b[2 * MPS] == 'c', "packets out of order");

    /* output queued just before close reaches the adapter: the adapter is
     * slow, so close comes while most of it still waits */
    F.slow_out_ms = 20;
    memset(out, 'x', sizeof(out));

    while (sent < sizeof(out))
    {
        ssize_t w = write(fd, out + sent, sizeof(out) - sent);

        if (w > 0) { sent += (size_t)w; }
        else { usleep(1000); }
    }

    usb_serial_close(fd);
    CHECK(F.tx_len == sizeof(out), "close dropped queued output: %zu of %zu sent",
          F.tx_len, sizeof(out));
}

static void test_shutdown_and_unplug(void)
{
    uint8_t p[MPS], b[16];
    int fd, i, failed = 0;
    double t;

    /* nobody reads: close still finishes */
    fake_reset();
    cp2105();
    sysfs(0x10c4, 0xea70);
    fd = open_port("usb:10c4:ea70");
    memset(p, 'z', sizeof(p));

    for (i = 0; i < RXQ - 1; i++) { rx_queue(p, MPS); }

    usleep(300 * 1000);                 /* the pseudo-terminal fills */
    t = now();
    usb_serial_close(fd);
    CHECK(now() - t < 2.0, "close with a full receive queue took %.1f s", now() - t);

    /* unplugged: the port fails instead of writes going nowhere */
    fake_reset();
    cp2105();
    sysfs(0x10c4, 0xea70);
    fd = open_port("usb:10c4:ea70");
    F.gone = 1;
    t = now();

    while (now() - t < 2.0 && !failed)
    {
        failed = write(fd, "x", 1) < 0 || (read(fd, b, sizeof(b)) < 0 && errno != EAGAIN);
        usleep(10 * 1000);
    }

    CHECK(failed, "after unplug, the port still looked alive");
    usb_serial_close(fd);
    CHECK(slots_free() == USBSER_MAX, "unplugged port: slot not freed");
}

/* what the rest of Hamlib does with a "usb:" port */
static void test_hamlib_paths(void)
{
    hamlib_port_t cat, ptt;
    char host[256], portstr[6];
    struct termios t;
    int st = -1;

    /* not a network address */
    CHECK(parse_hoststr((char *)"usb:", 5, host, portstr) != RIG_OK, "usb: taken for a host");
    CHECK(parse_hoststr((char *)"usbfd:42", 9, host, portstr) != RIG_OK, "usbfd:42 taken for a host");
    CHECK(parse_hoststr((char *)"usb:10c4:ea70", 14, host, portstr) != RIG_OK,
          "usb:10c4:ea70 taken for a host");

    /* CAT on @0, RTS PTT through ser_open() on @1 */
    fake_reset();
    cp2105();
    sysfs(0x10c4, 0xea70);
    port(&cat, "usb:10c4:ea70@0");
    cat.parm.serial.handshake = RIG_HANDSHAKE_XONXOFF;
    cat.fd = usb_serial_open(&cat);
    CHECK(cat.fd >= 0, "CAT open = %d", cat.fd);
    CHECK(serial_setup(&cat) == RIG_OK, "serial_setup on the CAT port");
    CHECK(tcgetattr(cat.fd, &t) == 0 && (t.c_iflag & IXON), "XON/XOFF not set on the port");
    port(&ptt, "usb:10c4:ea70@1");
    CHECK(ser_open(&ptt) >= 0 && F.claimed[1], "ser_open() did not open the PTT port");
    CHECK(ser_set_rts(&ptt, 1) == RIG_OK, "RTS on the PTT port");

    /* a failed RTS is not reported as set, nor sent again with DTR */
    F.ctl_fail = 1;
    CHECK(usb_serial_set_rts(ptt.fd, 0) != RIG_OK, "RTS off with the adapter failing");
    CHECK(usb_serial_get_rts(ptt.fd, &st) == RIG_OK && st == 1, "a failed RTS off changed the state");
    F.ctl_fail = 0;
    CHECK(usb_serial_set_rts(ptt.fd, 0) == RIG_OK, "RTS off");
    F.ctl_fail = 1;
    CHECK(usb_serial_set_rts(ptt.fd, 1) != RIG_OK, "RTS on with the adapter failing");
    CHECK(usb_serial_get_rts(ptt.fd, &st) == RIG_OK && st == 0, "a failed RTS on reported as on");
    F.ctl_fail = 0;
    F.nctl = 0;
    CHECK(usb_serial_set_dtr(ptt.fd, 1) == RIG_OK && F.nctl == 1
          && !(F.ctl[0].value & 0x02), "a DTR change keyed RTS after a failed RTS");
    ser_close(&ptt);
    usb_serial_close(cat.fd);
    CHECK(!F.claimed[0] && !F.claimed[1], "ports not released");
}

/* an FTDI's two status bytes per packet do not reach the caller */
static void test_ftdi(void)
{
    const uint8_t pkt[6] = { 0x11, 0x60, 'A', 'B', 'C', 'D' };
    const uint8_t empty[2] = { 0x11, 0x60 };
    uint8_t b[4];
    int fd;

    fake_reset();
    ft232r();
    sysfs(0x0403, 0x6001);
    fd = open_port("usb:0403:6001");
    CHECK(fd >= 0, "FTDI open = %d", fd);
    rx_queue(empty, 2);
    rx_queue(pkt, 6);
    CHECK(read_all(fd, b, 4, 2) == 4 && !memcmp(b, "ABCD", 4), "FTDI status bytes not removed");
    usb_serial_close(fd);
}

/* CDC-ACM: found by class, its requests go to the control interface */
static void test_cdc_acm(void)
{
    int fd, i, coded = 0;

    fake_reset();
    cdc_acm_dev();
    sysfs(0x1209, 0x4321);
    CHECK(open_port("usb:") < 0, "\"usb:\" picked an unlisted CDC device");
    fd = open_port("usb:1209:4321");
    CHECK(fd >= 0, "CDC-ACM open = %d", fd);
    CHECK(F.claimed[0] && F.claimed[1], "control and data interfaces not both claimed");

    for (i = 0; i < F.nctl; i++)
    {
        if (F.ctl[i].req == 0x20 && F.ctl[i].index == 0 && F.ctl[i].data[0] == (38400 & 0xff))
        {
            coded = 1;
        }
    }

    CHECK(coded, "SET_LINE_CODING not sent to the control interface");
    usb_serial_close(fd);
    CHECK(!F.claimed[0] && !F.claimed[1], "CDC interfaces not released");
}

int main(void)
{
    char tmpl[] = "/tmp/testusbserial.XXXXXX";

    if (!mkdtemp(tmpl))
    {
        perror("mkdtemp");
        return 1;
    }

    snprintf(sysfs_root, sizeof(sysfs_root), "%s", tmpl);
    pthread_mutex_init(&F.lock, NULL);
    rig_set_debug(RIG_DEBUG_NONE);

    signal(SIGALRM, watchdog);
    RUN(test_path_forms);
    RUN(test_two_interfaces);
    RUN(test_failed_opens);
    RUN(test_no_lost_bytes);
    RUN(test_shutdown_and_unplug);
    RUN(test_hamlib_paths);
    RUN(test_ftdi);
    RUN(test_cdc_acm);

    {
        char cmd[300];

        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", tmpl);

        if (system(cmd) != 0) { /* only a leftover directory */ }
    }

    printf("testusbserial: %s\n", failures ? "FAILED" : "ok");
    return failures ? 1 : 0;
}

#else

#include <stdio.h>

int main(void)
{
    printf("testusbserial: no Linux usbfs, skipped\n");
    return 77;
}

#endif
