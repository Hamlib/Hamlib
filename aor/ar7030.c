/*
 *  Hamlib AOR backend - AR7030 description
 *  Copyright (c) 2000-2004 by Stephane Fillod
 *
 *	$Id: ar7030.c,v 1.1 2004-04-16 19:58:10 fillods Exp $
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

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

#define AR7030_LEVEL (RIG_LEVEL_NONE)

#define AR7030_PARM (RIG_PARM_NONE)

#define AR7030_VFO_OPS (RIG_OP_NONE)

#define AR7030_VFO (RIG_VFO_A|RIG_VFO_B)

/*
 * Data was obtained from AR7030 pdf on http://www.aoruk.com
 */

static int ar7030_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
#ifdef AR7030Control_SOURCE
static int ar7030_open(RIG *rig);
static int ar7030_set_vfo(RIG *rig, vfo_t vfo);
static int ar7030_get_vfo(RIG *rig, vfo_t *vfo);
static int ar7030_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int ar7030_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int ar7030_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int ar7030_set_powerstat(RIG *rig, powerstat_t status);
static char *ar7030_get_info(RIG *rig);
#endif

/*
 * ar7030 rig capabilities.
 *
 * Parts from AR7030Control package, James A. Watson (jimbo@eureka.lk)
 * and also from DReaM project
 */
const struct rig_caps ar7030_caps = {
.rig_model =  RIG_MODEL_AR7030,
.model_name = "AR7030",
.mfg_name =  "AOR",
.version =  "0.1",
.copyright =  "LGPL",
.status =  RIG_STATUS_UNTESTED,
.rig_type =  RIG_TYPE_RECEIVER,
.ptt_type =  RIG_PTT_NONE,
.dcd_type =  RIG_DCD_NONE,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  1200,
.serial_rate_max =  1200,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_XONXOFF,	/* TBC */
.write_delay =  0,
.post_write_delay =  0,
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

.priv =  NULL,
.rig_init =  NULL,
.rig_cleanup =  NULL,
#if 0
.rig_open =  ar7030_open,
#endif
.rig_close =  NULL,

.set_freq =  ar7030_set_freq,
#if 0
.get_freq =  ar7030_get_freq,
.set_mode =  ar7030_set_mode,
.get_mode =  ar7030_get_mode,
.set_vfo =  ar7030_set_vfo,
.get_vfo =  ar7030_get_vfo,

.set_level =  ar7030_set_level,
.get_level =  ar7030_get_level,

//.get_parm =  ar7030_get_parm,

.set_powerstat =  ar7030_set_powerstat,
.get_info =  ar7030_get_info,
#endif

};

/*
 * Function definitions below
 */



static int rxr_writeByte(RIG *rig, unsigned char c)
{
	return write_block(&rig->state.rigport, &c, 1);
}

static int rxr_readByte(RIG *rig, unsigned char *c)
{
	unsigned char buf[] = {0x71}; // Read command
	int retval;

        retval = write_block(&rig->state.rigport, buf, 1);
	if (retval != RIG_OK)
		return retval;

	retval = read_block(&rig->state.rigport, c, 1);

	return retval;
}

int ar7030_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
  int numSteps = (int)((double)freq * 376.635223 + 0.5);

  if (vfo != RIG_VFO_CURR)
	  return -RIG_EINVAL;

  rxr_writeByte(rig, 0x50); /* Select working mem (page 0) */
  rxr_writeByte(rig, 0x31); /* Frequency address = 01AH */
  rxr_writeByte(rig, 0x4A);


  /* Exact multiplicand is (2^24) / 44545 */
  rxr_writeByte(rig, 0x30+(int)(numSteps/1048576));
  numSteps %= 1048576;
  rxr_writeByte(rig, 0x60+(int)(numSteps/65536));
  numSteps %= 65536;
  rxr_writeByte(rig, 0x30+(int)(numSteps/4096));
  numSteps %= 4096;
  rxr_writeByte(rig, 0x60+(int)(numSteps/256));
  numSteps %= 256;
  rxr_writeByte(rig, 0x30+(int)(numSteps/16));
  numSteps %= 16;
  rxr_writeByte(rig, 0x60+(int)(numSteps % 16));

  rxr_writeByte(rig, 0x21);
  rxr_writeByte(rig, 0x2C);

  return RIG_OK;
}

