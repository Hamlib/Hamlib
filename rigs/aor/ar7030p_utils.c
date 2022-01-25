/*
 *  Hamlib AOR backend - AR7030 Plus utility functions
 *  Copyright (c) 2009-2010 by Larry Gadallah (VE6VQ)
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
 * Version 2009.12.31 Larry Gadallah (VE6VQ)
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include <hamlib/rig.h>
#include "ar7030p.h"
#include "serial.h"
#include "idx_builtin.h"

static enum PAGE_e curPage = NONE; /* Current memory page */
static unsigned int curAddr = 65535; /* Current page address */
static enum LOCK_LVL_e curLock = LOCK_0; /* Current lock level */
static const unsigned int AR7030_PAGE_SIZE[] =
{
    256, 256, 512, 4096, 4096,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    8
}; /* Page size table */


#if 0
/*
 *     Code Ident Operation
 *     0x   NOP   No Operation
 */
int NOP(RIG *rig, unsigned char x)
{
    int rc = RIG_OK;

    unsigned char op = ((0x0f & x) | op_NOP);

    assert(NULL != rig);

    rc = write_block(&rig->state.rigport, (char *) &op, 1);

    if (0 != rc)
    {
        rc = -RIG_EIO;
    }

    return (rc);
}

/*
 *     Code Ident Operation
 *     3x   SRH   Set H-register    x -> H-register (4-bits)
 */
int SRH(RIG *rig, unsigned char x)
{
    int rc = RIG_OK;

    unsigned char op = ((0x0f & x) | op_SRH);

    assert(NULL != rig);

    rc = write_block(&rig->state.rigport, (char *) &op, 1);

    if (0 != rc)
    {
        rc = -RIG_EIO;
    }

    return (rc);
}

/*
 *     Code Ident Operation
 *     5x   PGE   Set page          x -> Page register (4-bits)
 */
int PGE(RIG *rig, enum PAGE_e page)
{
    int rc = RIG_OK;

    unsigned char op = ((0x0f & page) | op_PGE);

    assert(NULL != rig);

    switch (page)
    {
    case WORKING:
    case BBRAM:
    case EEPROM1:
    case EEPROM2:
    case EEPROM3:
    case ROM:
        rc = write_block(&rig->state.rigport, (char *) &op, 1);

        if (0 != rc)
        {
            rc = -RIG_EIO;
        }

        break;

    case NONE:
    default:
        rig_debug(RIG_DEBUG_VERBOSE, "PGE: invalid page %d\n", page);

        rc = -RIG_EINVAL;
        break;
    };

    return (rc);
}

/*
 *     Code Ident Operation
 *     4x   ADR   Set address       0Hx -> Address register (12-bits)
 *                                  0 -> H-register
 */
int ADR(RIG *rig, unsigned char x)
{
    int rc = RIG_OK;

    unsigned char op = ((0x0f & x) | op_ADR);

    assert(NULL != rig);

    rc = write_block(&rig->state.rigport, (char *) &op, 1);

    if (0 != rc)
    {
        rc = -RIG_EIO;
    }

    return (rc);
}

/*
 *     Code Ident Operation
 *     1x   ADH   Set address high  x -> Address register (high 4-bits)
 */
int ADH(RIG *rig, unsigned char x)
{
    int rc = RIG_OK;

    unsigned char op = ((0x0f & x) | op_ADH);

    assert(NULL != rig);

    rc = write_block(&rig->state.rigport, (char *) &op, 1);

    if (0 != rc)
    {
        rc = -RIG_EIO;
    }

    return (rc);
}

/*
 *     Code Ident Operation
 *     6x   WRD   Write data        Hx -> [Page, Address]
 *                                  Address register + 1 -> Address register
 *                                  0 -> H-register, 0 -> Mask register
 */
int WRD(RIG *rig, unsigned char out)
{
    int rc = RIG_OK;

    unsigned char op = ((0x0f & out) | op_WRD);

    assert(NULL != rig);

    rc = write_block(&rig->state.rigport, (char *) &op, 1);

    if (0 != rc)
    {
        rc = -RIG_EIO;
    }

    return (rc);
}

/*
 *     Code Ident Operation
 *     9x   MSK   Set mask          Hx -> Mask register <1>
 *                                  0 -> H-register
 */
int MSK(RIG *rig, unsigned char mask)
{
    int rc = RIG_OK;

    unsigned char op = ((0x0f & mask) | op_MSK);

    assert(NULL != rig);

    rc = write_block(&rig->state.rigport, (char *) &op, 1);

    if (0 != rc)
    {
        rc = -RIG_EIO;
    }

    return (rc);
}

