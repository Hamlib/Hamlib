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
 * See usb_serial.h.  The CP210x protocol follows Silicon Labs AN571.  The
 * FTDI, CH34x, PL2303 and CDC-ACM ones are translated from
 * usb-serial-for-android (MIT; see usb_serial.LICENSE).
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

/* What this calls to reach usbfs and sysfs, and to start its threads:
 * tests/testusbserial.c defines these to a fake adapter before including
 * this file. */
#ifndef USBSER_IOCTL
#define USBSER_IOCTL ioctl
#define USBSER_READ read
#define USBSER_LSEEK lseek
#define USBSER_OPEN open
#define USBSER_CLOSE close
#define USBSER_SYSFS "/sys/bus/usb/devices"
#define USBSER_PTHREAD_CREATE pthread_create
#endif

struct usbser;

typedef struct
{
    const char *name;
    int (*setup)(struct usbser *u, const hamlib_port_t *rp);
    int (*set_lines)(struct usbser *u);
    int (*get_status)(struct usbser *u, int *cts, int *dsr, int *car);
    void (*fini)(struct usbser *u);
    /* received bytes as the chip sends them, in place: the data's length */
    int (*rx_filter)(struct usbser *u, uint8_t *b, int n);
} usbser_chip_t;

struct usbser
{
    int used;
    int usbfd;                          /* usbfs */
    int own_usbfd;                      /* we opened it (not "usbfd:N") */
    int ifno;
    int ctl_if;                         /* CDC-ACM's control interface, or -1 */
    unsigned int ep_in, ep_out;
    unsigned int in_mps;                /* ep_in's wMaxPacketSize */
    unsigned int dev_class, mps0;       /* from the device descriptor */
    unsigned int bcd_usb, bcd_device;
    int ninterfaces;
    int cdc_data;                       /* ifno is a CDC data interface */
    int variant;                        /* the chip's: FTDI H/multi-port, PL2303 type */
    const usbser_chip_t *chip;
    int master, slave;                  /* the pseudo-terminal; Hamlib has slave */
    int rts, dtr;
    volatile int run;
    volatile int gone;                  /* the adapter went away */
    int pumps;                          /* pump threads still running */
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
                uint16_t index, void *data, uint16_t len)
{
    struct usbdevfs_ctrltransfer c;
    int r;

    memset(&c, 0, sizeof(c));
    c.bRequestType = type;
    c.bRequest = req;
    c.wValue = value;
    c.wIndex = index;
    c.wLength = len;
    c.timeout = 1000;
    c.data = data;
    r = USBSER_IOCTL(u->usbfd, USBDEVFS_CONTROL, &c);

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
    return USBSER_IOCTL(u->usbfd, USBDEVFS_BULK, &b);
}

/* Framing every chip refuses rather than replaces: a port opened at
 * settings other than the ones asked for would only fail later, quietly. */
static int framing(const hamlib_port_t *rp, int min_bits, int *bits, int *stop)
{
    *bits = rp->parm.serial.data_bits > 0 ? rp->parm.serial.data_bits : 8;
    *stop = rp->parm.serial.stop_bits > 0 ? rp->parm.serial.stop_bits : 1;

    if (*bits < min_bits || *bits > 8 || (*stop != 1 && *stop != 2))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: %d data bits, %d stop bits: not supported\n",
                  __func__, *bits, *stop);
        return -RIG_EINVAL;
    }

    return RIG_OK;
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
    int bits, stop, r;

    if ((r = framing(rp, 5, &bits, &stop)) != RIG_OK)
    {
        return r;
    }

    if (stop == 2)
    {
        line |= 2;
    }

    switch (rp->parm.serial.parity)
    {
    case RIG_PARITY_ODD:   line |= 1 << 4; break;
    case RIG_PARITY_EVEN:  line |= 2 << 4; break;
    case RIG_PARITY_MARK:  line |= 3 << 4; break;
    case RIG_PARITY_SPACE: line |= 4 << 4; break;
    default: break;
    }

    line |= (uint16_t)(bits << 8);

    if (rp->parm.serial.handshake == RIG_HANDSHAKE_HARDWARE)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: CP210x hardware handshake not implemented, using none\n", __func__);
    }

    b[0] = (uint8_t)rate;
    b[1] = (uint8_t)(rate >> 8);
    b[2] = (uint8_t)(rate >> 16);
    b[3] = (uint8_t)(rate >> 24);

    if (ctrl(u, CP_OUT, CP_IFC_ENABLE, 1, (uint16_t)u->ifno, NULL, 0) < 0
            || ctrl(u, CP_OUT, CP_SET_BAUDRATE, 0, (uint16_t)u->ifno, b, 4) < 0
            || ctrl(u, CP_OUT, CP_SET_LINE_CTL, line, (uint16_t)u->ifno, NULL, 0) < 0)
    {
        return -RIG_EIO;
    }

    ctrl(u, CP_OUT, CP_PURGE, 0x000f, (uint16_t)u->ifno, NULL, 0);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: %d baud, line control 0x%04x\n", __func__,
              rate, line);
    return RIG_OK;
}

