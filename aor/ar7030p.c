
/*
 *  Hamlib AOR backend - AR7030 Plus description
 *  Copyright (c) 2000-2006 by Stephane Fillod & Fritz Melchert
 *
 *	$Id: $
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

/*
 * Version 2009.11.21 Larry Gadallah (VE6VQ)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include <hamlib/rig.h>
#include "ar7030p.h"
#include "serial.h"
#include "idx_builtin.h"

#define AR7030P_MODES ( RIG_MODE_AM | \
                        RIG_MODE_SSB | \
                        RIG_MODE_CW | \
                        RIG_MODE_RTTY | \
                        RIG_MODE_FM | \
                        RIG_MODE_AMS )

#define AR7030P_FUNC ( RIG_FUNC_FAGC | \
                       RIG_FUNC_NB | \
                       RIG_FUNC_ANF | \
                       RIG_FUNC_AIP | \
                       RIG_FUNC_MN | \
                       RIG_FUNC_RF | \
                       RIG_FUNC_LOCK | \
                       RIG_FUNC_MUTE | \
                       RIG_FUNC_SQL )

#define AR7030P_LEVEL ( RIG_LEVEL_PREAMP | \
                        RIG_LEVEL_ATT | \
                        RIG_LEVEL_AF | \
                        RIG_LEVEL_RF | \
                        RIG_LEVEL_SQL | \
                        RIG_LEVEL_PBT_IN | \
                        RIG_LEVEL_CWPITCH | \
                        RIG_LEVEL_NOTCHF | \
                        RIG_LEVEL_AGC | \
                        RIG_LEVEL_RAWSTR | \
                        RIG_LEVEL_STRENGTH )

#define AR7030P_PARM ( RIG_PARM_APO | \
                       RIG_PARM_TIME | \
                       RIG_PARM_BAT )

#define AR7030P_VFO_OPS ( RIG_OP_CPY | \
                          RIG_OP_XCHG | \
                          RIG_OP_TOGGLE )

#define AR7030P_VFO ( RIG_VFO_A | \
                      RIG_VFO_B | \
                      RIG_VFO_CURR )

#define AR7030P_STR_CAL { 8, { \
                               { 10, -113 }, \
	                       { 10, -103 }, \
		               { 10,  -93 }, \
		               { 10,  -83 }, \
		               { 10,  -73 }, \
		               { 10,  -63 }, \
		               { 20,  -43 }, \
		               { 20,  -23 }, \
		             } }

// Channel capabilities
#define AR7030P_MEM_CAP { \
	.freq = 1,	\
	.mode = 1,	\
	.width = 1,	\
	.funcs = RIG_FUNC_FAGC, \
	.levels = RIG_LEVEL_ATT | RIG_LEVEL_AGC, \
}

struct ar7030p_priv_caps
{
  int max_freq_len;
  int info_len;
  int mem_len;
  int pbs_info_len;
  int pbs_len;
  int chan_num;
};

static const struct ar7030p_priv_caps ar7030p_priv_caps = {
  .max_freq_len = 3,
  .info_len = 14,
  .mem_len = 17,
  .pbs_info_len = 1,
  .pbs_len = 1,
  .chan_num = 0,
};

/* TODO - move this somewhere where it belongs */
static unsigned int filterTab[ 6 + 1 ] = { 0 };

/*
 * /brief Open I/O to receiver
 *
 * /param rig Pointer to rig struct
 *
 * /return 0 on success, < 0 on failure
 */
static int ar7030p_open( RIG * rig )
{
  int rc = -RIG_OK;
  int i;
  unsigned char v;

  assert( NULL != rig );

  rc = lockRx( rig, LOCK_1 );
  if ( RIG_OK == rc )
  {
    /* Load calibration table */
    rig->state.str_cal.size = rig->caps->str_cal.size;
    for ( i = 0; i < rig->state.str_cal.size; i++ )
    {
      rc = readByte( rig, EEPROM1, SM_CAL + i, &v );
      if ( RIG_OK != rc )
      {
	break;
      }
      rig->state.str_cal.table[ i ].val = rig->caps->str_cal.table[ i ].val;
      rig->state.str_cal.table[ i ].raw = (int) v;
    }

    if ( RIG_OK == rc )
    {
      /* Load filter BW table */
      for ( i = 1; i <= 6; i++ )
      {
	rc = getFilterBW( rig, i );
	if ( 0 > rc )
        {
	  rc = -RIG_EIO;
	  break;
	}
        else
	{
	  filterTab[i] = (unsigned int) rc;
	}
      }
    }

    rc = lockRx( rig, LOCK_0 );

    rig_debug( RIG_DEBUG_VERBOSE, "%s: \n", __func__ );
  }

  return ( rc );
}

/*
 * /brief Close I/O to receiver
 *
 * /param rig Pointer to rig struct
 *
 * /return 0 on success, < 0 on failure
 */
static int ar7030p_close( RIG * rig )
{
  assert( NULL != rig );

  rig_debug( RIG_DEBUG_VERBOSE, "%s: \n", __func__ );

  return ( RIG_OK );
}

static const char *ar7030p_get_info( RIG * rig )
{
  static char version[10] = "";
  unsigned int i;
  char *p = &( version[ 0 ] );

  assert( NULL != rig );

  for ( i = 0; i < pageSize(ROM); i++ )
  {
    if ( RIG_OK != readByte( rig, ROM, i, (unsigned char *) p++ ) )
    {
      p = NULL;
      break;
    }
  }

  if ( NULL != p )
  {
    *p++ = '\0';
    p = &( version[ 0 ] );

    rig_debug( RIG_DEBUG_VERBOSE, "%s: ident - %s\n", __func__, version );
  }

  return ( p );
}

