
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
		{ 10, -93 }, \
		{ 10, -83 }, \
		{ 10, -73 }, \
		{ 10, -63 }, \
		{ 20, -43 }, \
		{ 20, -23 }, \
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
};

static const struct ar7030p_priv_caps ar7030p_priv_caps = {
  .max_freq_len = 3,
  .info_len = 14,
  .mem_len = 17,
  .pbs_info_len = 1,
  .pbs_len = 1,
};

static enum PAGE_e curPage = NONE; /* Current memory page */
static unsigned int curAddr = 65535; /* Current page address */

#if 0
/*
 *     Code Ident Operation
 *     0x   NOP   No Operation
 */
static int NOP( RIG *rig, unsigned char x )
{
  int rc = -RIG_OK;

  unsigned char op = ( ( 0x0f & x ) | op_NOP );

  assert( NULL != rig );

  rc = write_block( &rig->state.rigport, ( char * ) &op, 1 );
  if ( 0 != rc )
  {
    rc = -RIG_EIO;
  }

  return ( rc );
}

/*
 *     Code Ident Operation
 *     3x   SRH   Set H-register    x -> H-register (4-bits)
 */
static int SRH( RIG *rig, unsigned char x )
{
  int rc = -RIG_OK;

  unsigned char op = ( ( 0x0f & x ) | op_SRH );

  assert( NULL != rig );

  rc = write_block( &rig->state.rigport, ( char * ) &op, 1 );
  if ( 0 != rc )
  {
    rc = -RIG_EIO;
  }

  return ( rc );
}

/*
 *     Code Ident Operation
 *     5x   PGE   Set page          x -> Page register (4-bits)
 */
static int PGE( RIG *rig, enum PAGE_e page )
{
  int rc = -RIG_OK;

  unsigned char op = ( ( 0x0f & page ) | op_PGE );

  assert( NULL != rig );

  switch ( page )
  {
  case WORKING:
  case BBRAM:
  case EEPROM1:
  case EEPROM2:
  case EEPROM3:
  case ROM:
    rc = write_block( &rig->state.rigport, ( char * ) &op, 1 );
    if ( 0 != rc )
    {
      rc = -RIG_EIO;
    }
    break;

  case NONE:
  default:
    rig_debug( RIG_DEBUG_VERBOSE, "PGE: invalid page %d\n", page );

    rc = -RIG_EINVAL;
    break;
  };

  return ( rc );
}

/*
 *     Code Ident Operation
 *     4x   ADR   Set address       0Hx -> Address register (12-bits)
 *                                  0 -> H-register
 */
static int ADR( RIG *rig, unsigned char x )
{
  int rc = -RIG_OK;

  unsigned char op = ( ( 0x0f & x ) | op_ADR );

  assert( NULL != rig );

  rc = write_block( &rig->state.rigport, ( char * ) &op, 1 );
  if ( 0 != rc )
  {
    rc = -RIG_EIO;
  }

  return ( rc );
}

/*
 *     Code Ident Operation
 *     1x   ADH   Set address high  x -> Address register (high 4-bits)
 */
static int ADH( RIG *rig, unsigned char x )
{
  int rc = -RIG_OK;

  unsigned char op = ( ( 0x0f & x ) | op_ADH );

  assert( NULL != rig );

  rc = write_block( &rig->state.rigport, ( char * ) &op, 1 );
  if ( 0 != rc )
  {
    rc = -RIG_EIO;
  }

  return ( rc );
}

/*
 *     Code Ident Operation
 *     6x   WRD   Write data        Hx -> [Page, Address]
 *                                  Address register + 1 -> Address register
 *                                  0 -> H-register, 0 -> Mask register
 */
static int WRD( RIG *rig, unsigned char out )
{
  int rc = -RIG_OK;

  unsigned char op = ( ( 0x0f & out ) | op_WRD );

  assert( NULL != rig );

  rc = write_block( &rig->state.rigport, ( char * ) &op, 1 );
  if ( 0 != rc )
  {
    rc = -RIG_EIO;
  }

  return ( rc );
}

/*
 *     Code Ident Operation
 *     9x   MSK   Set mask          Hx -> Mask register <1>
 *                                  0 -> H-register
 */
static int MSK( RIG *rig, unsigned char mask )
{
  int rc = -RIG_OK;

  unsigned char op = ( ( 0x0f & mask ) | op_MSK );

  assert( NULL != rig );

  rc = write_block( &rig->state.rigport, ( char * ) &op, 1 );
  if ( 0 != rc )
  {
    rc = -RIG_EIO;
  }

  return ( rc );
}

/*
 *     Code Ident Operation
 *     2x   EXE   Execute routine x
 */
static int EXE( RIG *rig, enum ROUTINE_e routine )
{
  int rc = -RIG_OK;

  unsigned char op = ( ( 0x0f & routine ) | op_EXE );

  assert( NULL != rig );

  switch ( routine )
  {
  case RESET:
  case SET_FREQ:
  case SET_MODE:
  case SET_PASS:
  case SET_ALL:
  case SET_AUDIO:
  case SET_RFIF:
  case DIR_RX_CTL:
  case DIR_DDS_CTL:
  case DISP_MENUS:
  case DISP_FREQ:
  case DISP_BUFF:
  case READ_SIGNAL:
  case READ_BTNS:
    rc = write_block( &rig->state.rigport, ( char * ) &op, 1 );
    if ( 0 != rc )
    {
      rc = -RIG_EIO;
    }
    break;

  default:
    rig_debug( RIG_DEBUG_VERBOSE, "EXE: invalid routine %d\n", routine );

    rc = -RIG_EINVAL;
    break;
  };

  return ( rc );
}