/*
 *     Code Ident Operation
 *     2x   EXE   Execute routine x
 */
int EXE(RIG *rig, enum ROUTINE_e routine)
{
    int rc = RIG_OK;

    unsigned char op = ((0x0f & routine) | op_EXE);

    assert(NULL != rig);

    switch (routine)
    {
    case RESET:
    case SET_FREQ:
    case SET_MODE:
    case SET_PASS:
    case SET_ALL:
    case SET_AUDIO:
    case SET_RFIF:
    case DIR_RX_CTL:
    case DIR_DDS_CTL:
    case DISP_MENUS:
    case DISP_FREQ:
    case DISP_BUFF:
    case READ_SIGNAL:
    case READ_BTNS:
        rc = write_block(&rig->state.rigport, (char *) &op, 1);

        if (0 != rc)
        {
            rc = -RIG_EIO;
        }

        break;

    default:
        rig_debug(RIG_DEBUG_VERBOSE, "EXE: invalid routine %d\n", routine);

        rc = -RIG_EINVAL;
        break;
    };

    return (rc);
}

/*
 *     Code Ident Operation
 *     7x   RDD   Read data         [Page, Address] -> Serial output
 *                                  Address register + x -> Address register
 */
int RDD(RIG *rig, unsigned char len)
{
    int rc = RIG_OK;
    unsigned char inChr = 0;
    unsigned char op = ((0x0f & len) | op_RDD);

    assert(NULL != rig);

    rc = write_block(&rig->state.rigport, (char *) &op, 1);

    if (0 != rc)
    {
        rc = -RIG_EIO;
    }
    else
    {
        rc = read_block(&rig->state.rigport, (char *) &inChr, len);

        if (1 != rc)
        {
            rc = -RIG_EIO;
        }
        else
        {
            rc = (int) inChr;
        }
    }

    return (rc);
}

/*
 *     Code Ident Operation
 *     8x   LOC   Set lock level x
 */
int LOC(RIG *rig, enum LOCK_LVL_e level)
{
    int rc = RIG_OK;

    unsigned char op = ((0x0f & level) | op_LOC);

    assert(NULL != rig);

    switch (level)
    {
    case LOCK_0:
    case LOCK_1:
    case LOCK_2:
    case LOCK_3:
        rc = write_block(&rig->state.rigport, (char *) &op, 1);

        if (0 != rc)
        {
            rc = -RIG_EIO;
        }

        break;

    default:
        rig_debug(RIG_DEBUG_VERBOSE, "LOC: invalid lock level %d\n", level);

        rc = -RIG_EINVAL;
        break;
    };

    return (rc);
}

/*
 *     Code Ident Operation
 *     Ax   BUT   Operate button x  <1>
 */
int BUT(RIG *rig, enum BUTTON_e button)
{
    int rc = RIG_OK;

    unsigned char op = ((0x0f & button) | op_BUT);

    assert(NULL != rig);

    switch (button)
    {
    case BTN_NONE:
        break;

    case BTN_UP:
    case BTN_DOWN:
    case BTN_FAST:
    case BTN_FILTER:
    case BTN_RFIF:
    case BTN_MEMORY:
    case BTN_STAR:
    case BTN_MENU:
    case BTN_POWER:
        rc = write_block(&rig->state.rigport, (char *) &op, 1);

        if (0 != rc)
        {
            rc = -RIG_EIO;
        }

        break;

    default:
        rig_debug(RIG_DEBUG_VERBOSE, "BUT: invalid button %d\n", button);

        rc = -RIG_EINVAL;
        break;
    };

    return (rc);
}

#endif // 0

/*
 * /brief Execute routine
 *
 * /param rig Pointer to rig struct
 * /param rtn Receiver routine to execute
 *
 * \return RIG_OK on success, error code on failure
 *
 */
int execRoutine(RIG *rig, enum ROUTINE_e rtn)
{
    int rc = -RIG_EIO;
    unsigned char v = EXE((rtn & 0x0f));

    assert(NULL != rig);

    if (0 == write_block(&rig->state.rigport, &v, 1))
    {
        rc = RIG_OK;

        rig_debug(RIG_DEBUG_VERBOSE, "%s: routine %2d\n", __func__, rtn);
    }

    return (rc);
}