/*
 * /brief Set receiver frequency
 *
 * /param rig Pointer to rig struct
 * /param vfo VFO to operate on
 * /param freq Frequency to set
 *
 */
static int ar7030p_set_freq( RIG * rig, vfo_t vfo, freq_t freq )
{
  int rc = RIG_OK;
  const struct rig_caps *caps;

  assert( NULL != rig );

  rc = lockRx( rig, LOCK_1 );
  if ( RIG_OK == rc )
  {
    caps = rig->caps;

    if ( ( caps->rx_range_list1[ 0 ].end   > freq ) && 
	 ( caps->rx_range_list1[ 0 ].start < freq ) )
    {
      switch( vfo )
      {
      case RIG_VFO_CURR:
      case RIG_VFO_A:
	rc = write3Bytes( rig, WORKING, FREQU, hzToDDS( freq ) );
	break;

      case RIG_VFO_B:
	rc = write3Bytes( rig, WORKING, FREQU_B, hzToDDS( freq ) );
	break;

      default:
	rc = -RIG_EINVAL;
      };
    }
    else
    {
      rc = -RIG_EINVAL;
    }

    rc = execRoutine( rig, SET_ALL );

    rc = lockRx( rig, LOCK_0 );
  }

  return( rc );
}

/*
 * /brief Get receiver frequency
 *
 * /param rig Pointer to rig struct
 * /param vfo VFO to operate on
 * /param freq Pointer to hold frequency value (in Hz)
 *
 */
static int ar7030p_get_freq( RIG * rig, vfo_t vfo, freq_t * freq )
{
  int rc = RIG_OK;
  unsigned int x;

  rc = lockRx( rig, LOCK_1 );
  if ( RIG_OK == rc )
  {
    switch( vfo )
    {
    case RIG_VFO_CURR:
    case RIG_VFO_A:
      rc = read3Bytes( rig, WORKING, FREQU, &x );
      if ( RIG_OK == rc )
      {
	*freq = ddsToHz( x );
      }
      break;

    case RIG_VFO_B:
      rc = read3Bytes( rig, WORKING, FREQU_B, &x );
      {
	*freq = ddsToHz( x );
      }
      break;

    default:
      rc = -RIG_EINVAL;
    };

    rc = lockRx( rig, LOCK_0 );
  }

  return( rc );
}

/*
 * /brief Set receiver mode
 *
 * /param rig Pointer to rig struct
 * /param vfo VFO to operate on
 * /param mode Mode to set
 * /param width Bandwidth to set
 *
 */
static int ar7030p_set_mode( RIG * rig, vfo_t vfo, rmode_t mode,
                            pbwidth_t width )
{
  int rc = RIG_OK;
  unsigned char ar_mode = (unsigned char) USB;
  unsigned char ar_filter = (unsigned char) FILTER_3;
  int i;

  rc = lockRx( rig, LOCK_1 );
  if ( RIG_OK == rc )
  {
    /* TODO - deal with selected VFO */
    ar_mode = modeToNative( mode );

    rc = writeByte( rig, WORKING, MODE, ar_mode );
    if ( RIG_OK == rc )
    {
      if ( RIG_PASSBAND_NORMAL == width )
      {
	width = rig_passband_normal( rig, mode );
      }
      else
      {
	/* TODO - get filter BWs at startup */
        ar_filter = (unsigned char) 6;
        for ( i = 1; i <= 6; i++ )
	{
          if ( width <= filterTab[ i ] )
	  {
            if ( filterTab[ i ] < filterTab[ (int) ar_filter ] )
	    {
	      ar_filter = (unsigned char) i;
	    }
	  }

	  rig_debug( RIG_DEBUG_VERBOSE, "%s: width %d ar_filter %d filterTab[%d] %d\n", 
                     __func__, width, ar_filter, i, filterTab[i] );
	}
      }

      rc = writeByte( rig, WORKING, FILTER, ar_filter );
      if ( RIG_OK == rc )
      {
	rc = execRoutine( rig, SET_ALL );
      }
    }

    rc = lockRx( rig, LOCK_0 );
  }

  return( rc );
}

/*
 * /brief Get receiver mode and bandwidth
 *
 * /param rig Pointer to rig struct
 * /param vfo VFO to operate on
 * /param mode Pointer to value to hold mode
 * /param width Pointer to value to hold bandwidth
 *
 */
static int ar7030p_get_mode( RIG * rig, vfo_t vfo, rmode_t * mode,
                            pbwidth_t * width )
{
  int rc = RIG_OK;
  unsigned char bcd_bw;
  unsigned char m;

  assert( NULL != rig );
  assert( NULL != mode );
  assert( NULL != width );

  rc = lockRx( rig, LOCK_1 );
  if ( RIG_OK == rc )
  { 
    /* TODO - deal with selected VFO */
    rc = readByte( rig, WORKING, MODE, &m );
    if ( RIG_OK == rc )
    {
      *mode = modeToHamlib( m );
      rc = readByte( rig, WORKING, FLTBW, &bcd_bw );
      if ( RIG_OK == rc )
      {
	*width = (pbwidth_t) ((int) bcd2Int( bcd_bw ) * 100);
      }
    }

    rc = lockRx( rig, LOCK_0 );
  }

  return( rc );
}