static int cp_set_lines(struct usbser *u)
{
    /* bit 0 DTR, bit 1 RTS; bits 8 and 9 say both are being set */
    uint16_t v = (uint16_t)(0x0300 | (u->dtr ? 0x01 : 0) | (u->rts ? 0x02 : 0));

    return ctrl(u, CP_OUT, CP_SET_MHS, v, (uint16_t)u->ifno, NULL, 0) < 0 ? -RIG_EIO : RIG_OK;
}

static int cp_get_status(struct usbser *u, int *cts, int *dsr, int *car)
{
    uint8_t s = 0;

    if (ctrl(u, CP_IN, CP_GET_MDMSTS, 0, (uint16_t)u->ifno, &s, 1) < 1)
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
    ctrl(u, CP_OUT, CP_IFC_ENABLE, 0, (uint16_t)u->ifno, NULL, 0);
}

static const usbser_chip_t cp210x = { "CP210x", cp_setup, cp_set_lines, cp_get_status, cp_fini, NULL };

/* RIG_PARITY_* as CDC (and PL2303) line coding numbers it: none, odd, even,
 * mark, space. */
static uint8_t cdc_parity(const hamlib_port_t *rp)
{
    switch (rp->parm.serial.parity)
    {
    case RIG_PARITY_ODD:   return 1;
    case RIG_PARITY_EVEN:  return 2;
    case RIG_PARITY_MARK:  return 3;
    case RIG_PARITY_SPACE: return 4;
    default:               return 0;
    }
}

static int rate_of(const hamlib_port_t *rp)
{
    return rp->parm.serial.rate > 0 ? rp->parm.serial.rate : 9600;
}

/* ---- FTDI (FT232R, FT2232H, FT4232H, FT232H, FT-X) ---- */

#define FT_OUT          0x40            /* vendor, device */
#define FT_IN           0xc0
#define FT_RESET        0x00
#define FT_MODEM_CTRL   0x01
#define FT_SET_FLOW     0x02
#define FT_SET_BAUD     0x03
#define FT_SET_DATA     0x04
#define FT_GET_STATUS   0x05

/* Every request names the port, 1 for the first. */
static uint16_t ft_port(const struct usbser *u) { return (uint16_t)(u->ifno + 1); }

static int ft_set_baud(struct usbser *u, int rate)
{
    int divisor, sub;
    uint16_t value, index = 0;

    if (rate > 3500000 || rate < 183)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: %d baud: out of range\n", __func__, rate);
        return -RIG_EINVAL;
    }

    if (rate >= 2500000) { divisor = 0; sub = 0; }
    else if (rate >= 1750000) { divisor = 1; sub = 0; }
    else
    {
        divisor = ((24000000 << 1) / rate + 1) >> 1;    /* in 1/8ths, rounded */
        sub = divisor & 7;
        divisor >>= 3;
    }

    value = (uint16_t)divisor;

    /* the sub-integer divisor's three bits: 15..14 of value, 0 of index */
    switch (sub)
    {
    case 4: value |= 0x4000; break;
    case 2: value |= 0x8000; break;
    case 1: value |= 0xc000; break;
    case 3: index = 1; break;
    case 5: value |= 0x4000; index = 1; break;
    case 6: value |= 0x8000; index = 1; break;
    case 7: value |= 0xc000; index = 1; break;
    default: break;
    }

    if (u->variant)                     /* H and multi-port chips: and the port */
    {
        index = (uint16_t)(index << 8 | ft_port(u));
    }

    return ctrl(u, FT_OUT, FT_SET_BAUD, value, index, NULL, 0) < 0 ? -RIG_EIO : RIG_OK;
}

static int ft_setup(struct usbser *u, const hamlib_port_t *rp)
{
    int bits, stop, r;
    uint16_t data, flow = 0;

    if ((r = framing(rp, 7, &bits, &stop)) != RIG_OK)
    {
        return r;
    }

    /* the baud rate's index carries the port on H chips and on chips with
     * more than one (bcdDevice 0x07xx-0x09xx: FT2232H, FT4232H, FT232H) */
    u->variant = (u->bcd_device >> 8) == 7 || (u->bcd_device >> 8) == 8
                 || (u->bcd_device >> 8) == 9 || u->ninterfaces > 1;
    data = (uint16_t)(bits | cdc_parity(rp) << 8 | (stop == 2 ? 0x1000 : 0));

    if (rp->parm.serial.handshake == RIG_HANDSHAKE_HARDWARE)
    {
        flow = 0x100;                   /* RTS/CTS */
    }

    if (ctrl(u, FT_OUT, FT_RESET, 0, ft_port(u), NULL, 0) < 0
            || (r = ft_set_baud(u, rate_of(rp))) != RIG_OK
            || ctrl(u, FT_OUT, FT_SET_DATA, data, ft_port(u), NULL, 0) < 0
            || ctrl(u, FT_OUT, FT_SET_FLOW, 0, (uint16_t)(flow | ft_port(u)), NULL, 0) < 0)
    {
        return r != RIG_OK ? r : -RIG_EIO;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: %d baud, data 0x%04x%s\n", __func__,
              rate_of(rp), data, flow ? ", RTS/CTS" : "");
    return RIG_OK;
}

