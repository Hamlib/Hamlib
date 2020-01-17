/*
 *  Hamlib AOR backend - AR7030 Plus description
 *  Copyright (c) 2000-2010 by Stephane Fillod & Fritz Melchert
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

#ifndef _AR7030P_H
#define _AR7030P_H 1

#include "hamlib/rig.h"
#include "token.h"

/*

AR-7030 Computer remote control protocol.

Information for firmware releases 1.1A, 1.2A, 1.4A and 1.4B

1) Remote control overview.

The AR-7030 receiver allows remote control of all of its functions by means
of a direct memory access system. A controlling computer can read and modify
the internal memory maps of the receiver to set required parameters and then
call for the receiver's control program to process the new settings. Commands
to the receiver are byte structured in binary format, so it is not possible
to control from a terminal.

All multi-byte numbers within the receiver are binary, stored MSB first.

2) Receiver frequency configuration.

Receive frequency is set by two oscillators - local and carrier. In AM and FM
modes the carrier oscillator is not used, and the final IF frequency is 455
kHz. In Sync mode the carrier oscillator is offset by +20.29kHz before mixing
with the IF.

The IF frequencies have a fixed inter-conversion frequency of 44.545MHz and,
because of the high-side local oscillator, both IF's are inverted.

The receiver controller processes the following variables to establish the
tuned frequency :-

[local offset]   Frequency shift applied to local oscillator.
[carrier offset] 455.00kHz for LSB, USB, Data and CW modes /
                 434.71kHz for Sync mode.

[filter offset]  IF Filter frequency at the (vestigial) carrier position as an
                 offset from 455kHz.

[PBS]  User set filter shift.
[BFO]  User set offset between carrier position and frequency display.
[TUNE] Receiver tuned frequency as shown on display.

The relationship between these variables and the tuning is as follows :-

[carrier offset] + [filter offset] + [PBS] + [BFO] ==> Carrier oscillator
45.000MHz + [filter offset] + [PBS]                ==> [local offset]
[TUNE] + [local offset]                            ==> Local oscillator

3) Serial data protocol.

All data transfers are at 1200 baud, No parity, 8 bits, 1 stop bit
(1200 N 8 1). There is no hardware or software flow control other than that
inherent in the command structure. The receiver can accept data at any time at
full rate provided the IR remote controller is not used or is disabled.
A maximum of one byte can be transmitted for each byte received, so data flow
into a controlling computer is appropriately limited.

Each byte sent to the receiver is a complete command - it is best thought of
as two hexadecimal digits - the first digit is the operation code, the second
digit is 4-bits of data relating to the operation. Because the receiver
operates with 8-bit bytes, intermediate 4-bit values are stored in registers
in the receiver for recombination and processing. For example to write into the
receiver's memory, the following steps would be followed:-

a) Send address high order 4-bits into H-register
b) Send address low order 4-bits and set Address register
c) Send first data byte high order 4-bits into H-register
d) Send first data byte low order 4-bits and execute Write Data Operation
e) Send second data byte high order 4-bits into H-register
f) Send second data byte low order 4-bits and execute Write Data Operation
g) Repeat (e) and (f) for each subsequent byte to be written.

4) Memory organisation.

Different memory areas in the receiver are referenced by selecting Pages -
up to 16 pages are supported.

The memory is broadly divided into 3 sections :-

a) Working memory - where all current operating variables are stored and
registers and stack are located. This memory is volatile and data is lost
when power to the receiver is removed.

b) Battery sustained memory - where duplicate parameters are stored for
retention when power is removed. This memory area is also used for storage
of filter parameters, setup memories and squelch and BFO settings for the
frequency memories and contains the real time clock registers.

c) EEPROM - where frequency, mode, filter and PBS information for the
frequency memories is stored. Additionally S-meter and IF calibration values
are stored here. This memory can be read or written to download and upload
the receiver's frequency memories, but repetitive writing should be avoided
because the memory devices will only support a finite number of write cycles.

5) Variations between A and B types and firmware revisions.
Type A firmware supports only basic receiver functions, type B extends
operations and includes support for the Notch / Noise Blanker option.
The whole of the type A memory map is retained in type B, but more
memory and operations are added for the extended functions of type B.

In the following information, circled note numbers are included to indicate
where items are specific to one type or revision of the firmware:-

    <1> Applicable to type B firmware only.
    <2> Applicable to revision 1.4 only, types A and B
    <3> Function is changed or added to in type B

6) Operation codes.
The high order 4-bits of each byte sent to the receiver is the operation code,
the low order 4-bits is data (shown here as x) :-

Code Ident    Operation
0x   NOP      No Operation

3x   SRH      Set H-register            x => H-register (4-bits)

5x   PGE      Set page                  x => Page register (4-bits)

4x   ADR      Set address             0Hx => Address register (12-bits)
                                        0 => H-register

1x   ADH      Set address high          x => Address register (high 4-bits)

6x   WRD      Write data               Hx => [Page, Address]
                     Address register + 1 => Address register
                                        0 => H-register,
                                        0 => Mask register

9x   MSK <1>  Set mask                 Hx => Mask register
                                        0 => H-register
2x   EXE      Execute routine x

Ax   BUT <1>  Operate button x

7x   RDD      Read data   [Page, Address] => Serial output
                     Address register + x => Address register

8x   LOC      Set lock level x
*/
#if 1

#define NOP(x) (unsigned char) ( 0x00 | ( 0x0f & (x) ) )
#define SRH(x) (unsigned char) ( 0x30 | ( 0x0f & (x) ) )
#define PGE(x) (unsigned char) ( 0x50 | ( 0x0f & (x) ) )
#define ADR(x) (unsigned char) ( 0x40 | ( 0x0f & (x) ) )
#define ADH(x) (unsigned char) ( 0x10 | ( 0x0f & (x) ) )
#define WRD(x) (unsigned char) ( 0x60 | ( 0x0f & (x) ) )
#define MSK(x) (unsigned char) ( 0x90 | ( 0x0f & (x) ) )
#define EXE(x) (unsigned char) ( 0x20 | ( 0x0f & (x) ) )
#define BUT(x) (unsigned char) ( 0xa0 | ( 0x0f & (x) ) )
#define RDD(x) (unsigned char) ( 0x70 | ( 0x0f & (x) ) )
#define LOC(x) (unsigned char) ( 0x80 | ( 0x0f & (x) ) )

#endif // 0

enum OPCODE_e
{
  op_NOP = 0x00,
  op_SRH = 0x30,
  op_PGE = 0x50,
  op_ADR = 0x40,
  op_ADH = 0x10,
  op_WRD = 0x60,
  op_MSK = 0x90,
  op_EXE = 0x20,
  op_BUT = 0xa0,
  op_RDD = 0x70,
  op_LOC = 0x80
};

