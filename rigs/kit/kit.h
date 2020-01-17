/*
 *  Hamlib KIT backend - main header
 *  Copyright (c) 2004-2012 by Stephane Fillod
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

#ifndef _KIT_H
#define _KIT_H 1

#include "hamlib/rig.h"

extern const struct rig_caps elektor304_caps;
extern const struct rig_caps elektor507_caps;
extern const struct rig_caps si570avrusb_caps;
extern const struct rig_caps si570picusb_caps;
extern const struct rig_caps si570peaberry1_caps;
extern const struct rig_caps si570peaberry2_caps;
extern const struct rig_caps drt1_caps;
extern const struct rig_caps dwt_caps;
extern const struct rig_caps usrp0_caps;
extern const struct rig_caps usrp_caps;
extern const struct rig_caps dds60_caps;
extern const struct rig_caps miniVNA_caps;
extern const struct rig_caps funcube_caps;
extern const struct rig_caps funcubeplus_caps;
extern const struct rig_caps fifisdr_caps;
extern const struct rig_caps hiqsdr_caps;
extern const struct rig_caps fasdr_caps;
extern const struct rig_caps rshfiq_caps;

extern const struct rot_caps pcrotor_caps;

#endif /* _KIT_H */
