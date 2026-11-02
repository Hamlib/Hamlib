/*
 *  Hamlib Icom CI-V frame tests
 *  Copyright (c) 2026 by Mikael Nousiainen OH3BHX
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
/* SPDX-License-Identifier: LGPL-2.1-or-later */

/* Tests for the CI-V frame helpers shared by every Icom rig (rigs/icom/frame.c),
 * serial and network alike: how a reply is matched to the command it answers. */

#ifdef HAVE_CONFIG_H
#include "hamlib/config.h"
#endif

#include "acutest.h"

#include <string.h>

#include "hamlib/rig.h"
#include "frame.h"
#include "icom_defs.h"

#define RADIO 0x56   /* the IC-746's default CI-V address */

/* The reply to a get echoes the command and subcommand as sent, then carries
 * the data. Built here with make_cmd_frame(), addresses swapped as the radio
 * sends them, so the matcher is checked against the encoder it must agree
 * with -- for one-, two- and three-byte subcommands alike. */
void test_reply_to_own_command_matches(void)
{
    static const int subcmds[] =
    {
        -1, 0x00, 0x05, 0xff, 0x501, 0x502, 0x514, 0x575, 0x010203
    };
    static const unsigned char data[] = { 0x80, 0x01 };
    unsigned char frame[MAXFRAMELEN];
    size_t i;

    for (i = 0; i < sizeof(subcmds) / sizeof(subcmds[0]); i++)
    {
        int len = make_cmd_frame(frame, CTRLID, RADIO, C_CTL_MEM, subcmds[i],
                                 data, sizeof(data));

        TEST_CHECK_(icom_frame_matches_cmd(C_CTL_MEM, subcmds[i], frame, len),
                    "reply to subcmd 0x%x not matched", subcmds[i]);
    }
}

/* The regression: an IC-746 S_MEM_LCD_CONT (0x501) get. The reply carries the
 * subcommand as the two bytes 05 01, and comparing 0x501 with the single byte
 * 0x05 rejected every such reply until the read timed out. */
void test_multibyte_subcmd_reply_matches(void)
{
    static const unsigned char reply[] =
    {
        PR, PR, CTRLID, RADIO, C_CTL_MEM, 0x05, 0x01, 0x01, 0x28, FI
    };

    TEST_CHECK(icom_frame_matches_cmd(C_CTL_MEM, 0x501, reply,
                                      sizeof(reply)));
}

/* A frame answering something else is skipped, so the transaction reads on:
 * another command, a different subcommand, or a different first byte of a
 * multi-byte one. */
void test_other_replies_do_not_match(void)
{
    unsigned char frame[MAXFRAMELEN];
    int len;

    len = make_cmd_frame(frame, CTRLID, RADIO, C_CTL_MEM, 0x502, NULL, 0);
    TEST_CHECK(!icom_frame_matches_cmd(C_CTL_MEM, 0x501, frame, len));
    TEST_CHECK(!icom_frame_matches_cmd(C_CTL_MEM, 0x02, frame, len));
    TEST_CHECK(!icom_frame_matches_cmd(C_CTL_MEM, 0x602, frame, len));
    TEST_CHECK(!icom_frame_matches_cmd(C_RD_FREQ, -1, frame, len));

    len = make_cmd_frame(frame, CTRLID, RADIO, C_CTL_MEM, 0x05, NULL, 0);
    TEST_CHECK(!icom_frame_matches_cmd(C_CTL_MEM, 0x06, frame, len));
}

/* No subcommand to compare: the command alone decides. */
void test_no_subcmd_matches_on_cmd(void)
{
    unsigned char frame[MAXFRAMELEN];
    static const unsigned char freq[] = { 0x00, 0x00, 0x07, 0x14, 0x00 };
    int len = make_cmd_frame(frame, CTRLID, RADIO, C_RD_FREQ, -1, freq,
                             sizeof(freq));

    TEST_CHECK(icom_frame_matches_cmd(C_RD_FREQ, -1, frame, len));
}

/* Too short to hold a command byte: never a match, and never read past. */
void test_short_frame_does_not_match(void)
{
    static const unsigned char frame[] = { PR, PR, CTRLID, RADIO, C_CTL_MEM };

    TEST_CHECK(!icom_frame_matches_cmd(C_CTL_MEM, -1, frame, 4));
    TEST_CHECK(icom_frame_matches_cmd(C_CTL_MEM, -1, frame, 5));
}

TEST_LIST =
{
    { "reply_to_own_command_matches", test_reply_to_own_command_matches },
    { "multibyte_subcmd_reply_matches", test_multibyte_subcmd_reply_matches },
    { "other_replies_do_not_match",   test_other_replies_do_not_match },
    { "no_subcmd_matches_on_cmd",     test_no_subcmd_matches_on_cmd },
    { "short_frame_does_not_match",   test_short_frame_does_not_match },
    { NULL, NULL }
};
