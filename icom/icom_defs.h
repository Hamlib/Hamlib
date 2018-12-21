/*
 *  Hamlib CI-V backend - defines for the ICOM "CI-V" interface.
 *  Copyright (c) 2000-2016 by Stephane Fillod
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

#ifndef _ICOM_DEFS_H
#define _ICOM_DEFS_H 1

/*
 *  CI-V frame codes
 */
#define PR		0xfe		/* Preamble code */
#define CTRLID		0xe0		/* Controllers's default address */
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
 *

 	Most radios have 2 or 3 recieve passbands available.  Where only 2 are available they
	are selected by 01 for wide and 02 for narrow  Actual bandwidth is determined by the filters
	installed.  With the newer DSP rigs there are 3 presets 01 = wide 02 = middle and 03 = narrow.
	Acutally you can set change any of these presets to any thing you want.

 * Notes:
 * The following only applies to IC-706.
 * 1.  When wide or normal op available: add "00" for wide, "01" normal
 * 	  Normal or narrow op: add "00" for normal, "01" for narrow
 * 	  Wide, normal or narrow op: add "00" for wide, "01" normal, "02" narrow
 * 2. Memory channel number 1A=0100/1b=0101, 2A=0102/2b=0103,
 * 	  3A=0104/3b=0105, C1=0106, C2=0107
 */
#define C_SND_FREQ	0x00		/* Send frequency data  trancieve mode does not ack*/
#define C_SND_MODE	0x01		/* Send mode data, Sc  for trancieve mode does not ack */
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
#define C_RD_OFFS	0x0c		/* Read duplex offset frequency; default changes with HF/6M/2M */
#define C_SET_OFFS	0x0d		/* Set duplex offset frequency */
#define C_CTL_SCAN	0x0e		/* Control scan, Sc */
#define C_CTL_SPLT	0x0f		/* Control split, and duplex mode Sc */
#define C_SET_TS	0x10		/* Set tuning step, Sc */
#define C_CTL_ATT	0x11		/* Set/get attenuator, Sc */
#define C_CTL_ANT	0x12		/* Set/get antenna, Sc */
#define C_CTL_ANN	0x13		/* Control announce (speech synth.), Sc */
#define C_CTL_LVL	0x14		/* Set AF/RF/squelch, Sc */
#define C_RD_SQSM	0x15		/* Read squelch condition/S-meter level, Sc */
#define C_CTL_FUNC	0x16		/* Function settings (AGC,NB,etc.), Sc */
#define C_SND_CW	0x17		/* Send CW message */
#define C_SET_PWR	0x18		/* Set Power ON/OFF, Sc */
#define C_RD_TRXID	0x19		/* Read transceiver ID code */
#define C_CTL_MEM	0x1a		/* Misc memory/bank/rig control functions, Sc */
#define C_SET_TONE	0x1b		/* Set tone frequency */
#define C_CTL_PTT	0x1c		/* Control Transmit On/Off, Sc */
#define C_CTL_MTEXT	0x70		/* Microtelecom Extension */
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
#define S_AM_N	0x02		/* Set to AM-N */
#define S_CW	0x03		/* Set to CW */
#define S_RTTY	0x04		/* Set to RTTY */
#define S_FM	0x05		/* Set to FM */
#define S_FM_N	0x05		/* Set to FM-N */
#define S_WFM	0x06		/* Set to Wide FM */
#define S_CWR	0x07		/* Set to CW Reverse */
#define S_RTTYR	0x08		/* Set to RTTY Reverse */
#define S_AMS	0x11		/* Set to AMS */
#define S_PSK	0x12		/* 7800 PSK USB */
#define S_PSKR	0x13		/* 7800 PSK LSB */
#define S_SAML	0x14		/* Set to AMS-L */
#define S_SAMU	0x15		/* Set to AMS-U */
#define S_P25	0x16		/* Set to P25 */
#define S_DSTAR	0x17		/* Set to D-STAR */
#define S_DPMR	0x18		/* Set to dPMR */
#define S_NXDNVN 0x19		/* Set to NXDN_VN */
#define S_NXDN_N 0x20		/* Set to NXDN-N */
#define S_DCR	0x21		/* Set to DCR */