/*
 * The following code is a WIP, and not complete yet
 */
#ifdef AR7030Control_SOURCE
/*!
Sets all of the receivers parameters from current memory values.  This routine 
must be called after setFreq(), setMode() etc.
*/
static int tune(RIG *rig)
{
  return rxr_writeByte(rig, 0x24);
}


int ar7030_open(RIG *rig)
{
  _numFilters=0; 	// number of filters fitted
  _enhanced=1;		// 0 = type A, 1 = type 'B'

  getIdent(_idString);
  if ( (_idString[7] == 'A') ) {
    _enhanced = 0;
  } else {
    _enhanced = 1;
  }
  unsigned char tmpchr;
  for (int i = 1; i < 7 ; i++) {
    setMemPtr(rig, 1, (0x85 + (0x04 * (i-1))) );
    if (retval != RIG_OK) return retval;

    retval = rxr_readByte(rig, &tmpchr);
    if (retval != RIG_OK) return retval;

    _filterBandWidth[i] = (tmpchr == 0xFF) ? 0.0 : ((float)charToBCD(tmpchr))/10.0;
    if (_filterBandWidth[i] > 0.0) {
      _numFilters++;
    }
  }

  _firmwareVersion = (float)(strtod(_idString+5,0))/10.0;

}



/*!
Writes the sets ident to a char buffer specified by the pointer id.  
No checks are made on the size of the buffer which must be at least 15 chars.
*/
char *ar7030_get_info(RIG *rig)
{
	static char ident[15];
	int i, retval;

	retval = rxr_writeByte(rig, 0x5F);
	if (retval != RIG_OK) return retval;

	retval = rxr_writeByte(rig, 0x40);
    	if (retval != RIG_OK) return retval;

	for (i=0; i<8 ; i++) {
    		retval = rxr_readByte(rig, ident+i);
    		if (retval != RIG_OK) return retval;
  	}
  	ident[i]='\0';

  	return ident;
}
 




/****************************************************************************
* Frequency Routines.                                                       * 
****************************************************************************/

/*!
Sets the frequency (in kHz) to the value specified by freq.  The frequency 
change does not become effective until the next tune().  Caution, there is 
no checking yet i.e. it is up to the programmer to make sure that only valid 
frequencies are entered. 
*/
int ar7030_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
  int numSteps = (int)(freq * 376.635223 + 0.5);

  /*
   * VFO: setMemPtr(RIG, 0,0x1a);	
   */
  if (vfo != RIG_VFO_CURR)
	  return -RIG_EINVAL;

// TODO some checks here for validity of input values
//
  rxr_writeByte(rig, 0x30+(int)(numSteps/1048576));
  numSteps %= 1048576;
  rxr_writeByte(rig, 0x60+(int)(numSteps/65536));
  numSteps %= 65536;
  rxr_writeByte(rig, 0x30+(int)(numSteps/4096));
  numSteps %= 4096;
  rxr_writeByte(rig, 0x60+(int)(numSteps/256));
  numSteps %= 256;
  rxr_writeByte(rig, 0x30+(int)(numSteps/16));
  numSteps %= 16;
  rxr_writeByte(rig, 0x60+(int)(numSteps % 16));

  tune(rig);

  return RIG_OK;
}