static int ft_set_lines(struct usbser *u)
{
    /* the high byte says which lines are being set */
    uint16_t v = (uint16_t)(0x0300 | (u->dtr ? 0x01 : 0) | (u->rts ? 0x02 : 0));

    return ctrl(u, FT_OUT, FT_MODEM_CTRL, v, ft_port(u), NULL, 0) < 0 ? -RIG_EIO : RIG_OK;
}

static int ft_get_status(struct usbser *u, int *cts, int *dsr, int *car)
{
    uint8_t s[2] = { 0, 0 };

    if (ctrl(u, FT_IN, FT_GET_STATUS, 0, ft_port(u), s, 2) < 2)
    {
        return -RIG_EIO;
    }

    if (cts) { *cts = (s[0] & 0x10) != 0; }
    if (dsr) { *dsr = (s[0] & 0x20) != 0; }
    if (car) { *car = (s[0] & 0x80) != 0; }

    return RIG_OK;
}

/* Every packet the chip sends starts with two status bytes, data or not. */
static int ft_rx_filter(struct usbser *u, uint8_t *b, int n)
{
    int mps = u->in_mps ? (int)u->in_mps : 64;
    int src, dst = 0;

    for (src = 0; src < n; src += mps)
    {
        int len = (src + mps < n ? src + mps : n) - (src + 2);

        if (len > 0)
        {
            memmove(b + dst, b + src + 2, (size_t)len);
            dst += len;
        }
    }

    return dst;
}

static const usbser_chip_t ftdi = { "FTDI", ft_setup, ft_set_lines, ft_get_status, NULL, ft_rx_filter };

/* ---- CH340 / CH341 ---- */

#define CH_OUT          0x40            /* vendor, device */
#define CH_IN           0xc0
#define CH_READ_VERSION 0x5f
#define CH_READ_REG     0x95
#define CH_WRITE_REG    0x9a
#define CH_SERIAL_INIT  0xa1
#define CH_MODEM_CTRL   0xa4

static int ch_set_baud(struct usbser *u, int rate)
{
    long factor, divisor;

    if (rate == 921600) { divisor = 7; factor = 0xf300; }
    else if (rate == 307200) { divisor = 7; factor = 0xd900; }
    else
    {
        factor = 1532620800L / rate;
        divisor = 3;

        while (factor > 0xfff0 && divisor > 0)
        {
            factor >>= 3;
            divisor--;
        }

        if (factor > 0xfff0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: %d baud: not supported\n", __func__, rate);
            return -RIG_EINVAL;
        }

        factor = 0x10000 - factor;
    }

    divisor |= 0x0080;                  /* else the CH341A waits for a full buffer */

    if (ctrl(u, CH_OUT, CH_WRITE_REG, 0x1312, (uint16_t)((factor & 0xff00) | divisor), NULL, 0) < 0
            || ctrl(u, CH_OUT, CH_WRITE_REG, 0x0f2c, (uint16_t)(factor & 0xff), NULL, 0) < 0)
    {
        return -RIG_EIO;
    }

    return RIG_OK;
}

static int ch_setup(struct usbser *u, const hamlib_port_t *rp)
{
    uint8_t v[2];
    int bits, stop, r;
    uint16_t lcr = 0x80 | 0x40;         /* RX and TX on */

    if ((r = framing(rp, 5, &bits, &stop)) != RIG_OK)
    {
        return r;
    }

    lcr |= (uint16_t)(bits - 5);

    switch (rp->parm.serial.parity)
    {
    case RIG_PARITY_ODD:   lcr |= 0x08; break;
    case RIG_PARITY_EVEN:  lcr |= 0x08 | 0x10; break;
    case RIG_PARITY_MARK:  lcr |= 0x08 | 0x20; break;
    case RIG_PARITY_SPACE: lcr |= 0x08 | 0x20 | 0x10; break;
    default: break;
    }

    if (stop == 2) { lcr |= 0x04; }

    /* the chip's own start-up sequence, register reads included */
    if (ctrl(u, CH_IN, CH_READ_VERSION, 0, 0, v, 2) < 2
            || ctrl(u, CH_OUT, CH_SERIAL_INIT, 0, 0, NULL, 0) < 0
            || (r = ch_set_baud(u, rate_of(rp))) != RIG_OK
            || ctrl(u, CH_IN, CH_READ_REG, 0x2518, 0, v, 2) < 2
            || ctrl(u, CH_OUT, CH_WRITE_REG, 0x2518, lcr, NULL, 0) < 0
            || ctrl(u, CH_IN, CH_READ_REG, 0x0706, 0, v, 2) < 2
            || ctrl(u, CH_OUT, CH_SERIAL_INIT, 0x501f, 0xd90a, NULL, 0) < 0
            || (r = ch_set_baud(u, rate_of(rp))) != RIG_OK
            || ctrl(u, CH_OUT, CH_WRITE_REG, 0x2727,
                    rp->parm.serial.handshake == RIG_HANDSHAKE_HARDWARE ? 0x0101 : 0, NULL, 0) < 0)
    {
        return r != RIG_OK ? r : -RIG_EIO;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: %d baud, line control 0x%02x\n", __func__,
              rate_of(rp), lcr);
    return RIG_OK;
}