#define S_R7000_SSB	0x05	/* Set to SSB on R-7000 */

/* filter width coding for older ICOM rigs with 2 filter width */
/* there is no special 'wide' for that rigs */
#define PD_MEDIUM_2	0x01	/* Medium */
#define PD_NARROW_2	0x02	/* Narrow */

/* filter width coding for newer ICOM rigs with 3 filter width */
#define PD_WIDE_3	0x01	/* Wide */
#define PD_MEDIUM_3	0x02	/* Medium */
#define PD_NARROW_3	0x03	/* Narrow */

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
#define S_BANK	0xa0		/* Select memory bank (aka 'memory group' with IC-R8600) */

/*
 * Scan control (C_CTL_SCAN) subcommands
 */
#define S_SCAN_STOP	0x00		/* Stop scan/window scan */
#define S_SCAN_START	0x01		/* Programmed/Memory scan */
#define S_SCAN_PROG	0x02		/* Programmed scan */
#define S_SCAN_DELTA	0x03		/* Delta-f scan */
#define S_SCAN_WRITE	0x04		/* auto memory-write scan */
#define S_SCAN_FPROG	0x12		/* Fine programmed scan */
#define S_SCAN_FDELTA	0x13		/* Fine delta-f scan */
#define S_SCAN_MEM2	0x22		/* Memory scan */
#define S_SCAN_SLCTN	0x23		/* Selected number memory scan */
#define S_SCAN_SLCTM	0x24		/* Selected mode memory scan */
#define S_SCAN_PRIO	0x42		/* Priority / window scan */
#define S_SCAN_NSLCT	0xB0        /* Set as non select channel */
#define S_SCAN_SLCT	0xB1        /* Set as select channel */
#define S_SCAN_SL_NUM	0xB2		/* select programed mem scan 7800 only */
#define S_SCAN_RSMOFF   0xD0        /* Set scan resume OFF */
#define S_SCAN_RSMON    0xD3        /* Set scan resume ON */


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
#define S_ATT_RD	-1		/* Without subcommand, reads out setting */
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
 * Set AGC (S_FUNC_AGC) data
 */
#define D_AGC_FAST	0x00
#define D_AGC_MID	0x01
#define D_AGC_SLOW	0x02
#define D_AGC_SUPERFAST	0x03 /* IC746 pro */

/*
 * Set antenna (C_SET_ANT) subcommands
 */
#define S_ANT_RD	-1		/* Without subcommand, reads out setting */
#define S_ANT1		0x00		/* Antenna 1 */
#define S_ANT2		0x01		/* Antenna 2 */

/*
 * Announce control (C_CTL_ANN) subcommands
 */
#define S_ANN_ALL	0x00		/* Announce all */
#define S_ANN_FREQ	0x01		/* Announce freq */
#define S_ANN_MODE  0x02        /* Announce operating mode */

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
#define S_LVL_PBTOUT		0x08		/* Twin PBT setting (outside) */
#define S_LVL_CWPITCH		0x09		/* CW pitch setting */
#define S_LVL_RFPOWER		0x0a		/* RF power setting */
#define S_LVL_MICGAIN		0x0b		/* MIC gain setting */
#define S_LVL_KEYSPD		0x0c		/* Key Speed setting */
#define S_LVL_NOTCHF		0x0d		/* Notch freq. setting */
#define S_LVL_COMP			0x0e		/* Compressor level setting */
#define S_LVL_BKINDL		0x0f		/* BKin delay setting */
#define S_LVL_BALANCE		0x10		/* Balance setting (Dual watch) */
#define S_LVL_AGC			0x11		/* AGC (7800) */
#define S_LVL_NB			0x12		/* NB setting */
#define S_LVL_DIGI			0x13		/* DIGI-SEL (7800) */
#define S_LVL_DRIVE			0x14		/* DRIVE gain setting */
#define S_LVL_MON			0x15		/* Monitor gain setting */
#define S_LVL_VOXGAIN		0x16		/* VOX gain setting */
#define S_LVL_ANTIVOX		0x17		/* Anti VOX gain setting */
#define S_LVL_CONTRAST		0x18		/* CONTRAST level setting */
#define S_LVL_BRIGHT		0x19		/* BRIGHT level setting */