/*
Note that the H-register is zeroed after use, and that the high order 4-bits
of the Address register must be set (if non-zero) after the low order 8-bits.
The Address register is automatically incremented by one after a write data
operation and by x after a read data operation. When writing to any of the
EEPROM memory pages a time of 10ms per byte has to be allowed. For this reason
it is recommended that instructions SRH and WRD are always used together
(even if the SRH is not needed) since this will ensure that the EEPROM has
sufficient time to complete its write cycle.

Additionally to allow time for local receiver memory updates and SNC detector
sampling in addition to the EEPROM write cycle, it is recommended to lock the
receiver to level 2 or 3, or add a NOP instruction after each write. This is
not required for firmware revision 1.4 but locking is still recommended.

The mask operation helps with locations in memory that are shared by two
parameters and aids setting and clearing bits. The mask operates only in
Page 0.

If bits in the mask are set, then a following write operation will leave the
corresponding bits unchanged. The mask register is cleared after a write so
that subsequent writes are processed normally. Because it defaults to zero at
reset, the mask is inoperative unless specifically set.

The operate button instruction uses the same button codes as are returned
from routine 15 (see section 8), with an additional code of zero which
operates the power button, but will not  switch the receiver off. Also code
0 will switch the receiver on (from standby state).

7) Memory pages.

Page   0       Working memory           (RAM)    256 bytes.
Page   1       Battery sustained memory (RAM)    256 bytes.
Page   2       Non-volatile memory      (EEPROM) 512 bytes.
Page   3 <1>   Non-volatile memory      (EEPROM) 4096 bytes.
Page   4 <1>   Non-volatile memory      (EEPROM) 4096 bytes.
Pages  5 - 14  Not assigned.
Page  15       Receiver Ident           (ROM)    8 bytes.
*/
enum PAGE_e
{
  NONE = -1,
  WORKING = 0,
  BBRAM = 1,
  EEPROM1 = 2,
  EEPROM2 = 3,
  EEPROM3 = 4,
  ROM = 15
};

/*
The ident is divided into model number (5 bytes), software revision (2 bytes)
and type letter (1 byte).

e.g. 7030_14A => Model AR-7030, revision 1.4, type letter A.

8) Lock levels.

Level 0 Normal operation.

Level 1 IR remote control disabled.

  Front panel buttons ignored.
  Front panel spin-wheels logged but not actioned.
  Display update (frequency & S-meter) continues.

Level 2 As level 1, but display update suspended.

In revisions before 1.4 squelch operation is inhibited, which results in
no audio output after a mode change. In revision 1.4 squelch operation
continues and mode changing is as expected.

Level 3 Remote operation exclusively.

Lock level 1 is recommended during any multi-byte reads or writes of the
receiver's memory to prevent data contention between internal and remote
memory access. See also EEPROM notes in section (6)
*/
enum LOCK_LVL_e
{
    LOCK_0 = 0,
    LOCK_1 = 1,
    LOCK_2 = 2,
    LOCK_3 = 3,
    LOCK_NONE = 4
};

/*
8) Routines.

Routine  0     Reset Setup receiver as at switch-on.

Routine  1     Set frequency Program local oscillator from frequ area and
               setup RF filters and oscillator range.

Routine  2     Set mode Setup from mode byte in memory and display mode,
               select preferred filter and PBS, BFO values etc.

Routine  3     Set passband Setup all IF parameters from filter, pbsval and
               bfoval bytes.

Routine  4     Set all Set all receiver parameters from current memory values.

Routine  5 <2> Set audio Setup audio controller from memory register values.

Routine  6 <2> Set RF-IF Setup RF Gain, IF Gain and AGC speed. Also sets Notch
               Filter and Noise Blanker if these options are fitted.

Routine  7     Not assigned

Routine  8     Not assigned

Routine  9     Direct Rx control Program control register from rxcon area.

Routine 10     Direct DDS control Program local oscillator and carrier
               oscillator DDS systems from wbuff area.
               The 32-bits at wbuff control the carrier frequency,
               value is 385674.4682 / kHz. The 32 bits at wbuff+4 control
               the local osc frequency, value is 753270.4456 / MHz.

Routine 11     Display menus Display menus from menu1 and menu2 bytes.

Routine 12     Display frequency Display frequency from frequ area.

Routine 13     Display buffer Display ASCII data in wbuff area. First byte is
               display address, starting at 128 for the top line and 192
               for the bottom line. An address value of 1 clears the display.
               Data string (max length 24 characters) ends with a zero byte.

Routine 14     Read signal strength Transmits byte representing received
               signal strength (read from AGC voltage). Output is 8-bit
               binary in range 0 to 255.

Routine 15     Read buttons Transmits byte indicating state of front panel
               buttons. Output is 8-bit binary with an offset of +48
               (i.e. ASCII numbers). Buttons held continuously will only be
               registered once.
*/

enum ROUTINE_e
{
  RESET       = 0,
  SET_FREQ    = 1,
  SET_MODE    = 2,
  SET_PASS    = 3,
  SET_ALL     = 4,
  SET_AUDIO   = 5,
  SET_RFIF    = 6,
  DIR_RX_CTL  = 9,
  DIR_DDS_CTL = 10,
  DISP_MENUS  = 11,
  DISP_FREQ   = 12,
  DISP_BUFF   = 13,
  READ_SIGNAL = 14,
  READ_BTNS   = 15
};

/*
Button codes :-

               0 = None pressed        5 = RF-IF button
               1 = Mode up button      6 = Memory button
               2 = Mode down button    7 = * button
               3 = Fast button         8 = Menu button
               4 = Filter button       9 = Power button
*/
enum BUTTON_e
{
  BTN_NONE   = 0,
  BTN_UP     = 1,
  BTN_DOWN   = 2,
  BTN_FAST   = 3,
  BTN_FILTER = 4,
  BTN_RFIF   = 5,
  BTN_MEMORY = 6,
  BTN_STAR   = 7,
  BTN_MENU   = 8,
  BTN_POWER  = 9
};

/*
Note that the work buffer wbuff area in memory is used continuously by the
receiver unless lock levels 2 or 3 are invoked. Lock levels of 1 or more
should be used when reading any front panel controls to prevent erratic
results.

10) Battery sustained RAM (Memory page 1)

   Address  Ident    Length      Description
   0 0x000             13 bytes  Real time clock / timer registers :-
   0 0x000  rt_con      1 byte   Clock control register
   2 0x002  rt_sec      1 byte   Clock seconds (2 BCD digits)
   3 0x003  rt_min      1 byte   Clock minutes (2 BCD digits)
   4 0x004  rt_hrs      1 byte   Clock hours (2 BCD digits - 24 hr format)
   5 0x005  rt_dat      1 byte   Clock year (2 bits) and date (2 BCD digits)
   6 0x006  rt_mth      1 byte   Clock month (2 BCD digits - low 5 bits only)
   8 0x008  tm_con      1 byte   Timer control register
  10 0x00A  tm_sec      1 byte   Timer seconds (2 BCD digits)
  11 0x00B  tm_min      1 byte   Timer minutes (2 BCD digits)
  12 0x00C  tm_hrs      1 byte   Timer hours (2 BCD digits - 24 hr format)
  13 0x00D             15 bytes  Power-down save area :-
  13 0x00D  ph_cal      1 byte   Sync detector phase cal value
  14 0x00E  pd_slp      1 byte   Timer run / sleep time in minutes
  15 0x00F  pd_dly      1 byte   Scan delay value x 0.125 seconds
  16 0x010  pd_sst      1 byte   Scan start channel
  17 0x011  pd_ssp      1 byte   Scan stop channel
  18 0x012  pd_stp      2 bytes  Channel step size
  20 0x014  pd_sql      1 byte   Squelch
  21 0x015  pd_ifg      1 byte   IF gain
  22 0x016  pd_flg      1 byte   Flags (from pdflgs)
  23 0x017  pd_frq      3 bytes  Frequency
  26 0x01A  pd_mod <3>  1 byte   Mode (bits 0-3) and
                                   NB threshold (bits 4-7)
  27 0x01B  pd_vol <3>  1 byte   Volume (bits 0-5) and
                                   rx memory hundreds (bits 6&7)
  28 0x01C             26 bytes  Receiver setup save area :-
  28 0x01C  md_flt      1 byte   AM mode : Filter (bits 0-3) and
                                   AGC speed (bits 4-7)
  29 0x01D  md_pbs      1 byte   AM mode : PBS value
  30 0x01E  md_bfo      1 byte   AM mode : BFO value
  31 0x01F              3 bytes  Ditto for Sync mode
  34 0x022              3 bytes  Ditto for NFM mode -
                                   except Squelch instead of BFO
  37 0x025              3 bytes  Ditto for Data mode
  40 0x028              3 bytes  Ditto for CW mode
  43 0x02B              3 bytes  Ditto for LSB mode
  46 0x02E              3 bytes  Ditto for USB mode
  49 0x031  st_aud <3>  1 byte   Audio bass setting (bits 0-4)
                                   bit 5 Notch auto track enable
                                   bit 6 Ident search enable
                                   bit 7 Ident preview enable
  50 0x032              1 byte   Audio treble setting (bits 0-3) and
                                   RF Gain (bits 4-7)
  51 0x033              1 byte   Aux output level - left channel
  52 0x034              1 byte   Aux output level - right channel
  53 0x035  st_flg      1 byte   Flags (from stflgs)
  54 0x036             26 bytes  Setup memory A (configured as above)
  80 0x050             26 bytes  Setup memory B (configured as above)
 106 0x06A             26 bytes  Setup memory C (configured as above)
 132 0x084             24 bytes  Filter data area :-
 132 0x084  fl_sel      1 byte   Filter 1 : selection bits and IF bandwidth
 133 0x085  fl_bw       1 byte   Filter 1 : bandwidth (2 BCD digits, x.x kHz)
 134 0x086  fl_uso      1 byte   Filter 1 : USB offset value x 33.19Hz
 135 0x087  fl_lso      1 byte   Filter 1 : LSB offset value x 33.19Hz
 136 0x088              4 bytes  Ditto for filter 2
 140 0x08C              4 bytes  Ditto for filter 3
 144 0x090              4 bytes  Ditto for filter 4
 148 0x094              4 bytes  Ditto for filter 5
 152 0x098              4 bytes  Ditto for filter 6
 156 0x09C  mem_sq    100 bytes  Squelch / BFO values for
                                   frequency memories 0 to 99
                                   (BFO for Data and CW modes,
                                    Squelch for others)
*/
#define MAX_MEM_SQL_PAGE0 (99)