/*
 *     Code Ident Operation
 *     7x   RDD   Read data         [Page, Address] -> Serial output
 *                                  Address register + x -> Address register
 */
static int RDD( RIG *rig, unsigned char len )
{
  int rc = -RIG_OK;
  unsigned char inChr = 0;
  unsigned char op = ( ( 0x0f & len ) | op_RDD );

  assert( NULL != rig );

  rc = write_block( &rig->state.rigport, ( char * ) &op, 1 );
  if ( 0 != rc )
  {
    rc = -RIG_EIO;
  }
  else
  {
    rc = read_block( &rig->state.rigport, ( char * ) &inChr, len );
    if ( 1 != rc )
    {
      rc = -RIG_EIO;
    }
    else 
    {
      rc = (int) inChr;
    }
  }

  return ( rc );
}

/*
 *     Code Ident Operation
 *     8x   LOC   Set lock level x
 */
static int LOC( RIG *rig, enum LOCK_LVL_e level )
{
  int rc = -RIG_OK;

  unsigned char op = ( ( 0x0f & level ) | op_LOC );

  assert( NULL != rig );

  switch ( level )
  {
  case LOCK_0:
  case LOCK_1:
  case LOCK_2:
  case LOCK_3:
    rc = write_block( &rig->state.rigport, ( char * ) &op, 1 );
    if ( 0 != rc )
    {
      rc = -RIG_EIO;
    }
    break;

  default:
    rig_debug( RIG_DEBUG_VERBOSE, "LOC: invalid lock level %d\n", level );

    rc = -RIG_EINVAL;
    break;
  };

  return ( rc );
}

/*
 *     Code Ident Operation
 *     Ax   BUT   Operate button x  <1>
 */
static int BUT( RIG *rig, enum BUTTON_e button )
{
  int rc = -RIG_OK;

  unsigned char op = ( ( 0x0f & button ) | op_BUT );

  assert( NULL != rig );

  switch ( button )
  {
  case BTN_NONE:
    break;

  case BTN_UP:
  case BTN_DOWN:
  case BTN_FAST:
  case BTN_FILTER:
  case BTN_RFIF:
  case BTN_MEMORY:
  case BTN_STAR:
  case BTN_MENU:
  case BTN_POWER:
    rc = write_block( &rig->state.rigport, ( char * ) &op, 1 );
    if ( 0 != rc )
    {
      rc = -RIG_EIO;
    }
    break;

  default:
    rig_debug( RIG_DEBUG_VERBOSE, "BUT: invalid button %d\n", button );

    rc = -RIG_EINVAL;
    break;
  };

  return ( rc );
}

#endif // 0

/*
 * \brief Convert one byte BCD value to int
 *
 * \param bcd BCD value (0-99)
 *
 * \return Integer value of BCD parameter (0-99), 0xff on failure
 */
static unsigned char BCD2Int( const unsigned char bcd )
{
  unsigned char rc = (unsigned char) 0xff;
  unsigned char hi = ((bcd & 0xf0) >> 4);
  unsigned char lo =  (bcd & 0x0f);

  if ( (unsigned char) 0x0a > hi )
  {
    rc = hi * (unsigned char) 10;
    if ( (unsigned char) 0x0a > lo )
    {
      rc += lo;
    }
    else
    {
      rc = (unsigned char) 0xff;
    }
  }

  return( rc );
}

/*
 * \brief Convert int into 2 digit BCD number
 *
 * \param int Integer value (0-99)
 *
 * \return 2 digit BCD equvalent (0-99), 0xff on failure
 */
static unsigned char Int2BCD( const unsigned char val )
{
  unsigned char rc = (unsigned char) 0xff;
  unsigned char tens = (val / (unsigned char) 10);
  unsigned char ones = (val % (unsigned char) 10);

  if ( (unsigned char) 10 > tens )
  {
    rc = ( tens << 4 );
    if ( (unsigned char) 10 > ones )
    {
      rc = rc | ones;
    }
    else
    {
      rc = (unsigned char) 0xff;
    }
  }

  return ( rc );
}

/*
 * \brief Convert raw AGC value to calibrated level in dBm
 *
 * \param rig Pointer to rig struct
 * \param rawAgc raw AGC value (0-255)
 * \param tab Pointer to calibration table struct
 *
 * \return Calibrated level in dBm, large positive value on failure
 */
static int getCalLevel( RIG * rig, unsigned char rawAgc, cal_table_t *tab )
{
  int i;
  int rc = 100000;
  int raw = (int) rawAgc;

  assert( NULL != rig );
  assert( NULL != tab );

  for ( i = 0; i < tab->size; i++ )
  {
    if ( 0 > ( tab->table[ i ].raw - raw ) )
    {
      rc = tab->table[ i ].val;
      rc += ( raw / tab->table[ i ].val ) * 10;
      /* TODO: 20 db steps for higher values */
      break;
    }
    else
    {
      raw = raw - tab->table[ i ].raw;
    }
  }

  /* TODO - factor in RFAGC setting */
  return ( rc );
}

