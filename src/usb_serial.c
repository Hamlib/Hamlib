/*
 *  Hamlib Interface - USB-serial adapters driven from user space
 *  Copyright (c) 2026 by Rafael Diniz
 *
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
 * See usb_serial.h.  The chip protocols follow the Linux kernel drivers
 * (drivers/usb/serial/cp210x.c) and Silicon Labs AN571.
 */

#include "hamlib/config.h"

#include <string.h>
#include <errno.h>

#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "usb_serial.h"

#if defined(__linux__) && defined(HAVE_LINUX_USBDEVICE_FS_H)

#include <dirent.h>
#include <fcntl.h>
#include <linux/usbdevice_fs.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define USBSER_MAX 4                    /* adapters open at once */

struct usbser;

typedef struct
{
    const char *name;
    int (*setup)(struct usbser *u, const hamlib_port_t *rp);
    int (*set_lines)(struct usbser *u);
    int (*get_status)(struct usbser *u, int *cts, int *dsr, int *car);
    void (*fini)(struct usbser *u);
} usbser_chip_t;

struct usbser
{
    int used;
    int usbfd;                          /* usbfs */
    int own_usbfd;                      /* we opened it (not "usbfd:N") */
    int ifno;
    unsigned int ep_in, ep_out;
    const usbser_chip_t *chip;
    int master, slave;                  /* the pseudo-terminal; Hamlib has slave */
    int rts, dtr;
    volatile int run;
    pthread_t rx, tx;
};

static struct usbser usbser[USBSER_MAX];
static pthread_mutex_t usbser_lock = PTHREAD_MUTEX_INITIALIZER;

static struct usbser *by_fd(int fd)
{
    int i;

    if (fd < 0)
    {
        return NULL;
    }

    for (i = 0; i < USBSER_MAX; i++)
    {
        if (usbser[i].used && usbser[i].slave == fd)
        {
            return &usbser[i];
        }
    }

    return NULL;
}

/* ---- usbfs ---- */

static int ctrl(struct usbser *u, uint8_t type, uint8_t req, uint16_t value,
                void *data, uint16_t len)
{
    struct usbdevfs_ctrltransfer c;
    int r;

    memset(&c, 0, sizeof(c));
    c.bRequestType = type;
    c.bRequest = req;
    c.wValue = value;
    c.wIndex = (uint16_t)u->ifno;
    c.wLength = len;
    c.timeout = 1000;
    c.data = data;
    r = ioctl(u->usbfd, USBDEVFS_CONTROL, &c);

    if (r < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: request 0x%02x/0x%02x failed: %s\n", __func__,
                  type, req, strerror(errno));
    }

    return r;
}

static int bulk(struct usbser *u, unsigned int ep, void *buf, size_t len,
                int timeout_ms)
{
    struct usbdevfs_bulktransfer b;

    b.ep = ep;
    b.len = (unsigned int)len;
    b.timeout = (unsigned int)timeout_ms;
    b.data = buf;
    return ioctl(u->usbfd, USBDEVFS_BULK, &b);
}

/* ---- CP210x (AN571) ---- */

#define CP_OUT          0x41            /* host to device, vendor, interface */
#define CP_IN           0xc1
#define CP_IFC_ENABLE   0x00
#define CP_SET_LINE_CTL 0x03
#define CP_SET_MHS      0x07
#define CP_GET_MDMSTS   0x08
#define CP_PURGE        0x12
#define CP_SET_BAUDRATE 0x1e