/*
 * /brief Set address for I/O with radio
 *
 * /param rig Pointer to rig struct
 * /param page Memory page number (0-4, 15)
 * /param addr Address offset within page (0-4095, depending on page)
 *
 * \return RIG_OK on success, error code on failure
 *
 * Statics curPage and curAddr shadow radio's copies so that
 * page and address are only set when needed
 */
static int setAddr(RIG *rig, enum PAGE_e page, unsigned int addr)
{
    int rc = RIG_OK;
    unsigned char v;

    assert(NULL != rig);

    if ((EEPROM3 >= page) || (ROM == page))
    {
        if (AR7030_PAGE_SIZE[page] > addr)
        {
            if (curPage != page)
            {
                v = PGE(page);

                if (0 == write_block(&rig->state.rigport, &v, 1))
                {
                    curPage = page;
                    rc = RIG_OK;

                    rig_debug(RIG_DEBUG_VERBOSE, "%s: set page %2d\n", __func__, page);
                }
                else
                {
                    rc = -RIG_EIO;
                }
            }

            if (curAddr != addr)
            {
                v = SRH((0x0f0 & addr) >> 4);

                rc = write_block(&rig->state.rigport, &v, 1);

                if (rc != RIG_OK)
                {
                    return -RIG_EIO;
                }

                v = ADR((0x00f & addr));

                if (0 == write_block(&rig->state.rigport, &v, 1))
                {
                    if (0xff < addr)
                    {
                        v = ADH((0xf00 & addr) >> 8);

                        if (0 == write_block(&rig->state.rigport, &v, 1))
                        {
                            curAddr = addr;
                            rc = RIG_OK;

                            rig_debug(RIG_DEBUG_VERBOSE, "%s: set addr 0x%04x\n", __func__, addr);
                        }
                        else
                        {
                            rc = -RIG_EIO;
                        }
                    }
                    else
                    {
                        curAddr = addr;
                        rc = RIG_OK;

                        rig_debug(RIG_DEBUG_VERBOSE, "%s: set addr 0x%04x\n", __func__, addr);
                    }
                }
                else
                {
                    rc = -RIG_EIO;
                }
            }
        }
        else
        {
            rc = -RIG_EINVAL; /* invalid address */
        }
    }
    else
    {
        rc = -RIG_EINVAL; /* invalid page */
    }

    return (rc);
}

/*
 * /brief Write one byte to the receiver
 *
 * /param rig Pointer to rig struct
 * /param page Memory page number (0-4, 15)
 * /param addr Address offset within page (0-4095, depending on page)
 * /param x    Value to write to radio
 *
 * \return RIG_OK on success, error code on failure
 *
 */
int writeByte(RIG *rig, enum PAGE_e page, unsigned int addr, unsigned char x)
{
    int rc;
    unsigned char hi = SRH((x & 0xf0) >> 4);
    unsigned char lo = WRD(x & 0x0f);

    assert(NULL != rig);

    rc = setAddr(rig, page, addr);

    if (RIG_OK == rc)
    {
        rc = -RIG_EIO;

        if (0 == write_block(&rig->state.rigport, &hi, 1))
        {
            if (0 == write_block(&rig->state.rigport, &lo, 1))
            {
                rc = RIG_OK;
                curAddr++;

                rig_debug(RIG_DEBUG_VERBOSE, "%s: wrote byte 0x%02x\n", __func__, x);
            }
        }
    }

    return (rc);
}

/*
 * /brief Write two bytes to the receiver
 *
 * /param rig  Pointer to rig struct
 * /param page Memory page number (0-4, 15)
 * /param addr Address offset within page (0-4095, depending on page)
 * /param x    Value to write to radio
 *
 * \return Number of bytes written, 0 on error. Get error code with getErrno.
 *
 */
int writeShort(RIG *rig, enum PAGE_e page, unsigned int addr, unsigned short x)
{
    int rc;
    unsigned char v = (unsigned char)((x & 0xff00) >> 8);

    rc = writeByte(rig, page, addr, v);

    if (RIG_OK == rc)
    {
        v = (unsigned char)(x & 0x00ff);
        rc = writeByte(rig, page, addr + 1, v);
    }

    return (rc);
}

/*
 * /brief Write three bytes to the receiver
 *
 * /param rig  Pointer to rig struct
 * /param page Memory page number (0-4, 15)
 * /param addr Address offset within page (0-4095, depending on page)
 * /param x    Value to write to radio
 *
 * \return Number of bytes written, 0 on error. Get error code with getErrno.
 *
 */
