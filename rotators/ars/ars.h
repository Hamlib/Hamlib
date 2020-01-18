/*
 *  Hamlib Rotator backend - ARS interface protocol
 *  Copyright (c) 2010 by Stephane Fillod
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

#ifndef _ROT_ARS_H
#define _ROT_ARS_H 1

#include "hamlib/rig.h"

struct ars_priv_data {
    unsigned adc_res;
    int brake_off;
    int curr_move;
    unsigned char pp_control;
    unsigned char pp_data;
#ifdef HAVE_PTHREAD
    pthread_t thread;
    int set_pos_active;
    azimuth_t target_az;
    elevation_t target_el;
#endif
};

extern const struct rot_caps rci_az_rot_caps;
extern const struct rot_caps rci_azel_rot_caps;

#endif /* _ROT_ARS_H */
