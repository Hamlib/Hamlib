/*
 *  Hamlib AOR backend - AR7030 description
 *  Copyright (c) 2000-2004 by Stephane Fillod
 *
 *	$Id: ar7030.c,v 1.3 2004-12-08 21:13:25 fillods Exp $
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

//
// Version 2004.11.29 F.Melchert (DC9RP)
//
 
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <math.h>

#include <hamlib/rig.h>
#include "aor.h"
#include "serial.h"


/* 
 * So far, only set_freq is implemented. Maintainer wanted!
 *
 * TODO:
 * 	- everything: this rig has nothing in common with other aor's.
 *	- LEVEL_AF, LEVEL_SQL, LEVEL_AGC, PARM_TIME, FUNC_LOCK
 *
 *	set_mem, get_mem, set_channel, get_channel
 */
#define AR7030_MODES (RIG_MODE_AM|RIG_MODE_AMS|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)

#define AR7030_FUNC_ALL (RIG_FUNC_NONE)

#define AR7030_LEVEL (RIG_LEVEL_AF|RIG_LEVEL_RF)

#define AR7030_PARM (RIG_PARM_NONE)

#define AR7030_VFO_OPS (RIG_OP_NONE)

#define AR7030_VFO (RIG_VFO_A|RIG_VFO_B)

/*
 * Data was obtained from AR7030 pdf on http://www.aoruk.com
 */


/****************************************************************************
* Misc Routines                                                             *
****************************************************************************/

static int rxr_writeByte(RIG *rig, unsigned char c)
{
  return write_block(&rig->state.rigport, &c, 1);
}


static int rxr_readByte(RIG *rig)
{
  unsigned char response[1];
  unsigned char buf[] = {0x71}; // Read command
  int retval;
  retval = write_block(&rig->state.rigport, buf, 1);
  retval = read_block(&rig->state.rigport, response, 1);
  return response[0];
}

/*!
Umwandlung von BCD nach char
*/
int BCD_To_int(RIG *rig, int c)
{
  if (((c & 0x0F) < 0x0a) && ((c & 0xF0) < 0xa0))  // Test pseudo Tetrade
  {
    return (((c >> 4)*10)+(c & 0x0F));
  }
  return (-1);
}// End of method BCD_To_char(

/****************************************************************************
* Routines to set receiver lock levels                                      *
****************************************************************************/

/*!
Locks, or unlocks if called with argument 0, the receiver, disabling the
front panel controls or updates.  The level of locking is determined by the
argument level which may be in the range 0 (no lock) to 3 (Remote operation
exclusively).  Calling the method without arguments sets the lock level to
1.  It is recommended to lock to this level during any multi byte read or
writes to prevent data contention between internal and remote access.
Calls with invalid arguments are ignored.
*/
static void unlock(RIG *rig)
{
    rxr_writeByte(rig, 0x80);
}

// Level 1 = 0x81	IR remote control disabled.
// 			Front panel buttons ignored.
// 			Front panel spin-wheels logged but not actioned.
// 			Display update (frequency & S-meter) continues.
static void setLock(RIG *rig, int level)
{
  if ((0 <= level) && (level <= 3)) {
    rxr_writeByte(rig, 0x80+level);
  }
}

// Level 2 = 0x82	As level 1, but display update suspended. In revisions before 1.4 
// 			squelch operation is inhibited, which results in no audio output
// 			after a mode change. In revision 1.4 squelch operation continues
// 			and mode changing is as expected.
// Level 3 = 0x83	Remote operation exclusively.



static void setMemPtr(RIG *rig, int page, int address) 
{
  rxr_writeByte(rig, 0x50 + page);			//Set Page
  if (address <= 0xFF) {				//*** <= 8 Bit Adresse ***
    rxr_writeByte(rig, 0x30 + (address >> 4));		//Set H-Register 4 Bits
    rxr_writeByte(rig, 0x40 + (address & 0x0F));	//Set Adress(12 Bits = (4 Bit H Register) + 8 Bit)
  } else {						//*** > 8 Bit Adresse ***
    rxr_writeByte(rig, 0x30 + ((address >> 4) & 0x0F)) ;//Set H-Register 4 Bits
    rxr_writeByte(rig, 0x40 + (address & 0x0F));	//Set Adress(12 Bits = (4 Bit H Register) + 8 Bit)
    rxr_writeByte(rig, 0x10 + (address >> 8));		//Set Adress high(12 Bits=(4 Bit H Register)+8 Bit)
  }
}