enum FILTER_e
{
  FILTER_1 = 1,
  FILTER_2 = 2,
  FILTER_3 = 3,
  FILTER_4 = 4,
  FILTER_5 = 5,
  FILTER_6 = 6
};

enum BBRAM_mem_e
{
  RT_CON = 0,
  RT_SEC = 2,
  RT_MIN = 3,
  RT_HRS = 4,
  RT_DAT = 5,
  RT_MTH = 6,
  TM_CON = 8,
  TM_SEC = 10,
  TM_MIN = 11,
  TM_HRS = 12,
  PH_CAL = 13,
  PD_SLP = 14,
  PD_DLY = 15,
  PD_SST = 16,
  PD_SSP = 17,
  PD_STP = 18,
  PD_SQL = 20,
  PD_IFG = 21,
  PD_FLG = 22,
  PD_FRQ = 23,
  PD_MOD = 26,
  PD_VOL = 27,
  MD_FLT = 28,
  MD_PBS = 29,
  MD_BFO = 30,
  ST_AUD = 49,
  ST_FLG = 53,
  FL_SEL = 132,
  FL_BW  = 133,
  FL_USO = 134,
  FL_LSO = 135,
  MEM_SQ = 156
};

/*
11) EEPROM (Memory page 2)

   Address  Ident    Length      Description
   0 0x000              4 bytes  Frequency memory data :-
   0 0x000  mem_fr      3 bytes  Memory 00 : 24-bit frequency
   3 0x003  mem_md      1 byte   bits 0 - 3 mode
                                   bits 4 - 6 filter
                                   bit 7 scan lockout
   4 0x004            396 bytes  Ditto for memories 01 to 99
 400 0x190  mem_pb    100 bytes  PBS values for frequency memories 0 to 99
 500 0x1F4  sm_cal      8 bytes  S-meter calibration values :-
 500 0x1F4              1 byte     RSS offset for S1 level
 501 0x1F5              1 byte     RSS steps up to S3 level
 502 0x1F6              1 byte     RSS steps up to S5 level
 503 0x1F7              1 byte     RSS steps up to S7 level
 504 0x1F8              1 byte     RSS steps up to S9 level
 505 0x1F9              1 byte     RSS steps up to S9+10 level
 506 0x1FA              1 byte     RSS steps up to S9+30 level
 507 0x1FB              1 byte     RSS steps up to S9+50 level
 508 0x1FC  if_cal      2 bytes  RSS offsets for -20dB
                                   and -8dB filter alignment
 510 0x1FE  if_def      1 byte   Default filter numbers for
                                   narrow and wide (2 BCD digits)
 511 0x1FF  option <1>  1 byte   Option information :-
                                   bit 0 Noise blanker
                                   bit 1 Notch filter
                                   bit 2 10 dB step attenuator (DX version)
*/
#define MAX_MEM_FREQ_PAGE2 (99)
#define MAX_MEM_PBS_PAGE2 (99)

enum EEPROM1_mem_e
{
  MEM_FR = 0,
  MEM_MD = 3,
  MEM_PB = 400,
  SM_CAL = 500,
  IF_CAL = 508,
  IF_DEF = 510,
  OPTION = 511
};

/*
12) EEPROM (Memory page 3) .

   Address  Ident    Length      Description
   0 0x000              4 bytes  Frequency memory data :-
   0 0x000  mex_fr      3 bytes  Memory 100 : 24-bit frequency
   3 0x003  mex_md      1 byte   bits 0 - 3 mode
                                   bits 4 - 6 filter
                                   bit 7 scan lockout
   4 0x004           1196 bytes  Ditto for memories 101 to 399
1200 0x4B0              8 bytes  Timer memory data :-
1200 0x4B0  mtm_mn      1 byte   Timer memory 0 : minutes (2 BCD digits)
1201 0x4B1  mtm_hr      1 byte   hours (2 BCD digits)
1202 0x4B2  mtm_dt      1 byte   date (2 BCD digits)
1203 0x4B3  mtm_mt      1 byte   month (2 BCD digits)
1204 0x4B4  mtm_ch      2 bytes  rx channel (hundreds and 0-99)
1206 0x4B6  mtm_rn      1 byte   run time
1207 0x4B7  mtm_ac      1 byte   active (0 = not active)
1208 0x4B8             72 bytes  Ditto for timer memories 1 to 9
1280 0x500             16 bytes  Frequency memory data :-
1280 0x500  mex_sq      1 byte   Memory 0 : Squelch / BFO (not used for
                                   mems 0 to 99)
                                   (BFO for Data and CW modes)
1281 0x501  mex_pb      1 byte   PBS value (not used for mems 0 to 99)
1282 0x502  mex_id     14 bytes  Text Ident
1296 0x510           2800 bytes  Ditto for memories 1 to 175
*/
#define MAX_MEM_FREQ_PAGE3 (399)
#define MAX_MEM_SQL_PAGE3 (175)
#define MAX_MEM_PBS_PAGE3 (175)
#define MAX_MEM_ID_PAGE3 (175)

enum EEPROM2_mem_e
{
  MEX_FR = 0,
  MEX_MD = 3,
  MEM_MN = 1200,
  MTM_HR = 1201,
  MTM_DT = 1202,
  MTM_MT = 1203,
  MTM_CH = 1204,
  MTM_RN = 1206,
  MTM_AC = 1207,
  MEX_SQ = 1280,
  MEX_PB = 1281,
  MEX_ID = 1282
};