static int cp_setup(struct usbser *u, const hamlib_port_t *rp)
{
    int rate = rp->parm.serial.rate > 0 ? rp->parm.serial.rate : 9600;
    uint8_t b[4];
    uint16_t line = 0;

    switch (rp->parm.serial.stop_bits)
    {
    case 2: line |= 2; break;
    default: break;                     /* 1 stop bit */
    }

    switch (rp->parm.serial.parity)
    {
    case RIG_PARITY_ODD:   line |= 1 << 4; break;
    case RIG_PARITY_EVEN:  line |= 2 << 4; break;
    case RIG_PARITY_MARK:  line |= 3 << 4; break;
    case RIG_PARITY_SPACE: line |= 4 << 4; break;
    default: break;
    }

    line |= (uint16_t)((rp->parm.serial.data_bits >= 5
                        && rp->parm.serial.data_bits <= 8
                        ? rp->parm.serial.data_bits : 8) << 8);

    if (rp->parm.serial.handshake == RIG_HANDSHAKE_HARDWARE)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: CP210x hardware handshake not implemented, using none\n", __func__);
    }

    b[0] = (uint8_t)rate;
    b[1] = (uint8_t)(rate >> 8);
    b[2] = (uint8_t)(rate >> 16);
    b[3] = (uint8_t)(rate >> 24);

    if (ctrl(u, CP_OUT, CP_IFC_ENABLE, 1, NULL, 0) < 0
            || ctrl(u, CP_OUT, CP_SET_BAUDRATE, 0, b, 4) < 0
            || ctrl(u, CP_OUT, CP_SET_LINE_CTL, line, NULL, 0) < 0)
    {
        return -RIG_EIO;
    }

    ctrl(u, CP_OUT, CP_PURGE, 0x000f, NULL, 0);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: %d baud, line control 0x%04x\n", __func__,
              rate, line);
    return RIG_OK;
}

static int cp_set_lines(struct usbser *u)
{
    /* bit 0 DTR, bit 1 RTS; bits 8 and 9 say both are being set */
    uint16_t v = (uint16_t)(0x0300 | (u->dtr ? 0x01 : 0) | (u->rts ? 0x02 : 0));

    return ctrl(u, CP_OUT, CP_SET_MHS, v, NULL, 0) < 0 ? -RIG_EIO : RIG_OK;
}

static int cp_get_status(struct usbser *u, int *cts, int *dsr, int *car)
{
    uint8_t s = 0;

    if (ctrl(u, CP_IN, CP_GET_MDMSTS, 0, &s, 1) < 1)
    {
        return -RIG_EIO;
    }

    if (cts) { *cts = (s & 0x10) != 0; }
    if (dsr) { *dsr = (s & 0x20) != 0; }
    if (car) { *car = (s & 0x80) != 0; }

    return RIG_OK;
}

static void cp_fini(struct usbser *u)
{
    ctrl(u, CP_OUT, CP_IFC_ENABLE, 0, NULL, 0);
}

static const usbser_chip_t cp210x = { "CP210x", cp_setup, cp_set_lines, cp_get_status, cp_fini };

static const struct
{
    uint16_t vid, pid;
    const usbser_chip_t *chip;
} usbser_ids[] =
{
    { 0x10c4, 0xea60, &cp210x },        /* CP2102/CP2104/CP2109: Icom IC-7100/7300, many cables */
    { 0x10c4, 0xea70, &cp210x },        /* CP2105 */
    { 0x10c4, 0xea71, &cp210x },        /* CP2108 */
};

static const usbser_chip_t *chip_for(unsigned int vid, unsigned int pid)
{
    size_t i;

    for (i = 0; i < sizeof(usbser_ids) / sizeof(usbser_ids[0]); i++)
    {
        if (usbser_ids[i].vid == vid && usbser_ids[i].pid == pid)
        {
            return usbser_ids[i].chip;
        }
    }

    return NULL;
}

/* ---- finding the device ---- */

static int sysfs_read(const char *dev, const char *name, char *out, size_t len)
{
    char p[512];
    FILE *f;

    snprintf(p, sizeof(p), "/sys/bus/usb/devices/%s/%s", dev, name);
    f = fopen(p, "r");

    if (!f)
    {
        return -1;
    }

    if (!fgets(out, (int)len, f))
    {
        fclose(f);
        return -1;
    }

    fclose(f);
    out[strcspn(out, "\r\n")] = '\0';
    return 0;
}