/****************************************************************************
* Routines                                                                  *
****************************************************************************/
// Routine 0	Reset	Setup receiver as at switch-on.
// Routine 1	Set frequency	Program local oscillator from frequ area and setup 
//		RF filters and oscillator range.
// Routine 2	Set mode	Setup from mode byte in memory and display mode, 
//		select preferred filter and PBS, BFO values etc.
// Routine 3	Set passband	Setup all IF parameters from filter, pbsval and bfoval bytes.
// Routine 4	Set all	Set all receiver parameters from current memory values
static void Execute_Routine_4_1(RIG *rig, char mp , char ad , int numSteps) 
{
  setLock(rig, 1);		//Set Lock Level
  setMemPtr(rig, mp , ad );	//page, address
  // 0x30 = Set H-register  x  --->  H-register (4-bits)
  //        The high order 4-bits of each byte sent to the receiver is the operation code, 
  //        the low order 4-bits is data (shown here as x) 
  rxr_writeByte(rig, 0x30 | (0x0F & (char)(numSteps>>4)));
  // 0x60 = Write data  Hx  
  //        --->  [Page, Address] Address register + 1  
  //        --->  Address register 0  
  //        --->  H-register,  0  
  //        --->  Mask register
  rxr_writeByte(rig, 0x60 | (0x0F & (char)(numSteps)));

  //Execute routine 
  //Set all Set all receiver parameters from current memory values  
  rxr_writeByte(rig, 0x24);
  unlock(rig);			//Set UnLock Level
}

static void Execute_Routine_4_3(RIG *rig, char mp , char ad , int numSteps) 
{
  setLock(rig, 1);		//Set Lock Level
  setMemPtr(rig, mp , ad );	//page, address
  rxr_writeByte(rig, 0x30 | (0x0F & (char)(numSteps>>20)));
  rxr_writeByte(rig, 0x60 | (0x0F & (char)(numSteps>>16)));
  rxr_writeByte(rig, 0x30 | (0x0F & (char)(numSteps>>12)));
  rxr_writeByte(rig, 0x60 | (0x0F & (char)(numSteps>>8)));
  rxr_writeByte(rig, 0x30 | (0x0F & (char)(numSteps>>4)));
  rxr_writeByte(rig, 0x60 | (0x0F & (char)(numSteps)));

  //Execute routine 
  //Set all Set all receiver parameters from current memory values  
  rxr_writeByte(rig, 0x24);
  unlock(rig);			//Set UnLock Level
}

// Routine 5	Set audio	Setup audio controller from memory register values.
// Routine 6	Set RF-IF	Setup RF Gain, IF Gain and AGC speed. Also sets Notch Filter and 
//		Noise Blanker if these options are fitted.
static void Execute_Routine_6_1(RIG *rig, char mp , char ad , int numSteps) 
{
  //setLock(rig, 1);		//Set Lock Level
  setMemPtr(rig, mp , ad );	//page, address
  rxr_writeByte(rig, 0x30 | (0x0F & (char)(numSteps>>4)));
  rxr_writeByte(rig, 0x60 | (0x0F & (char)(numSteps)));
  rxr_writeByte(rig, 0x24);
  //unlock(rig);			//Set UnLock Level
}



 
// frequ    Mem_Page=0   Address=1A
static int ar7030_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
  unsigned int frequ_i = (int)(round(freq * .3766352228));
  Execute_Routine_4_3(rig, 0 , 0x1a , frequ_i);
  return RIG_OK;
}
//frequ    Mem_Page=0   Address=1A
static int ar7030_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
  unsigned int frequ_i = 0;
  setMemPtr(rig ,0 ,0x1a);
  frequ_i = (int)(rxr_readByte(rig) << 16);
  frequ_i = frequ_i + (int)(rxr_readByte(rig) << 8);
  frequ_i = frequ_i + (int)(rxr_readByte(rig));
  *freq =  ((float)(frequ_i) * 2.65508890157896);
  return RIG_OK;
} 
 