/*
 * /brief Get memory channel parameters
 *
 * /param rig Pointer to rig struct
 * /param chan Channel number (0-399)
 * /param freq Pointer to frequency value
 * /param mode Pointer to mode value (1-7)
 * /param filt Pointer to filter value (1-6) 
 * /param pbs Pointer to passband tuning value
 * /param sql Pointer to squelch value (0-255)
 * /param id Pointer to channel ident string (14 chars)
 *
 */
static void ar7030p_get_memory( RIG * rig, const unsigned int chan,
                        double *const freq, unsigned char *const mode,
                        unsigned char *const filt, unsigned char *const pbs,
                        unsigned char *const sql, char *const id )
{
  int rc = RIG_OK;
  unsigned char v;
  unsigned int f;
  unsigned char *p = (unsigned char *) id;
  int i;

  assert( NULL != rig );
  assert( NULL != freq );
  assert( NULL != mode );
  assert( NULL != filt );
  assert( NULL != pbs );
  assert( NULL != sql );
  assert( NULL != id );

  rc = lockRx( rig, LOCK_1 );
  if ( RIG_OK == rc )
  {
    /* Squelch values */
    if ( 100 > chan )
    {
      rc = readByte( rig, BBRAM, (MEM_SQ + chan), &v ); /* mem_sq */
    }
    else if ( 176 > chan )
    {
      rc = readByte( rig, EEPROM2, (MEX_SQ + (chan * 16) ), &v ); /* mex_sq */
    }
    else
    {
      rc = readByte( rig, EEPROM3, (MEY_SQ + ((chan - 176) * 16) ), &v ); /* mey_sq */  
    }

    if ( RIG_OK == rc )
    {
      *sql = v;
    }
  
    /* Frequency, mode and filter values */
    if ( 100 > chan )
    {
      rc = read3Bytes( rig, EEPROM2, (MEM_FR + (chan * 4) ), &f ); /* mem_fr */
      rc = readByte(   rig, EEPROM2, (MEM_MD + (chan * 4) ), &v ); /* mem_md */
    }
    else
    {
      rc = read3Bytes( rig, EEPROM3, (MEX_FR + ((chan - 100) * 4) ), &f ); /* mex_fr */
      rc = readByte(   rig, EEPROM3, (MEX_MD + ((chan - 100) * 4) ), &v ); /* mex_md */
    }

    if ( RIG_OK == rc )
    {
      *freq = ddsToHz( f );
      *mode = ( v & 0x07 );
      *filt = ( ( v & 0x70 ) >> 4 );
      /* lockout = ( ( v & 0x80 ) >> 7 ); */
    }

    /* PBT values */
    if ( 100 > chan )
    {
      rc = readByte( rig, EEPROM1, (MEM_PB + chan), &v ); /* mem_pb */
    }
    else if ( 176 > chan )
    {
      rc = readByte( rig, EEPROM2, (MEX_PB + (chan * 16)), &v ); /* mex_pb */
    }
    else
    {
      rc = readByte( rig, EEPROM3, (MEY_PB + ((chan - 176) * 16)), &v ); /* mey_pb */
    }

    if ( RIG_OK == rc )
    {
      *pbs = v;
    }

    /* Memory ID values */
    for ( i = 0; i < 14; i++ )
    {
      if ( 176 > chan )
      {
	rc = readByte( rig, EEPROM2, (MEX_ID + (chan * 16)), p++ ); /* mex_id */
      }
      else
      {
	rc = readByte( rig, EEPROM3, (MEY_ID + ((chan - 176) * 16)), p++ ); /* mey_id */
      }

      if ( RIG_OK != rc )
      {
	p = (unsigned char *) id;
	break;
      }
    }
    *p++ = '\0';

    rc = lockRx( rig, LOCK_0 );
  }

}

/*
 * /brief Set receiver levels
 *
 * /param rig Pointer to rig struct
 * /param vfo VFO to operate on
 * /param level Level to set
 * /param val Value to set level to
 *
 * /return RIG_OK on success
 */