/* The usbfs node of the first device matching vid:pid[:serial]; vid 0: any
 * supported chip. */
static int find_node(unsigned int vid, unsigned int pid, const char *serial,
                     char *node, size_t len)
{
    DIR *dir = opendir("/sys/bus/usb/devices");
    struct dirent *e;
    int found = -1;

    if (!dir)
    {
        return -1;
    }

    while (found < 0 && (e = readdir(dir)))
    {
        char v[16], p[16], s[128] = "", bus[16], dev[16];
        unsigned int dv, dp;

        if (e->d_name[0] == '.' || strchr(e->d_name, ':'))
        {
            continue;                   /* interfaces have a ':' */
        }

        if (sysfs_read(e->d_name, "idVendor", v, sizeof(v))
                || sysfs_read(e->d_name, "idProduct", p, sizeof(p)))
        {
            continue;
        }

        dv = (unsigned int)strtoul(v, NULL, 16);
        dp = (unsigned int)strtoul(p, NULL, 16);

        if (vid ? (dv != vid || dp != pid) : !chip_for(dv, dp))
        {
            continue;
        }

        sysfs_read(e->d_name, "serial", s, sizeof(s));

        if (serial && *serial && strcmp(serial, s))
        {
            continue;
        }

        if (sysfs_read(e->d_name, "busnum", bus, sizeof(bus))
                || sysfs_read(e->d_name, "devnum", dev, sizeof(dev)))
        {
            continue;
        }

        snprintf(node, len, "/dev/bus/usb/%03d/%03d", atoi(bus), atoi(dev));
        rig_debug(RIG_DEBUG_VERBOSE, "%s: %04x:%04x \"%s\" is %s\n", __func__, dv,
                  dp, s, node);
        found = 0;
    }

    closedir(dir);
    return found;
}

/* The first interface with a bulk IN and a bulk OUT endpoint, from the
 * descriptors usbfs returns on read(): the device's, then its configuration's. */
static int find_endpoints(struct usbser *u, int want, unsigned int *vid,
                          unsigned int *pid)
{
    uint8_t d[4096];
    ssize_t n, i;
    int ifno = -1;
    unsigned int in = 0, out = 0;

    lseek(u->usbfd, 0, SEEK_SET);
    n = read(u->usbfd, d, sizeof(d));

    if (n < 18 || d[1] != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: no device descriptor\n", __func__);
        return -RIG_EIO;
    }

    *vid = (unsigned int)(d[8] | d[9] << 8);
    *pid = (unsigned int)(d[10] | d[11] << 8);

    for (i = d[0]; i + 2 <= n && d[i] >= 2; i += d[i])
    {
        if (d[i + 1] == 4)              /* interface */
        {
            if (in && out && (want < 0 || ifno == want))
            {
                break;
            }

            ifno = d[i + 2];
            in = out = 0;
        }
        else if (d[i + 1] == 5 && ifno >= 0 && (d[i + 3] & 3) == 2)   /* bulk */
        {
            if (d[i + 2] & 0x80)
            {
                if (!in) { in = d[i + 2]; }
            }
            else if (!out)
            {
                out = d[i + 2];
            }
        }
    }

    if (ifno < 0 || !in || !out || (want >= 0 && ifno != want))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: %04x:%04x has no bulk endpoint pair%s\n",
                  __func__, *vid, *pid, want >= 0 ? " on that interface" : "");
        return -RIG_EIO;
    }

    u->ifno = ifno;
    u->ep_in = in;
    u->ep_out = out;
    return RIG_OK;
}

/* ---- the pump ---- */