/*!
Current mode :- 
		RIG_MODE_NONE =  	0,	< None 
1 = AM		RIG_MODE_AM =    	(1<<0),	< Amplitude Modulation 
5 = CW		RIG_MODE_CW =    	(1<<1),	< CW 
7 = USB		RIG_MODE_USB =		(1<<2),	< Upper Side Band 
6 = LSB		RIG_MODE_LSB =		(1<<3),	< Lower Side Band 
4 = Data	RIG_MODE_RTTY =		(1<<4),	< Remote Teletype 
3 = NFM 	RIG_MODE_FM =    	(1<<5),	< "narrow" band FM 
		RIG_MODE_WFM =   	(1<<6),	< broadcast wide FM 
		RIG_MODE_CWR =   	(1<<7),	< CW reverse sideband 
		RIG_MODE_RTTYR =	(1<<8),	< RTTY reverse sideband 
2 = Sync	RIG_MODE_AMS =    	(1<<9),	< Amplitude Modulation Synchronous 
		RIG_MODE_PKTLSB =       (1<<10),< Packet/Digital LSB mode (dedicated port) 
		RIG_MODE_PKTUSB =       (1<<11),< Packet/Digital USB mode (dedicated port) 
		RIG_MODE_PKTFM =        (1<<12),< Packet/Digital FM mode (dedicated port) 
		RIG_MODE_ECSSUSB =      (1<<13),< Exalted Carrier Single Sideband USB 
		RIG_MODE_ECSSLSB =      (1<<14),< Exalted Carrier Single Sideband LSB 
		RIG_MODE_FAX =          (1<<15) < Facsimile Mode 
*/
//m,M     get_mode/set_mode       FM, USB, LSB, CW, WFM, etc. passband is in Hz (pass 0 for default)
static int ar7030_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
  // mode    Mem_Page=0   Address=1D
  switch(mode)
  {
    case RIG_MODE_AM :
      Execute_Routine_4_1(rig, 0 , 0x1d , 1);	//AM
      break;
    case RIG_MODE_AMS :
      Execute_Routine_4_1(rig, 0 , 0x1d , 2);	//Sync
      break;
    case RIG_MODE_FM :
      Execute_Routine_4_1(rig, 0 , 0x1d , 3);	//NFM
      break;
    case RIG_MODE_RTTY :
      Execute_Routine_4_1(rig, 0 , 0x1d , 4);	//Data
      break;
    case RIG_MODE_CW :
      Execute_Routine_4_1(rig, 0 , 0x1d , 5);	//CW
      break;
    case RIG_MODE_LSB :
      Execute_Routine_4_1(rig, 0 , 0x1d , 6);	//LSB
      break;
    case RIG_MODE_USB :
      Execute_Routine_4_1(rig, 0 , 0x1d , 7);	//USB
      break;
    default :
      return -RIG_EINVAL;
  }

  // filter    Mem_Page=0   Address=34
  Execute_Routine_4_1(rig, 0, 0x34, width);
  
  return RIG_OK;
}

static int ar7030_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
  // mode    Mem_Page=0   Address=1D                                *
  setMemPtr(rig ,0 ,0x1d);
  switch(rxr_readByte(rig))
  {
    case 1:
      *mode = RIG_MODE_AM;	//AM
      break;
    case 2:
      *mode = RIG_MODE_AMS;	//Sync
      break;
    case 3:
      *mode = RIG_MODE_FM;	//NFM
      break;
    case 4:
      *mode = RIG_MODE_RTTY;	//Data
      break;
    case 5:
      *mode = RIG_MODE_CW;	//CW
      break;
    case 6:
      *mode = RIG_MODE_LSB;	//LSB
      break;
    case 7:
      *mode = RIG_MODE_USB;	//USB
      break;
    default :
      return -RIG_EINVAL;
  }  
  // fltbw    Mem_Page=0   Address=38
  setMemPtr(rig ,0 ,0x38);
  if ((*width = BCD_To_int(rig, rxr_readByte(rig))*100) < 0)
  {
    return -RIG_EINVAL;
  }

  return RIG_OK;
  
}