static int ch_set_lines(struct usbser *u)
{
    /* active low: bit 5 DTR, bit 6 RTS */
    uint16_t v = (uint16_t)~((u->dtr ? 0x20 : 0) | (u->rts ? 0x40 : 0));

    return ctrl(u, CH_OUT, CH_MODEM_CTRL, v, 0, NULL, 0) < 0 ? -RIG_EIO : RIG_OK;
}

static int ch_get_status(struct usbser *u, int *cts, int *dsr, int *car)
{
    uint8_t s[2] = { 0xff, 0xff };

    if (ctrl(u, CH_IN, CH_READ_REG, 0x0706, 0, s, 2) < 2)
    {
        return -RIG_EIO;
    }

    /* active low */
    if (cts) { *cts = !(s[0] & 0x01); }
    if (dsr) { *dsr = !(s[0] & 0x02); }
    if (car) { *car = !(s[0] & 0x08); }

    return RIG_OK;
}

static const usbser_chip_t ch34x = { "CH34x", ch_setup, ch_set_lines, ch_get_status, NULL, NULL };

/* ---- PL2303 (01, T, HX, HXN) ---- */

#define PL_OUT          0x40            /* vendor, device */
#define PL_IN           0xc0
#define PL_CLASS_OUT    0x21            /* class, interface */
#define PL_SET_LINE     0x20
#define PL_SET_CONTROL  0x22

enum { PL_01, PL_T, PL_HX, PL_HXN };

static int pl_vendor_in(struct usbser *u, uint16_t value, uint16_t index, uint8_t *b)
{
    return ctrl(u, PL_IN, u->variant == PL_HXN ? 0x81 : 0x01, value, index, b, 1) == 1
           ? RIG_OK : -RIG_EIO;
}

static int pl_vendor_out(struct usbser *u, uint16_t value, uint16_t index)
{
    return ctrl(u, PL_OUT, u->variant == PL_HXN ? 0x80 : 0x01, value, index, NULL, 0) < 0
           ? -RIG_EIO : RIG_OK;
}

static void pl_purge(struct usbser *u)
{
    if (u->variant == PL_HXN)
    {
        pl_vendor_out(u, 0x07, 3);       /* both pipes */
    }
    else
    {
        pl_vendor_out(u, 0x08, 0);
        pl_vendor_out(u, 0x09, 0);
    }
}

/* Which PL2303 it is, from its device descriptor; a T answers 0x8080. */
static int pl_variant(struct usbser *u)
{
    uint8_t b;
    int saved = u->variant;

    if (u->dev_class == 0x02 || u->mps0 != 64)
    {
        return PL_01;
    }

    if (u->bcd_usb != 0x200)
    {
        return PL_HX;
    }

    u->variant = PL_HX;                 /* pl_vendor_in's request 0x01 */

    if ((u->bcd_device == 0x300 || u->bcd_device == 0x500)
            && pl_vendor_in(u, 0x8080, 0, &b) == RIG_OK)
    {
        u->variant = saved;
        return PL_T;
    }

    u->variant = saved;
    return PL_HXN;
}

/* A rate the chip has no table entry for, as its divisor encoding. */
static uint32_t pl_rate(const struct usbser *u, int rate)
{
    static const int std[] = { 75, 150, 300, 600, 1200, 1800, 2400, 3600, 4800, 7200,
                               9600, 14400, 19200, 28800, 38400, 57600, 115200, 230400,
                               460800, 614400, 921600, 1228800, 2457600, 3000000, 6000000
                             };
    uint32_t baseline = 12000000U * 32, mantissa, exponent = 0;
    size_t i;

    if (u->variant == PL_HXN)
    {
        return (uint32_t)rate;
    }

    for (i = 0; i < sizeof(std) / sizeof(std[0]); i++)
    {
        if (std[i] == rate)
        {
            return (uint32_t)rate;
        }
    }

    mantissa = baseline / (uint32_t)rate;

    if (u->variant == PL_T)
    {
        while (mantissa >= 2048 && exponent < 15)
        {
            mantissa >>= 1;
            exponent++;
        }

        return mantissa + ((exponent & ~1U) << 12) + ((exponent & 1U) << 16) + (1U << 31);
    }

    while (mantissa >= 512 && exponent < 7)
    {
        mantissa >>= 2;
        exponent++;
    }

    return mantissa + (exponent << 9) + (1U << 31);
}