static void *rx_pump(void *arg)
{
    struct usbser *u = arg;
    uint8_t b[512];

    while (u->run)
    {
        int n = bulk(u, u->ep_in, b, sizeof(b), 100);
        int off = 0;

        if (n < 0)
        {
            if (errno == ENODEV)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: adapter gone\n", __func__);
                break;
            }

            if (errno != ETIMEDOUT)
            {
                hl_usleep(20 * 1000);
            }

            continue;
        }

        while (off < n)
        {
            ssize_t w = write(u->master, b + off, (size_t)(n - off));

            if (w < 0)
            {
                if (errno == EINTR || errno == EAGAIN)
                {
                    hl_usleep(1000);
                    continue;
                }

                break;
            }

            off += (int)w;
        }
    }

    return NULL;
}

static void *tx_pump(void *arg)
{
    struct usbser *u = arg;
    uint8_t b[512];

    while (u->run)
    {
        struct pollfd p;
        ssize_t n;

        p.fd = u->master;
        p.events = POLLIN;
        p.revents = 0;

        if (poll(&p, 1, 100) <= 0 || !(p.revents & POLLIN))
        {
            continue;
        }

        n = read(u->master, b, sizeof(b));

        if (n > 0 && bulk(u, u->ep_out, b, (size_t)n, 1000) != n)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: write to the adapter failed: %s\n", __func__,
                      strerror(errno));
        }
    }

    return NULL;
}

/* ---- the interface ---- */

int usb_serial_is_path(const char *pathname)
{
    return pathname && (!strncmp(pathname, "usb:", 4)
                        || !strncmp(pathname, "usbfd:", 6));
}

int usb_serial_is_fd(int fd)
{
    int r;

    pthread_mutex_lock(&usbser_lock);
    r = by_fd(fd) != NULL;
    pthread_mutex_unlock(&usbser_lock);
    return r;
}

static void release(struct usbser *u)
{
    if (u->run)
    {
        u->run = 0;
        pthread_join(u->rx, NULL);
        pthread_join(u->tx, NULL);
    }

    if (u->chip && u->ifno >= 0)
    {
        u->rts = u->dtr = 0;
        u->chip->set_lines(u);
        u->chip->fini(u);
    }

    if (u->ifno >= 0)
    {
        unsigned int ifno = (unsigned int)u->ifno;
        struct usbdevfs_ioctl c;

        ioctl(u->usbfd, USBDEVFS_RELEASEINTERFACE, &ifno);
        /* give the interface back to a kernel driver that had it */
        c.ifno = u->ifno;
        c.ioctl_code = USBDEVFS_CONNECT;
        c.data = NULL;
        ioctl(u->usbfd, USBDEVFS_IOCTL, &c);
    }

    if (u->own_usbfd && u->usbfd >= 0) { close(u->usbfd); }
    if (u->slave >= 0) { close(u->slave); }
    if (u->master >= 0) { close(u->master); }

    memset(u, 0, sizeof(*u));
}