/*
13) EEPROM (Memory page 4) <1>

   Address  Ident    Length      Description
   0 0x000             16 bytes  Frequency memory data :-
   0 0x000  mey_sq      1 byte   Memory 176 : Squelch / BFO
                                   (BFO for Data and CW modes)
   1 0x001  mey_pb      1 byte   PBS value
   2 0x002  mey_id     14 bytes  Text Ident
  16 0x010           3568 bytes  Ditto for memories 177 to 399
3584 0xE00  mex_hx    400 bytes  Frequency fast find index
                                   (1 byte for each memory 0 to 399)
                                   Index value is bits 9 to 16 of 24-bit
                                   frequency stored in each memory. Empty
                                   memories (frequency zero) should have
                                   a random index byte.
3984 0xF90            112 bytes  spare
*/

enum EEPROM3_mem_e
{
  MEY_SQ = 0,
  MEY_PB = 1,
  MEY_ID = 2,
  MEX_HX = 3584
};

/*
14) Working memory (Memory page 0)

Areas not specifically addressed are used as workspace by the internal
processor. - Keep out (by order).

   Address  Ident    Length      Description
  16 0x010  snphs       1 byte   Sync detector phase offset cal value
  17 0x011  slptim      1 byte   Sleep time (minutes)
  18 0x012  scnst       1 byte   Scan start channel
  19 0x013  scnsp       1 byte   Scan stop channel
  20 0x014  scndly      1 byte   Scan delay time value x 0.125 seconds
  21 0x015  chnstp      2 bytes  16-bit channel step size,
                                   value is 376.6352 / kHz
  23 0x017  sqlsav      1 byte   Squelch save value (non-fm mode)
  24 0x018  ifgain      1 byte   IF gain value (zero is max gain)
  26 0x01A  frequ       3 bytes  24-bit tuned frequency,
                                   value is 376635.2228 / MHz.
  29 0x01D  mode        1 byte   Current mode :- 1 = AM   4 = Data
                                                 2 = Sync 5 = CW
                                                 3 = NFM  6 = LSB
                                                 7 = USB
  30 0x01E             10 bytes  Audio control registers :-
  30 0x01E  af_vol      1 byte   Main channel volume (6-bits, values 15 to 63)
  31 0x01F  af_vll      1 byte   Left channel balance
                                   (5-bits, half of volume value above)
  32 0x020  af_vlr      1 byte   Right channel balance (as above)
  33 0x021  af_bas <1>  1 byte   Main channel bass
                                   (bits 0-4, values 6 to 25, 15 is flat)
     bit 5  nchtrk               Notch auto track enable
     bit 6  idauto               Ident auto search enable
     bit 7  idprev               Ident auto preview enable
  34 0x022  af_trb <3>  1 byte   Main channel treble
                                   (bits 0-3, values 2 to 10, 6 is flat)
     bit 4  nb_opt               Noise blanker menus enabled
     bit 5  nt_opt               Notch Filter menus enabled
     bit 6  step10               10 dB RF attenuator fitted
  35 0x023  af_axl      1 byte   Left aux channel level
                                   (bits 0-5, values 27 to 63)
  36 0x024  af_axr <3>  1 byte   Right aux channel level
                                   (bits 0-5, values 27 to 63)
     bit 7  nchsr                Notch search running
  37 0x025  af_axs <3>  1 byte   Aux channel source (bits 0-3)
     bit 4  nchen                Notch filter active
     bit 5  nchsig               Notch filter signal detected
     bit 6  axmut                Aux output mute
     bit 7  nchato               Notch auto tune active
  38 0x026  af_opt <3>  1 byte   Option output source (bits 0-3)
     bit 4  idover               Ident on LCD over frequency
     bit 5  idsrdn               Ident search downwards
     bit 7  idsrch               Ident search in progress
  39 0x027  af_src      1 byte   Main channel source
     bit 6  afmut                Main output mute
  40 0x028  rxcon       3 bytes  Receiver control register mapping :-
     byte 1 bit 0  rx_fs3        Filter select : FS3
     byte 1 bit 1  rx_fs2        Filter select : FS2
     byte 1 bit 2  rx_fs1        Filter select : FS1
     byte 1 bit 3  rx_fs4        Filter select : FS4
     byte 1 bit 4  rx_pre        Preamplifier enable
     byte 1 bit 5  rx_atr        Atten : 0 = 20dB / 1 = 40dB
     byte 1 bit 6  rx_rff        Input filter : 0 = HF / 1 = LF
     byte 1 bit 7  rx_atn        Attenuator enable
     byte 2 bit 0  rx_as1        AGC speed : 00 = Slow
     byte 2 bit 1  rx_as2          10 = Med
                                   11 = Fast
     byte 2 bit 2  rx_agi        AGC inhibit
     byte 2 bit 3  rx_en         LO and HET enable
     byte 2 bit 4  rx_aux        Aux relay enable
     byte 2 bit 5  rx_fs5        Filter select : FS5
     byte 2 bit 6  rx_fs6        Filter select : FS6
     byte 2 bit 7  rx_ibw        IF b/w : 0 = 4kHz / 1 = 10kHz
     byte 3 bit 0  rx_chg        Fast charge enable
     byte 3 bit 1  rx_pwr        PSU enable
     byte 3 bit 2  rx_svi        Sync VCO inhibit
     byte 3 bit 3  rx_agm        AGC mode : 0 = peak / 1 = mean
     byte 3 bit 4  rx_lr1        LO range : 00 = 17 - 30 MHz
     byte 3 bit 5  rx_lr2                   10 = 10 - 17 MHz
                                            01 = 4 - 10 MHz
                                            11 = 0 - 4 MHz
     byte 3 bit 6  rx_sbw        Sync b/w : 0 = Wide / 1 = Narrow
     byte 3 bit 7  rx_car        Car sel : 0 = AM / 1 = DDS
  43 0x02B  bits        3 bytes  General flags :-
     byte 1 bit 6  lock1         Level 1 lockout
     byte 1 bit 7  lock2         Level 2 lockout
     byte 2 bit 0  upfred        Update frequency display
     byte 2 bit 1  upmend        Update menus
     byte 2 bit 2  tune4x        Tune 4 times faster (AM & NFM)
     byte 2 bit 3  quickly       Quick tuning (fast AGC, Sync)
     byte 2 bit 4  fast          Fast tuning mode
     byte 2 bit 5  sncpt1        Auto sync - frequency lock
     byte 2 bit 6  sncpt2        Auto sync - phase lock
     byte 2 bit 7  sncal         Sync detector calibrating
     byte 3 bit 0  sqlch         Squelch active (i.e. low signal)
     byte 3 bit 1  mutsql        Mute on squelch (current setting)
     byte 3 bit 2  bscnmd        Scan mode for VFO B
     byte 3 bit 3  dualw         Dual watch active
     byte 3 bit 4  scan          Scan active
     byte 3 bit 5  memlk         Current memory scan lockout
     byte 3 bit 6  pbsclr        Enable PBS CLR from IR remote
 <2> byte 3 bit 7  memodn        MEM button scans downwards
  46 0x02E  pdflgs      1 byte   Flags saved at power-down :-
     bit 0  power                Power on
     bit 1  flock                Tuning locked
     bit 2  batop                Battery operation (for fast chg)
 <1> bit 3  nben                 Noise blanker active
 <1> bit 4  nblong               Noise blanker long pulse
  47 0x02F  stflgs      1 byte   Flags saved in setup memories :-
     bit 0  mutsav               Mute on squelch (non-fm mode)
     bit 1  mutaux               Mute aux output on squelch
     bit 2  axren                Aux relay on timer
     bit 3  axrsql               Aux relay on squelch
     bit 4  snauto               Auto sync mode
     bit 5  snarr                Sync detector narrow bandwidth
     bit 6  scanmd               Scan runs irrespective of squelch
     bit 7  autorf               RF gain auto controlled
  48 0x030  rfgain      1 byte   Current RF gain setting (0 to 5) (0=max gain)
  49 0x031  rfagc       1 byte   Current RF AGC setting (added to above)
  50 0x032  agcspd      1 byte   Current AGC speed : 0 = Fast   2 = Slow
                                                     1 = Medium 3 = Off
  51 0x033  sqlval      1 byte   Squelch value (current setting)
  52 0x034  filter      1 byte   Current filter number (1 to 6)
  53 0x035  pbsval      1 byte   PBS offset (x33.19Hz)
  54 0x036  bfoval      1 byte   BFO offset (x33.19Hz)
  55 0x037  fltofs      1 byte   Filter centre frequency offset (x33.19Hz)
  56 0x038  fltbw       1 byte   Filter bandwidth (2 BCD digits : x.x kHz)
  57 0x039  ircode:     2 bytes  Current / last IR command code
  59 0x03B  spnpos      1 byte   Misc spin-wheel movement } 0 = no movement
  60 0x03C  volpos      1 byte   Volume control movement } +ve = clockwise
  61 0x03D  tunpos      1 byte   Tuning control movement } -ve = anti-clockwise
  62 0x03E  lstbut      1 byte   Last button pressed
  63 0x03F  smval       2 bytes  Last S-meter reading (bars + segments)
  65 0x041  mestmr      1 byte   Message time-out timer
  66 0x042  rfgtmr      1 byte   RF gain delay timer
  67 0x043  updtmr      1 byte   Sustained RAM update timer
  68 0x044  agctmr      1 byte   AGC speed restore delay timer
  69 0x045  snctmr      1 byte   Auto sync refresh timer
  70 0x046  scntmr      1 byte   Scan delay timer
  71 0x047  irdly       1 byte   IR remote auto repeat delay counter
  72 0x048  runtmr      1 byte   Sleep mode timer
  73 0x049  snfrq       1 byte   Sync detector frequency offset cal value
  74 0x04A  frange      1 byte   Input / LO range
  75 0x04B  menu1 <3>   1 byte   Current left menu (type A and B menu
                                                    numbers are different)
  76 0x04C  menu2 <3>   1 byte   Current right menu (type A and B menu
                                                     numbers are different)
  77 0x04D  memno       1 byte   Current memory number
  78 0x04E  setno       1 byte   Setup / config selection - load / save
  85 0x055  mempg <1>   1 byte   Memory page (hundreds - value 0 to 3)
  86 0x056  nbthr <1>   1 byte   Noise blanker threshold (values 0 to 15)
  87 0x057  hshfr <1>   1 byte   Current tuned frequ index value
                                   (during ident search)
  88 0x058  nchtmr <1>  1 byte   Notch filter auto tune / search timer
  90 0x059  wbuff      26 bytes  Work buffer
 115 0x073  keymd       1 byte   IR remote +/- keys function
 116 0x074  keybuf     20 bytes  IR remote key input buffer
 136 0x088  frofs:      4 bytes  32-bit local osc offset
 140 0x08C  carofs      4 bytes  32-bit carrier osc offset
 144 0x090  smofs       1 byte   S-meter starting offset
 145 0x091  smscl       7 bytes  S-meter segment values
 152 0x098  ifcal       2 bytes  RSS offsets for -20 dB and
                                   -5 dB filter alignment
 154 0x09A  ifdef       1 byte   Default filter numbers for narrow and wide
                                   (2 digits)
 155 0x09B  vfo_b      22 bytes  VFO B storage area :-
 155 0x09B              1 byte     B : Scan delay time
 156 0x09C              2 bytes    B : Channel step size
 158 0x09E              1 byte     B : Squelch save value (non-fm mode)
 159 0x09F              1 byte     B : IF gain value
 160 0x0A0              1 byte         not used
 161 0x0A1              3 bytes    B : Tuned frequency
 164 0x0A4              1 byte     B : Mode
 165 0x0A5              1 byte     B : Volume
 166 0x0A6              1 byte     B : Left channel balance
 167 0x0A7              1 byte     B : Right channel balance
 168 0x0A8              1 byte     B : Bass response
 169 0x0A9              1 byte     B : Treble response
 170 0x0AA              1 byte     B : RF gain
 171 0x0AB              1 byte     B : RF AGC
 172 0x0AC              1 byte     B : AGC speed
 173 0x0AD              1 byte     B : Squelch value
 174 0x0AE              1 byte     B : Filter number
 175 0x0AF              1 byte     B : PBS offset
 176 0x0B0              1 byte     B : BFO offset
 218 0x0DA  savmnu <1>  1 byte   Saved menu 1 number during ident display
 219 0x0DB  srchm <1>   2 bytes  Ident search memory (page and number)
 222 0x0DD  idtmr <1>   1 byte   Auto ident search start timer
 223 0x0DE  nchfr <1>   2 bytes  16-bit notch filter frequency,
                                   value is 6553.6 / kHz
*/