static int ar7030p_set_level( RIG * rig, vfo_t vfo, setting_t level,
                             value_t val )
{
  int rc = RIG_OK;
  unsigned char v;

  rc = lockRx( rig, LOCK_1 );
  if ( RIG_OK == rc )
  {
    /* TODO - deal with selected VFO */
    switch ( level )
    {
    case RIG_LEVEL_PREAMP:
      /* Scale parameter */
      if ( 10 <= val.i ) 
      {
	v = (unsigned char) 0;
      }
      else
      {
	v = (unsigned char) 1;
      }

      rc = writeByte( rig, WORKING, RFGAIN, v ); /* rfgain */

      rig_debug( RIG_DEBUG_VERBOSE, "%s: rfgain %d (%d)\n", __func__, val.i, v );

      rc = execRoutine( rig, SET_ALL );
      break;

    case RIG_LEVEL_ATT:
      /* Scale parameter */
      if ( 10 > val.i ) 
      {
	v = (unsigned char) 1;
      }
      else if ( 20 > val.i )
      {
	v = (unsigned char) 2;
      }
      else if ( 40 > val.i )
      {
	v = (unsigned char) 3;
      }
      else if ( 80 > val.i )
      {
	v = (unsigned char) 4;
      }
      else 
      {
        v = (unsigned char) 5;
      }

      rc = writeByte( rig, WORKING, RFGAIN, v ); /* rfgain */

      rig_debug( RIG_DEBUG_VERBOSE, "%s: rfgain %d (%d)\n", __func__, val.i, v );

      rc = execRoutine( rig, SET_ALL );
      break;

    case RIG_LEVEL_AF:
      /* Scale parameter */
      v = ( unsigned char ) ( ( val.f * ( VOL_MAX - VOL_MIN ) ) + VOL_MIN );
      v = ( v & 0x3f );

      rc = writeByte( rig, WORKING, AF_VOL, v ); /* af_vol */

      rig_debug( RIG_DEBUG_VERBOSE, "%s: af_vol %f (%d)\n", __func__, val.f, v );

      v = ( ( v >> 1 ) & 0x1f );    /* half value for L/R volume */

      rc = writeByte( rig, WORKING, AF_VLL, v ); /* af_vll */
      rc = writeByte( rig, WORKING, AF_VLR, v ); /* af_vlr */

      rc = execRoutine( rig, SET_AUDIO );
      break;

    case RIG_LEVEL_RF:
      /* Scale parameter, values 0 (99%) to 130 (3%) */
      v = (unsigned char) (134U - ((unsigned int) (val.f * 135.0)));

      rc = writeByte( rig, WORKING, IFGAIN, v ); /* ifgain */

      rig_debug( RIG_DEBUG_VERBOSE, "%s: ifgain %f (%d)\n", __func__, val.f, v );

      rc = execRoutine( rig, SET_ALL );
      break;

    case RIG_LEVEL_SQL:
      /* Scale parameter */
      v = (unsigned char) (val.f * 255.0);

      rc = writeByte( rig, WORKING, SQLVAL, v ); /* sqlval */

      rig_debug( RIG_DEBUG_VERBOSE, "%s: sqlval %f (%d)\n", __func__, val.f, v );

      rc = execRoutine( rig, SET_ALL );
      break;

    case RIG_LEVEL_PBT_IN:
      /* Scale parameter */
      v = (unsigned char) (val.f / (HZ_PER_STEP * 12.5));

      rc = writeByte( rig, WORKING, PBSVAL, v ); /* pbsval */

      rig_debug( RIG_DEBUG_VERBOSE, "%s: pbsval %f (%d)\n", __func__, val.f, v );

      rc = execRoutine( rig, SET_ALL );
      break;

    case RIG_LEVEL_CWPITCH:
      /* Scale parameter */
      v = (unsigned char) (val.f / (HZ_PER_STEP * 12.5));

      rc = writeByte( rig, WORKING, BFOVAL, v ); /* bfoval */

      rig_debug( RIG_DEBUG_VERBOSE, "%s: bfoval %f (%d)\n", __func__, val.f, v );

      rc = execRoutine( rig, SET_ALL );
      break;

    case RIG_LEVEL_NOTCHF:
      rc = -RIG_ENIMPL;
      break;

    case RIG_LEVEL_AGC:
      /* Scale parameter */
      v = agcToNative( val.i );
      rc = writeByte( rig, WORKING, AGCSPD, v ); /* agcspd */

      rig_debug( RIG_DEBUG_VERBOSE, "%s: agcspd %d (%d)\n", __func__, val.i, v );

      rc = execRoutine( rig, SET_ALL );
      break;

    default:
      rc = -RIG_EINVAL;
    };

    rc = lockRx( rig, LOCK_0 );
  }

  return( rc );
}


/*
 * /brief Get receiver levels
 *
 * /param rig Pointer to rig struct
 * /param vfo VFO to operate on
 * /param level Level to get
 * /param val Pointer to value to get
 *
 * /return RIG_OK on success
 */
