/*
 *  Hamlib Interface - setting2idx for builtin constants
 *  Copyright (c) 2002-2005 by Stephane Fillod and Frank Singleton
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

#ifndef _IDX_BUILTIN_H
#define _IDX_BUILTIN_H 1

#include <hamlib/rig.h>
#include <hamlib/rotator.h>

/*
 * only for Hamlib internal use (backend caps)
 * This is a rig_setting2idx version that works for builtin_constant,
 * hence allowing its use in array initializers. The compiler simplifies
 * everything at compile time.
 *
 * struct rig_caps foo = {
 *  .level_gran =  { [LVL_PREAMP] = { .min = 0, .max = 20, .step = 10 } },
 * }
 *
 * Of course, it can't work with setting2idx_builtin(RIG_LEVEL_XX|RIG_LEVEL_YY)
 */

// This is future-proofed for 64 levels
#define setting2idx_builtin(s)  (( \
                 (s)==(1ull<<0)?0: \
                 (s)==(1ull<<1)?1: \
                 (s)==(1ull<<2)?2: \
                 (s)==(1ull<<3)?3: \
                 (s)==(1ull<<4)?4: \
                 (s)==(1ull<<5)?5: \
                 (s)==(1ull<<6)?6: \
                 (s)==(1ull<<7)?7: \
                 (s)==(1ull<<8)?8: \
                 (s)==(1ull<<9)?9: \
                 (s)==(1ull<<10)?10:   \
                 (s)==(1ull<<11)?11:   \
                 (s)==(1ull<<12)?12:   \
                 (s)==(1ull<<13)?13:   \
                 (s)==(1ull<<14)?14:   \
                 (s)==(1ull<<15)?15:   \
                 (s)==(1ull<<16)?16:   \
                 (s)==(1ull<<17)?17:   \
                 (s)==(1ull<<18)?18:   \
                 (s)==(1ull<<19)?19:   \
                 (s)==(1ull<<20)?20:   \
                 (s)==(1ull<<21)?21:   \
                 (s)==(1ull<<22)?22:   \
                 (s)==(1ull<<23)?23:   \
                 (s)==(1ull<<24)?24:   \
                 (s)==(1ull<<25)?25:   \
                 (s)==(1ull<<26)?26:   \
                 (s)==(1ull<<27)?27:   \
                 (s)==(1ull<<28)?28:   \
                 (s)==(1ull<<29)?29:   \
                 (s)==(1ull<<30)?30:   \
                 (s)==(1ull<<31)?31:   \
                 (s)==(1ull<<32)?32:   \
                 (s)==(1ull<<33)?33:   \
                 (s)==(1ull<<34)?34:   \
                 (s)==(1ull<<35)?35:   \
                 (s)==(1ull<<36)?36:   \
                 (s)==(1ull<<37)?37:   \
                 (s)==(1ull<<38)?38:   \
                 (s)==(1ull<<39)?39:   \
                 (s)==(1ull<<40)?40:   \
                 (s)==(1ull<<41)?41:   \
                 (s)==(1ull<<42)?42:   \
                 (s)==(1ull<<43)?43:   \
                 (s)==(1ull<<44)?44:   \
                 (s)==(1ull<<45)?45:   \
                 (s)==(1ull<<46)?46:   \
                 (s)==(1ull<<47)?47:   \
                 (s)==(1ull<<48)?48:   \
                 (s)==(1ull<<49)?49:   \
                 (s)==(1ull<<50)?50:   \
                 (s)==(1ull<<51)?51:   \
                 (s)==(1ull<<52)?52:   \
                 (s)==(1ull<<53)?53:   \
                 (s)==(1ull<<54)?54:   \
                 (s)==(1ull<<55)?55:   \
                 (s)==(1ull<<56)?56:   \
                 (s)==(1ull<<57)?57:   \
                 (s)==(1ull<<58)?58:   \
                 (s)==(1ull<<59)?59:   \
                 (s)==(1ull<<60)?60:   \
                 (s)==(1ull<<61)?61:   \
                 (s)==(1ull<<62)?62:   \
                 (s)==(1ull<<63)?63:   \
                 -1) \
                )

#define LVL_PREAMP        setting2idx_builtin(RIG_LEVEL_PREAMP)
#define LVL_ATT           setting2idx_builtin(RIG_LEVEL_ATT)
#define LVL_VOXDELAY      setting2idx_builtin(RIG_LEVEL_VOXDELAY)
#define LVL_AF            setting2idx_builtin(RIG_LEVEL_AF)
#define LVL_RF            setting2idx_builtin(RIG_LEVEL_RF)
#define LVL_SQL           setting2idx_builtin(RIG_LEVEL_SQL)
#define LVL_IF            setting2idx_builtin(RIG_LEVEL_IF)
#define LVL_APF           setting2idx_builtin(RIG_LEVEL_APF)
#define LVL_NR            setting2idx_builtin(RIG_LEVEL_NR)
#define LVL_PBT_IN        setting2idx_builtin(RIG_LEVEL_PBT_IN)
#define LVL_PBT_OUT       setting2idx_builtin(RIG_LEVEL_PBT_OUT)
#define LVL_CWPITCH       setting2idx_builtin(RIG_LEVEL_CWPITCH)
#define LVL_RFPOWER       setting2idx_builtin(RIG_LEVEL_RFPOWER)
#define LVL_MICGAIN       setting2idx_builtin(RIG_LEVEL_MICGAIN)
#define LVL_KEYSPD        setting2idx_builtin(RIG_LEVEL_KEYSPD)
#define LVL_NOTCHF        setting2idx_builtin(RIG_LEVEL_NOTCHF)
#define LVL_COMP          setting2idx_builtin(RIG_LEVEL_COMP)
#define LVL_AGC           setting2idx_builtin(RIG_LEVEL_AGC)
#define LVL_BKINDL        setting2idx_builtin(RIG_LEVEL_BKINDL)
#define LVL_BALANCE       setting2idx_builtin(RIG_LEVEL_BALANCE)
#define LVL_METER         setting2idx_builtin(RIG_LEVEL_METER)
#define LVL_VOXGAIN       setting2idx_builtin(RIG_LEVEL_VOXGAIN)
#define LVL_ANTIVOX       setting2idx_builtin(RIG_LEVEL_ANTIVOX)
#define LVL_SLOPE_LOW     setting2idx_builtin(RIG_LEVEL_SLOPE_LOW)
#define LVL_SLOPE_HIGH    setting2idx_builtin(RIG_LEVEL_SLOPE_HIGH)
#define LVL_BKIN_DLYMS    setting2idx_builtin(RIG_LEVEL_BKIN_DLYMS)

