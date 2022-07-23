/*
 *  Hamlib Barrett backend - main header
 *  Copyright (c) 2017 by Michael Black W9MDB
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

#ifndef _CODAN_H
#define _CODAN_H 1

#include "hamlib/rig.h"

#define BACKEND_VER "20211228"

#define EOM "\x0d"
#define TRUE 1
#define FALSE 0
// For the current implemented command set 64 is long enough
// This will need a lot more room for some channel commands like IDFA which return all channels
// But that would 9999*41 or 406KB so didn't do that right now
#define CODAN_DATA_LEN 64

extern const struct rig_caps envoy_caps;
extern const struct rig_caps ngs_caps;

struct codan_priv_data {
    char cmd_str[CODAN_DATA_LEN];       /* command string buffer */
    char ret_data[CODAN_DATA_LEN];      /* returned data--max value, most are less */
};

extern int codan_transaction(RIG *rig, char *cmd, int expected, char **result);

extern int codan_init(RIG *rig);
extern int codan_cleanup(RIG *rig);
extern int codan_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
extern int codan_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
extern int codan_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
extern int codan_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                            pbwidth_t *width);
extern int codan_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);

#endif /* _CODAN_H */