/*!
	RIG_LEVEL_NONE =	0,	< None 
	RIG_LEVEL_PREAMP =	(1<<0),	< Preamp, arg int (dB) 
	RIG_LEVEL_ATT =		(1<<1),	< Attenuator, arg int (dB) 
	RIG_LEVEL_VOX =		(1<<2),	< VOX delay, arg int (tenth of seconds) 
	RIG_LEVEL_AF =		(1<<3),	< Volume, arg float [0.0..1.0] 
	RIG_LEVEL_RF =		(1<<4),	< RF gain (not TX power), arg float [0.0..1.0] 
	RIG_LEVEL_SQL =		(1<<5),	< Squelch, arg float [0.0 .. 1.0] 
	RIG_LEVEL_IF =		(1<<6),	< IF, arg int (Hz) 
	RIG_LEVEL_APF =		(1<<7),	< APF, arg float [0.0 .. 1.0] 
	RIG_LEVEL_NR =		(1<<8),	< Noise Reduction, arg float [0.0 .. 1.0] 
	RIG_LEVEL_PBT_IN =	(1<<9),	< Twin PBT (inside), arg float [0.0 .. 1.0] 
	RIG_LEVEL_PBT_OUT =	(1<<10),< Twin PBT (outside), arg float [0.0 .. 1.0] 
	RIG_LEVEL_CWPITCH =	(1<<11),< CW pitch, arg int (Hz) 
	RIG_LEVEL_RFPOWER =	(1<<12),< RF Power, arg float [0.0 .. 1.0] 
	RIG_LEVEL_MICGAIN =	(1<<13),< MIC Gain, arg float [0.0 .. 1.0] 
	RIG_LEVEL_KEYSPD =	(1<<14),< Key Speed, arg int (WPM) 
	RIG_LEVEL_NOTCHF =	(1<<15),< Notch Freq., arg int (Hz) 
	RIG_LEVEL_COMP =	(1<<16),< Compressor, arg float [0.0 .. 1.0] 
	RIG_LEVEL_AGC =		(1<<17),< AGC, arg int (see enum agc_level_e) 
	RIG_LEVEL_BKINDL =	(1<<18),< BKin Delay, arg int (tenth of dots) 
	RIG_LEVEL_BALANCE =	(1<<19),< Balance (Dual Watch), arg float [0.0 .. 1.0] 
	RIG_LEVEL_METER =	(1<<20),< Display meter, arg int (see enum meter_level_e) 

	RIG_LEVEL_VOXGAIN =	(1<<21),< VOX gain level, arg float [0.0 .. 1.0] 
	RIG_LEVEL_VOXDELAY =		  RIG_LEVEL_VOX,	< VOX delay, arg int (tenth of seconds) 
	RIG_LEVEL_ANTIVOX =	(1<<22),< anti-VOX level, arg float [0.0 .. 1.0] 
	RIG_LEVEL_LINEOUT =	(1<<23),< Lineout Volume, arg float [0.0 .. 1.0] 

		< These ones are not settable 
	RIG_LEVEL_RAWSTR =	(1<<26),< Raw (A/D) value for signal strength, specific to each rig, arg int
	RIG_LEVEL_SQLSTAT =	(1<<27),< SQL status, arg int (open=1/closed=0). Deprecated, use get_dcd
					  instead 
	RIG_LEVEL_SWR =		(1<<28),< SWR, arg float 
	RIG_LEVEL_ALC =		(1<<29),< ALC, arg float 
	RIG_LEVEL_STRENGTH =	(1<<30) < Effective (calibrated) signal strength relative to S9, arg int(dB)
	RIG_LEVEL_BWC =		(1<<31) < Bandwidth Control, arg int (Hz) 
*/
static int ar7030_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
  float val_i = val.f;    
  switch(level)
  {
    case RIG_LEVEL_AF :
      // af_vol    Mem_Page=0   Address=1E 
      // Fehlerbehandlung
      val_i = (val_i *100 / 2) + 15;
      val_i = (val_i > 63) ? 63 : val_i;
      val_i = (val_i < 15) ? 15 : val_i;
      Execute_Routine_4_1(rig, 0 ,0x1e , val_i);
      return RIG_OK;
    case RIG_LEVEL_ATT : 
      return -RIG_ENIMPL;
    case RIG_LEVEL_IF : 
      return -RIG_ENIMPL;
    case RIG_LEVEL_RF : 
      // rfgain    Mem_Page=0   Address=30
      Execute_Routine_6_1(rig, 0 ,0x30 , ((val_i * 10) - 1) * -1) ;
      return RIG_OK;
    default :
      return -RIG_EINVAL;
  }
}

