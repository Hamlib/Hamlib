/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * yaesu.h - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *           (C) Stephane Fillod 2001-2010
 *
 * Common yaesu declarations for hamlib
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

#ifndef _YAESU_H
#define _YAESU_H 1

#include "idx_builtin.h"

/*
 * Length (in bytes) of yaesu command sequences
 */

#define YAESU_CMD_LENGTH                     5

/*
 * Basic Data structure for yaesu native cmd set
 */

struct yaesu_cmd_set {
  unsigned char ncomp;		        /* 1 = complete, 0 = incomplete, needs extra info */
  unsigned char nseq[YAESU_CMD_LENGTH];	/* native cmd sequence */
};

typedef struct yaesu_cmd_set yaesu_cmd_set_t;

extern const struct rig_caps ft100_caps;
extern const struct rig_caps ft450_caps;
extern struct rig_caps ft450d_caps;
extern const struct rig_caps ft736_caps;
extern const struct rig_caps ft747_caps;
extern const struct rig_caps ft757gx_caps;
extern const struct rig_caps ft757gx2_caps;
extern const struct rig_caps ft600_caps;
extern const struct rig_caps ft767gx_caps;
extern const struct rig_caps ft817_caps;
extern const struct rig_caps ft857_caps;
extern const struct rig_caps ft897_caps;
extern const struct rig_caps ft897d_caps;
extern const struct rig_caps ft847_caps;
extern const struct rig_caps ft840_caps;
extern const struct rig_caps ft890_caps;
extern const struct rig_caps ft891_caps;
extern const struct rig_caps ft900_caps;
extern const struct rig_caps ft920_caps;
extern const struct rig_caps ft950_caps;
extern const struct rig_caps ft980_caps;
extern const struct rig_caps ft990_caps;
extern const struct rig_caps ft990uni_caps;
extern const struct rig_caps ft991_caps;
extern const struct rig_caps ft1000mp_caps;
extern const struct rig_caps ft1000mpmkv_caps;
extern const struct rig_caps ft1000mpmkvfld_caps;
extern const struct rig_caps ft1000d_caps;
extern const struct rig_caps ft2000_caps;
extern const struct rig_caps ftdx3000_caps;
extern const struct rig_caps ftdx5000_caps;
extern const struct rig_caps ft9000_caps;
extern const struct rig_caps frg100_caps;
extern const struct rig_caps frg8800_caps;
extern const struct rig_caps frg9600_caps;
extern const struct rig_caps vr5000_caps;
extern const struct rig_caps vx1700_caps;
extern const struct rig_caps ftdx1200_caps;
extern const struct rig_caps ft847uni_caps;
extern const struct rig_caps ftdx101d_caps;
extern const struct rig_caps ft818_caps;
extern const struct rig_caps ftdx10_caps;
extern const struct rig_caps ftdx101mp_caps;
extern const struct rig_caps mchfqrp_caps;
extern const struct rig_caps ft650_caps;
extern const struct rig_caps ft710_caps;

#endif /* _YAESU_H */