int usb_serial_open(hamlib_port_t *rp)
{
    struct usbser *u = NULL;
    char spec[HAMLIB_FILPATHLEN];
    char *a = spec, *at;
    char node[HAMLIB_FILPATHLEN] = "";
    int want = -1;
    char tty[64];
    unsigned int vid = 0, pid = 0;
    struct usbdevfs_disconnect_claim dc;
    struct termios t;
    int i, err = -RIG_EIO;

    pthread_mutex_lock(&usbser_lock);

    for (i = 0; i < USBSER_MAX && !u; i++)
    {
        if (!usbser[i].used)
        {
            u = &usbser[i];
        }
    }

    if (!u)
    {
        pthread_mutex_unlock(&usbser_lock);
        rig_debug(RIG_DEBUG_ERR, "%s: too many USB-serial ports open\n", __func__);
        return -RIG_ENOMEM;
    }

    memset(u, 0, sizeof(*u));
    u->used = 1;
    u->usbfd = u->master = u->slave = u->ifno = -1;
    pthread_mutex_unlock(&usbser_lock);

    /* "...@N": interface N of a multi-port chip (the CP2105's second UART) */
    snprintf(spec, sizeof(spec), "%s", rp->pathname);
    at = strrchr(spec, '@');

    if (at && at[1] >= '0' && at[1] <= '9')
    {
        want = atoi(at + 1);
        *at = '\0';
    }

    if (!strncmp(a, "usbfd:", 6))
    {
        u->usbfd = atoi(a + 6);
    }
    else
    {
        a += 4;

        if (*a == '/')
        {
            snprintf(node, sizeof(node), "%s", a);
        }
        else
        {
            char serial[128] = "";

            if (*a && sscanf(a, "%x:%x:%127[^\n]", &vid, &pid, serial) < 2)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: bad device \"%s\" (usb:VVVV:PPPP[:serial])\n",
                          __func__, rp->pathname);
                err = -RIG_EINVAL;
                goto fail;
            }

            if (find_node(vid, pid, serial, node, sizeof(node)) < 0)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: no USB-serial adapter matches \"%s\"\n",
                          __func__, rp->pathname);
                goto fail;
            }
        }

        u->usbfd = open(node, O_RDWR | O_CLOEXEC);

        if (u->usbfd < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: cannot open %s: %s\n", __func__, node,
                      strerror(errno));
            err = errno == EACCES ? -RIG_EACCESS : -RIG_EIO;
            goto fail;
        }

        u->own_usbfd = 1;
    }

    if ((err = find_endpoints(u, want, &vid, &pid)) != RIG_OK)
    {
        goto fail;
    }

    u->chip = chip_for(vid, pid);

    if (!u->chip)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: %04x:%04x is not a supported USB-serial chip\n",
                  __func__, vid, pid);
        err = -RIG_ENIMPL;
        goto fail;
    }

    /* A kernel driver bound to it (cp210x on a PC) is detached for now. */
    memset(&dc, 0, sizeof(dc));
    dc.interface = (unsigned int)u->ifno;

    if (ioctl(u->usbfd, USBDEVFS_DISCONNECT_CLAIM, &dc) < 0)
    {
        unsigned int ifno = (unsigned int)u->ifno;

        if (ioctl(u->usbfd, USBDEVFS_CLAIMINTERFACE, &ifno) < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: cannot claim interface %d: %s\n", __func__,
                      u->ifno, strerror(errno));
            u->ifno = -1;
            goto fail;
        }
    }

    if ((err = u->chip->setup(u, rp)) != RIG_OK || (err = u->chip->set_lines(u)) != RIG_OK)
    {
        goto fail;
    }

    u->master = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);

    if (u->master < 0 || grantpt(u->master) || unlockpt(u->master)
            || ptsname_r(u->master, tty, sizeof(tty)))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: no pseudo-terminal: %s\n", __func__,
                  strerror(errno));
        err = -RIG_EIO;
        goto fail;
    }

    if (tcgetattr(u->master, &t) == 0)
    {
        cfmakeraw(&t);
        tcsetattr(u->master, TCSANOW, &t);
    }

    u->slave = open(tty, O_RDWR | O_NOCTTY | O_NDELAY | O_CLOEXEC);

    if (u->slave < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: cannot open %s: %s\n", __func__, tty,
                  strerror(errno));
        err = -RIG_EIO;
        goto fail;
    }

    u->run = 1;

    if (pthread_create(&u->rx, NULL, rx_pump, u))
    {
        u->run = 0;
        goto fail;
    }

    if (pthread_create(&u->tx, NULL, tx_pump, u))
    {
        u->run = 0;
        pthread_join(u->rx, NULL);
        goto fail;
    }

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: %s %04x:%04x interface %d (in 0x%02x, out 0x%02x) on %s\n", __func__,
              u->chip->name, vid, pid, u->ifno, u->ep_in, u->ep_out, tty);
    return u->slave;

