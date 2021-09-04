/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ft817.h - (C) Chris Karpinsky 2001 (aa1vl@arrl.net)
 * This shared library provides an API for communicating
 * via serial interface to an FT-817 using the "CAT" interface.
 * The starting point for this code was Frank's ft847 implementation.
 *
 * Then, Tommi OH2BNS improved the code a lot in the framework of the
 * FT-857 backend. These improvements have now (August 2005) been
 * copied back and adopted for the FT-817.
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

#ifndef _FT817_H
#define _FT817_H 1

/*
 * No need to wait between written characters.
 */
#define FT817_WRITE_DELAY       1

/*
 * Wait 'delay' milliseconds after writing a command sequence.
 *
 * Setting this to zero means no delay but wait for an acknowledgement
 * from the rig after a command. This is undocumented but seems to work.
 * It's also the most optimal way as long as it works...
 *
 * A non-zero value disables waiting for the ack. Processing a command
 * seems to take about 60 ms so set this to 80 or so to be safe.
 */
#define FT817_POST_WRITE_DELAY      0

/*
 * Read timeout.
 */
#define FT817_TIMEOUT           3000

/*
 * Return from TX to RX may have a delay. If status is not changed
 * on the first attempt, wait this amount of milliseconds before
 * each next next attempts.
 */
#define FT817_RETRY_DELAY       100

/*
 * The time the TX, RX and FREQ/MODE status are cached (in millisec).
 * This optimises the common case of doing eg. rig_get_freq() and
 * rig_get_mode() in a row.
 *
 * The timeout is deliberately set lower than the time taken to process
 * a single command (~ 60 ms) so that a sequence
 *
 *   rig_get_freq();
 *   rig_set_freq();
 *   rig_get_freq();
 *
 * doesn't return a bogus (cached) value in the last rig_get_freq().
 */
#define FT817_CACHE_TIMEOUT     50

int ft817_set_powerstat(RIG *rig, powerstat_t status);
int ft817_read_ack(RIG *rig);

#endif /* _FT817_H */
