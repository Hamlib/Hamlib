/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * icom_defs.h - Copyright (C) 2000 Stephane Fillod
 * This include defines all kind of constants 
 * used by the ICOM "CI-V" interface.
 *
 *
 *    $Id: icom_defs.h,v 1.9 2001-06-10 22:27:08 f4cfe Exp $  
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * 
 */

#ifndef _ICOM_DEFS_H
#define _ICOM_DEFS_H 1

/*
 *  CI-V frame codes
 */
#define PR		0xfe		/* Preamble code */
#define CTRLID	0xe0		/* Controllers's default address */
#define BCASTID	0x00		/* Broadcast address */
#define FI		0xfd		/* End of message code */
#define ACK		0xfb		/* OK code */
#define NAK		0xfa		/* NG code */
#define COL		0xfc		/* CI-V bus collision detected */
#define PAD		0xff		/* Transmit padding */

#define ACKFRMLEN	6		/* reply frame length */

#define S_NONE -1


/*
 * Arguments length in bytes
 */
#define CHAN_NB_LEN 2
#define BANK_NB_LEN 2
#define OFFS_LEN 3

/*
 * Cn controller commands
 * Notes:
 * 1. When wide or normal op available: add "00" for wide, "01" normal
 * 	  Normal or narrow op: add "00" for normal, "01" for narrow
 * 	  Wide, normal or narrow op: add "00" for wide, "01" normal, "02" narrow
 * 2. Memory channel number 1A=0100/1b=0101, 2A=0102/2b=0103,
 * 	  3A=0104/3b=0105, C1=0106, C2=0107
 */
#define C_SND_FREQ	0x00		/* Send frequency data */
#define C_SND_MODE	0x01		/* Send mode data, Sc */
#define C_RD_BAND	0x02		/* Read band edge frequencies */
#define C_RD_FREQ	0x03		/* Read display frequency */
#define C_RD_MODE	0x04		/* Read display mode */
#define C_SET_FREQ	0x05		/* Set frequency data(1) */
#define C_SET_MODE	0x06		/* Set mode data, Sc */
#define C_SET_VFO	0x07		/* Set VFO */
#define C_SET_MEM	0x08		/* Set channel, Sc(2) */
#define C_WR_MEM	0x09		/* Write memory */
#define C_MEM2VFO	0x0a		/* Memory to VFO */
#define C_CLR_MEM	0x0b		/* Memory clear */
#define C_RD_OFFS	0x0c		/* Read duplex offset frequency */
#define C_SET_OFFS	0x0d		/* Set duplex offset frequency */
#define C_CTL_SCAN	0x0e		/* Control scan, Sc */
#define C_CTL_SPLT	0x0f		/* Control split, Sc */
#define C_SET_TS	0x10		/* Set tuning step, Sc */
#define C_CTL_ATT	0x11		/* Set attenuator, Sc */
#define C_CTL_ANT	0x12		/* Set antenna, Sc */
#define C_CTL_ANN	0x13		/* Control announce (speech synth.), Sc */
#define C_CTL_LVL	0x14		/* Set AF/RF/squelch, Sc */
#define C_RD_SQSM	0x15		/* Read squelch condiction/S-meter level, Sc */
#define C_CTL_FUNC	0x16		/* Function settings (AGC,NB,etc.), Sc */
#define C_SND_CW	0x17		/* Send CW message */
#define C_SET_PWR	0x18		/* Set Power ON/OFF, Sc */
#define C_RD_TRXID	0x19		/* Read transceiver ID code */
#define C_CTL_MEM	0x1a		/* Misc memory/bank functions, Sc */
#define C_SET_TONE	0x1b		/* Set tone frequency */
#define C_CTL_PTT	0x1c		/* Control Transmit On/Off, Sc */
#define C_CTL_MISC	0x7f		/* Miscellaneous control, Sc */

/*
 * Sc controller sub commands
 */


/*
 * Set mode data (C_SET_MODE) sub commands
 */
#define S_LSB	0x00		/* Set to LSB */
#define S_USB	0x01		/* Set to USB */
#define S_AM	0x02		/* Set to AM */
#define S_CW	0x03		/* Set to CW */
#define S_RTTY	0x04		/* Set to RTTY */
#define S_FM	0x05		/* Set to FM */
#define S_WFM	0x06		/* Set to Wide FM */


/*
 * Set VFO (C_SET_VFO) sub commands
 */
#define S_VFOA	0x00		/* Set to VFO A */
#define S_VFOB	0x01		/* Set to VFO B */
#define S_BTOA	0xa0		/* VFO A=B */
#define S_XCHNG	0xb0		/* Switch VFO A and B */
#define S_SUBTOMAIN	0xb1		/* MAIN = SUB */
#define S_DUAL_OFF	0xc0		/* Dual watch off */
#define S_DUAL_ON	0xc1		/* Dual watch on */
#define S_MAIN	0xd0		/* Select MAIN band */
#define S_SUB	0xd1		/* Select SUB band */
#define S_FRONTWIN	0xe0		/* Select front window */

/*
 * Set MEM (C_SET_MEM) sub commands
 */
#define S_BANK	0xa0		/* Select memory bank */

/*
 * Scan control (C_CTL_SCAN) subcommands
 */
#define S_STOP	0x00		/* Scan stop */
#define S_START	0x01		/* Scan start */


/*
 * Split control (S_CTL_SPLT) subcommands
 */