fail:
    pthread_mutex_lock(&usbser_lock);
    release(u);
    pthread_mutex_unlock(&usbser_lock);
    return err;
}

/* serial_setup() runs on the pseudo-terminal too (raw mode, its termios
 * checks); this gives the chip the port's line settings. */
int usb_serial_setup(hamlib_port_t *rp)
{
    struct usbser *u;
    int r = -RIG_EINVAL;

    pthread_mutex_lock(&usbser_lock);
    u = by_fd(rp->fd);

    if (u)
    {
        r = u->chip->setup(u, rp);
    }

    pthread_mutex_unlock(&usbser_lock);
    return r;
}

static int set_line(int fd, int rts, int state)
{
    struct usbser *u;
    int r = -RIG_EINVAL;

    pthread_mutex_lock(&usbser_lock);
    u = by_fd(fd);

    if (u)
    {
        if (rts) { u->rts = state != 0; }
        else { u->dtr = state != 0; }

        r = u->chip->set_lines(u);
    }

    pthread_mutex_unlock(&usbser_lock);
    return r;
}

int usb_serial_set_rts(int fd, int state) { return set_line(fd, 1, state); }
int usb_serial_set_dtr(int fd, int state) { return set_line(fd, 0, state); }

static int get_line(int fd, int rts, int *state)
{
    struct usbser *u;
    int r = -RIG_EINVAL;

    pthread_mutex_lock(&usbser_lock);
    u = by_fd(fd);

    if (u && state)
    {
        *state = rts ? u->rts : u->dtr;
        r = RIG_OK;
    }

    pthread_mutex_unlock(&usbser_lock);
    return r;
}

int usb_serial_get_rts(int fd, int *state) { return get_line(fd, 1, state); }
int usb_serial_get_dtr(int fd, int *state) { return get_line(fd, 0, state); }

int usb_serial_get_status(int fd, int *cts, int *dsr, int *car)
{
    struct usbser *u;
    int r = -RIG_EINVAL;

    pthread_mutex_lock(&usbser_lock);
    u = by_fd(fd);

    if (u)
    {
        r = u->chip->get_status ? u->chip->get_status(u, cts, dsr, car) : -RIG_ENIMPL;
    }

    pthread_mutex_unlock(&usbser_lock);
    return r;
}

void usb_serial_close(int fd)
{
    struct usbser *u;

    pthread_mutex_lock(&usbser_lock);
    u = by_fd(fd);

    if (u)
    {
        release(u);
    }

    pthread_mutex_unlock(&usbser_lock);
}

#else /* not Linux usbfs */

int usb_serial_is_path(const char *pathname)
{
    return pathname && (!strncmp(pathname, "usb:", 4)
                        || !strncmp(pathname, "usbfd:", 6));
}

int usb_serial_is_fd(int fd) { (void)fd; return 0; }

int usb_serial_open(hamlib_port_t *rp)
{
    rig_debug(RIG_DEBUG_ERR, "%s: \"%s\": USB-serial ports need Linux usbfs\n",
              __func__, rp->pathname);
    return -RIG_ENIMPL;
}

int usb_serial_setup(hamlib_port_t *rp) { (void)rp; return -RIG_ENIMPL; }
int usb_serial_set_rts(int fd, int state) { (void)fd; (void)state; return -RIG_ENIMPL; }
int usb_serial_set_dtr(int fd, int state) { (void)fd; (void)state; return -RIG_ENIMPL; }
int usb_serial_get_rts(int fd, int *state) { (void)fd; (void)state; return -RIG_ENIMPL; }
int usb_serial_get_dtr(int fd, int *state) { (void)fd; (void)state; return -RIG_ENIMPL; }
int usb_serial_get_status(int fd, int *cts, int *dsr, int *car)
{
    (void)fd; (void)cts; (void)dsr; (void)car;
    return -RIG_ENIMPL;
}
void usb_serial_close(int fd) { (void)fd; }

#endif