static int pl_setup(struct usbser *u, const hamlib_port_t *rp)
{
    uint8_t line[7], b;
    uint32_t rate;
    int bits, stop, r, hw = rp->parm.serial.handshake == RIG_HANDSHAKE_HARDWARE;

    if ((r = framing(rp, 5, &bits, &stop)) != RIG_OK)
    {
        return r;
    }

    u->variant = pl_variant(u);
    pl_purge(u);

    if (u->variant != PL_HXN)           /* the vendor's start-up sequence */
    {
        if (pl_vendor_in(u, 0x8484, 0, &b) || pl_vendor_out(u, 0x0404, 0)
                || pl_vendor_in(u, 0x8484, 0, &b) || pl_vendor_in(u, 0x8383, 0, &b)
                || pl_vendor_in(u, 0x8484, 0, &b) || pl_vendor_out(u, 0x0404, 1)
                || pl_vendor_in(u, 0x8484, 0, &b) || pl_vendor_in(u, 0x8383, 0, &b)
                || pl_vendor_out(u, 0, 1) || pl_vendor_out(u, 1, 0)
                || pl_vendor_out(u, 2, u->variant == PL_01 ? 0x24 : 0x44))
        {
            return -RIG_EIO;
        }
    }

    rate = pl_rate(u, rate_of(rp));
    line[0] = (uint8_t)rate;
    line[1] = (uint8_t)(rate >> 8);
    line[2] = (uint8_t)(rate >> 16);
    line[3] = (uint8_t)(rate >> 24);
    line[4] = stop == 2 ? 2 : 0;
    line[5] = cdc_parity(rp);
    line[6] = (uint8_t)bits;

    if (ctrl(u, PL_CLASS_OUT, PL_SET_LINE, 0, 0, line, 7) < 0)
    {
        return -RIG_EIO;
    }

    pl_purge(u);

    if (u->variant == PL_HXN ? pl_vendor_out(u, 0x0a, hw ? 0xfa : 0xff)
            : pl_vendor_out(u, 0, hw ? 0x61 : 0))
    {
        return -RIG_EIO;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: type %d, %d baud (0x%08x)%s\n", __func__,
              u->variant, rate_of(rp), rate, hw ? ", RTS/CTS" : "");
    return RIG_OK;
}

static int pl_set_lines(struct usbser *u)
{
    uint16_t v = (uint16_t)((u->dtr ? 0x01 : 0) | (u->rts ? 0x02 : 0));

    return ctrl(u, PL_CLASS_OUT, PL_SET_CONTROL, v, 0, NULL, 0) < 0 ? -RIG_EIO : RIG_OK;
}

static int pl_get_status(struct usbser *u, int *cts, int *dsr, int *car)
{
    uint8_t s = 0xff;
    int hxn = u->variant == PL_HXN;

    if (pl_vendor_in(u, hxn ? 0x80 : 0x87, 0, &s) != RIG_OK)
    {
        return -RIG_EIO;
    }

    /* active low */
    if (cts) { *cts = !(s & 0x08); }
    if (dsr) { *dsr = !(s & (hxn ? 0x20 : 0x04)); }
    if (car) { *car = !(s & (hxn ? 0x40 : 0x02)); }

    return RIG_OK;
}

static const usbser_chip_t pl2303 = { "PL2303", pl_setup, pl_set_lines, pl_get_status, pl_purge, NULL };

/* ---- CDC-ACM (USB class 2/2, by interface class, not by id) ---- */

#define ACM_OUT                 0x21    /* class, interface */
#define ACM_SET_LINE_CODING     0x20
#define ACM_SET_CONTROL_LINE    0x22

static int acm_setup(struct usbser *u, const hamlib_port_t *rp)
{
    uint8_t line[7];
    int rate = rate_of(rp), bits, stop, r;

    if ((r = framing(rp, 5, &bits, &stop)) != RIG_OK)
    {
        return r;
    }

    if (rp->parm.serial.handshake == RIG_HANDSHAKE_HARDWARE)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: CDC-ACM has no hardware handshake to set, using none\n", __func__);
    }

    line[0] = (uint8_t)rate;
    line[1] = (uint8_t)(rate >> 8);
    line[2] = (uint8_t)(rate >> 16);
    line[3] = (uint8_t)(rate >> 24);
    line[4] = stop == 2 ? 2 : 0;
    line[5] = cdc_parity(rp);
    line[6] = (uint8_t)bits;

    if (ctrl(u, ACM_OUT, ACM_SET_LINE_CODING, 0, (uint16_t)u->ctl_if, line, 7) < 0)
    {
        return -RIG_EIO;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: %d baud on control interface %d\n", __func__,
              rate, u->ctl_if);
    return RIG_OK;
}