int write3Bytes(RIG *rig, enum PAGE_e page, unsigned int addr, unsigned int x)
{
    int rc;
    unsigned char v = (unsigned char)((x & 0xff0000) >> 16);

    rc = writeByte(rig, page, addr, v);

    if (RIG_OK == rc)
    {
        v = (unsigned char)((x & 0x00ff00) >> 8);
        rc = writeByte(rig, page, addr + 1, v);

        if (RIG_OK == rc)
        {
            v = (unsigned char)(x & 0x0000ff);
            rc = writeByte(rig, page, addr + 2, v);
        }
    }

    return (rc);
}

#ifdef XXREMOVEDXX
// this function is not referenced anywhere
/*
 * /brief Write unsigned int (4 bytes) to the receiver
 *
 * /param rig  Pointer to rig struct
 * /param page Memory page number (0-4, 15)
 * /param addr Address offset within page (0-4095, depending on page)
 * /param x    Value to write to radio
 *
 * \return Number of bytes written, 0 on error. Get error code with getErrno.
 *
 */
int writeInt(RIG *rig, enum PAGE_e page, unsigned int addr, unsigned int x)
{
    int rc;
    unsigned char v = (unsigned char)((x & 0xff000000) >> 24);

    rc = writeByte(rig, page, addr, v);

    if (RIG_OK == rc)
    {
        v = (unsigned char)((x & 0x00ff0000) >> 16);
        rc = writeByte(rig, page, addr + 1, v);

        if (RIG_OK == rc)
        {
            v = (unsigned char)((x & 0x0000ff00) >> 8);
            rc = writeByte(rig, page, addr + 2, v);

            if (RIG_OK == rc)
            {
                v = (unsigned char)(x & 0x000000ff);
                rc = writeByte(rig, page, addr + 3, v);
            }
        }
    }

    return (rc);
}
#endif

/*
 * /brief Read one byte from the receiver
 *
 * /param rig Pointer to rig struct
 * /param page Memory page number (0-4, 15)
 * /param addr Address offset within page (0-4095, depending on page)
 * /param x    Pointer to value to read from radio
 *
 * \return RIG_OK on success, error code on failure
 *
 */
int readByte(RIG *rig, enum PAGE_e page, unsigned int addr, unsigned char *x)
{
    int rc = RIG_OK;
    unsigned char v = RDD(1);   // Read command

    assert(NULL != rig);
    assert(NULL != x);

    rc = setAddr(rig, page, addr);

    if (RIG_OK == rc)
    {
        rc = -RIG_EIO;

        if (0 == write_block(&rig->state.rigport, &v, 1))
        {
            if (1 == read_block(&rig->state.rigport, x, 1))
            {
                curAddr++;
                rc = RIG_OK;

                rig_debug(RIG_DEBUG_VERBOSE, "%s: read 0x%02x\n", __func__, *x);
            }
        }
    }

    return (rc);
}

/*
 * /brief Read an unsigned short (two bytes) from the receiver
 *
 * /param rig Pointer to rig struct
 * /param page Memory page number (0-4, 15)
 * /param addr Address offset within page (0-4095, depending on page)
 * /param x    Pointer to value to read from radio
 *
 * \return RIG_OK on success, error code on failure
 *
 */
int readShort(RIG *rig, enum PAGE_e page, unsigned int addr, unsigned short *x)
{
    int rc = RIG_OK;
    unsigned char v;

    assert(NULL != rig);
    assert(NULL != x);

    rc = readByte(rig, page, addr, &v);

    if (RIG_OK == rc)
    {
        *x = (unsigned short) v << 8;
        rc = readByte(rig, page, addr + 1, &v);

        if (RIG_OK == rc)
        {
            *x += (unsigned short) v;

            rig_debug(RIG_DEBUG_VERBOSE, "%s: read 0x%04x\n", __func__, *x);
        }
    }

    return (rc);
}

/*
 * /brief Read an unsigned int (three bytes) from the receiver
 *
 * /param rig Pointer to rig struct
 * /param page Memory page number (0-4, 15)
 * /param addr Address offset within page (0-4095, depending on page)
 * /param x    Pointer to value to read from radio
 *
 * \return RIG_OK on success, error code on failure
 *
 */