#define S_SPLT_OFF	0x00		/* Split OFF */
#define S_SPLT_ON	0x01		/* Split ON */
#define S_DUP_OFF	0x10		/* Simplex mode */
#define S_DUP_M 	0x11		/* Duplex - mode */
#define S_DUP_P		0x12		/* Duplex + mode */

/*
 * Set Attenuator (C_CTL_ATT) subcommands
 */
#define S_ATT_RD	0x00		/* Without subcommand, reads out setting */
#define S_ATT_OFF	0x00		/* Off */
#define S_ATT_6dB	0x06		/* 6 dB, IC-756Pro */
#define S_ATT_10dB	0x10		/* 10 dB */
#define S_ATT_12dB	0x12		/* 12 dB, IC-756Pro */
#define S_ATT_18dB	0x18		/* 18 dB, IC-756Pro */
#define S_ATT_20dB	0x20		/* 20 dB */
#define S_ATT_30dB	0x30		/* 30 dB, or Att on for IC-R75 */

/*
 * Set Preamp (S_FUNC_PAMP) data
 */
#define D_PAMP_OFF	0x00
#define D_PAMP1		0x01
#define D_PAMP2		0x02

/*
 * Set antenna (C_SET_ANT) subcommands
 */
#define S_ANT_RD	0x00		/* Without subcommand, reads out setting */
#define S_ANT1		0x00		/* Antenna 1 */
#define S_ANT2		0x01		/* Antenna 2 */

/*
 * Announce control (C_CTL_ANN) subcommands
 */
#define S_ANN_ALL	0x00		/* Announce all */
#define S_ANN_FREQ	0x01		/* Announce freq */

/*
 * Function settings (C_CTL_LVL) subcommands
 */
#define S_LVL_AF		0x01		/* AF level setting */
#define S_LVL_RF		0x02		/* RF level setting */
#define S_LVL_SQL		0x03		/* SQL level setting */
#define S_LVL_IF		0x04		/* IF shift setting */
#define S_LVL_APF		0x05		/* APF level setting */
#define S_LVL_NR		0x06		/* NR level setting */
#define S_LVL_PBTIN		0x07		/* Twin PBT setting (inside) */
#define S_LVL_PBTOUT	0x08		/* Twin PBT setting (outside) */
#define S_LVL_CWPITCH	0x09		/* CW pitch setting */
#define S_LVL_RFPOWER	0x0a		/* RF power setting */
#define S_LVL_MICGAIN	0x0b		/* MIC gain setting */
#define S_LVL_KEYSPD	0x0c		/* Key Speed setting */
#define S_LVL_NOTCHF	0x0d		/* Notch freq. setting */
#define S_LVL_COMP		0x0e		/* Compressor level setting */
#define S_LVL_BKINDL	0x0f		/* BKin delay setting */
#define S_LVL_BALANCE	0x10		/* Balance setting (Dual watch) */

/*
 * Read squelch condition/S-meter level (C_RD_SQSM) subcommands
 */
#define S_SQL	0x01		/* Read squelch condition */
#define S_SML	0x02		/* Read S-meter level */

/*
 * Function settings (C_CTL_FUNC) subcommands
 */
#define S_FUNC_PAMP	0x02		/* Preamp setting */
#define S_FUNC_AGCOFF	0x10		/* IC-R8500 only */
#define S_FUNC_AGCON 	0x11		/* IC-R8500 only */
#define S_FUNC_AGC	0x12		/* AGC setting */
#define S_FUNC_NBOFF	0x20		/* IC-R8500 only */
#define S_FUNC_NBON	0x21		/* IC-R8500 only */
#define S_FUNC_NB	0x22		/* NB setting */
#define S_FUNC_APFOFF	0x30		/* IC-R8500 only */
#define S_FUNC_APFON	0x31		/* IC-R8500 only */
#define S_FUNC_APF	0x32		/* APF setting */
#define S_FUNC_NR	0x40		/* NR setting */
#define S_FUNC_ANF	0x41		/* ANF setting */
#define S_FUNC_TONE	0x42		/* TONE setting */
#define S_FUNC_TSQL	0x43		/* TSQL setting */
#define S_FUNC_COMP	0x44		/* COMP setting */
#define S_FUNC_MON	0x45		/* Monitor setting */
#define S_FUNC_VOX	0x46		/* VOX setting */
#define S_FUNC_BKIN	0x47		/* BK-IN setting */
#define S_FUNC_MN	0x48		/* Manual notch setting */
#define S_FUNC_RNF	0x49		/* RTTY Filter Notch setting */

/*
 * Transceiver ID (C_RD_TRXID) subcommands
 */
#define S_TRXID	0x00		/* Read transceiver ID code */

/*
 * Set Power On/Off (C_SET_PWR) subcommands
 */
#define S_PWR_OFF	0x00
#define S_PWR_ON	0x01

/*
 * Transmit control (C_CTL_PTT) subcommands
 */
#define S_PTT_ON	0x00		/* no documentation, not tested! */
#define S_PTT_OFF	0x01		/* please confirm (IC-756Pro, IC-746) --SF */

/*
 * Memory contents (C_CTL_MEM) subcommands
 */
#define S_MEM_CNTNT	0x00
#define S_MEM_CNTNT_SLCT	0x01

/*
 * Tone control (C_SET_TONE) subcommands
 */
#define S_TONE_RPTR	0x00		/* Tone frequency setting for repeater user */
#define S_TONE_SQL	0x01		/* Tone frequency setting for squelch */

/*
 * C_RD_TRXID
 */
#define S_RD_TRXID 0x00

#endif /* _ICOM_DEFS_H */