static int ar7030p_get_level( RIG * rig, vfo_t vfo, setting_t level,
                             value_t * val )
{
  int rc = RIG_OK;
  unsigned char v;
  unsigned int x = 0;
  unsigned short s = 0;
  int i;

  rc = lockRx( rig, LOCK_1 );
  if ( RIG_OK == rc ) 
  {
    /* TODO - deal with selected VFO */
    switch ( level )
    {
    case RIG_LEVEL_PREAMP:
      rc = readByte( rig, WORKING, RFGAIN, &v ); /* rfgain */
      if ( RIG_OK == rc )
      {
	/* Scale parameter */
	if ( 0 == v )
	{
	  val->i = 10;
        }
        else
        {
	  val->i = 0;
        }

	rig_debug( RIG_DEBUG_VERBOSE, "%s: rfgain %d (%d)\n",
                   __func__, v, val->i );
      }
      break;

    case RIG_LEVEL_ATT:
      rc = readByte( rig, WORKING, RFGAIN, &v ); /* rfgain */
      if ( RIG_OK == rc )
      {
	/* Scale parameter */
	switch( v )
        {
	case 2:
	  val->i = 10;
	  break;

	case 3:
	  val->i = 20;
	  break;

	case 4:
	  val->i = 40;
	  break;

	default:
	case 0:
	case 1:
	  val->i = 0;    
	};

	rig_debug( RIG_DEBUG_VERBOSE, "%s: rfgain %d (%d)\n",
                   __func__, v, val->i );
      }
      break;

    case RIG_LEVEL_AF:
      rc = readByte( rig, WORKING, AF_VOL, &v ); /* af_vol */
      if ( RIG_OK == rc )
      {
	/* Scale parameter */
	v = ( v & 0x3f );
	val->f = (( (float) v - VOL_MIN) / ( VOL_MAX - VOL_MIN ) );

	rig_debug( RIG_DEBUG_VERBOSE, "%s: af_vol %d (%f)\n",
                   __func__, v, val->f );
      }
      break;

    case RIG_LEVEL_RF:
      rc = readByte( rig, WORKING, IFGAIN, &v ); /* ifgain */
      if ( RIG_OK == rc )
      {
	/* Scale parameter, values 0 (99%) to 130 (3%) */
	val->f = ((float) (134 - v) / 135.0 );

        rig_debug( RIG_DEBUG_VERBOSE, "%s: ifgain %d (%f)\n",
                    __func__, v, val->f );
      }
      break;

    case RIG_LEVEL_SQL:
      rc = readByte( rig, WORKING, SQLVAL, &v ); /* sqlval */
      if ( RIG_OK == rc )
      {
	/* Scale parameter */
	val->f = ((float) (v) / 255.0 );

	rig_debug( RIG_DEBUG_VERBOSE, "%s: sqlval %d (%f)\n",
                   __func__, v, val->f );
      }
      break;

    case RIG_LEVEL_PBT_IN:
      rc = readByte( rig, WORKING, PBSVAL, &v ); /* pbsval */
      if ( RIG_OK == rc )
      {
	/* Scale parameter */
	if (127 < v)
	{
	  v = v | 0xffffff00;
	}
	val->f = ((float) (v) * HZ_PER_STEP * 12.5 );

	rig_debug( RIG_DEBUG_VERBOSE, "%s: pbsval %d (%f)\n",
                   __func__, v, val->f );
      }
      break;

    case RIG_LEVEL_CWPITCH:
      rc = readByte( rig, WORKING, BFOVAL, &v ); /* bfoval */
      if ( RIG_OK == rc )
      {
	/* Scale parameter */
	if (127 < v)
	{
	  v = v | 0xffffff00;
	}
	val->f = ((float) (v) * HZ_PER_STEP * 12.5 );

	rig_debug( RIG_DEBUG_VERBOSE, "%s: bfoval %d (%f)\n",
                   __func__, v, val->f );
      }
      break;

    case RIG_LEVEL_NOTCHF:
      rc = readShort( rig, WORKING, NCHFR, &s ); /* nchfr */
      if ( RIG_OK == rc )
      {
	x = (unsigned int) s;

	/* Scale parameter */
	val->i = (int) ((float) (x) / NOTCH_STEP_HZ);
      
        rig_debug( RIG_DEBUG_VERBOSE, "%s: nchfr %d (%d)\n",
		   __func__, x, val->i );
      }
      break;

    case RIG_LEVEL_AGC:
      rc = readByte( rig, WORKING, AGCSPD, &v ); /* agcspd */
      if ( RIG_OK == rc )
      {
	/* Scale parameter */
        val->i = agcToHamlib( v );

	rig_debug( RIG_DEBUG_VERBOSE, "%s: agcspd %d (%d)\n",
                   __func__, v, val->i );
      }
      break;

    case RIG_LEVEL_RAWSTR:
      rc = readSignal( rig, &v );
      if ( RIG_OK == rc )
      {
	val->i = (int) v;
      }
      break;

    case RIG_LEVEL_STRENGTH:
      rc = readSignal( rig, &v );
      if ( RIG_OK == rc )
      {
        rc = getCalLevel( rig, v, &i );
        if ( RIG_OK == rc )
	{
	  val->i = i;
        }
      }
      break;

    default:
      rc = -RIG_EINVAL;
    }

    rc = lockRx( rig, LOCK_0 );
  }

  return( rc );
}

static int ar7030p_set_vfo( RIG * rig, vfo_t vfo )
{
  int rc = -RIG_OK;

  assert( NULL != rig );

  switch( vfo ) 
  {
  case RIG_VFO_B:
    rc = sendIRCode( rig, IR_VFO );
    break;

  case RIG_VFO_A:
  case RIG_VFO_CURR:
  default:
    break;
  }

  return ( -RIG_ENIMPL );
}

static int ar7030p_get_vfo( RIG * rig, vfo_t * vfo )
{
  int rc = -RIG_OK;

  assert( NULL != rig );
  assert( NULL != vfo );

  *vfo = RIG_VFO_CURR;

  return ( rc );
}

static int ar7030p_set_parm( RIG * rig, setting_t parm, value_t val )
{
  int rc = -RIG_ENIMPL;

  assert( NULL != rig );

  switch ( parm )
  {
  case RIG_PARM_APO:
    break;

  case RIG_PARM_TIME:
    break;

  case RIG_PARM_BAT:
    break;

  default:
    break;
  };

  return ( rc );
}

static int ar7030p_get_parm( RIG * rig, setting_t parm, value_t * val )
{
  int rc = -RIG_ENIMPL;

  assert( NULL != rig );
  assert( NULL != val );

  switch ( parm )
  {
  case RIG_PARM_APO:
    break;

  case RIG_PARM_TIME:
    break;

  case RIG_PARM_BAT:
    break;

  default:
    break;
  };

  return ( rc );
}

static int ar7030p_set_mem( RIG * rig, vfo_t vfo, int ch )
{
  int rc = -RIG_ENIMPL;

  assert( NULL != rig );

  return ( rc );
}