static int readFreq(RIG *rig, freq_t *freq)
{
  int i, frVal = 0;
  unsigned char readVal = 0;

  for (i=0 ; i<3 ; i++) {
    retval = rxr_readByte(rig, &readVal);
    if (retval != RIG_OK)
	    return retval;
    frVal = (frVal*256)+readVal;
    }
  }
  *freq = (freq_t)((double)frVal/376635.223);  	/* Hz */

  return RIG_OK;
}

/*!
Returns the current frequency in kHz.
*/
int ar7030_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
  float f;

  setMemPtr(0,0x1a);
  *freq = (freq_t)readFreq();

  return RIG_OK;
}




/****************************************************************************
* Mode select and interrogation Routines.                                    * 
* 1=AM, 2=SYNC, 3=NFM, 7=USB, 4=DATA, 5=CW, 6=LSB                           *
****************************************************************************/
int ar7030_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
	unsigned char mdbuf[BUFSZ],ackbuf[BUFSZ];
	int mdbuf_len, ack_len, aormode, retval;

	switch (mode) {
	case RIG_MODE_FM:       aormode = MD_FM; break;
	case RIG_MODE_AM:       aormode = MD_AM; break;
	case RIG_MODE_CW:       aormode = MD_CW; break;
	case RIG_MODE_USB:      aormode = MD_USB; break;
	case RIG_MODE_LSB:      aormode = MD_LSB; break;
	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported mode %d\n",
					__FUNCTION__,mode);
		return -RIG_EINVAL;
	}

	mdbuf_len = sprintf(mdbuf, "MD%c" EOM, aormode);
	retval = aor_transaction (rig, mdbuf, mdbuf_len, ackbuf, &ack_len);

	tune(rig);

	return retval;
}

#define	MD_AM	1
#define MD_SYNC	2
#define MD_FM	3
#define MD_USB	7
#define MD_DATA	4
#define MD_CW	5
#define MD_LSB	6

/*!
Returns the current mode, represented by an integer. 1=AM, 2=SYNC, 3=NFM, 
7=USB, 4=DATA, 5=CW, 6=LSB.
*/  
int ar7030_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
  	int m, retval;

	retval = setMemPtr(rig, 0,0x1d);	// Select working mem (page 0).

	if (retval != RIG_OK)
		return retval;

  	m = rxr_readByte(rig);

	*width = RIG_PASSBAND_NORMAL;
	switch (m) {
		case MD_AM:	*mode = RIG_MODE_AM; break;
		case MD_SYNC:	*mode = RIG_MODE_AMS; break;
		case MD_CW:	*mode = RIG_MODE_CW; break;
		case MD_FM:	*mode = RIG_MODE_FM; break;
		case MD_USB:	*mode = RIG_MODE_USB; break;
		case MD_LSB:	*mode = RIG_MODE_LSB; break;
		default:
			rig_debug(RIG_DEBUG_ERR,"%s: unsupported mode %d\n",
							__FUNCTION__, m);
			return -RIG_EPROTO;
	}
int AR7030::getFilter() const {
  int c = 0;
  setMemPtr(0,0x34);	
  c=(int)rxr.readByte();	
  return c;
} // End of method getFilter

	if (*width == RIG_PASSBAND_NORMAL)
		*width = rig_passband_normal(rig, *mode);

  return RIG_OK;
}



/*!
Returns the bandwidth, in kHz, of the filter specified by f.  If the value
of f is greater than the number of filters, a value of 0 is returned.
*/
static float ar7030_getFilterBandWidth(int f) const {
  if ( (f>0) && (f<=6) ) {
    return _filterBandWidth[f];
  } else {
    return 0.0;
  }
} // End of method getFilterBandWidth()


/*!
Returns the number of filters fitted to the set.
*/
int AR7030::getNumFilters() const {
  return _numFilters;
}
  