static int acm_set_lines(struct usbser *u)
{
    uint16_t v = (uint16_t)((u->dtr ? 0x01 : 0) | (u->rts ? 0x02 : 0));

    return ctrl(u, ACM_OUT, ACM_SET_CONTROL_LINE, v, (uint16_t)u->ctl_if, NULL, 0) < 0
           ? -RIG_EIO : RIG_OK;
}

/* CTS, DSR and DCD arrive as notifications on an interrupt endpoint, which
 * this does not read: get_status is not implemented. */
static const usbser_chip_t cdc_acm = { "CDC-ACM", acm_setup, acm_set_lines, NULL, NULL, NULL };

static const struct
{
    uint16_t vid, pid;
    const usbser_chip_t *chip;
} usbser_ids[] =
{
    { 0x10c4, 0xea60, &cp210x },        /* CP2102/CP2104/CP2109: Icom IC-7100/7300, many cables */
    { 0x10c4, 0xea70, &cp210x },        /* CP2105: Yaesu FT-710, FT-991A */
    { 0x10c4, 0xea71, &cp210x },        /* CP2108 */
    { 0x0403, 0x6001, &ftdi },          /* FT232R */
    { 0x0403, 0x6010, &ftdi },          /* FT2232H */
    { 0x0403, 0x6011, &ftdi },          /* FT4232H */
    { 0x0403, 0x6014, &ftdi },          /* FT232H */
    { 0x0403, 0x6015, &ftdi },          /* FT230X, FT231X, FT234XD */
    { 0x1a86, 0x7523, &ch34x },         /* CH340 */
    { 0x1a86, 0x5523, &ch34x },         /* CH341A */
    { 0x067b, 0x2303, &pl2303 },        /* PL2303 (01, T, HX) */
    { 0x067b, 0x23a3, &pl2303 },        /* PL2303GC (HXN) */
    { 0x067b, 0x23b3, &pl2303 },        /* PL2303GB */
    { 0x067b, 0x23c3, &pl2303 },        /* PL2303GT */
    { 0x067b, 0x23d3, &pl2303 },        /* PL2303GL */
    { 0x067b, 0x23e3, &pl2303 },        /* PL2303GE */
    { 0x067b, 0x23f3, &pl2303 },        /* PL2303GS */
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

    snprintf(p, sizeof(p), "%s/%s/%s", USBSER_SYSFS, dev, name);
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
    DIR *dir = opendir(USBSER_SYSFS);
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

/* The first interface with a bulk IN and a bulk OUT endpoint (or interface
 * "want"), from the descriptors usbfs returns on read(): the device's, then
 * its configuration's.  Also what the chips need from them: the device's
 * class, EP0 size and versions, the IN endpoint's packet size, the number
 * of interfaces, and for a CDC data interface its control interface (from
 * the Union descriptor, else the IAD, else the interface before it). */
static int find_endpoints(struct usbser *u, int want, unsigned int *vid,
                          unsigned int *pid)
{
    uint8_t d[4096];
    uint8_t cls[32];                    /* bInterfaceClass by number */
    int sub_acm[32];                    /* subclass 2 (ACM) under class 2 */
    int union_ctl[32];                  /* data interface -> control, from Union */
    int iad_first = -1, iad_count = 0;
    ssize_t n, i;
    int ifno = -1, cur = -1, best_if = -1, k;
    unsigned int in = 0, out = 0, mps = 0, best_in = 0, best_out = 0, best_mps = 0;

    memset(cls, 0, sizeof(cls));
    memset(sub_acm, 0, sizeof(sub_acm));

    for (k = 0; k < 32; k++) { union_ctl[k] = -1; }

    USBSER_LSEEK(u->usbfd, 0, SEEK_SET);
    n = USBSER_READ(u->usbfd, d, sizeof(d));

    if (n < 18 || d[1] != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: no device descriptor\n", __func__);
        return -RIG_EIO;
    }

    u->bcd_usb = (unsigned int)(d[2] | d[3] << 8);
    u->dev_class = d[4];
    u->mps0 = d[7];
    *vid = (unsigned int)(d[8] | d[9] << 8);
    *pid = (unsigned int)(d[10] | d[11] << 8);
    u->bcd_device = (unsigned int)(d[12] | d[13] << 8);
    u->ninterfaces = 0;

    for (i = d[0]; i + 2 <= n && d[i] >= 2; i += d[i])
    {
        if (d[i + 1] == 4 && i + 7 < n)                 /* interface */
        {
            if (best_if < 0 && cur >= 0 && in && out && (want < 0 || cur == want))
            {
                best_if = cur;
                best_in = in;
                best_out = out;
                best_mps = mps;
            }

            cur = d[i + 2];
            in = out = mps = 0;

            if (d[i + 3] == 0)                          /* alternate setting 0 */
            {
                u->ninterfaces++;
            }

            if (cur < 32)
            {
                cls[cur] = d[i + 5];
                sub_acm[cur] = d[i + 5] == 2 && d[i + 6] == 2;
            }
        }
        else if (d[i + 1] == 0x0b && i + 5 < n && d[i + 4] == 2 && d[i + 5] == 2)
        {
            /* an IAD grouping a CDC-ACM function: the one holding "want" */
            if (want < 0 ? iad_first < 0 : (want >= d[i + 2] && want < d[i + 2] + d[i + 3]))
            {
                iad_first = d[i + 2];
                iad_count = d[i + 3];
            }
        }
        else if (d[i + 1] == 0x24 && i + 4 < n && d[i + 2] == 0x06)
        {
            /* CDC Union: bMasterInterface, then its bSlaveInterfaces */
            for (k = 4; k < d[i] && i + k < n; k++)
            {
                if (d[i + k] < 32) { union_ctl[d[i + k]] = d[i + 3]; }
            }
        }
        else if (d[i + 1] == 5 && cur >= 0 && i + 5 < n && (d[i + 3] & 3) == 2)   /* bulk */
        {
            if (d[i + 2] & 0x80)
            {
                if (!in)
                {
                    in = d[i + 2];
                    mps = (unsigned int)(d[i + 4] | (d[i + 5] & 0x07) << 8);
                }
            }
            else if (!out)
            {
                out = d[i + 2];
            }
        }
    }

    if (best_if < 0 && cur >= 0 && in && out && (want < 0 || cur == want))
    {
        best_if = cur;
        best_in = in;
        best_out = out;
        best_mps = mps;
    }

    if (best_if < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: %04x:%04x has no bulk endpoint pair%s\n",
                  __func__, *vid, *pid, want >= 0 ? " on that interface" : "");
        return -RIG_EIO;
    }

    ifno = best_if;
    u->ifno = ifno;
    u->ep_in = best_in;
    u->ep_out = best_out;
    u->in_mps = best_mps;
    u->ctl_if = -1;
    u->cdc_data = ifno < 32 && (cls[ifno] == 0x0a || sub_acm[ifno]);

    if (u->cdc_data)
    {
        if (sub_acm[ifno])
        {
            u->ctl_if = ifno;                           /* one interface does both */
        }
        else if (union_ctl[ifno] >= 0)
        {
            u->ctl_if = union_ctl[ifno];
        }
        else if (iad_first >= 0 && ifno >= iad_first && ifno < iad_first + iad_count)
        {
            u->ctl_if = iad_first;
        }
        else if (ifno > 0 && sub_acm[ifno - 1])
        {
            u->ctl_if = ifno - 1;
        }
        else
        {
            u->cdc_data = 0;                            /* a data interface, but no ACM */
        }
    }

    return RIG_OK;
}