int read3Bytes(RIG *rig, enum PAGE_e page, unsigned int addr, unsigned int *x)
{
    int rc = RIG_OK;
    unsigned char v;

    assert(NULL != rig);
    assert(NULL != x);

    rc = readByte(rig, page, addr, &v);

    if (RIG_OK == rc)
    {
        *x = (unsigned int) v << 16;
        rc = readByte(rig, page, addr + 1, &v);

        if (RIG_OK == rc)
        {
            *x += (unsigned int) v << 8;
            rc = readByte(rig, page, addr + 2, &v);

            if (RIG_OK == rc)
            {
                *x += (unsigned int) v;

                rig_debug(RIG_DEBUG_VERBOSE, "%s: read 0x%06x\n", __func__, *x);
            }
        }
    }

    return (rc);
}

#ifdef XXREMOVEDXX
// this function is not referenced anywhere
/*
 * /brief Read an unsigned int (four bytes) from the receiver
 *
 * /param rig Pointer to rig struct
 * /param page Memory page number (0-4, 15)
 * /param addr Address offset within page (0-4095, depending on page)
 * /param x    Pointer to value to read from radio
 *
 * \return RIG_OK on success, error code on failure
 *
 */
int readInt(RIG *rig, enum PAGE_e page, unsigned int addr, unsigned int *x)
{
    int rc = 0;
    unsigned char v;

    assert(NULL != rig);
    assert(NULL != x);

    rc = readByte(rig, page, addr, &v);

    if (RIG_OK == rc)
    {
        *x = (unsigned int) v << 24;
        rc = readByte(rig, page, addr + 1, &v);

        if (RIG_OK == rc)
        {
            *x += (unsigned int) v << 16;
            rc = readByte(rig, page, addr + 2, &v);

            if (RIG_OK == rc)
            {
                *x += (unsigned int) v << 8;
                rc = readByte(rig, page, addr + 3, &v);
                {
                    *x += (unsigned int) v;

                    rig_debug(RIG_DEBUG_VERBOSE, "%s: read 0x%08x\n", __func__, *x);
                }
            }
        }
    }

    return (rc);
}
#endif

/*
 * /brief Read raw AGC value from the radio
 *
 * /param rig Pointer to rig struct
 *
 * \return RIG_OK on success, error code on failure
 */
int readSignal(RIG *rig, unsigned char *x)
{
    int rc;

    assert(NULL != rig);
    assert(NULL != x);

    rc = execRoutine(rig, READ_SIGNAL);   // Read raw AGC value

    if (RIG_OK == rc)
    {
        if (1 == read_block(&rig->state.rigport, x, 1))
        {
            rc = RIG_OK;

            rig_debug(RIG_DEBUG_VERBOSE, "%s: raw AGC %03d\n", __func__, *x);
        }
    }

    return (rc);
}

#ifdef XXREMOVEDXX
// this function is not referenced anywhere
/*
 * /brief Flush I/O with radio
 *
 * /param rig Pointer to rig struct
 *
 */
int flushBuffer(RIG *rig)
{
    int rc = -RIG_EIO;
    char v = '/';

    assert(NULL != rig);

    if (0 == write_block(&rig->state.rigport, &v, 1))
    {
        rc = RIG_OK;
    }

    return (rc);
}
#endif

/*
 * /brief Lock receiver for remote operations
 *
 * /param rig Pointer to rig struct
 * /param level Lock level (0-3)
 *
 */
int lockRx(RIG *rig, enum LOCK_LVL_e level)
{
    int rc = -RIG_EIO;
    unsigned char v;

    assert(NULL != rig);

    if (LOCK_NONE > level)   /* valid level? */
    {
        if (curLock != level)   /* need to change level? */
        {
            v = LOC(level);

            if (0 == write_block(&rig->state.rigport, &v, 1))
            {
                rc = RIG_OK;

                curLock = level;
            }
        }
        else
        {
            rc = RIG_OK;
        }
    }
    else
    {
        rc = -RIG_EINVAL;
    }

    return (rc);
}

/*
 * \brief Convert one byte BCD value to int
 *
 * \param bcd BCD value (0-99)
 *
 * \return Integer value of BCD parameter (0-99), -1 on failure
 */
int bcd2Int(const unsigned char bcd)
{
    int rc = -1;
    unsigned char hi = ((bcd & 0xf0) >> 4);
    unsigned char lo = (bcd & 0x0f);

    if ((unsigned char) 0x0a > hi)
    {
        rc = (int) hi * 10;

        if ((unsigned char) 0x0a > lo)
        {
            rc += (int) lo;
        }
        else
        {
            rc = -1;
        }
    }

    return (rc);
}