static int ar7030p_get_mem( RIG * rig, vfo_t vfo, int *ch )
{
  int rc = -RIG_ENIMPL;

  assert( NULL != rig );
  assert( NULL != ch );

  return ( rc );
}

static int ar7030p_vfo_op( RIG * rig, vfo_t vfo, vfo_op_t op )
{
  int rc = -RIG_ENIMPL;

  assert( NULL != rig );

  switch( op )
  {
  case RIG_OP_CPY:
    rc = -RIG_ENIMPL;
    break;

  case RIG_OP_XCHG:
    rc = -RIG_ENIMPL;
    break;

  case RIG_OP_TOGGLE:
    rc = sendIRCode( rig, IR_VFO );
    break;

  default:
    break;
  };

  return ( rc );
}

static int ar7030p_scan( RIG * rig, vfo_t vfo, scan_t scan, int ch )
{
  int rc = -RIG_ENIMPL;

  assert( NULL != rig );

  return ( rc );
}

static int ar7030p_get_dcd( RIG * rig, vfo_t vfo, dcd_t * dcd )
{
  int rc = -RIG_OK;
  unsigned char v;

  assert( NULL != rig );
  assert( NULL != dcd );

  rc = lockRx( rig, LOCK_1 );
  if ( RIG_OK == rc ) 
  {
    rc = readByte( rig, WORKING, BITS + 2, &v );
    if ( RIG_OK == rc )
    {
      if ( (v & 0x02) )
      {
	if ( (v & 0x01) ) /* low bit set if Squelch is NOT active/open */
	{
	  *dcd = RIG_DCD_OFF;
	}
        else
        {
	  *dcd = RIG_DCD_ON;
	}
      }
      else
      {
        *dcd = RIG_DCD_ON;
      }
    }

    rc = lockRx( rig, LOCK_0 );
  }

  return ( rc );
}

static int ar7030p_set_ts( RIG * rig, vfo_t vfo, shortfreq_t ts )
{
  int rc = RIG_OK;
  unsigned short v;

  assert( NULL != rig );

  rc = lockRx( rig, LOCK_1 );
  if ( RIG_OK == rc )
  {
    /* Scale parameter */
    v = (unsigned short) ((double) (ts + 1) / HZ_PER_STEP);

    rc = writeShort( rig, WORKING, CHNSTP, v ); /* chnstp */
    if ( RIG_OK == rc )
    {
      rc = execRoutine( rig, SET_ALL );

      rig_debug( RIG_DEBUG_VERBOSE, "%s: chnstp %d (%d)\n", __func__, ts, v );
    }

    rc = lockRx( rig, LOCK_0 );
  }

  return ( rc );
}

/*
 * /brief Get receiver tuning step size
 *
 * /param rig Pointer to rig struct
 * /param vfo VFO to operate on
 * /param ts Pointer to tuning step value
 *
 * /return RIG_OK on success
 */
static int ar7030p_get_ts( RIG * rig, vfo_t vfo, shortfreq_t * ts )
{
  int rc = RIG_OK;
  unsigned short v;
  double x;

  assert( NULL != rig );
  assert( NULL != ts );

  rc = lockRx( rig, LOCK_1 );
  if ( RIG_OK == rc )
  {
    rc = readShort( rig, WORKING, CHNSTP, &v ); /* chnstp */
    if ( RIG_OK == rc )
    {
      x = (double) v;
      *ts = (shortfreq_t) (x * HZ_PER_STEP);

      rig_debug( RIG_DEBUG_VERBOSE, "%s: step= %d\n", __func__, *ts );
    }

    rc = lockRx( rig, LOCK_0 );
  }

  return ( rc );
}

/*
 * /brief Set receiver power status
 *
 * /param rig Pointer to rig struct
 * /param status Power status to set
 *
 * /return RIG_OK on success
 */
static int ar7030p_set_powerstat( RIG * rig, powerstat_t status )
{
  int rc = -RIG_ENIMPL;

  assert( NULL != rig );

  rc = lockRx( rig, LOCK_1 );
  if ( RIG_OK == rc )
  {
    switch ( status )
    {
    case RIG_POWER_OFF:
      break;

    case RIG_POWER_ON: 
      break;

    default:
      break;
    }

    rc = lockRx( rig, LOCK_0 );
  }

  return( -RIG_ENIMPL );
}

/*
 * /brief Get receiver power status
 *
 * /param rig Pointer to rig struct
 * /param status Pointer to power status value
 *
 * /return RIG_OK on success
 */
static int ar7030p_get_powerstat( RIG * rig, powerstat_t * status )
{
  int rc = RIG_OK;
  unsigned char v;

  assert( NULL != rig );

  rc = lockRx( rig, LOCK_1 );
  if ( RIG_OK == rc )
  {
    rc = readByte( rig, WORKING, PDFLGS, &v );
    if ( RIG_OK == rc )
    {
      if ( 0 == ( v & 0x01 ) )
      {
	*status = RIG_POWER_OFF;
      }
      else
      {
	*status = RIG_POWER_ON;
      }
    }

    rc = lockRx( rig, LOCK_0 );
  }

  return( rc );
}

/*
 * /brief Reset receiver
 *
 * /param rig Pointer to rig struct
 * /param reset Reset operation to perform
 *
 * /return RIG_OK on success
 */