#define LVL_RAWSTR        setting2idx_builtin(RIG_LEVEL_RAWSTR)
#define LVL_SQLSTAT       setting2idx_builtin(RIG_LEVEL_SQLSTAT)
#define LVL_SWR           setting2idx_builtin(RIG_LEVEL_SWR)
#define LVL_ALC           setting2idx_builtin(RIG_LEVEL_ALC)
#define LVL_STRENGTH      setting2idx_builtin(RIG_LEVEL_STRENGTH)
/*#define LVL_BWC          setting2idx_builtin(RIG_LEVEL_BWC)*/

#define LVL_RFPOWER_METER setting2idx_builtin(RIG_LEVEL_RFPOWER_METER)
#define LVL_RFPOWER_METER_WATTS setting2idx_builtin(RIG_LEVEL_RFPOWER_METER_WATTS)
#define LVL_COMP_METER    setting2idx_builtin(RIG_LEVEL_COMP_METER)
#define LVL_VD_METER      setting2idx_builtin(RIG_LEVEL_VD_METER)
#define LVL_ID_METER      setting2idx_builtin(RIG_LEVEL_ID_METER)

#define LVL_NOTCHF_RAW    setting2idx_builtin(RIG_LEVEL_NOTCHF_RAW)
#define LVL_MONITOR_GAIN  setting2idx_builtin(RIG_LEVEL_MONITOR_GAIN)

#define LVL_NB            setting2idx_builtin(RIG_LEVEL_NB)

#define LVL_BRIGHT        setting2idx_builtin(RIG_LEVEL_BRIGHT)

#define LVL_SPECTRUM_MODE       setting2idx_builtin(RIG_LEVEL_SPECTRUM_MODE)
#define LVL_SPECTRUM_SPAN       setting2idx_builtin(RIG_LEVEL_SPECTRUM_SPAN)
#define LVL_SPECTRUM_EDGE_LOW  setting2idx_builtin(RIG_LEVEL_SPECTRUM_EDGE_LOW)
#define LVL_SPECTRUM_EDGE_HIGH setting2idx_builtin(RIG_LEVEL_SPECTRUM_EDGE_HIGH)
#define LVL_SPECTRUM_SPEED     setting2idx_builtin(RIG_LEVEL_SPECTRUM_SPEED)
#define LVL_SPECTRUM_REF       setting2idx_builtin(RIG_LEVEL_SPECTRUM_REF)
#define LVL_SPECTRUM_AVG       setting2idx_builtin(RIG_LEVEL_SPECTRUM_AVG)
#define LVL_SPECTRUM_ATT       setting2idx_builtin(RIG_LEVEL_SPECTRUM_ATT)

#define LVL_USB_AF             setting2idx_builtin(RIG_LEVEL_USB_AF)
#define LVL_AGC_TIME           setting2idx_builtin(RIG_LEVEL_AGC_TIME)
#define LVL_BAND_SELECT        setting2idx_builtin(RIG_LEVEL_BAND_SELECT)
#define LVL_51            setting2idx_builtin(RIG_LEVEL_51)
#define LVL_52            setting2idx_builtin(RIG_LEVEL_52)
#define LVL_53            setting2idx_builtin(RIG_LEVEL_53)
#define LVL_54            setting2idx_builtin(RIG_LEVEL_54)
#define LVL_55            setting2idx_builtin(RIG_LEVEL_55)
#define LVL_56            setting2idx_builtin(RIG_LEVEL_56)
#define LVL_57            setting2idx_builtin(RIG_LEVEL_57)
#define LVL_58            setting2idx_builtin(RIG_LEVEL_58)
#define LVL_59            setting2idx_builtin(RIG_LEVEL_59)
#define LVL_60            setting2idx_builtin(RIG_LEVEL_60)
#define LVL_61            setting2idx_builtin(RIG_LEVEL_61)
#define LVL_62            setting2idx_builtin(RIG_LEVEL_62)
#define LVL_63            setting2idx_builtin(RIG_LEVEL_63)

#define PARM_ANN       setting2idx_builtin(RIG_PARM_ANN)
#define PARM_APO       setting2idx_builtin(RIG_PARM_APO)
#define PARM_BACKLIGHT setting2idx_builtin(RIG_PARM_BACKLIGHT)
#define PARM_BEEP      setting2idx_builtin(RIG_PARM_BEEP)
#define PARM_TIME      setting2idx_builtin(RIG_PARM_TIME)
#define PARM_BAT       setting2idx_builtin(RIG_PARM_BAT)
#define PARM_KEYLIGHT  setting2idx_builtin(RIG_PARM_KEYLIGHT)

/* Rotator levels */

#define ROT_LVL_SPEED  setting2idx_builtin(ROT_LEVEL_SPEED)


#endif  /* _IDX_BUILTIN_H */