/*
 * /brief Open I/O to receiver
 *
 * /param rig Pointer to rig struct
 *
 * /return 0 on success, < 0 on failure
 */
static int ar7030p_open( RIG * rig )
{
  assert( NULL != rig );

  rig_debug( RIG_DEBUG_VERBOSE, "%s: \n", __func__ );

  curPage = NONE;
  curAddr = 65535;

  return ( -RIG_OK );
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

/*
 * /brief Write a byte to the radio
 *
 * /param rig Pointer to rig struct
 * /param c Data to write
 *
 * /return 0 on success, < 0 on failure
 */
static int writeByte( RIG * rig, unsigned char c )
{
  int rc = -1;
  unsigned char v = c;

  assert( NULL != rig );

  rc = write_block( &rig->state.rigport, (char *) &v, 1 );
  if ( 0 == rc )
  {
    rig_debug( RIG_DEBUG_VERBOSE, "%s: wrote byte 0x%02x\n", __func__, c );
    curAddr++;
  }
 
  return( rc );
}

/*
 * /brief Write data to the radio using the WRD opcode
 *
 * /param rig Pointer to rig struct
 * /param c Data to write
 *
 * /return 0 on success, < 0 on failure
 */
static int writeData( RIG * rig, unsigned char c )
{
  int rc = -1;
  unsigned char hi = SRH((c & 0xf0) >> 4);
  unsigned char lo = WRD( c & 0x0f);

  assert( NULL != rig );

  rc = write_block( &rig->state.rigport, (char *) &hi, 1 );
  if ( 0 == rc )
  {
    rc = write_block( &rig->state.rigport, (char *) &lo, 1 );
    if ( 0 == rc )
    {
      rig_debug( RIG_DEBUG_VERBOSE, "%s: wrote byte 0x%02x\n", __func__, c );
      curAddr++;
    }
  }
 
  return( rc );
}

/*
 * /brief Read a byte from the radio
 *
 * /param rig Pointer to rig struct
 *
 * /return Data read (0-255) on success, < 0 on failure
 */
static int readByte( RIG * rig )
{
  int rc = -1;
  unsigned char reply;
  unsigned char cmd = RDD( 1 ); // Read command

  assert( NULL != rig );

  rc = write_block( &rig->state.rigport, (char *) &cmd, 1 );
  if ( 0 == rc )
  {
    rc = read_block( &rig->state.rigport, (char *) &reply, 1 );
    if ( 1 == rc )
    {
      rc = (int) reply;
      curAddr++;
      rig_debug( RIG_DEBUG_VERBOSE, "%s: read byte 0x%02x\n", __func__, reply );
    }
  }

  return( rc );
}

/*
 * /brief Read raw AGC value from the radio
 *
 * /param rig Pointer to rig struct
 *
 * /return Raw AGC value (0-255) on success, < 0 on failure
 */
static int readSignal( RIG * rig )
{
  int rc = -1;
  char reply;
  unsigned char cmd = EXE( READ_SIGNAL ); // Read raw AGC value

  assert( NULL != rig );

  rc = write_block( &rig->state.rigport, (char *) &cmd, 1 );
  if ( 0 == rc )
  {
    rc = read_block( &rig->state.rigport, (char *) &reply, 1 );
    if ( 1 == rc )
    {
      rc = (int) reply;
    }
  }

  return( rc );
}

/*
 * /brief Flush I/O with radio
 *
 * /param rig Pointer to rig struct
 *
 */
static void flushBuffer( RIG * rig )
{
  (void) writeByte( rig, '/' );
}

/*
 * /brief Set address for I/O with radio
 *
 * /param rig Pointer to rig struct
 * /param page Memory page number (0-4, 15)
 * /param addr Address offset within page (0-4095, depending on page)
 *
 * Statics curPage and curAddr shadow radio's copies so that 
 * page and address are only set when needed
 */
static void setAddr( RIG * rig, enum PAGE_e page, unsigned int addr )
{
  assert( NULL != rig );

  if ( ( EEPROM3 >= page ) || ( ROM == page ) )
  {
    if ( PAGE_SIZE[page] > addr )
    {
      if ( curPage != page )
      {
        curPage = page;
        (void) writeByte( rig, PGE( page ) );
      }

      if ( curAddr != addr )
      {
        curAddr = addr;

        (void) writeByte( rig, SRH( ( 0x0f0 & addr ) >> 4 ) );
        (void) writeByte( rig, ADR( ( 0x00f & addr ) ) );

        if ( 0xff < addr )
        {
          (void) writeByte( rig, ADH( ( 0xf00 & addr ) >> 8 ) );
        }
      }
    }
  }
}

/*
 * \brief Get bandwidth of given filter
 *
 * \param rig Pointer to rig struct
 * \param filter Filter number (1-6)
 *
 * \return Filter bandwidth in Hz, -1 on failure
 */
static int getFilterBW( RIG *rig, enum FILTER_e filter )
{
  int rc = -1;
  unsigned char bw;

  (void) setAddr( rig, BBRAM, FL_SEL + ((filter - 1) * 4) );
  bw = (unsigned char) readByte( rig );

  if ( (unsigned char) -1 != bw )
  {
    rc = (int) BCD2Int( bw ) * 100;
  }

  rig_debug( RIG_DEBUG_VERBOSE, "%s: filter %d BW %d\n", __func__, filter, rc );

  return( rc );
}

/*
 * /brief Convert DDS steps to frequency in Hz
 *
 * /param steps DDS count
 *
 * /return Frequency in Hz or 0 on failure
 */
static double ddsToHz( const unsigned int steps )
{
  double rc = 0.0;

  rc = ( (double) steps * 44545000.0 / 16777216.0 );

  return( rc );
}

/*
 * /brief Convert frequency in Hz to DDS steps
 *
 * /param freq Frequency in Hz
 *
 * /return DDS steps (24 bits) or 0 on failure
 */
static unsigned int hzToDDS( const double freq )
{
  unsigned int rc = 0;
  double err[3] = { 0.0, 0.0, 0.0 };

  rc = (unsigned int) ( freq * 16777216.0 / 44545000.0 );

  /* calculate best DDS count based on bletcherous,
     irrational tuning step of 2.65508890151977539062 Hz/step
     (actual ratio is 44545000.0 / 16777216.0) */
  err[ 0 ] = fabs( freq - ddsToHz( (rc - 1) ) );
  err[ 1 ] = fabs( freq - ddsToHz( rc ) );
  err[ 2 ] = fabs( freq - ddsToHz( (rc + 1) ) );

  if ( err[ 0 ] < err[ 1 ] && err[ 0 ] < err[ 2 ] )
  {
    rc--;
  }
  else if ( err[ 2 ] < err[ 1 ] && err[ 2 ] < err[ 0 ] )
  {
    rc++;
  }

  rig_debug( RIG_DEBUG_VERBOSE, "%s: err[0 - 2] = %f %f %f rc 0x%08x\n",
	     __func__, err[ 0 ], err[ 1 ], err[ 2 ], rc );
  
  return( rc );
}

/*
 * /brief Set frequency in working memory
 *
 * /param rig Pointer to rig struct
 * /param freq Frequency in Hz
 *
 */
static void setFreq( RIG * rig, const double freq )
{
  unsigned int f = hzToDDS( freq );

  rig_debug( RIG_DEBUG_VERBOSE, "%s: DDS count 0x%08x\n", __func__, f );

  (void) writeData( rig, (unsigned char) ( f >> 16) );

  f = f & 0x00ffff;
  (void) writeData( rig, (unsigned char) ( f >> 8) );

  f = f & 0x0000ff;
  (void) writeData( rig, (unsigned char) f );
}

/*
 * /brief Get frequency from working memory
 *
 * /param rig Pointer to rig struct
 * /param freq Pointer to var to hold frequency value (in Hz)
 *
 */
static void getFreq( RIG * rig, double *const freq )
{
  unsigned int f = 0;
  int i;
  int c;

  for ( i = 0; i < 3; i++ )
  {
    c = readByte( rig );

    if ( -1 != c )
    {
      f = ( f << 8 ) + c;
    }
    else
    {
      return;
    }
  }

  rig_debug( RIG_DEBUG_VERBOSE, "%s: DDS count 0x%08x\n", __func__, f );
  
  *freq = ddsToHz( f );
}

/*
 * /brief Lock receiver for remote operations
 *
 * /param rig Pointer to rig struct
 * /param level Lock level (0-3)
 *
 */
static void lockRx( RIG * rig, enum LOCK_LVL_e level )
{
  if ( 4 > level )
  {
    (void) writeByte( rig, LOC( level ) );
  }
}


/*
 * /brief Unlock receiver from remote operations
 *
 * /param rig Pointer to rig struct
 *
 */
static void unlockRx( RIG * rig )
{
  (void) writeByte( rig, LOC( 0 ) );
}

/*
 * /brief Get receiver/software identification
 *
 * /param rig Pointer to rig struct
 * /param ident Pointer to char string to hold ID
 *
 */
static void ar7030p_get_ident( RIG * rig, char *const ident )
{
  unsigned int i;
  char *p = ( char * ) ident;

  (void) setAddr( rig, ROM, IDENT );

  for ( i = 0; i < PAGE_SIZE[ROM]; i++ )
  {
    *p++ = (char) readByte( rig );
  }

  *p++ = '\0';
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

  caps = rig->caps;

  lockRx( rig, 1 );

  if ( ( caps->rx_range_list1[ 0 ].end   > freq ) && 
       ( caps->rx_range_list1[ 0 ].start < freq ) )
  {
    switch( vfo )
    {
    case RIG_VFO_CURR:
    case RIG_VFO_A:
      (void) setAddr( rig, WORKING, FREQU );
      (void) setFreq( rig, freq );
      break;

    case RIG_VFO_B:
      (void) setAddr( rig, WORKING, FREQU_B );
      (void) setFreq( rig, freq );
      break;

    default:
      rc = -RIG_EINVAL;
    };

  }
  else
  {
    rc = -RIG_EINVAL;
  }

  if ( 0 != writeByte( rig, EXE( SET_ALL ) ) )
  {
    rc = -RIG_EIO;
  }

  unlockRx( rig );

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

  lockRx( rig, 1 );

  switch( vfo )
  {
  case RIG_VFO_CURR:
  case RIG_VFO_A:
    (void) setAddr( rig, WORKING, FREQU );
    getFreq( rig, freq );
    break;

  case RIG_VFO_B:
    (void) setAddr( rig, WORKING, FREQU_B );
    getFreq( rig, freq );
    break;

  default:
    rc = -RIG_EINVAL;
  };

  unlockRx( rig );

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

  /* TODO - deal with selected VFO */
  switch ( mode )
  {
  case RIG_MODE_AM:
    ar_mode = (unsigned char) AM;
    break;

  case RIG_MODE_AMS:
    ar_mode = (unsigned char) SAM;
    break;

  case RIG_MODE_FM:
    ar_mode = (unsigned char) FM;
    break;

  case RIG_MODE_RTTY:
    ar_mode = (unsigned char) DATA;
    break;

  case RIG_MODE_CW:
    ar_mode = (unsigned char) CW;
    break;

  case RIG_MODE_LSB:
    ar_mode = (unsigned char) LSB;
    break;

  case RIG_MODE_USB:
    ar_mode = (unsigned char) USB;
    break;

  default:
    rc = -RIG_EINVAL;
  }

  lockRx( rig, 1 );

  (void) setAddr( rig, WORKING, MODE );

  (void) writeByte( rig, WRD( ar_mode ) );

  if ( width == RIG_PASSBAND_NORMAL )
  {
    width = rig_passband_normal( rig, mode );
  }
  else
  {
    /* TODO - get filter BWs at startup */
    if ( width <= (pbwidth_t) getFilterBW( rig, FILTER_1 ) )
    {
      ar_filter = (unsigned char) FILTER_1;
    }
    else if ( width <= (pbwidth_t) getFilterBW( rig, FILTER_2 ) )
    {
      ar_filter = (unsigned char) FILTER_2;
    }
    else if ( width <= (pbwidth_t) getFilterBW( rig, FILTER_3 ) )
    {
      ar_filter = (unsigned char) FILTER_3;
    }
    else if ( width <= (pbwidth_t) getFilterBW( rig, FILTER_4 ) )
    {
      ar_filter = (unsigned char) FILTER_4;
    }
    else if ( width <= (pbwidth_t) getFilterBW( rig, FILTER_5 ) )
    {
      ar_filter = (unsigned char) FILTER_5;
    }
    else
    {
      ar_filter = (unsigned char) FILTER_6;
    }
  }

  (void) setAddr( rig, WORKING, FILTER );

  (void) writeByte( rig, WRD( ar_filter ) );

  (void) writeByte( rig, EXE( SET_ALL ) );

  unlockRx( rig );

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
 
  /* TODO - deal with selected VFO */
  (void) setAddr( rig, WORKING, MODE );

  switch ( readByte( rig ) )
  {
  case AM:
    *mode = RIG_MODE_AM;
    break;

  case SAM:
    *mode = RIG_MODE_AMS;
    break;

  case FM:
    *mode = RIG_MODE_FM;
    break;

  case DATA:
    *mode = RIG_MODE_RTTY;
    break;

  case CW:
    *mode = RIG_MODE_CW;
    break;

  case LSB:
    *mode = RIG_MODE_LSB;
    break;

  case USB:
    *mode = RIG_MODE_USB;
    break;

  default:
    rc = -RIG_EINVAL;
  }

  (void) setAddr( rig, WORKING, FLTBW );

  bcd_bw = (unsigned char) readByte( rig );

  if ( (unsigned char) -1 != bcd_bw )
  {
    *width = (pbwidth_t) ((int) BCD2Int( bcd_bw ) * 100);
  }
  else
  {
    rc = -RIG_EINVAL;
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
  unsigned int c;

  lockRx( rig, 1 );

  (void) setAddr( rig, BBRAM, ( MEM_SQ + chan ) );
  *sql = (unsigned char) readByte( rig );

  (void) setAddr( rig, BBRAM, ( chan * 4 ) );

  (void) getFreq( rig, freq );
  c = (unsigned int) readByte( rig );
  *mode = (unsigned char) ( c & 0x0f );
  *filt = (unsigned char) ( ( c & 0x70 ) >> 4 );

  (void) setAddr( rig, BBRAM, MEM_PB + chan );
  c = (unsigned int) readByte( rig );
  *pbs = (unsigned char) c;

  unlockRx( rig );
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

  lockRx( rig, LOCK_1 );

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

    (void) setAddr( rig, WORKING, RFGAIN );
    (void) writeData( rig, v ); /* rfgain */

    rig_debug( RIG_DEBUG_VERBOSE, "%s: rfgain %d (%d)\n", __func__, val.i, v );

    (void) writeByte( rig, EXE( SET_ALL ) );
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

    (void) setAddr( rig, WORKING, RFGAIN );
    (void) writeData( rig, v ); /* rfgain */

    rig_debug( RIG_DEBUG_VERBOSE, "%s: rfgain %d (%d)\n", __func__, val.i, v );

    (void) writeByte( rig, EXE( SET_ALL ) );
    break;

  case RIG_LEVEL_AF:
    /* Scale parameter */
    v = ( unsigned char ) ( ( val.f * ( VOL_MAX - VOL_MIN ) ) + VOL_MIN );
    v = ( v & 0x3f );

    (void) setAddr( rig, WORKING, AF_VOL );

    (void) writeData( rig, v ); /* af_vol */

    rig_debug( RIG_DEBUG_VERBOSE, "%s: af_vol %f (%d)\n", __func__, val.f, v );

    v = ( ( v >> 1 ) & 0x1f );    /* half value for L/R volume */

    (void) writeData( rig, v ); /* af_vll */
    (void) writeData( rig, v ); /* af_vlr */

    (void) writeByte( rig, EXE( SET_AUDIO ) );
    break;

  case RIG_LEVEL_RF:
    /* Scale parameter, values 0 (99%) to 130 (3%) */
    v = (unsigned char) (134U - ((unsigned int) (val.f * 135.0)));

    (void) setAddr( rig, WORKING, IFGAIN );
    (void) writeData( rig, v ); /* ifgain */

    rig_debug( RIG_DEBUG_VERBOSE, "%s: ifgain %f (%d)\n", __func__, val.f, v );

    (void) writeByte( rig, EXE( SET_ALL ) );
    break;

  case RIG_LEVEL_SQL:
    /* Scale parameter */
    v = (unsigned char) (val.f * 255.0);

    (void) setAddr( rig, WORKING, SQLVAL );
    (void) writeData( rig, v ); /* sqlval */

    rig_debug( RIG_DEBUG_VERBOSE, "%s: sqlval %f (%d)\n", __func__, val.f, v );

    (void) writeByte( rig, EXE( SET_ALL ) );
    break;

  case RIG_LEVEL_PBT_IN:
    /* Scale parameter */
    v = (unsigned char) (val.f / (HZ_PER_STEP * 12.5));

    (void) setAddr( rig, WORKING, PBSVAL );
    (void) writeData( rig, v ); /* pbsval */

    rig_debug( RIG_DEBUG_VERBOSE, "%s: pbsval %f (%d)\n", __func__, val.f, v );

    (void) writeByte( rig, EXE( SET_ALL ) );
    break;

  case RIG_LEVEL_CWPITCH:
    /* Scale parameter */
    v = (unsigned char) (val.f / (HZ_PER_STEP * 12.5));

    (void) setAddr( rig, WORKING, BFOVAL );
    (void) writeData( rig, v ); /* bfoval */

    rig_debug( RIG_DEBUG_VERBOSE, "%s: bfoval %f (%d)\n", __func__, val.f, v );

    (void) writeByte( rig, EXE( SET_ALL ) );
    break;

  case RIG_LEVEL_NOTCHF:
    rc = -RIG_ENIMPL;
    break;

  case RIG_LEVEL_AGC:
    /* Scale parameter */
    switch ( val.i )
    {
    case RIG_AGC_OFF:
      v = (unsigned char) AGC_OFF;
      break;

    case RIG_AGC_SLOW:
      v = (unsigned char) AGC_SLOW;
      break;

    case RIG_AGC_FAST:
      v = (unsigned char) AGC_FAST;
      break;

    case RIG_AGC_MEDIUM:
    default:
      v = (unsigned char) AGC_MED;
      break;
    };

    (void) setAddr( rig, WORKING, AGCSPD );
    (void) writeData( rig, v ); /* agcspd */

    rig_debug( RIG_DEBUG_VERBOSE, "%s: agcspd %d (%d)\n", __func__, val.i, v );

    (void) writeByte( rig, EXE( SET_ALL ) );
    break;

  default:
    rc = -RIG_EINVAL;
  };

  unlockRx( rig );

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
  unsigned int v = 0;
  unsigned int x = 0;

  lockRx( rig, LOCK_1 );

  /* TODO - deal with selected VFO */
  switch ( level )
  {
  case RIG_LEVEL_PREAMP:
    (void) setAddr( rig, WORKING, RFGAIN );
    v = (unsigned int) readByte( rig ); /* af_vol */

    if ( (unsigned int) -1 != v )
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
    (void) setAddr( rig, WORKING, RFGAIN );
    v = (unsigned int) readByte( rig ); /* af_vol */

    if ( (unsigned int) -1 != v )
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
    (void) setAddr( rig, WORKING, AF_VOL );
    v = (unsigned int) readByte( rig ); /* af_vol */

    if ( (unsigned int) -1 != v )
    {
      /* Scale parameter */
      v = ( v & 0x3f );
      val->f = (( (float) v - VOL_MIN) / ( VOL_MAX - VOL_MIN ) );

      rig_debug( RIG_DEBUG_VERBOSE, "%s: af_vol %d (%f)\n",
                  __func__, v, val->f );
    }
    break;

  case RIG_LEVEL_RF:
    (void) setAddr( rig, WORKING, IFGAIN );
    v = (unsigned int) readByte( rig ); /* ifgain */

    if ( (unsigned int) -1 != v )
    {
      /* Scale parameter, values 0 (99%) to 130 (3%) */
      val->f = ((float) (134 - v) / 135.0 );

      rig_debug( RIG_DEBUG_VERBOSE, "%s: ifgain %d (%f)\n",
                  __func__, v, val->f );
    }
    break;

  case RIG_LEVEL_SQL:
    (void) setAddr( rig, WORKING, SQLVAL );
    v = (unsigned int) readByte( rig ); /* sqlval */

    if ( (unsigned int) -1 != v )
    {
      /* Scale parameter */
      val->f = ((float) (v) / 255.0 );

      rig_debug( RIG_DEBUG_VERBOSE, "%s: sqlval %d (%f)\n",
                  __func__, v, val->f );
    }
    break;

  case RIG_LEVEL_PBT_IN:
    (void) setAddr( rig, WORKING, PBSVAL );
    v = (unsigned int) readByte( rig ); /* pbsval */

    if ( (unsigned int) -1 != v )
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
    (void) setAddr( rig, WORKING, BFOVAL );
    v = (unsigned int) readByte( rig ); /* bfoval */

    if ( (unsigned int) -1 != v )
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
    (void) setAddr( rig, WORKING, NCHFR );
    v = (unsigned int) readByte( rig ); /* nchfr */

    if ( (unsigned int) -1 != v )
    {
      x = (v << 8);
      v = readByte( rig ); /* nchfr + 1 */
      if ( -1 != v )
      {
        x += v;

        /* Scale parameter */
        val->i = (int) ((float) (x) / NOTCH_STEP_HZ);

        rig_debug( RIG_DEBUG_VERBOSE, "%s: nchfr %d (%d)\n",
                    __func__, x, val->i );
      }
      else
      {
	rc = -RIG_EIO;
      }
    }
    break;

  case RIG_LEVEL_AGC:
    (void) setAddr( rig, WORKING, AGCSPD );
    v = (unsigned int) readByte( rig ); /* agcspd */

    if ( (unsigned int) -1 != v )
    {
      /* Scale parameter */
      switch ( v )
      {
      case AGC_FAST:
	val->i = RIG_AGC_FAST;
	break;

      case AGC_MED:
	val->i = RIG_AGC_MEDIUM;
	break;

      case AGC_SLOW:
	val->i = RIG_AGC_SLOW;
	break;

      case AGC_OFF:
	val->i = RIG_AGC_OFF;
	break;

      default:
	rc = -RIG_EINVAL;
      }

      rig_debug( RIG_DEBUG_VERBOSE, "%s: agcspd %d (%d)\n",
                  __func__, v, val->i );
    }
    break;

  case RIG_LEVEL_RAWSTR:
    lockRx( rig, LOCK_1 );
    val->i = readSignal( rig );
    unlockRx( rig );
    break;

  case RIG_LEVEL_STRENGTH:
    rc = -RIG_ENIMPL;
    break;

  default:
    rc = -RIG_EINVAL;
  }

  unlockRx( rig );

  return( rc );
}

static int ar7030p_set_vfo( RIG * rig, vfo_t vfo )
{
  assert( NULL != rig );

  return ( -RIG_ENIMPL );
}

static int ar7030p_get_vfo( RIG * rig, vfo_t * vfo )
{
  assert( NULL != rig );
  assert( NULL != vfo );

  return ( -RIG_ENIMPL );
}

static int ar7030p_set_parm( RIG * rig, setting_t parm, value_t val )
{
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

  return ( -RIG_ENIMPL );
}

static int ar7030p_get_parm( RIG * rig, setting_t parm, value_t * val )
{
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

  return ( -RIG_ENIMPL );
}

static int ar7030p_set_mem( RIG * rig, vfo_t vfo, int ch )
{
  assert( NULL != rig );

  return ( -RIG_ENIMPL );
}

static int ar7030p_get_mem( RIG * rig, vfo_t vfo, int *ch )
{
  assert( NULL != rig );
  assert( NULL != ch );

  return ( -RIG_ENIMPL );
}

static int ar7030p_vfo_op( RIG * rig, vfo_t vfo, vfo_op_t op )
{
  assert( NULL != rig );

  switch( op )
  {
  case RIG_OP_CPY:
    break;

  case RIG_OP_XCHG:
    break;

  case RIG_OP_TOGGLE:
    break;

  default:
    break;
  };

  return ( -RIG_ENIMPL );
}

static int ar7030p_scan( RIG * rig, vfo_t vfo, scan_t scan, int ch )
{
  assert( NULL != rig );

  return ( -RIG_ENIMPL );
}

static int ar7030p_get_dcd( RIG * rig, vfo_t vfo, dcd_t * dcd )
{
  assert( NULL != rig );
  assert( NULL != dcd );

  return ( -RIG_ENIMPL );
}

static int ar7030p_set_ts( RIG * rig, vfo_t vfo, shortfreq_t ts )
{
  int rc = RIG_OK;
  unsigned int v;

  assert( NULL != rig );

  /* Scale parameter */
  v = (unsigned int) ((double) (ts + 1) / HZ_PER_STEP);

  (void) setAddr( rig, WORKING, CHNSTP );
  (void) writeData( rig, (unsigned char) ((v & 0xff00) >> 8) ); /* chnstp */
  (void) writeData( rig, (unsigned char)  (v & 0x00ff) ); /* chnstp + 1 */

  rig_debug( RIG_DEBUG_VERBOSE, "%s: chnstp %d (%d)\n", __func__, ts, v );

  (void) writeByte( rig, EXE( SET_ALL ) );
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
  unsigned int v;
  double x;

  assert( NULL != rig );
  assert( NULL != ts );

  setAddr( rig, WORKING, CHNSTP );

  v = (unsigned int) readByte( rig );
  if ( (unsigned int) -1 != v )
  {
    x = (double) ((v & 0xff) << 8);
    v = (unsigned int) readByte( rig );
    if ( (unsigned int) -1 != v )
    {
      x += (double) (v & 0xff); 
      *ts = (shortfreq_t) (x * HZ_PER_STEP);

      rig_debug( RIG_DEBUG_VERBOSE, "%s: step= %d\n", __func__, *ts );
    }
    else
    {
      rc = -RIG_EIO;
    }
  }
  else
  {
    rc = -RIG_EIO;
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
  int rc = RIG_OK;

  assert( NULL != rig );

  switch ( status )
  {
  case RIG_POWER_OFF:
    rc = -RIG_ENIMPL;
    break;

  case RIG_POWER_ON: 
    rc = -RIG_ENIMPL;
   break;

  default:
    rc = -RIG_EINVAL;
    break;
  }

  return( rc );
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

  assert( NULL != rig );

  rc = -RIG_ENIMPL;

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

  return ( -RIG_ENIMPL );
}

static const char *ar7030p_get_info( RIG * rig )
{
  static char version[10] = "";

  assert( NULL != rig );

  ar7030p_get_ident( rig, version );

  return ( version );
}

const struct rig_caps ar7030p_caps = {
  .rig_model = RIG_MODEL_AR7030P,
  .model_name = "AR7030 Plus",
  .mfg_name = "AOR",
  .version = "0.1",
  .copyright = "LGPL",
  .status = RIG_STATUS_NEW,
  .rig_type = RIG_TYPE_RECEIVER,
  .ptt_type = RIG_PTT_NONE,
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
  .ctcss_list = NULL,
  .dcs_list = NULL,
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

  .set_ptt = RIG_FUNC_NONE,
  .get_ptt = RIG_FUNC_NONE,

  .get_dcd = ar7030p_get_dcd,

  .set_rptr_shift = RIG_FUNC_NONE,
  .get_rptr_shift = RIG_FUNC_NONE,
  .set_rptr_offs = RIG_FUNC_NONE,
  .get_rptr_offs = RIG_FUNC_NONE,
  .set_split_freq = RIG_FUNC_NONE,
  .get_split_freq = RIG_FUNC_NONE,
  .set_split_mode = RIG_FUNC_NONE,
  .get_split_mode = RIG_FUNC_NONE,
  .set_split_vfo = RIG_FUNC_NONE,
  .get_split_vfo = RIG_FUNC_NONE,
  .set_rit = RIG_FUNC_NONE,
  .get_rit = RIG_FUNC_NONE,
  .set_xit = RIG_FUNC_NONE,
  .get_xit = RIG_FUNC_NONE,

  .set_ts = ar7030p_set_ts,
  .get_ts = ar7030p_get_ts,

  .set_dcs_code = RIG_FUNC_NONE,
  .get_dcs_code = RIG_FUNC_NONE,
  .set_tone = RIG_FUNC_NONE,
  .get_tone = RIG_FUNC_NONE,
  .set_ctcss_tone = RIG_FUNC_NONE,
  .get_ctcss_tone = RIG_FUNC_NONE,
  .set_dcs_sql = RIG_FUNC_NONE,
  .get_dcs_sql = RIG_FUNC_NONE,
  .set_tone_sql = RIG_FUNC_NONE,
  .get_tone_sql = RIG_FUNC_NONE,
  .set_ctcss_sql = RIG_FUNC_NONE,
  .get_ctcss_sql = RIG_FUNC_NONE,
  .power2mW = RIG_FUNC_NONE,
  .mW2power = RIG_FUNC_NONE,

  .set_powerstat = ar7030p_set_powerstat,
  .get_powerstat = ar7030p_get_powerstat,
  .reset = ar7030p_reset,

  .set_ant = RIG_FUNC_NONE,
  .get_ant = RIG_FUNC_NONE,

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
  .send_dtmf = RIG_FUNC_NONE,
  .recv_dtmf = RIG_FUNC_NONE,
  .send_morse = RIG_FUNC_NONE,
  .set_bank = RIG_FUNC_NONE,

  .set_mem = ar7030p_set_mem,
  .get_mem = ar7030p_get_mem,
  .vfo_op = ar7030p_vfo_op,
  .scan = ar7030p_scan,

  .set_trn = RIG_FUNC_NONE,
  .get_trn = RIG_FUNC_NONE,

  .decode_event = ar7030p_decode_event,
  .set_channel = ar7030p_set_channel,
  .get_channel = ar7030p_get_channel,
  .get_info = ar7030p_get_info,

  .set_chan_all_cb = RIG_FUNC_NONE,
  .get_chan_all_cb = RIG_FUNC_NONE,
  .set_mem_all_cb = RIG_FUNC_NONE,
  .get_mem_all_cb = RIG_FUNC_NONE,

};