static int ar7030_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
  switch(level)
  {
    case RIG_LEVEL_AF :
      // af_vol    Mem_Page=0   Address=1E
      setMemPtr(rig ,0 ,0x1e);
      val->f = ((float)(rxr_readByte(rig) - 15) * 2) / 100; 
      return RIG_OK;
    case RIG_LEVEL_ATT : 
      return -RIG_ENIMPL;
    case RIG_LEVEL_STRENGTH : 
      return -RIG_ENIMPL;
    case RIG_LEVEL_IF : 
      return -RIG_ENIMPL;
    case RIG_LEVEL_RF : 
      // rfgain    Mem_Page=0   Address=30
      setMemPtr(rig ,0 ,0x30);
      val->f = (float)((rxr_readByte(rig) * -1) + 1) / 10;
      return RIG_OK;
    default :
      return -RIG_EINVAL;
  }
}


const struct rig_caps ar7030_caps = {
.rig_model =  RIG_MODEL_AR7030,
.model_name = "AR7030",
.mfg_name =  "AOR",
.version =  "0.3",
.copyright =  "LGPL",
.status =  RIG_STATUS_ALPHA,
.rig_type =  RIG_TYPE_RECEIVER,
.ptt_type =  RIG_PTT_NONE,
.dcd_type =  RIG_DCD_NONE,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  1200,
.serial_rate_max =  1200,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE,	/* TBC */
.write_delay =  0,
.post_write_delay =  0,	//Device: /dev/ttyS0
//.post_write_delay =  85,	//Device: /dev/tts/USB0 < 85 sec timedout after 0 chars
.timeout =  500,	/* 0.5 second */
.retry =  0,
.has_get_func =  AR7030_FUNC_ALL,
.has_set_func =  AR7030_FUNC_ALL,
.has_get_level =  AR7030_LEVEL,
.has_set_level =  RIG_LEVEL_SET(AR7030_LEVEL),
.has_get_parm =  AR7030_PARM,
.has_set_parm =  RIG_PARM_NONE,
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.ctcss_list =  NULL,
.dcs_list =  NULL,
.preamp =   { RIG_DBLST_END, },
.attenuator =   { RIG_DBLST_END, },
.max_rit =  Hz(0),
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),
.targetable_vfo =  0,
.transceive =  RIG_TRN_OFF,
.bank_qty =   0,
.chan_desc_sz =  100,	/* FIXME */
.vfo_ops =  AR7030_VFO_OPS,

.chan_list =  { RIG_CHAN_END, },	/* FIXME: memory channel list: 1000 memories */

.rx_range_list1 =  {
	{kHz(10),MHz(2600),AR7030_MODES,-1,-1,AR7030_VFO},
	RIG_FRNG_END,
	},
.tx_range_list1 =  { RIG_FRNG_END, },

