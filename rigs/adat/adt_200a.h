// ---------------------------------------------------------------------------
//    ADT-200A
// ---------------------------------------------------------------------------
//
//  adt_200a.h
//
//  Created by Frank Goenninger DG1SBG.
//  Copyright © 2011, 2012 Frank Goenninger.
//
//   This library is free software; you can redistribute it and/or
//   modify it under the terms of the GNU Lesser General Public
//   License as published by the Free Software Foundation; either
//   version 2.1 of the License, or (at your option) any later version.
//
//   This library is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//   Lesser General Public License for more details.
//
//   You should have received a copy of the GNU Lesser General Public
//   License along with this library; if not, write to the Free Software
//   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA


#if !defined( __ADT_200A_INCLUDED__ )
#define __ADT_200A_INCLUDED__

// ---------------------------------------------------------------------------
//    ADAT INCLUDES
// ---------------------------------------------------------------------------

#include "adat.h"

// ---------------------------------------------------------------------------
//    ADT-200A USB DEFINITIONS
// ---------------------------------------------------------------------------

#define ADT_200A_VENDOR_ID                      0x0403
#define ADT_200A_PRODUCT_ID                     0x6001

#define ADT_200A_VENDOR_NAME                    "FTDI"
#define ADT_200A_PRODUCT_NAME                   "TRX3C Serial C945D5B"

#define ADT_200A_USB_INTERFACE_NR               0x00
#define ADT_200A_USB_CONFIGURATION_VALUE        0x01
#define ADT_200A_ALTERNATE_SETTIMG              0x00

#define ADT_200A_USB_INPUT_ENDPOINT             0x81
#define ADT_200A_USB_INPUT_MAX_PACKET_SIZE      64

#define ADT_200A_USB_OUTPUT_ENDPOINT            b0x02
#define ADT_200A_USB_OUTPUT_MAX_PACKET_SIZE     64

// ---------------------------------------------------------------------------
//    ADT-200A CAPS DEFINITIONS
// ---------------------------------------------------------------------------

#define ADT_200A_GET_LEVEL                      \
    (                                           \
        RIG_LEVEL_PREAMP |                      \
        RIG_LEVEL_ATT |                         \
        RIG_LEVEL_AF |                          \
        RIG_LEVEL_NR |                          \
        RIG_LEVEL_CWPITCH |                     \
        RIG_LEVEL_RFPOWER |                     \
        RIG_LEVEL_MICGAIN |                     \
        RIG_LEVEL_KEYSPD |                      \
        RIG_LEVEL_METER |                       \
        RIG_LEVEL_BKIN_DLYMS |                  \
        RIG_LEVEL_RAWSTR |                      \
        RIG_LEVEL_SWR |                         \
        RIG_LEVEL_ALC )

#define ADT_200A_SET_LEVEL                      \
    (                                           \
        RIG_LEVEL_PREAMP |                      \
        RIG_LEVEL_ATT |                         \
        RIG_LEVEL_AF |                          \
        RIG_LEVEL_NR |                          \
        RIG_LEVEL_CWPITCH |                     \
        RIG_LEVEL_RFPOWER |                     \
        RIG_LEVEL_MICGAIN |                     \
        RIG_LEVEL_KEYSPD |                      \
        RIG_LEVEL_METER |                       \
        RIG_LEVEL_BKIN_DLYMS |                  \
        RIG_LEVEL_ALC )

#define ADT_200A_MODES                          \
    (                                           \
        RIG_MODE_AM |                           \
        RIG_MODE_CW |                           \
        RIG_MODE_USB |                          \
        RIG_MODE_LSB |                          \
        RIG_MODE_FM |                           \
        RIG_MODE_CWR |                          \
        RIG_MODE_SAL |                          \
        RIG_MODE_SAH )

// ADT-200A VFO #defines

#define ADT_200A_FRA RIG_VFO_N(0)
#define ADT_200A_FRB RIG_VFO_N(1)
#define ADT_200A_FRC RIG_VFO_N(2)

#define ADT_200A_VFO (ADT_200A_FRA|ADT_200A_FRB|ADT_200A_FRC)

#define ADT_200A_RIT 9999
#define ADT_200A_XIT 9999

// This is more-than-likely not accurate
#define ADT_200A_STR_CAL {9, {\
                       { 0, -60},\
                       { 3, -48},\
                       { 6, -36},\
                       { 9, -24},\
                       {12, -12},\
                       {15,   0},\
                       {20,  20},\
                       {25,  40},\
                       {30,  60}}\
                       }


// ADT-200A FUNCs

#define ADT_200A_FUNCS (RIG_FUNC_VOX|RIG_FUNC_NB|RIG_FUNC_NR)

// ---------------------------------------------------------------------------
//    END OF FILE
// ---------------------------------------------------------------------------

#endif