#define HZ_PBS_STEP \
  ((44545000.0 * 25.0)/(16777216.0 * 2.0)) /* 33.1886 Hz/Step */
#define NOTCH_STEP_HZ (6.5536) /* 6.5536 Hz/Step */
#define VOL_MIN (15)
#define VOL_MAX (63)
#define BASS_MIN (6)
#define BASS_MAX (25)
#define TREB_MIN (2)
#define TREB_MAX (10)
#define AUX_MIN (27)
#define AUX_MAX (63)

enum MODE_e
{
  MODE_NONE = 0,
  AM   = 1,
  SAM  = 2,
  FM   = 3,
  DATA = 4,
  CW   = 5,
  LSB  = 6,
  USB  = 7
};

enum AGC_decay_e
{
  DECAY_SLOW = 0,
  DECAY_MED  = 2,
  DECAY_FAST = 3
};

enum LO_range_e
{
  LO_17_30 = 0,
  LO_4_10  = 1,
  LO_10_17 = 2,
  LO_0_4   = 3
};

enum AGC_spd_e
{
  AGC_NONE = -1,
  AGC_FAST = 0,
  AGC_MED  = 1,
  AGC_SLOW = 2,
  AGC_OFF  = 3
};

enum WORKING_mem_e
{
  SNPHS    = 16,
  SLPTIM   = 17,
  SCNST    = 18,
  SCNSP    = 19,
  SCNDLY   = 20,
  CHNSTP   = 21,
  SQLSAV   = 23,
  IFGAIN   = 24,
  FREQU    = 26,
  MODE     = 29,
  AF_VOL   = 30,
  AF_VLL   = 31,
  AF_VLR   = 32,
  AF_BAS   = 33,
  AF_TRB   = 34,
  AF_AXL   = 35,
  AF_AXR   = 36,
  AF_AXS   = 37,
  AF_OPT   = 38,
  AF_SRC   = 39,
  RXCON    = 40,
  BITS     = 43,
  PDFLGS   = 46,
  STFLGS   = 47,
  RFGAIN   = 48,
  RFAGC    = 49,
  AGCSPD   = 50,
  SQLVAL   = 51,
  FILTER   = 52,
  PBSVAL   = 53,
  BFOVAL   = 54,
  FLTOFS   = 55,
  FLTBW    = 56,
  IRCODE   = 57,
  SPNPOS   = 59,
  VOLPOS   = 60,
  TUNPOS   = 61,
  LSTBUT   = 62,
  SMVAL    = 63,
  MESTMR   = 65,
  RFGTMR   = 66,
  UPDTMR   = 67,
  AGCTMR   = 68,
  SNCTMR   = 69,
  SCNTMR   = 70,
  IRDLY    = 71,
  RUNTMR   = 72,
  SNFRQ    = 73,
  FRANGE   = 74,
  MENU1    = 75,
  MENU2    = 76,
  MEMNO    = 77,
  SETNO    = 78,
  MEMPG    = 85,
  NBTHR    = 86,
  HSHFR    = 87,
  NCHTMR   = 88,
  WBUFF    = 90,
  KEYMD    = 115,
  KEYBUF   = 116,
  FROFS    = 136,
  CAROFS   = 140,
  SMOFS    = 144,
  SMSCL    = 145,
  IFCAL    = 152,
  IFDEF    = 154,
  VFO_B    = 155,
  SCNDLY_B = 155,
  CHNSTP_B = 156,
  SQLSAV_B = 158,
  IFGAIN_B = 159,
  FREQU_B  = 161,
  MODE_B   = 164,
  AF_VOL_B = 165,
  AF_VLL_B = 166,
  AF_VLR_B = 167,
  AF_BAS_B = 168,
  AF_TRB_B = 169,
  RFGAIN_B = 170,
  RFAGC_B  = 171,
  AGCSPD_B = 172,
  SQLVAL_B = 173,
  FILTER_B = 174,
  PBSVAL_B = 175,
  BFOVAL_B = 176,
  SAVMNU   = 218,
  SRCHM    = 219,
  IDTMR    = 222,
  NCHFR    = 223
};