/* ---- the pump ---- */

/* A pump stopping.  The last one to stop after the adapter went away hangs
 * up the pseudo-terminal, so Hamlib's reads and writes on it fail (EIO)
 * instead of writes going nowhere until a reply times out. */
static void pump_done(struct usbser *u)
{
    if (__atomic_sub_fetch(&u->pumps, 1, __ATOMIC_ACQ_REL) == 0 && u->gone
            && u->master >= 0)
    {
        close(u->master);
        u->master = -1;
    }
}

static int unplugged(int err)
{
    return err == ENODEV || err == ESHUTDOWN;
}

static void *rx_pump(void *arg)
{
    struct usbser *u = arg;
    uint8_t b[1024];
    /* One packet per transfer: usbfs hands back nothing of a transfer that
     * times out, so a longer one would drop the full packets that arrived
     * before the deadline. */
    size_t want = u->in_mps ? u->in_mps : 64;

    if (want > sizeof(b))
    {
        want = sizeof(b);
    }

    while (u->run && !u->gone)
    {
        int n = bulk(u, u->ep_in, b, want, 100);
        int off = 0;

        if (n < 0)
        {
            if (unplugged(errno))
            {
                rig_debug(RIG_DEBUG_ERR, "%s: adapter gone\n", __func__);
                u->gone = 1;
                break;
            }

            if (errno != ETIMEDOUT)
            {
                hl_usleep(20 * 1000);
            }

            continue;
        }

        if (u->chip->rx_filter)
        {
            n = u->chip->rx_filter(u, b, n);
        }

        /* the master is non-blocking: with nobody reading the port, this
         * waits for room, but never past a close */
        while (off < n && u->run && !u->gone)
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

    pump_done(u);
    return NULL;
}

/* Until close, and then what is still queued: bytes written just before
 * a close reach the adapter. */
static void *tx_pump(void *arg)
{
    struct usbser *u = arg;
    uint8_t b[512];

    while (!u->gone)
    {
        struct pollfd p;
        ssize_t n;

        p.fd = u->master;
        p.events = POLLIN;
        p.revents = 0;

        if (poll(&p, 1, 100) <= 0 || !(p.revents & POLLIN)
                || (n = read(u->master, b, sizeof(b))) <= 0)
        {
            if (!u->run)
            {
                break;                  /* closing, and nothing left */
            }

            continue;
        }

        if (bulk(u, u->ep_out, b, (size_t)n, 1000) != n)
        {
            if (unplugged(errno))
            {
                rig_debug(RIG_DEBUG_ERR, "%s: adapter gone\n", __func__);
                u->gone = 1;
                break;
            }

            rig_debug(RIG_DEBUG_WARN, "%s: write to the adapter failed: %s\n", __func__,
                      strerror(errno));
        }
    }

    pump_done(u);
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

/* Release a claimed interface, and give it back to a kernel driver that
 * had it. */
static void give_back(struct usbser *u, int ifno)
{
    unsigned int i = (unsigned int)ifno;
    struct usbdevfs_ioctl c;

    if (ifno < 0 || u->usbfd < 0)
    {
        return;
    }

    USBSER_IOCTL(u->usbfd, USBDEVFS_RELEASEINTERFACE, &i);
    c.ifno = ifno;
    c.ioctl_code = USBDEVFS_CONNECT;
    c.data = NULL;
    USBSER_IOCTL(u->usbfd, USBDEVFS_IOCTL, &c);
}

/* Claim an interface, detaching a kernel driver bound to it (cp210x,
 * ftdi_sio, cdc_acm on a PC) for now. */
static int take(struct usbser *u, int ifno)
{
    struct usbdevfs_disconnect_claim dc;
    unsigned int i = (unsigned int)ifno;

    memset(&dc, 0, sizeof(dc));
    dc.interface = i;

    if (USBSER_IOCTL(u->usbfd, USBDEVFS_DISCONNECT_CLAIM, &dc) < 0
            && USBSER_IOCTL(u->usbfd, USBDEVFS_CLAIMINTERFACE, &i) < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: cannot claim interface %d: %s\n", __func__,
                  ifno, strerror(errno));
        return -RIG_EIO;
    }

    return RIG_OK;
}

static void release(struct usbser *u)
{
    if (u->run)
    {
        u->run = 0;
        pthread_join(u->rx, NULL);
        pthread_join(u->tx, NULL);
    }

    if (u->chip && u->ifno >= 0 && !u->gone)
    {
        u->rts = u->dtr = 0;
        u->chip->set_lines(u);

        if (u->chip->fini) { u->chip->fini(u); }
    }

    give_back(u, u->ifno);

    if (u->ctl_if != u->ifno)
    {
        give_back(u, u->ctl_if);
    }

    if (u->own_usbfd && u->usbfd >= 0) { USBSER_CLOSE(u->usbfd); }
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
    u->usbfd = u->master = u->slave = u->ifno = u->ctl_if = -1;
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

        u->usbfd = USBSER_OPEN(node, O_RDWR | O_CLOEXEC);

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

    if (!u->chip && u->cdc_data)
    {
        u->chip = &cdc_acm;             /* a class device: any vendor's */
    }

    if (!u->chip)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: %04x:%04x is not a supported USB-serial chip\n",
                  __func__, vid, pid);
        err = -RIG_ENIMPL;
        goto fail;
    }

    if (u->chip != &cdc_acm)
    {
        u->ctl_if = -1;
    }

    if ((err = take(u, u->ifno)) != RIG_OK)
    {
        u->ifno = u->ctl_if = -1;
        goto fail;
    }

    if (u->ctl_if >= 0 && u->ctl_if != u->ifno && (err = take(u, u->ctl_if)) != RIG_OK)
    {
        u->ctl_if = -1;
        goto fail;
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

    fcntl(u->master, F_SETFL, fcntl(u->master, F_GETFL) | O_NONBLOCK);

    u->slave = open(tty, O_RDWR | O_NOCTTY | O_NDELAY | O_CLOEXEC);

    if (u->slave < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: cannot open %s: %s\n", __func__, tty,
                  strerror(errno));
        err = -RIG_EIO;
        goto fail;
    }

    u->run = 1;
    u->pumps = 2;

    if (USBSER_PTHREAD_CREATE(&u->rx, NULL, rx_pump, u))
    {
        u->run = 0;
        err = -RIG_EIO;
        goto fail;
    }

    if (USBSER_PTHREAD_CREATE(&u->tx, NULL, tx_pump, u))
    {
        u->run = 0;
        pthread_join(u->rx, NULL);
        err = -RIG_EIO;
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
    /* never a descriptor: 0 would hand the caller stdin as the port */
    return err < 0 ? err : -RIG_EIO;
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