/*
 * Read squelch condition/S-meter level/other meter levels (C_RD_SQSM) subcommands
 */
#define S_SQL	0x01		/* Read squelch condition */
#define S_SML	0x02		/* Read S-meter level */
#define S_RFML	0x11		/* Read RF-meter level */
#define S_SWR	0x12		/* Read SWR-meter level */
#define S_ALC	0x13		/* Read ALC-meter level */
#define S_CMP	0x14		/* Read COMP-meter level */
#define S_VD	0x15		/* Read Vd-meter level */
#define S_ID	0x16		/* Read Id-meter level */

/*
 * Function settings (C_CTL_FUNC) subcommands  Set and Read
 */
#define S_FUNC_PAMP	0x02		/* Preamp setting */
#define S_FUNC_AGCOFF	0x10		/* IC-R8500 only */
#define S_FUNC_AGCON 	0x11		/* IC-R8500 only */
#define S_FUNC_AGC	0x12		/* AGC setting presets: the dsp models allow these to be modified */
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
#define S_FUNC_RF	0x49		/* RTTY Filter setting */
#define S_FUNC_AFC  	0x4A        /* Auto Frequency Control (AFC) setting */
#define S_FUNC_DTCS	0x4B		/*DTCS tone code squelch setting*/
#define S_FUNC_VSC	0x4C		/* voice squelch control useful for scanning*/
#define S_FUNC_MANAGC	0x4D		/* manual AGC */
#define S_FUNC_DIGISEL	0x4E		/* DIGI-SEL */
#define S_FUNC_TW_PK	0x4F		/* RTTY Twin Peak filter 0= off 1 = on */
#define S_FUNC_DIAL_LK	0x50		/* Dial lock */

/*
 * Set Power On/Off (C_SET_PWR) subcommands
 */
#define S_PWR_OFF	0x00
#define S_PWR_ON	0x01
#define S_PWR_STDBY	0x02

/*
 * Transmit control (C_CTL_PTT) subcommands
 */
#define S_PTT		0x00
#define S_ANT_TUN	0x01	/* Auto tuner 0=OFF, 1 = ON, 2=Start Tuning */

/*
 * Misc contents (C_CTL_MEM) subcommands applies to newer rigs.
 *
 * Beware the IC-7200 which is non-standard.
 */
#define S_MEM_CNTNT		0x00	/* Memory content 2 bigendian */
#define S_MEM_BAND_REG		0x01	/* band stacking register */
#define S_MEM_FILT_WDTH		0x03	/* current passband filter width */
#define S_MEM_PARM		0x05	/* rig parameters; extended parm # + param value:  should be coded */
					/* in the rig files because they are different for each rig */
#define S_MEM_DATA_MODE		0x06	/* data mode */
#define S_MEM_TX_PB		0x07	/* SSB tx passband */
#define S_MEM_FLTR_SHAPE	0x08	/* DSP filter shape 0=sharp 1=soft */

					/* Icr75c */
#define S_MEM_CNTNT_SLCT	0x01
#define S_MEM_FLT_SLCT		0x01
#define S_MEM_MODE_SLCT		0x02
                                    /* For IC-910H rig. */