enum ROM_mem_e
{
  IDENT = 0
};

#define HZ_PER_STEP  ( 44545000.0 / 16777216.0 )  /* 2.655 Hz/Step */
#define STEPS_PER_HZ ( 16777216.0 / 44545000.0 )  /* 0.3766 Steps/Hz */
#define MAX_FREQ (32010000.0)
#define MIN_FREQ (10000.0)

/*
RS232 signal meter reading - additional comments

Several commercial organisations are using the AR7030 for signal monitoring
purposes and wish to accurately log signal meter level.  The information is
given in the RS232 PROTOCOL LISTING but the subject is fairly complex.  A
summary of the required process is given here, the text has been generated by
John Thorpe in response to a commercial request for more detailed guidance
(November 2001).

Reading the input signal strength from the AR7030 is not too difficult, but
some maths is needed to convert the level into dBm.

Each set is calibrated after manufacture and a set of S-meter calibration
values stored in EEPROM in the receiver. This means that the signal strength
readings should be quite good and consistent. I think that you should get less
than 2dB change with frequency and maybe 3dB with temperature. Initial
calibration error should be less than +/- 2dB.

I think the sets that you use have been modified for DRM use have some
changes in the IF stage. This will require that the sets are re-calibrated if
you are to get accurate results. The SM7030 service kit has a calibration
program (for PC) and is available from AOR.

The signal strength is read from the AGC voltage within the 7030 so AGC
should be switched on and RF Gain set to maximum. To read AGC voltage send
opcode 02EH (execute routine 14) and the receiver will return a single byte
value between 0 and 255 which is the measured AGC voltage.

The calibration table is stored in EEPROM, so the control software should
read this when connection to the receiver is established and store the data
in an array for computing.

Calibration data is 8 bytes long and is stored in Page2 at locations
500 (0x01F4) to 507 (0x01FB). Use the PaGE opcode (0x52) then SRH, ADR, ADH
to setup the address, then 8 RDD opcodes to read the data, as below :-

Opcode   Hex    Operation

PGE  2   0x52   Set page 2
SRH 15   0x3F   H register = 15
ADR  4   0x44   Set address 0x0F4
ADH  1   0x11   Set address 0x1F4
RDD +1   0x71   Read byte 1 of cal data
RDD +1   0x71   Read byte 2 of cal data
. . .
RDD +1   0x71   Read byte 8 of cal data

PGE  0   0x50   Return to page 0 for subsequent control operations

The first byte of calibration data holds the value of the AGC voltage for a
signal level of -113dBm (S1). Successive bytes hold the incremental values
for 10dB increases in signal level :-

Cal data   Typical Value   RF signal level

byte 1     64              -113dBm
byte 2     10              -103dBm
byte 3     10              -93dBm
byte 4     12              -83dBm
byte 5     12              -73dBm
byte 6     15              -63dBm
byte 7     30              -43dBm (note 20dB step)
byte 8     20              -23dBm (note 20dB step)
*/
#define CAL_TAB_LENGTH (8)
#define STEP_SIZE_LOW  (10)
#define STEP_SIZE_HIGH (20)