/*!
Sets the current mode to that represented by the integer m, defined as 
follows; 1=AM, 2=SYNC, 3=NFM, 7=USB, 4=DATA, 5=CW, 6=LSB.  The mode 
change does not come into effect until the next tune() command.
*/
int ar7030_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
	int retval, m;

  	retval = setMemPtr(rig, 0,0x1d);
	if (retval != RIG_OK)
		return retval;

	switch (mode) {
	case RIG_MODE_FM:       m = MD_FM; break;
	case RIG_MODE_AM:       m = MD_AM; break;
	case RIG_MODE_AMS:       m = MD_SYNC; break;
	case RIG_MODE_CW:       m = MD_CW; break;
	case RIG_MODE_USB:      m = MD_USB; break;
	case RIG_MODE_LSB:      m = MD_LSB; break;
	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported mode %d\n",
					__FUNCTION__,mode);
		return -RIG_EINVAL;
	}

/*!
Sets the filter.  Valid arguments depend on the set being 
controlled.  Standard sets have four filters and an option for two more.  
Calls with invalid arguments are ignored.
*/
void AR7030::setFilter(int f) const {
  if ((f <= _numFilters) && (f > 0)) { //check valid filter number
    setMemPtr(0,0x34);
    rxr.writeByte(0x60+f);
  }
} // End of method setFilter()

  	retval = rxr_writeByte(rig, 0x60 + m);

	return retval;
}


#if 0
/****************************************************************************
* Memory Routines                                                           *
* Memory data is handled in structure type ar7030Memory :-                 *
* int number, int sql, float freq, int filter, int mode                      *
* float pbs, char textID[15], bool scanLockout                              *
*                                                                           *
****************************************************************************/

/*
A few private routines to set the memory pointer to locations containing 
frequency etc.  No checks on the input value are made in these routines.
*/

void AR7030::setPtrFreqMem(int n) const {
  if ( n <= 99 ) { 
    setMemPtr(2, (0x00 + (4 * n)));
  } else {
    setMemPtr(3, (0x00 + (4 * (n - 100))) );
  } 
}

void AR7030::setPtrSQLMem(int n) const {
/*
Firmware version 1.7 has a bug that puts the SQL/BFO
data in a memory 100 lower than it should be.
*/
  int mapBugOffset = (_firmwareVersion == (float)1.7) ? 100 : 0;
  if ( n <= 99 ) { // SQL/BFO
    setMemPtr(1, (0x9C + n) );
  } else if ( (100 <= n) && (n <= (175+mapBugOffset) ) ) {
    setMemPtr(3, ( 0x500 + (16 * (n-mapBugOffset) ) ) );
  } else {
    setMemPtr(4,(16 * (n-(176+mapBugOffset) ) ) );
  }
} // End of method setPtrSQLMem()


void AR7030::setPtrPBSMem(int n) const {
  int mapBugOffset = (_firmwareVersion == (float)1.7) ? 100 : 0;
  if ( n <= 99 ) { // PBS
    setMemPtr(2,(0x190 + n));
  } else if ( (100 <= n) && (n <= (175 +mapBugOffset) ) ) {
    setMemPtr(3, (0x501 + (16 * (n-mapBugOffset) ) ) );
  } else {
    setMemPtr(4,(0x01+(16 * (n-(176+mapBugOffset)) ) ) );
  }
} // End of method setPtrPBSMem()


void AR7030::setPtrIdentMem(int n) const {
  if ( n <= 175 ) {
    setMemPtr(3,(0x502 + (16 * n) ) );       
  } else {		  
    setMemPtr(4,(0x02 + (16 * (n-176))));
  } 
} // End of method setPtrIdentMem()