#define S_MEM_RDWR_MEM      0x00    /* Read/write memory channel */
#define S_MEM_SATMEM        0x01    /* Satellite memory */
#define S_MEM_VOXGAIN       0x02    /* VOX gain level (0=0%, 255=100%) */
#define S_MEM_VOXDELAY      0x03    /* VOX delay (0=0.0 sec, 20=2.0 sec) */
#define S_MEM1_VOXDELAY     0x05    /* VOX delay (0=0.0 sec, 20=2.0 sec) */
#define S_MEM_ANTIVOX       0x04    /* anti VOX setting */
#define S_MEM_RIT           0x06    /* RIT (0=off, 1=on, 2=sub dial) */
#define S_MEM_SATMODE       0x07    /* Satellite mode (on/off) */
#define S_MEM_BANDSCOPE     0x08    /* Simple bandscope (on/off) */

/* IC-R8600 and others */
#define S_MEM_SKIP_SLCT_OFF 0x00
#define S_MEM_SKIP_SLCT_ON  0x10
#define S_MEM_PSKIP_SLCT_ON 0x20
#define S_MEM_DUP_OFF       0x00
#define S_MEM_DUP_MINUS     0x01
#define S_MEM_DUP_PLUS      0x02
#define S_MEM_TSTEP_OFF     0x00
#define S_MEM_TSTEP_ON      0x01
#define S_FUNC_IPPLUS       0x07 /* IP+ subcommand 0x1a 0x07 */


/*
 * Tone control (C_SET_TONE) subcommands
 */
#define S_TONE_RPTR	0x00		/* Tone frequency setting for repeater receive */
#define S_TONE_SQL	0x01		/* Tone frequency setting for squelch */
#define S_TONE_DTCS	0x02		/* DTCS code and polarity for squelch */

/*
 * Transceiver ID (C_RD_TRXID) subcommands
 */
#define S_RD_TRXID 0x00

/*
 * C_CTL_MISC	OptoScan extension
 */
#define S_OPTO_LOCAL	0x01
#define S_OPTO_REMOTE	0x02
#define S_OPTO_TAPE_ON	0x03
#define S_OPTO_TAPE_OFF	0x04
#define S_OPTO_RDSTAT	0x05
#define S_OPTO_RDCTCSS	0x06
#define S_OPTO_RDDCS	0x07
#define S_OPTO_RDDTMF	0x08
#define S_OPTO_RDID 	0x09
#define S_OPTO_SPKRON 	0x0a
#define S_OPTO_SPKROFF 	0x0b
#define S_OPTO_5KSCON 	0x0c
#define S_OPTO_5KSCOFF 	0x0d
#define S_OPTO_NXT	 	0x0e
#define S_OPTO_SCON 	0x0f
#define S_OPTO_SCOFF 	0x10

/*
 * OmniVIPlus (Omni VI) extensions
 */
#define C_OMNI6_XMT      0x16

/*
 * S_MEM_MODE_SLCT	Misc CI-V Mode settings
 */
#define S_PRM_BEEP		0x02
#define S_PRM_CWPITCH	0x10
#define S_PRM_LANG		0x15
#define S_PRM_BACKLT	0x21
#define S_PRM_SLEEP		0x32
#define S_PRM_SLPTM		0x33
#define S_PRM_TIME		0x27

/*
 * Tokens for Extra Level and Parameters common to multiple rigs.  Use token # > 99.  Defined here so they
 * will be available in ICOM name space. They have different internal commands primarily in dsp rigs.  These
 * tokens are used ext_lvl and ext_parm funtions in the individual rig files.
 * Extra parameters which are rig specific should be coded in the individual rig files and token #s < 100.
 */

#define TOKEN_BACKEND(t) (t)

#define TOK_RTTY_FLTR TOKEN_BACKEND(100)
#define TOK_SSBBASS TOKEN_BACKEND(101)
#define TOK_SQLCTRL TOKEN_BACKEND(102)
#define TOK_LEVEL_MONITOR TOKEN_BACKEND(103)


#endif /* _ICOM_DEFS_H */