/*
 * \brief Convert raw AGC value to calibrated level in dBm
 *
 * \param rig Pointer to rig struct
 * \param rawAgc raw AGC value (0-255)
 * \param tab Pointer to calibration table struct
 * \param dbm Pointer to value to hold calibrated level (S9 = 0 dBm)
 *
 * \return RIG_OK on success, error code on failure
 *
 * To calculate the signal level, table values should be subtracted from
 * the AGC voltage in turn until a negative value would result. This gives
 * the rough level from the table position. The accuracy can be improved by
 * proportioning the remainder into the next table step. See the following
 * example :-
 *
 * A read signal strength operation returns a value of 100
 * Subtract cal byte 1 (64) leaves 36 level > -113dBm
 * Subtract cal byte 2 (10) leaves 26 level > -103dBm
 * Subtract cal byte 3 (10) leaves 16 level > -93dBm
 * Subtract cal byte 4 (12) leaves 4 level > -83dBm
 * Test cal byte 5 (12) - no subtraction
 * Fine adjustment value = (remainder) / (cal byte 5) * (level step)
 *                       = 4 / 12 * 10 = 3dB
 * Signal level = -83dBm + 3dB = -80dB
 *
 * The receiver can operate the RF attenuator automatically if the signal
 * level is likely to overload the RF stages. Reading the RFAGC byte (page 0,
 * location 49) gives the attenuation in 10dB steps. This value should be
 * read and added to the value calculated above.
 */
int getCalLevel(RIG *rig, unsigned char rawAgc, int *dbm)
{
    int rc = RIG_OK;
    int i;
    int raw = (int) rawAgc;
    int step;
    unsigned char v;

    assert(NULL != rig);
    assert(NULL != dbm);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: raw AGC %03d\n", __func__, rawAgc);

    for (i = 0; i < rig->state.str_cal.size; i++)
    {
        *dbm = rig->state.str_cal.table[ i ].val;

        rig_debug(RIG_DEBUG_VERBOSE, "%s: got cal table[ %d ] dBm value %d\n", __func__,
                  i, *dbm);

        /* if the remaining difference in the raw value is negative */
        if (0 > (raw - rig->state.str_cal.table[ i ].raw))
        {
            /* calculate step size */
            if (0 < i)
            {
                step = rig->state.str_cal.table[ i ].val -
                       rig->state.str_cal.table[ i - 1 ].val;
            }
            else
            {
                step = 20; /* HACK - try and fix minimum AGC readings */
            }

            rig_debug(RIG_DEBUG_VERBOSE, "%s: got step size %d\n", __func__, step);

            /* interpolate the final value */
            *dbm -= step; /* HACK - table seems to be off by one index */
            *dbm += (int)(((double) raw / (double) rig->state.str_cal.table[ i ].raw) *
                          (double) step);

            rig_debug(RIG_DEBUG_VERBOSE, "%s: interpolated dBm value %d\n", __func__, *dbm);

            /* we're done, stop going through the table */
            break;
        }
        else
        {
            /* calculate the remaining raw value */
            raw = raw - rig->state.str_cal.table[ i ].raw;

            rig_debug(RIG_DEBUG_VERBOSE, "%s: residual raw value %d\n", __func__, raw);
        }
    }

    /* Factor in Attenuator/preamp settings */
    /* 40 0x028  rxcon 3 bytes  Receiver control register mapping */
    rc = readByte(rig, WORKING, RXCON, &v);

    if (RIG_OK == rc)
    {
        if (0x80 & v)   /* byte 1 bit 7  rx_atn        Attenuator enable */
        {
            if (0xa0 & v)
            {
                /* HACK - Settings menu on radio says Atten step is 10 dB, not 20 dB */
                *dbm += 20; /* byte 1 bit 5  rx_atr        Atten : 0 = 20dB / 1 = 40dB */
            }
            else
            {
                *dbm += 10; /* byte 1 bit 5  rx_atr        Atten : 0 = 20dB / 1 = 40dB */
            }
        }

        if (0x10 & v)   /* byte 1 bit 4  rx_pre        Preamplifier enable */
        {
            *dbm -= 10;
        }

        rig_debug(RIG_DEBUG_VERBOSE, "%s: RXCON 0x%02x, adjusted dBm value %d\n",
                  __func__, (int) v, *dbm);
    }

    /* Adjust to S9 == 0 scale */
    *dbm += 73; /* S9 == -73 dBm */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: S9 adjusted dBm value %d\n", __func__, *dbm);

    return (rc);
}

/*
 * \brief Get bandwidth of given filter
 *
 * \param rig Pointer to rig struct
 * \param filter Filter number (1-6)
 *
 * \return Filter bandwidth in Hz, -1 on failure
 */