/*
To calculate the signal level, table values should be subtracted from the AGC
voltage in turn until a negative value would result. This gives the rough
level from the table position. The accuracy can be improved by proportioning
the remainder into the next table step. See the following example :-

A read signal strength operation returns a value of 100
Subtract cal byte 1 (64) leaves 36 level > -113dBm
Subtract cal byte 2 (10) leaves 26 level > -103dBm
Subtract cal byte 3 (10) leaves 16 level > -93dBm
Subtract cal byte 4 (12) leaves 4 level > -83dBm
Test cal byte 5 (12) - no subtraction
Fine adjustment value = (remainder) / (cal byte 5) * (level step)
                      = 4 / 12 * 10 = 3dB
Signal level = -83dBm + 3dB = -80dB

The receiver can operate the RF attenuator automatically if the signal level
is likely to overload the RF stages. Reading the RFAGC byte (page 0, location
49) gives the attenuation in 10dB steps. This value should be read and added
to the value calculated above.

Further discussion has taken place on the subject of PC control with the
designer, the comments may be of assistance to other operators...

As far as I can tell all of the commands and operations work exactly as
documented so when the client talks of "the set frequency command doesn't
work" they are obviously doing something wrong.

Similarly, I am unable to duplicate the effects that they notice with
changing audio settings after changing modes. There are some issues with the
parameters that they are changing which I will address later, but first they
must sort out the basic communication so that the receiver control is as
expected. Further issues cannot really be sorted until this is working
properly.

Programming issues...

Since I have no Knowledge of what programming system the client is using
these are only general comments. The receiver control is in 8-bit binary code
so any communication must maintain all 8 bits (and not truncate bit 7 as some
printer outputs do).

It is also essential that no extra characters are added to the output stream
so check that the software is not adding carriage returns, line feeds, nulls
or end-of-file markers to the output. If this might be a problem, monitor the
computer to receiver communication with a serial line monitor or another
computer running a simple RS-232 reading program.

There is some sample BASIC code in the "AR-7030 Computer remote control
protocol" document which gives subroutines that cover the commonly used
receiver settings. Use this as a starting point for your own routines. The
published routines have been thoroughly tested and work without problems.

http://www.aoruk.com/pdf/comp.pdf
http://www.aoruk.com/7030bulletin.htm#7030_rs232_s-meter

With all "buffered" RS-232 connections it is possible for the computer and
receiver to get out of step when using two-way communication. For this reason
I included some "flush input buffer" routines in the sample code. Using these
ensures that missed characters or extra characters inserted due to noise or
disconnection do not disrupt communication between the computer and receiver,
and a recovery after communications failure can be automatic.

Because the receiver's remote control is by direct access to memory and
processor it is a very flexible system but is also able to disrupt receiver
operation if incorrectly used. Only a few bytes of information stored in the
receiver's memory affect S-meter calibration and AOR (UK) hold records of
this data for each receiver made so that in the event of corruption it can be
re-programmed.

See the note that follows regarding AGC calibration.

All other working memory contents can be set to sensible values by a "Set
defaults" operation from the front panel. Most, but not all, of the working
memory is re-established by executing a remote "Reset" command (0x20) which
can be done as a last resort after control failure.

Specific parameter settings...

The client describes the correct operations for setting mode and frequency
but if, as he states, the set frequency command (021h) does not work then
this needs to be investigated. This may lead to discovering the cause of
other problems suffered by the client.

Note that changing the frequency in this way re-tunes the receiver but does
not update the display on the front panel. A "Display frequency" command is
included for this purpose.

To set the receiver main volume, three locations need to be written -
Page 0, addr 0x1e, 0x1f & 0x20. Details are in the protocol document, note the
minimum value (for zero volume) is 15. The aux channel level change is as
described by the client and after writing new values into the RAM will need
either a "Set audio" command or a "Set all" command to make the change. I can
find no reason for, nor duplicate, the effect of changing mode altering the
aux level so this effect also needs investigating - maybe the clients "write
to memory" is writing too many locations ?

To initialise several receiver parameters I would recommend locking the
receiver, writing all of the required memory data, sending a "Set all"
command and then unlocking if required. There is no need to send individual
"Set" commands after each parameter.

Unless very special requirements are needed (mainly test, setup and alignment
) the 3 rxcon locations should not be written. When a "Set all" command is
sent these will be programmed by the receiver firmware to appropriate values
for the mode, frequency and filters selected.

Only the parameters that need changing need to be written, all other values
will be maintained. The locations that the client needs to program for the
parameters he lists are as follows:-

    (all Page 0)
    frequency  frequ   0x1a 0x1b 0x1c
    mode       mode    0x1d
    volume     af_vol  0x1e 0x1f 0x20 (values=0x0f 0x07 0x07 for min volume)
    aux level  af_axl  0x23 0x24
    agc speed  agcspd  0x32
    squelch    sqlval  0x33
    filter     filter  0x34
    IF gain    ifgain  0x18
    RF gain    rfgain  0x30 (value=0x01 for no pre-amp)
    message    wbuff   0x59 (max 26 bytes)

If the required parameter values are unknown, I recommend setting the
receiver as required through the front panel controls and then reading the
value of the memory locations affected using the "read data" operation.

15) Sample routines (in MS QBASIC)

REM Sample subroutines for communication with the AR-7030 A-type
REM These subroutines use the following variables :-
REM      rx.freq#       frequency in kHz (double precision)
REM      rx.mode        mode number (1 to 7)
REM      rx.filt        filter number (1 to 6)
REM      rx.mem         memory number (0 to 99)
REM      rx.pbs         passband shift value (-4.2 to +4.2 in kHz)
REM      rx.sql         squelch value (0 to 255)
REM      ident$         -model number, revision and type

REM Subroutine to open comms link to receiver
open.link:
    open "com1:1200,n,8,1,cd0,cs0,ds0,rs" for random as #1 len = 1
    field #1, 1 as input.byte$
    return

REM Subroutine to flush QBASIC serial input buffer
flush.buffer:
    print #1,"//";
    do
        time.mark# = timer
        do while timer - time.mark# < 0.2
        loop
        if eof(1) then exit do
        get #1
    loop
    return

REM Subroutines to lock and unlock receiver controls
lock.rx:
    print #1,chr$(&H81);                     ' Set lockout level 1
    return

unlock.rx:
    print #1,chr$(&H80);                     ' Lockout level 0 (not locked)
    return

REM Subroutine to read byte from comms link
read.byte:
    read.value = -1                          ' Value assigned for read error
    time.mark# = timer
    print #1,chr$(&H71);                     ' Read byte command
    do while timer - time.mark# < 0.3
        if eof(1) = 0 then
            get #1
            read.value = asc(input.byte$)
            exit do
            end if
    loop
    return

REM Subroutine to set receiver frequency and mode
tune.rx:
    gosub lock.rx
    print #1,chr$(&H50);                     ' Select working mem (page 0)
    print #1,chr$(&H31);chr$(&H4A);          ' Frequency address = 01AH
    gosub send.freq                          ' Write frequency
    print #1,chr$(&H60+rx.mode);             ' Write mode
    print #1,chr$(&H24);                     ' Tune receiver
    gosub unlock.rx
    return

REM Subroutine to store data into receiver's frequency memory
set.memory:
    mem.loc = rx.mem+156                     ' Squelch memory origin
    mem.h = int(mem.loc/16)
    mem.l = mem.loc mod 16
    print #1,chr$(&H51);                     ' Select squelch memory (page 1)
    print #1,chr$(&H30+mem.h);
    print #1,chr$(&H40+mem.l);               ' Set memory address
    print #1,chr$(&H30+int(rx.sql/16))
    print #1,chr$(&H60+rx.sql mod 16)        ' Write squelch value
    mem.loc = rx.mem*4                       ' Frequency memory origin
    mem.t = int(mem.loc/256)
    mem.loc = mem.loc mod 256
    mem.h = int(mem.loc/16)
    mem.l = mem.loc mod 16
    print #1,chr$(&H52);                     ' Select frequency memory (page 2)
    print #1,chr$(&H30+mem.h);
    print #1,chr$(&H40+mem.l);               ' Set memory address
    print #1,chr$(&H10+mem.t);
    gosub send.freq                          ' Write frequency
    print #1,chr$(&H30+rx.filt);
    print #1,chr$(&H60+rx.mode);             ' Write filter and mode
    mem.loc = rx.mem+400-256                 ' PBS memory origin
    mem.h = int(mem.loc/16)
    mem.l = mem.loc mod 16
    pbs.val = 255 and int(rx.pbs/0.033189+0.5)
    print #1,chr$(&H30+mem.h);
    print #1,chr$(&H40+mem.l);               ' Set memory address
    print #1,chr$(&H11);
    print #1,chr$(&H30+int(pbs.val/16))
    print #1,chr$(&H60+pbs.val mod 16)       ' Write passband value
    return

REM Subroutine to read data from receiver's frequency memory
read.memory:
    mem.loc = rx.mem+156                     ' Squelch memory origin
    mem.h = int(mem.loc/16)
    mem.l = mem.loc mod 16
    print #1,chr$(&H51);                     ' Select squelch memory (page 1)
    print #1,chr$(&H30+mem.h);
    print #1,chr$(&H40+mem.l);               ' Set memory address
    gosub read.byte                          ' Read squelch value
    rx.sql = read.value
    mem.loc = rx.mem*4                       ' Frequency memory origin
    mem.t = int(mem.loc/256)
    mem.loc = mem.loc mod 256
    mem.h = int(mem.loc/16)
    mem.l = mem.loc mod 16
    print #1,chr$(&H52);                     ' Select frequency memory (page 2)
    print #1,chr$(&H30+mem.h);
    print #1,chr$(&H40+mem.l);               ' Set memory address
    print #1,chr$(&H10+mem.t);
    gosub read.freq                          ' Read frequency
    gosub read.byte                          ' Read filter and mode
    if read.value < 0 then return
    rx.filt = int(read.value/16)
    rx.mode = read.value mod 16
    mem.loc = rx.mem+400-256                 ' PBS memory origin
    mem.h = int(mem.loc/16)
    mem.l = mem.loc mod 16
    print #1,chr$(&H30+mem.h);
    print #1,chr$(&H40+mem.l);               ' Set memory address
    print #1,chr$(&H11);
    gosub read.byte                          ' Read passband value
    if read.value < 0 then return
    if read.value > 127 then read.value = 256-read.value
    rx.pbs = read.value*0.033189
    return

REM Subroutine to read receiver ident string
read.ident:
    print #1,chr$(&H5F);                     ' Select ident memory (page 15)
    print #1,chr$(&H40);                     ' Set address 0
    ident$=""
    for read.loop = 1 to 8
        gosub read.byte                      ' Read 8-byte ident
        if read.value < 0 then exit for
        ident$ = ident$+chr$(read.value)
    next read.loop
    return

REM Subroutine to send frequency (Called only from other routines)
send.freq:
    fr.val# = int(rx.freq#*376.635223+0.5)   ' Convert kHz to steps
                                             ' Exact multiplicand is
                                             ' (2^24)/44545
    print #1,chr$(&H30+int(fr.val#/1048576));
    fr.val# = fr.val# mod 1048576            ' Write frequency as 6 hex digits
    print #1,chr$(&H60+int(fr.val#/65536));
    fr.val# = fr.val# mod 65536
    print #1,chr$(&H30+int(fr.val#/4096));
    fr.val# = fr.val# mod 4096
    print #1,chr$(&H60+int(fr.val#/256));
    fr.val# = fr.val# mod 256
    print #1,chr$(&H30+int(fr.val#/16));
    print #1,chr$(&H60+(fr.val# mod 16));
    return

REM Subroutine to read frequency (Called only from other routines)
read.freq:
    fr.val# = 0
    for read.loop = 1 to 3
        gosub read.byte ' Read frequency as 3 bytes
        if read.value < 0 then exit for
        fr.val# = fr.val#*256+read.value
    next read.loop
    rx.freq# = fr.val#/376.635223 ' Convert steps to kHz
    return
*/