.rx_range_list2 =  {
	{kHz(10),MHz(2600),AR7030_MODES,-1,-1,AR7030_VFO},
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list2 =  { RIG_FRNG_END, },	/* no tx range, this is a scanner! */

.tuning_steps =  {
	 {AR7030_MODES,50},
	 {AR7030_MODES,100},
	 {AR7030_MODES,200},
	 {AR7030_MODES,500},
	 {AR7030_MODES,kHz(1)},
	 {AR7030_MODES,kHz(2)},
	 {AR7030_MODES,kHz(5)},
	 {AR7030_MODES,kHz(6.25)},
	 {AR7030_MODES,kHz(9)},
	 {AR7030_MODES,kHz(10)},
	 {AR7030_MODES,12500},
	 {AR7030_MODES,kHz(20)},
	 {AR7030_MODES,kHz(25)},
	 {AR7030_MODES,kHz(30)},
	 {AR7030_MODES,kHz(50)},
	 {AR7030_MODES,kHz(100)},
	 {AR7030_MODES,kHz(200)},
	 {AR7030_MODES,kHz(250)},
	 {AR7030_MODES,kHz(500)},
#if 0
	 {AR7030_MODES,0},	/* any tuning step */
#endif
	 RIG_TS_END,
	},
        /* mode/filter list, .remember =  order matters! */
.filters =  {
        /* mode/filter list, .remember =  order matters! */
		{RIG_MODE_SSB|RIG_MODE_CW, kHz(3)}, 
		{RIG_MODE_FM|RIG_MODE_AM, kHz(15)}, 
		{RIG_MODE_FM|RIG_MODE_AM, kHz(6)}, 	/* narrow */
		{RIG_MODE_FM|RIG_MODE_AM, kHz(30)}, /* wide */
		RIG_FLT_END,
	},

  .priv =  NULL,	/* priv */

//  .rig_init =     ar7030_init,
//  .rig_cleanup =  ar7030_cleanup,
//  .rig_open =     ar7030_open,
//  .rig_close =    ar7030_close,

  .set_freq =     ar7030_set_freq,	
  .get_freq =     ar7030_get_freq,	
  .set_mode =     ar7030_set_mode,    
  .get_mode =     ar7030_get_mode,    
//  .set_vfo =      ar7030_set_vfo,
//  .get_vfo =      ar7030_get_vfo,
  
//  .set_powerstat =  ar7030_set_powerstat,
//  .get_powerstat =  ar7030_get_powerstat,
  .set_level =    ar7030_set_level,   
  .get_level =    ar7030_get_level,   
//  .set_func =      ar7030_set_func,
//  .get_func =      ar7030_get_func,
//  .set_parm =      ar7030_set_parm,
//  .get_parm =      ar7030_get_parm,

//  .get_info =      ar7030_get_info,


//  .set_ptt = 	ar7030_set_ptt,
//  .get_ptt = 	ar7030_get_ptt,
//  .get_dcd = 	ar7030_get_dcd,
//  .set_rptr_shift = 	ar7030_set_rptr_shift,
//  .get_rptr_shift = 	ar7030_get_rptr_shift,
//  .set_rptr_offs = 	ar7030_set_rptr_offs,
//  .get_rptr_offs = 	ar7030_get_rptr_offs,
//  .set_ctcss_tone = 	ar7030_set_ctcss_tone,
//  .get_ctcss_tone = 	ar7030_get_ctcss_tone,
//  .set_dcs_code = 	ar7030_set_dcs_code,
//  .get_dcs_code = 	ar7030_get_dcs_code,
//  .set_ctcss_sql = 	ar7030_set_ctcss_sql,
//  .get_ctcss_sql = 	ar7030_get_ctcss_sql,
//  .set_dcs_sql = 	ar7030_set_dcs_sql,
//  .get_dcs_sql = 	ar7030_get_dcs_sql,
//  .set_split_freq = 	ar7030_set_split_freq,
//  .get_split_freq = 	ar7030_get_split_freq,
//  .set_split_mode = 	ar7030_set_split_mode,
//  .get_split_mode = 	ar7030_get_split_mode,
//  .set_split_vfo = 	ar7030_set_split_vfo,
//  .get_split_vfo = 	ar7030_get_split_vfo,
//  .set_rit = 	ar7030_set_rit,
//  .get_rit = 	ar7030_get_rit,
//  .set_xit = 	ar7030_set_xit,
//  .get_xit = 	ar7030_get_xit,
//  .set_ts = 	ar7030_set_ts,
//  .get_ts = 	ar7030_get_ts,
//  .set_ant = 	ar7030_set_ant,
//  .get_ant = 	ar7030_get_ant,
//  .set_bank = 	ar7030_set_bank,
//  .set_mem = 	ar7030_set_mem,
//  .get_mem = 	ar7030_get_mem,
//  .vfo_op = 	ar7030_vfo_op,
//  .scan = 		ar7030_scan,
//  .send_dtmf =  ar7030_send_dtmf,
//  .recv_dtmf =  ar7030_recv_dtmf,
//  .send_morse =  ar7030_send_morse,
//  .set_channel = 	ar7030_set_channel,
//  .get_channel = 	ar7030_get_channel,
//  .set_trn = 	ar7030_set_trn,
//  .get_trn = 	ar7030_get_trn,
};