/*!
Writes the contents of a memory location specified by num to a variable of 
the type ar7070MemoryData, mem.  The definition of the type 
ar7030MemoryData is pert of the file ar7030.h.  Checks are made as to the 
validity of the argument num and invalid calls will result in the mem member 
freq containing a value of 0.0.
*/
void AR7030::getMem(int num,ar7030MemoryData &mem) const {

  if ( (num < 0) || (!_enhanced && (num>99)) ) { // check for valid number
    return;  
  }    
  unsigned char chr;

  mem.number = num;

  mem.freq = readMemFreq(num); // Frequency

  chr = rxr.readByte(); // Mode/Filter/scan byte follows frequency 
  mem.mode = chr & 0x0F;
  mem.filter = (chr >> 4) & 0x07;
  mem.scanLockout = (chr >> 7);

  setPtrSQLMem(num); 	//BFO for CW and data modes
  if ((mem.mode==4) || (mem.mode==5)) {
    mem.bfo = rxr.readByte()*0.03319;
  } else {		// SQL for all others
    mem.sql=(int)( ( (float)rxr.readByte()*99.0 )/150.0);
  }
     
  setPtrPBSMem(num); // PBS
  mem.pbs = 0.03319 * (signed char)rxr.readByte(); // convert to kHz

  if ( _enhanced ) {  // Text
    setPtrIdentMem(num);
    for (int i=0; i<14 ; i++) { 
      mem.textID[i]=(char)rxr.readByte();
    }
    mem.textID[14]='\0';
  }
} // End of method getMem()


/*!
Sets the contents of a memory location to that defined in the variable of type
ar7070MemoryData, mem.  The definition of the type ar7030MemoryData is pert of the 
file ar7030.h.  This function has not been fully tested yet.
*/
void AR7030::setMem(ar7030MemoryData &mem) const {

  if ( (mem.number < 0) || (!_enhanced && (mem.number>99)) ) {
    return;  
  }    
  unsigned char tmpchr = 0;  // general  purpose temporary variable

  setPtrFreqMem(mem.number);
  writeFreq(mem.freq);

//  TODO fast find memory

  tmpchr =  (mem.mode & 0x0F) | (mem.filter << 4) | (mem.scanLockout << 7);
  rxr.writeByte(0x30 + (tmpchr>>4));
  rxr.writeByte(0x60 + (tmpchr & 0x0F));

  setPtrSQLMem(mem.number); 	// BFO for CW and Data modes
  if ((mem.mode==4) || (mem.mode==5)) { 
    tmpchr = (unsigned char)(mem.bfo/0.03319);
    rxr.writeByte(0x30 + (tmpchr >> 4));
    rxr.writeByte(0x60 + (tmpchr & 0x0F));
  } else {	     		// SQL for all other modes
    tmpchr = (unsigned char)( (mem.sql*150.0)/99.0);
    rxr.writeByte(0x30 + (tmpchr >> 4));
    rxr.writeByte(0x60 + (tmpchr & 0x0F));
  }

  setPtrPBSMem(mem.number); //PBS
  (signed)tmpchr = mem.pbs/0.03319;
  rxr.writeByte(0x30 + (tmpchr >> 4));
  rxr.writeByte(0x60 + (tmpchr & 0x0F));

  if ( _enhanced ) {   // Ident
    setPtrIdentMem(mem.number);
    for (int i=0; i<14 ; i++) { 
      tmpchr = mem.textID[i];
      rxr.writeByte(0x30 + (tmpchr >> 4));
      rxr.writeByte(0x60 + (tmpchr & 0x0F));
    }
  }
} // End of method setMem()


/*!
Returns the frequency in the memory specified by num.
*/
float AR7030::getMemFreq(int num) const {
  if ( (num<0) || (!_enhanced && (num >99)) || (num>399) ) {
    return 0.0;
  }
  return readMemFreq(num);
} // End of method getMemFreq()


float AR7030::readMemFreq(int num) const {
  setPtrFreqMem(num);  
  return readFreq(); 
} // End of method readFreq()




/*!
Turns the receiver on from standby mode.
*/
int ar7030_set_powerstat(RIG *rig, powerstat_t status)
{
	if (status != RIG_POWER_ON)
		return -RIG_EINVAL;

  	return rxr_writeByte(rig, 0xA0);
}

#endif

#endif	/* AR7030Control_SOURCE */