/*
 * (from http://www.aoruk.com/archive/pdf/ir.pdf)
 *
 * AOR AR7030 receiver infrared protocol listing
 *
 * There have been two types of IR7030 infrared hand controller employed
 * by the AR7030. Late in 2005 a VERSION 2 handset (IR7030-2) was adopted
 * in production. The protocol is slightly different, so a matching CPU
 * must be employed (firmware 1xA or 1xB uses the original IR7030,
 * firmware 2xA or 2xB uses the later IR7030-2).
 *
 * IR7030                           IR7030-2
 * NEC protocol 16 bit              NEC protocol 16 bit
 *
 * Address 026 HEX                  Address 04D HEX
 * I.R key          Hex value       I.R key          Hex value
 * 1                0C              1                11
 * 2                11              2                13
 * 3                12              3                1C
 * 4                10              4                15
 * 5                19              5                16
 * 6                1A              6                14
 * 7                18              7                19
 * 8                1D              8                17
 * 9                1E              9                1B
 * 0                15              0                1D
 * . DECIMAL        16              . DECIMAL        12
 * CLEAR            13              CLEAR            07
 * BACKSPACE        1C              BACKSPACE        1F
 * kHz              17              kHz              1A
 * MHz              1F              MHz              1E
 * CW/NFM           8               CW/NFM           0F
 * LSB/USB          0D              LSB/USB          10
 * AM/SYNC          0E              AM/SYNC          18
 * + MODIFY         2               + MODIFY         01
 * - MODIFY         6               - MODIFY         0B
 * TUNE UP          3               TUNE UP          04
 * TUNE DOWN        7               TUNE DOWN        05
 * VOLUME UP        0B              VOLUME UP        02
 * VOLUME DOWN      0F              VOLUME DOWN      03
 * PASSBAND MODIFY  0               PASSBAND MODIFY  09
 * FILTER MODIFY    1               FILTER MODIFY    08
 * BASS MODIFY      5               BASS MODIFY      0A
 * TREBLE MODIFY    14              TREBLE MODIFY    0C
 * VFO SELECT A/B   0A              VFO SELECT A/B   0E
 * MEMORY STORE     4               MEMORY STORE     0D
 * MEMORY PREVIEW   9               MEMORY PREVIEW   00
 * MEMORY RECALL    1B              MEMORY RECALL    06
 *
 * www.aoruk.com - 25.07.2006
 */

/*
 * These are the translated key codes shown in the last IR code
 * address 58 in page 0.
 */
enum IR_CODE_e
{
  IR_ONE = 0x12,
  IR_TWO = 0x14,
  IR_THREE = 0x1d,
  IR_FOUR = 0x16,
  IR_FIVE = 0x17,
  IR_SIX = 0x15,
  IR_SEVEN = 0x1a,
  IR_EIGHT = 0x18,
  IR_NINE = 0x1c,
  IR_ZERO = 0x1e,
  IR_DOT = 0x13,
  IR_CLR = 0x08,
  IR_BS = 0x20,
  IR_KHZ = 0x1b,
  IR_MHZ = 0x1f,
  IR_CWFM = 0x10,
  IR_LSBUSB = 0x11,
  IR_AMSYNC = 0x19,
  IR_PLUS = 0x02,
  IR_MINUS = 0x0c,
  IR_TUN_UP = 0x05,
  IR_TUN_DWN = 0x06,
  IR_VOL_UP = 0x03,
  IR_VOL_DWN = 0x04,
  IR_PBS = 0x0a,
  IR_TREBLE = 0x0d,
  IR_BASS = 0x0b,
  IR_VFO = 0x0f,
  IR_MEM_STO = 0x0e,
  IR_MEM_PRE = 0x01,
  IR_MEM_RCL = 0x07,
  IR_NONE = -1
};

/* backend conf */
#define TOK_CFG_MAGICCONF  TOKEN_BACKEND(1)


/* ext_level's and ext_parm's tokens */
#define TOK_EL_MAGICLEVEL  TOKEN_BACKEND(1)
#define TOK_EL_MAGICFUNC   TOKEN_BACKEND(2)
#define TOK_EL_MAGICOP     TOKEN_BACKEND(3)
#define TOK_EP_MAGICPARM   TOKEN_BACKEND(4)


/* Utility function prototypes */

#if 0
int NOP( RIG *rig, unsigned char x );
int SRH( RIG *rig, unsigned char x );
int PGE( RIG *rig, enum PAGE_e page );
int ADR( RIG *rig, unsigned char x );
int ADH( RIG *rig, unsigned char x );
int WRD( RIG *rig, unsigned char out );
int MSK( RIG *rig, unsigned char mask );
int EXE( RIG *rig, enum ROUTINE_e routine );
int RDD( RIG *rig, unsigned char len );
int LOC( RIG *rig, enum LOCK_LVL_e level );
int BUT( RIG *rig, enum BUTTON_e button );
#endif // 0

int execRoutine( RIG * rig, enum ROUTINE_e rtn );

int writeByte( RIG *rig, enum PAGE_e page, unsigned int addr, unsigned char x );
int writeShort( RIG *rig, enum PAGE_e page, unsigned int addr, unsigned short x );
int write3Bytes( RIG *rig, enum PAGE_e page, unsigned int addr, unsigned int x );
int writeInt( RIG *rig, enum PAGE_e page, unsigned int addr, unsigned int x );

int readByte( RIG *rig, enum PAGE_e page, unsigned int addr, unsigned char *x );
int readShort( RIG *rig, enum PAGE_e page, unsigned int addr, unsigned short *x );
int read3Bytes( RIG *rig, enum PAGE_e page, unsigned int addr, unsigned int *x );
int readInt( RIG *rig, enum PAGE_e page, unsigned int addr, unsigned int *x );

int readSignal( RIG * rig, unsigned char *x );
int flushBuffer( RIG * rig );
int lockRx( RIG * rig, enum LOCK_LVL_e level );

int bcd2Int( const unsigned char bcd );
unsigned char int2BCD( const unsigned int val );

int getCalLevel( RIG * rig, unsigned char rawAgc, int *dbm );
int getFilterBW( RIG *rig, enum FILTER_e filter );
freq_t ddsToHz( const unsigned int steps );
unsigned int hzToDDS( const freq_t freq );
float pbsToHz( const unsigned char steps );
unsigned char hzToPBS( const float freq );
rmode_t modeToHamlib( const unsigned char mode );
unsigned char modeToNative( const rmode_t mode );
enum agc_level_e agcToHamlib( const unsigned char agc );
unsigned char agcToNative( const enum agc_level_e agc );
int pageSize( const enum PAGE_e page );
int sendIRCode( RIG *rig, enum IR_CODE_e code );

#endif /* _AR7030P_H */
