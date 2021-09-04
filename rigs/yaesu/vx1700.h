/*
 *  Copyright (c) 2010-2011 by Mikhail Kshevetskiy (mikhail.kshevetskiy@gmail.com)
 *
 *  Code based on VX-1700 CAT manual:
 *  http://www.vertexstandard.com/downloadFile.cfm?FileID=3397&FileCatID=135&FileName=VX-1700_CAT_MANUAL_10_14_2008.pdf&FileContentType=application%2Fpdf
 *
 *  WARNING: this manual have two errors
 *    1) Status Update Command (10h), U=01 returns  0..199 for channels 1..200
 *    2) Frequency Data (bytes 1--4 of 9-Byte VFO Data Assignment, Status Update
 *       Command (10h), U=02 and U=03) uses bytes 1--3 for frequency, byte 4 is
 *       not used and always zero. Thus bytes 0x15,0xBE,0x68,0x00 means
 *       frequency = 10 * 0x15BE68 = 10 * 1425000 = 14.25 MHz
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

#ifndef _VX1700_H
#define _VX1700_H 1

#include <hamlib/rig.h>
#include <tones.h>


#define	VX1700_MIN_CHANNEL	1
#define	VX1700_MAX_CHANNEL	200

#define	VX1700_MODES	(RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_AM | RIG_MODE_SAL | RIG_MODE_SAH)

#define	VX1700_VFO_ALL	(RIG_VFO_A|RIG_VFO_MEM)
#define	VX1700_ANTS	RIG_ANT_1
#define	VX1700_VFO_OPS	(RIG_OP_UP|RIG_OP_DOWN|RIG_OP_TO_VFO|RIG_OP_FROM_VFO)

#define	VX1700_FILTER_WIDTH_NARROW	kHz(0.5)
#define	VX1700_FILTER_WIDTH_WIDE	kHz(2.2)
#define	VX1700_FILTER_WIDTH_SSB		kHz(2.2)
#define	VX1700_FILTER_WIDTH_AM		kHz(6.0)

/* Returned data length in bytes */
#define	VX1700_MEM_CHNL_LENGTH		1	/* 0x10 p1=01 return size */
#define	VX1700_OP_DATA_LENGTH		19	/* 0x10 p1=02 return size */
#define	VX1700_VFO_DATA_LENGTH		18	/* 0x10 p1=03 return size */
#define	VX1700_READ_METER_LENGTH	5	/* 0xf7 return size */
#define	VX1700_STATUS_FLAGS_LENGTH	5	/* 0xfa return size */

/* BCD coded frequency length */
#define	VX1700_BCD_DIAL			8



#endif /* _VX1700_H */