static int ar7030p_reset( RIG * rig, reset_t reset )
{
  int rc = RIG_OK;

  assert( NULL != rig );

  switch ( reset )
  {
  case RIG_RESET_SOFT:
    rc = -RIG_ENIMPL;
    break;

  default:
    rc = -RIG_EINVAL;
  }

  return( rc );
}

static int ar7030p_set_func( RIG * rig, vfo_t vfo, setting_t func,
                             int status )
{
  assert( NULL != rig );

  return ( -RIG_ENIMPL );
}

static int ar7030p_get_func( RIG * rig, vfo_t vfo, setting_t func,
                             int *status )
{
  assert( NULL != rig );
  assert( NULL != status );

  return ( -RIG_ENIMPL );
}

static int ar7030p_decode_event( RIG * rig )
{
  assert( NULL != rig );

  return ( -RIG_ENIMPL );
}

static int ar7030p_set_channel( RIG * rig, const channel_t * chan )
{
  assert( NULL != rig );
  assert( NULL != chan );

  return ( -RIG_ENIMPL );
}

static int ar7030p_get_channel( RIG * rig, channel_t * chan )
{
  assert( NULL != rig );
  assert( NULL != chan );

  chan->vfo = RIG_VFO_MEM;
  chan->ant = RIG_ANT_NONE;
  chan->freq = 0;
  chan->mode = RIG_MODE_NONE;
  chan->width = RIG_PASSBAND_NORMAL;
  chan->tx_freq = 0;
  chan->tx_mode = RIG_MODE_NONE;
  chan->tx_width = RIG_PASSBAND_NORMAL;
  chan->split = RIG_SPLIT_OFF;
  chan->tx_vfo = RIG_VFO_NONE;
  chan->rptr_shift = RIG_RPT_SHIFT_NONE;
  chan->rptr_offs = 0;
  chan->tuning_step = 0;
  chan->rit = 0;
  chan->xit = 0;
  chan->funcs = 0;
  chan->levels[rig_setting2idx(RIG_LEVEL_AGC)].i = RIG_AGC_OFF;
  chan->levels[rig_setting2idx(RIG_LEVEL_ATT)].i = 0;
  chan->ctcss_tone = 0;
  chan->ctcss_sql = 0;
  chan->dcs_code = 0;
  chan->dcs_sql = 0;
  chan->scan_group = 0;
  chan->flags = RIG_CHFLAG_SKIP;
  strcpy(chan->channel_desc, "");

  return ( -RIG_ENIMPL );
}