int getFilterBW(RIG *rig, enum FILTER_e filter)
{
    int rc;
    unsigned char bw;

    rc = readByte(rig, BBRAM, (FL_BW + ((filter - 1) * 4)), &bw);

    if (RIG_OK == rc)
    {
        rc = bcd2Int(bw) * 100;
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: readByte err: %s\n", __func__, strerror(rc));
        return rc;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: filter %1d BW %5d\n", __func__, filter, rc);

    return (rc);
}

/*
 * /brief Convert DDS steps to frequency in Hz
 *
 * /param steps DDS count
 *
 * /return Frequency in Hz or 0 on failure
 */
freq_t ddsToHz(const unsigned int steps)
{
    freq_t rc = 0.0;

    rc = ((freq_t) steps * 44545000.0 / 16777216.0);

    return (rc);
}

/*
 * /brief Convert frequency in Hz to DDS steps
 *
 * /param freq Frequency in Hz
 *
 * /return DDS steps (24 bits) or 0 on failure
 */
unsigned int hzToDDS(const freq_t freq)
{
    unsigned int rc = 0;
    double err[3] = { 0.0, 0.0, 0.0 };

    rc = (unsigned int)(freq * 16777216.0 / 44545000.0);

    /* calculate best DDS count based on bletcherous,
       irrational tuning step of 2.65508890151977539062 Hz/step
       (actual ratio is 44545000.0 / 16777216.0) */
    err[ 0 ] = fabs(freq - ddsToHz((rc - 1)));
    err[ 1 ] = fabs(freq - ddsToHz(rc));
    err[ 2 ] = fabs(freq - ddsToHz((rc + 1)));

    if (err[ 0 ] < err[ 1 ] && err[ 0 ] < err[ 2 ])
    {
        rc--;
    }
    else if (err[ 2 ] < err[ 1 ] && err[ 2 ] < err[ 0 ])
    {
        rc++;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: err[0 - 2] = %f %f %f rc 0x%08x\n",
              __func__, err[ 0 ], err[ 1 ], err[ 2 ], rc);

    return (rc);
}

/*
 * /brief Convert PBS/BFO steps to frequency in Hz
 *
 * /param steps PBS/BFO offset steps
 *
 * /return Frequency in Hz or 0 on failure
 *
 * Max +ve offset is 127, max -ve offset is 128
 * Min -ve offset is 255
 */
float pbsToHz(const unsigned char steps)
{
    freq_t rc = 0.0;

    /* treat steps as a 1's complement signed 8-bit number */
    if (128 > steps)
    {
        rc = (((float) steps * 12.5 * 44545000.0) / 16777216.0);
    }
    else
    {
        rc = (((float)(~steps & 0x7f) * -12.5 * 44545000.0) / 16777216.0);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: raw %d hz %f\n", __func__, steps, rc);

    return (rc);
}

#ifdef XXREMOVEDXX
// this function is not referenced anywhere
/*
 * /brief Convert PBS/BFO offset frequency in Hz to steps
 *
 * /param freq Offset frequency in Hz
 *
 * /return steps (8 bits) or 0 on failure
 */
unsigned char hzToPBS(const float freq)
{
    unsigned char rc;
    int steps;

    if (0 < freq)
    {
        steps = (((freq + 0.5) * 16777216.0) / (44545000.0 * 12.5));
    }
    else
    {
        steps = (((freq - 0.5) * 16777216.0) / (44545000.0 * 12.5));
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: steps %d\n", __func__, steps);

    if (0 <= steps)
    {
        rc = (unsigned char)(steps & 0x7f);
    }
    else if (-128 < steps)
    {
        rc = (unsigned char)(steps + 255);
    }
    else
    {
        rc = (unsigned char) 0;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: hz %f rc %d\n", __func__, freq, rc);

    return (rc);
}
#endif

/*
 * /brief Convert native Mode to Hamlib mode
 *
 * /param mode Native mode value
 *
 * /return Hamlib mode value
 */
rmode_t modeToHamlib(const unsigned char mode)
{
    rmode_t rc = RIG_MODE_NONE;

    switch (mode)
    {
    case AM:
        rc = RIG_MODE_AM;
        break;

    case SAM:
        rc = RIG_MODE_AMS;
        break;

    case FM:
        rc = RIG_MODE_FM;
        break;

    case DATA:
        rc = RIG_MODE_RTTY;
        break;

    case CW:
        rc = RIG_MODE_CW;
        break;

    case LSB:
        rc = RIG_MODE_LSB;
        break;

    case USB:
        rc = RIG_MODE_USB;
        break;

    default:
        break;
    };

    rig_debug(RIG_DEBUG_VERBOSE, "%s: Native %s, Hamlib %s\n",
              __func__, rig_strrmode(mode), rig_strrmode(rc));

    return (rc);
}

/*
 * /brief Convert Hamlib Mode to native mode
 *
 * /param mode Hamlib mode value
 *
 * /return Native mode value
 */
unsigned char modeToNative(const rmode_t mode)
{
    unsigned char rc = (unsigned char) MODE_NONE;

    switch (mode)
    {
    case RIG_MODE_AM:
        rc = (unsigned char) AM;
        break;

    case RIG_MODE_AMS:
        rc = (unsigned char) SAM;
        break;

    case RIG_MODE_FM:
        rc = (unsigned char) FM;
        break;

    case RIG_MODE_RTTY:
        rc = (unsigned char) DATA;
        break;

    case RIG_MODE_CW:
        rc = (unsigned char) CW;
        break;

    case RIG_MODE_LSB:
        rc = (unsigned char) LSB;
        break;

    case RIG_MODE_USB:
        rc = (unsigned char) USB;
        break;

    default:
        break;
    };

    rig_debug(RIG_DEBUG_VERBOSE, "%s: Hamlib %s, native %d\n",
              __func__, rig_strrmode(mode), rc);

    return (rc);
}

/*
 * /brief Convert native AGC speed to Hamlib AGC speed
 *
 * /param agc Native AGC speed value
 *
 * /return Hamlib AGC speed value
 */
enum agc_level_e agcToHamlib(const unsigned char agc)
{
    enum agc_level_e rc = RIG_AGC_AUTO;

    switch (agc)
    {
    case AGC_FAST:
        rc = RIG_AGC_FAST;
        break;

    case AGC_MED:
        rc = RIG_AGC_MEDIUM;
        break;

    case AGC_SLOW:
        rc = RIG_AGC_SLOW;
        break;

    case AGC_OFF:
        rc = RIG_AGC_OFF;
        break;

    default:
        break;
    };

    rig_debug(RIG_DEBUG_VERBOSE, "%s: Native %d, Hamlib %d\n",
              __func__, agc, rc);

    return (rc);
}

/*
 * /brief Convert Hamlib AGC speed to native AGC speed
 *
 * /param agc Hamlib AGC speed value
 *
 * /return Native AGC speed value
 */
unsigned char agcToNative(const enum agc_level_e agc)
{
    unsigned char rc = (unsigned char) AGC_NONE;

    switch (agc)
    {
    case RIG_AGC_OFF:
        rc = (unsigned char) AGC_OFF;
        break;

    case RIG_AGC_FAST:
        rc = (unsigned char) AGC_FAST;
        break;

    case RIG_AGC_SLOW:
        rc = (unsigned char) AGC_SLOW;
        break;

    case RIG_AGC_MEDIUM:
        rc = (unsigned char) AGC_MED;
        break;

    case RIG_AGC_SUPERFAST:
    case RIG_AGC_USER:
    case RIG_AGC_AUTO:
    default:
        rc = (unsigned char) AGC_NONE;
        break;
    };

    rig_debug(RIG_DEBUG_VERBOSE, "%s: Hamlib %d, native %d\n",
              __func__, agc, rc);

    return (rc);
}

/*
 * /brief Get page size
 *
 * /param page Page to get size of
 *
 * /return Page size, -1 on error
 */
int pageSize(const enum PAGE_e page)
{
    int rc = -1;

    if ((WORKING <= page) && (EEPROM3 >= page))
    {
        rc = (int) AR7030_PAGE_SIZE[ page ];
    }
    else if (ROM == page)
    {
        rc = (int) AR7030_PAGE_SIZE[ page ];
    }
    else
    {
        rc = -1;
    }

    return (rc);
}

/*
 * /brief Set and execute IR controller code
 *
 * /param code IR code to execute
 *
 * \return RIG_OK on success, error code on failure
 */
int sendIRCode(RIG *rig, enum IR_CODE_e code)
{
    int rc;
    unsigned char v = (unsigned char) code;

    assert(NULL != rig);

    rc = writeByte(rig, WORKING, IRCODE, v);

    if (RIG_OK == rc)
    {
        rc = execRoutine(rig, SET_ALL);

        if (RIG_OK == rc)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: set IR code %d\n", __func__, code);
        }
    }

    return (rc);
}