const struct rig_caps ar7030p_caps = {
  .rig_model = RIG_MODEL_AR7030P,
  .model_name = "AR7030 Plus",
  .mfg_name = "AOR",
  .version = "0.1",
  .copyright = "LGPL",
  .status = RIG_STATUS_NEW,
  .rig_type = RIG_TYPE_RECEIVER,

  .dcd_type = RIG_DCD_RIG,

  .port_type = RIG_PORT_SERIAL,
  .serial_rate_min = 1200,
  .serial_rate_max = 1200,
  .serial_data_bits = 8,
  .serial_stop_bits = 1,
  .serial_parity = RIG_PARITY_NONE,
  .serial_handshake = RIG_HANDSHAKE_NONE,
  .write_delay = 0,
  .post_write_delay = 12,
  .timeout = 650,
  .retry = 0,

  .has_get_func = AR7030P_FUNC,
  .has_set_func = AR7030P_FUNC,
  .has_get_level = AR7030P_LEVEL,
  .has_set_level = RIG_LEVEL_SET( AR7030P_LEVEL ),
  .has_get_parm = AR7030P_PARM,
  .has_set_parm = RIG_PARM_SET( AR7030P_PARM ),

  .level_gran = {
                 [LVL_PREAMP]   = {.min = {.i = 0},    .max = {.i = 10} },
                 [LVL_ATT]      = {.min = {.i = 0},    .max = {.i = 20} },
                 [LVL_RF]       = {.min = {.f = 0.0},  .max = {.f = 1.0} },
                 [LVL_AF]       = {.min = {.f = 0.0},  .max = {.f = 1.0} },
                 [LVL_SQL]      = {.min = {.f = 0.0},  .max = {.f = 1.0} },
                 [LVL_IF]       = {.min = {.i = 255},  .max = {.i = 0} },
                 [LVL_PBT_IN]  = {.min = {.f = -4248.0}, .max = {.f = 4248.0} },
                 [LVL_CWPITCH]  = {.min = {.i = -4248}, .max = {.i = 4248} },
                 [LVL_NOTCHF]   = {.min = {.i = 0},    .max = {.i = 10000} },
                 [LVL_AGC]      = {.min = {.i = 0},    .max = {.i = 10} },
                 [LVL_BALANCE]  = {.min = {.f = -1.0}, .max = {.f = 1.0} },
                 [LVL_RAWSTR]   = {.min = {.i = 0},    .max = {.i = 255} },
                 [LVL_STRENGTH] = {.min = {.i = 0},    .max = {.i = 255} },
                 },

  .extparms = NULL,
  .extlevels = NULL,

  .parm_gran = {
                [PARM_APO]  = {.min = {.i = 1}, .max = {.i = 86400} },
                [PARM_TIME] = {.min = {.i = 0}, .max = {.i = 86400} },
                [PARM_BAT]  = {.min = {.f = 0.0}, .max = {.f = 1.0} },
                },

  .preamp = {10, RIG_DBLST_END,},
  .attenuator = {10, 20, RIG_DBLST_END,},
  .max_rit = Hz( 0 ),
  .max_xit = Hz( 0 ),
  .max_ifshift = Hz( 4248 ),
  .announces = RIG_ANN_NONE,
  .vfo_ops = AR7030P_VFO_OPS,
  .scan_ops = RIG_SCAN_STOP | RIG_SCAN_MEM | RIG_SCAN_VFO,
  .targetable_vfo = 0,
  .transceive = RIG_TRN_OFF,
  .bank_qty = 0,
  .chan_desc_sz = 14,
  .priv = (void *)NULL,

  .chan_list = {{0, 399, RIG_MTYPE_MEM, AR7030P_MEM_CAP}, RIG_CHAN_END,},

  .rx_range_list1 = {
                     {kHz( 10 ), kHz( 32010 ), AR7030P_MODES, -1, -1,
                      AR7030P_VFO},
                     RIG_FRNG_END,
                     },
  .tx_range_list1 = {RIG_FRNG_END,},

  .rx_range_list2 = {
                     {kHz( 10 ), kHz( 32010 ), AR7030P_MODES, -1, -1,
                      AR7030P_VFO},
                     RIG_FRNG_END,
                     },
  .tx_range_list2 = {RIG_FRNG_END,},

  .tuning_steps = {
                   {AR7030P_MODES, Hz( 10 )},
                   {AR7030P_MODES, Hz( 20 )},
                   {AR7030P_MODES, Hz( 50 )},
                   {AR7030P_MODES, Hz( 100 )},
                   {AR7030P_MODES, Hz( 200 )},
                   {AR7030P_MODES, Hz( 500 )},
                   {AR7030P_MODES, kHz( 1 )},
                   {AR7030P_MODES, kHz( 2 )},
                   {AR7030P_MODES, kHz( 5 )},
                   {AR7030P_MODES, kHz( 6.25 )},
                   {AR7030P_MODES, kHz( 9 )},
                   {AR7030P_MODES, kHz( 10 )},
                   {AR7030P_MODES, Hz( 12500 )},
                   {AR7030P_MODES, kHz( 20 )},
                   {AR7030P_MODES, kHz( 25 )},
                   RIG_TS_END,
                   },

  .filters = {
              {RIG_MODE_FM,   kHz( 9.5 )},
              {RIG_MODE_FM,   kHz( 0 )},
              {RIG_MODE_FM,   kHz( 0 )},
              {RIG_MODE_AMS,  kHz( 6.5 )},
              {RIG_MODE_AMS,  kHz( 5.3 )},
              {RIG_MODE_AMS,  kHz( 9.5 )},
              {RIG_MODE_AM,   kHz( 5.3 )},
              {RIG_MODE_AM,   kHz( 3.7 )},
              {RIG_MODE_AM,   kHz( 6.5 )},
              {RIG_MODE_SSB,  kHz( 2.0 )},
              {RIG_MODE_SSB,  kHz( 1.4 )},
              {RIG_MODE_SSB,  kHz( 3.7 )},
              {RIG_MODE_CW,   kHz( 1.4 )},
              {RIG_MODE_CW,   kHz( 0 )},
              {RIG_MODE_CW,   kHz( 2.0 )},
              {RIG_MODE_RTTY, kHz( 1.4 )},
              {RIG_MODE_RTTY, kHz( 0 )},
              {RIG_MODE_RTTY, kHz( 2.0 )},
              RIG_FLT_END,
              },

  .str_cal = AR7030P_STR_CAL,
  .cfgparams = NULL,
  .priv = ( void * ) &ar7030p_priv_caps,

  .rig_open = ar7030p_open,
  .rig_close = ar7030p_close,
  .set_freq = ar7030p_set_freq,
  .get_freq = ar7030p_get_freq,
  .set_mode = ar7030p_set_mode,
  .get_mode = ar7030p_get_mode,
  .set_vfo = ar7030p_set_vfo,
  .get_vfo = ar7030p_get_vfo,

  .get_dcd = ar7030p_get_dcd,

  .set_ts = ar7030p_set_ts,
  .get_ts = ar7030p_get_ts,

  .set_powerstat = ar7030p_set_powerstat,
  .get_powerstat = ar7030p_get_powerstat,
  .reset = ar7030p_reset,

  .set_level = ar7030p_set_level,
  .get_level = ar7030p_get_level,
  .set_func = ar7030p_set_func,
  .get_func = ar7030p_get_func,
  .set_parm = ar7030p_set_parm,
  .get_parm = ar7030p_get_parm,

  .set_ext_level = RIG_FUNC_NONE,
  .get_ext_level = RIG_FUNC_NONE,
  .set_ext_parm = RIG_FUNC_NONE,
  .get_ext_parm = RIG_FUNC_NONE,
  .set_conf = RIG_FUNC_NONE,
  .get_conf = RIG_FUNC_NONE,

  .set_mem = ar7030p_set_mem,
  .get_mem = ar7030p_get_mem,
  .vfo_op = ar7030p_vfo_op,
  .scan = ar7030p_scan,

  .decode_event = ar7030p_decode_event,
  .set_channel = ar7030p_set_channel,
  .get_channel = ar7030p_get_channel,
  .get_info = ar7030p_get_info,

  .set_chan_all_cb = RIG_FUNC_NONE,
  .get_chan_all_cb = RIG_FUNC_NONE,
  .set_mem_all_cb = RIG_FUNC_NONE,
  .get_mem_all_cb = RIG_FUNC_NONE,

};
