/*
 * dumpcaps.c - Copyright (C) 2000-2003 Stephane Fillod
 * This programs dumps the capabilities of a backend rig.
 *
 *
 *    $Id: dumpcaps.c,v 1.37 2003-04-06 18:40:35 fillods Exp $  
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


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <hamlib/rig.h>
#include "misc.h"


static int print_ext(RIG *rig, const struct confparams *cfp, rig_ptr_t ptr);
static const char *decode_mtype(chan_type_t type);
int range_sanity_check(const struct freq_range_list range_list[], int rx);
int ts_sanity_check(const struct tuning_step_list tuning_step[]);
static void dump_chan_caps(const channel_cap_t *chan);

/*
 * the rig may be in rig_init state, but not openned
 */
int dumpcaps (RIG* rig)
{ 
	const struct rig_caps *caps;
	int status,i;
	char freqbuf[20];
	int backend_warnings=0;
	char prntbuf[256];

	if (!rig || !rig->caps)
		return -RIG_EINVAL;

	caps = rig->caps;

	printf("Caps dump for model %d\n",caps->rig_model);
	printf("Model name:\t%s\n",caps->model_name);
	printf("Mfg name:\t%s\n",caps->mfg_name);
	printf("Backend version:\t%s\n",caps->version);
	printf("Backend copyright:\t%s\n",caps->copyright);
	printf("Backend status:\t%s\n", strstatus(caps->status));
	printf("Rig type:\t");
	switch (caps->rig_type & RIG_TYPE_MASK) {
	case RIG_TYPE_TRANSCEIVER:
			printf("Transceiver\n");
			break;
	case RIG_TYPE_HANDHELD:
			printf("Handheld\n");
			break;
	case RIG_TYPE_MOBILE:
			printf("Mobile\n");
			break;
	case RIG_TYPE_RECEIVER:
			printf("Receiver\n");
			break;
	case RIG_TYPE_PCRECEIVER:
			printf("PC Receiver\n");
			break;
	case RIG_TYPE_SCANNER:
			printf("Scanner\n");
			break;
	case RIG_TYPE_TRUNKSCANNER:
			printf("Trunking scanner\n");
			break;
	case RIG_TYPE_COMPUTER:
			printf("Computer\n");
			break;
	case RIG_TYPE_TUNER:
			printf("Tuner\n");
			break;
	case RIG_TYPE_OTHER:
			printf("Other\n");
			break;
	default:
			printf("Unknown\n");
			backend_warnings++;
	}

	printf("PTT type:\t");
	switch (caps->ptt_type) {
	case RIG_PTT_RIG:
			printf("rig capable\n");
			break;
	case RIG_PTT_PARALLEL:
			printf("thru parallel port (DATA0)\n");
			break;
	case RIG_PTT_SERIAL_RTS:
			printf("thru serial port (CTS/RTS)\n");
			break;
	case RIG_PTT_SERIAL_DTR:
			printf("thru serial port (DTR/DSR)\n");
			break;
	case RIG_PTT_NONE:
			printf("None\n");
			break;
	default:
			printf("Unknown\n");
			backend_warnings++;
	}

	printf("DCD type:\t");
	switch (caps->dcd_type) {
	case RIG_DCD_RIG:
			printf("rig capable\n");
			break;
	case RIG_DCD_PARALLEL:
			printf("thru parallel port (DATA1? STROBE?)\n");
			break;
	case RIG_DCD_SERIAL_CTS:
			printf("thru serial port (CTS/RTS)\n");
			break;
	case RIG_DCD_SERIAL_DSR:
			printf("thru serial port (DTR/DSR)\n");
			break;
	case RIG_DCD_NONE:
			printf("None\n");
			break;
	default:
			printf("Unknown\n");
			backend_warnings++;
	}

	printf("Port type:\t");
	switch (caps->port_type) {
	case RIG_PORT_SERIAL:
			printf("RS-232\n");
			break;
	case RIG_PORT_DEVICE:
			printf("device driver\n");
			break;
	case RIG_PORT_NETWORK:
			printf("network link\n");
			break;
	case RIG_PORT_NONE:
			printf("None\n");
			break;
	default:
			printf("Unknown\n");
			backend_warnings++;
	}

	printf("Serial speed: %d..%d bauds, %d%c%d %s\n", caps->serial_rate_min,
					caps->serial_rate_max,caps->serial_data_bits,
					caps->serial_parity==RIG_PARITY_NONE?'N':
					(caps->serial_parity==RIG_PARITY_ODD?'O':'E'),
					caps->serial_stop_bits,
					caps->serial_handshake==RIG_HANDSHAKE_NONE?"":
					(caps->serial_handshake==RIG_HANDSHAKE_XONXOFF?"XONXOFF":"CTS/RTS")
					);

	printf("Write delay %dms, timeout %dms, %d retry\n",
					caps->write_delay,caps->timeout,caps->retry);
	printf("Post Write delay %dms\n",
					caps->post_write_delay);

	printf("Has targetable VFO: %s\n",
					caps->targetable_vfo?"yes":"no");

	printf("Has transceive: %s\n",
					caps->transceive?"yes":"no");

	printf("Announce: 0x%x\n", caps->announces);
	printf("Max RIT: -%ld.%ldkHz/+%ld.%ldkHz\n", 
					caps->max_rit/1000, caps->max_rit%1000,
					caps->max_rit/1000, caps->max_rit%1000);

	printf("Max XIT: -%ld.%ldkHz/+%ld.%ldkHz\n", 
					caps->max_xit/1000, caps->max_xit%1000,
					caps->max_xit/1000, caps->max_xit%1000);

	printf("Max IF-SHIFT: -%ld.%ldkHz/+%ld.%ldkHz\n", 
					caps->max_ifshift/1000, caps->max_ifshift%1000,
					caps->max_ifshift/1000, caps->max_ifshift%1000);

	printf("Preamp:");
	for(i=0; i<MAXDBLSTSIZ && caps->preamp[i] != 0; i++)
			printf(" %ddB", caps->preamp[i]);
	if (i == 0)
		printf(" none");
	printf("\n");
	printf("Attenuator:");
	for(i=0; i<MAXDBLSTSIZ && caps->attenuator[i] != 0; i++)
			printf(" %ddB",caps->attenuator[i]);
	if (i == 0)
		printf(" none");
	printf("\n");

	sprintf_func(prntbuf, caps->has_get_func);
	printf("Get functions: %s\n", prntbuf);

	sprintf_func(prntbuf, caps->has_set_func);
	printf("Set functions: %s\n", prntbuf);

	sprintf_level(prntbuf, caps->has_get_level);
	printf("Get level: %s\n", prntbuf);

	sprintf_level(prntbuf, caps->has_set_level);
	printf("Set level: %s\n", prntbuf);
	if (caps->has_set_level&RIG_LEVEL_READONLY_LIST) {
			printf("Warning: backend can set readonly levels!\n");
			backend_warnings++;
	}
	printf("Extra levels:");
	rig_ext_level_foreach(rig, print_ext, NULL);
	printf("\n");

	sprintf_parm(prntbuf, caps->has_get_parm);
	printf("Get parameters: %s\n", prntbuf);

	sprintf_parm(prntbuf, caps->has_set_parm);
	printf("Set parameters: %s\n", prntbuf);
	if (caps->has_set_parm&RIG_PARM_READONLY_LIST) {
			printf("Warning: backend can set readonly parms!\n");
			backend_warnings++;
	}
	printf("Extra parameters:");
	rig_ext_parm_foreach(rig, print_ext, NULL);
	printf("\n");

#if 0
	/* FIXME: use rig->state.vfo_list instead */
	printf("VFO list: ");
	if (caps->vfo_list!=0) {
		if ((caps->vfo_list&RIG_VFO_A)==RIG_VFO_A) printf("VFOA ");
		if ((caps->vfo_list&RIG_VFO_B)==RIG_VFO_A) printf("VFOB ");
		if ((caps->vfo_list&RIG_VFO_C)==RIG_VFO_A) printf("VFOC ");
		printf("\n");
	} else {
			printf(" none! This backend might be bogus!\n");
	}

	printf("Number of channels:\t%d\n", caps->chan_qty);
#endif
	printf("Number of banks:\t%d\n", caps->bank_qty);
	printf("Memory name desc size:\t%d\n", caps->chan_desc_sz);

	printf("Memories:");
	for (i=0; i<CHANLSTSIZ && caps->chan_list[i].type; i++) {
		printf("\n\t%d..%d:   \t%s", caps->chan_list[i].start,
						caps->chan_list[i].end,
						decode_mtype(caps->chan_list[i].type));
		printf("\n\t  mem caps: ");
		dump_chan_caps(&caps->chan_list[i].mem_caps);
	}
	if (i == 0)
		printf(" none");
	printf("\n");

/* TODO: print rx/tx ranges here */
	status = range_sanity_check(caps->tx_range_list1,0);
	printf("TX ranges status, region 1:\t%s (%d)\n",status?"Bad":"OK",status);
	if (status) backend_warnings++;
	status = range_sanity_check(caps->rx_range_list1,1);
	printf("RX ranges status, region 1:\t%s (%d)\n",status?"Bad":"OK",status);
	if (status) backend_warnings++;

	status = range_sanity_check(caps->tx_range_list2,0);
	printf("TX ranges status, region 2:\t%s (%d)\n",status?"Bad":"OK",status);
	if (status) backend_warnings++;
	status = range_sanity_check(caps->rx_range_list2,1);
	printf("RX ranges status, region 2:\t%s (%d)\n",status?"Bad":"OK",status);
	if (status) backend_warnings++;

	printf("Tuning steps:");
	for (i=0; i<TSLSTSIZ && !RIG_IS_TS_END(caps->tuning_steps[i]); i++) {
		if (caps->tuning_steps[i].ts == RIG_TS_ANY)
			strcpy(freqbuf, "ANY");
		else
			sprintf_freq(freqbuf,caps->tuning_steps[i].ts);
		sprintf_mode(prntbuf,caps->tuning_steps[i].modes);
		printf("\n\t%s:   \t%s", freqbuf, prntbuf);
	}
	if (i==0) {
			printf(" none! This backend might be bogus!");
			backend_warnings++;
	}
	printf("\n");
	status = ts_sanity_check(caps->tuning_steps);
	printf("Tuning steps status:\t%s (%d)\n",status?"Bad":"OK",status);
	if (status) backend_warnings++;

	printf("Filters:");
	for (i=0; i<FLTLSTSIZ && !RIG_IS_FLT_END(caps->filters[i]); i++) {
		if (caps->filters[i].width == RIG_FLT_ANY)
			strcpy(freqbuf, "ANY");
		else
			sprintf_freq(freqbuf,caps->filters[i].width);
		sprintf_mode(prntbuf,caps->filters[i].modes);
		printf("\n\t%s:   \t%s", freqbuf, prntbuf);
	}
	if (i==0) {
			printf(" none! This backend might be bogus!");
			backend_warnings++;
	}
	printf("\n");

        printf("Bandwidths:");
	for (i=1; i < 1<<10; i<<=1) {
		pbwidth_t pbnorm = rig_passband_normal(rig, i);

		if (pbnorm == 0)
			continue;

		sprintf_freq(freqbuf, pbnorm);
		printf("\n\t%s\tnormal: %s,\t", strrmode(i), freqbuf);

		sprintf_freq(freqbuf, rig_passband_narrow(rig, i));
		printf("narrow: %s,\t", freqbuf);

		sprintf_freq(freqbuf, rig_passband_wide(rig, i));
		printf("wide: %s", freqbuf);
	}
	printf("\n");

	printf("Has priv data:\t%c\n",caps->priv!=NULL?'Y':'N');
	/*
	 * TODO: keep me up-to-date with API call list!
	 */
	printf("Has init:\t%c\n",caps->rig_init!=NULL?'Y':'N');
	printf("Has cleanup:\t%c\n",caps->rig_cleanup!=NULL?'Y':'N');
	printf("Has open:\t%c\n",caps->rig_open!=NULL?'Y':'N');
	printf("Has close:\t%c\n",caps->rig_close!=NULL?'Y':'N');
	printf("Can set conf:\t%c\n",caps->set_conf!=NULL?'Y':'N');
	printf("Can get conf:\t%c\n",caps->get_conf!=NULL?'Y':'N');
	printf("Can set frequency:\t%c\n",caps->set_freq!=NULL?'Y':'N');
	printf("Can get frequency:\t%c\n",caps->get_freq!=NULL?'Y':'N');
	printf("Can set mode:\t%c\n",caps->set_mode!=NULL?'Y':'N');
	printf("Can get mode:\t%c\n",caps->get_mode!=NULL?'Y':'N');
	printf("Can set vfo:\t%c\n",caps->set_vfo!=NULL?'Y':'N');
	printf("Can get vfo:\t%c\n",caps->get_vfo!=NULL?'Y':'N');
	printf("Can set ptt:\t%c\n",caps->set_ptt!=NULL?'Y':'N');
	printf("Can get ptt:\t%c\n",caps->get_ptt!=NULL?'Y':'N');
	printf("Can get dcd:\t%c\n",caps->get_dcd!=NULL?'Y':'N');
	printf("Can set repeater duplex:\t%c\n",caps->set_rptr_shift!=NULL?'Y':'N');
	printf("Can get repeater duplex:\t%c\n",caps->get_rptr_shift!=NULL?'Y':'N');
	printf("Can set repeater offset:\t%c\n",caps->set_rptr_offs!=NULL?'Y':'N');
	printf("Can get repeater offset:\t%c\n",caps->get_rptr_offs!=NULL?'Y':'N');
	printf("Can set split freq:\t%c\n",caps->set_split_freq!=NULL?'Y':'N');
	printf("Can get split freq:\t%c\n",caps->get_split_freq!=NULL?'Y':'N');
	printf("Can set split mode:\t%c\n",caps->set_split_mode!=NULL?'Y':'N');
	printf("Can get split mode:\t%c\n",caps->get_split_mode!=NULL?'Y':'N');
	printf("Can set split vfo:\t%c\n",caps->set_split_vfo!=NULL?'Y':'N');
	printf("Can get split vfo:\t%c\n",caps->get_split_vfo!=NULL?'Y':'N');
	printf("Can set tuning step:\t%c\n",caps->set_ts!=NULL?'Y':'N');
	printf("Can get tuning step:\t%c\n",caps->get_ts!=NULL?'Y':'N');
	printf("Can set RIT:\t%c\n",caps->set_rit!=NULL?'Y':'N');
	printf("Can get RIT:\t%c\n",caps->get_rit!=NULL?'Y':'N');
	printf("Can set XIT:\t%c\n",caps->set_xit!=NULL?'Y':'N');
	printf("Can get XIT:\t%c\n",caps->get_xit!=NULL?'Y':'N');
	printf("Can set CTCSS:\t%c\n",caps->set_ctcss_tone!=NULL?'Y':'N');
	printf("Can get CTCSS:\t%c\n",caps->get_ctcss_tone!=NULL?'Y':'N');
	printf("Can set DCS:\t%c\n",caps->set_dcs_code!=NULL?'Y':'N');
	printf("Can get DCS:\t%c\n",caps->get_dcs_code!=NULL?'Y':'N');
	printf("Can set CTCSS squelch:\t%c\n",caps->set_ctcss_sql!=NULL?'Y':'N');
	printf("Can get CTCSS squelch:\t%c\n",caps->get_ctcss_sql!=NULL?'Y':'N');
	printf("Can set DCS squelch:\t%c\n",caps->set_dcs_sql!=NULL?'Y':'N');
	printf("Can get DCS squelch:\t%c\n",caps->get_dcs_sql!=NULL?'Y':'N');
	printf("Can set power stat:\t%c\n",caps->set_powerstat!=NULL?'Y':'N');
	printf("Can get power stat:\t%c\n",caps->get_powerstat!=NULL?'Y':'N');
	printf("Can reset:\t%c\n",caps->reset!=NULL?'Y':'N');
	printf("Can get ant:\t%c\n",caps->get_ant!=NULL?'Y':'N');
	printf("Can set ant:\t%c\n",caps->set_ant!=NULL?'Y':'N');
	printf("Can set transceive:\t%c\n",caps->set_trn!=NULL?'Y':'N');
	printf("Can get transceive:\t%c\n",caps->get_trn!=NULL?'Y':'N');
	printf("Can set func:\t%c\n",caps->set_func!=NULL?'Y':'N');
	printf("Can get func:\t%c\n",caps->get_func!=NULL?'Y':'N');
	printf("Can set level:\t%c\n",caps->set_level!=NULL?'Y':'N');
	printf("Can get level:\t%c\n",caps->get_level!=NULL?'Y':'N');
	printf("Can set param:\t%c\n",caps->set_parm!=NULL?'Y':'N');
	printf("Can get param:\t%c\n",caps->get_parm!=NULL?'Y':'N');
	printf("Can send DTMF:\t%c\n",caps->send_dtmf!=NULL?'Y':'N');
	printf("Can recv DTMF:\t%c\n",caps->recv_dtmf!=NULL?'Y':'N');
	printf("Can send Morse:\t%c\n",caps->send_morse!=NULL?'Y':'N');
	printf("Can decode events:\t%c\n",caps->decode_event!=NULL?'Y':'N');
	printf("Can set bank:\t%c\n",caps->set_bank!=NULL?'Y':'N');
	printf("Can set mem:\t%c\n",caps->set_mem!=NULL?'Y':'N');
	printf("Can get mem:\t%c\n",caps->get_mem!=NULL?'Y':'N');
	printf("Can set channel:\t%c\n",caps->set_channel!=NULL?'Y':'N');
	printf("Can get channel:\t%c\n",caps->get_channel!=NULL?'Y':'N');
	printf("Can ctl mem/vfo:\t%c\n",caps->vfo_op!=NULL?'Y':'N');
	printf("Can scan:\t%c\n",caps->scan!=NULL?'Y':'N');
	printf("Can get info:\t%c\n",caps->get_info!=NULL?'Y':'N');
	

	printf("\nOverall backend warnings: %d\n", backend_warnings);

	return backend_warnings;
}


static int print_ext(RIG *rig, const struct confparams *cfp, rig_ptr_t ptr)
{
	printf(" %s", cfp->name);

	return 1;       /* process them all */
}


/*
 * NB: this function is not reentrant, because of the static buf.
 * 		but who cares?  --SF
 */
static const char *decode_mtype(chan_type_t type)
{
		switch(type) {
				case RIG_MTYPE_NONE: return "NONE";
				case RIG_MTYPE_MEM: return "MEM";
				case RIG_MTYPE_EDGE: return "EDGE";
				case RIG_MTYPE_CALL: return "CALL";
				case RIG_MTYPE_MEMOPAD: return "MEMOPAD";
				default: return "UNKNOWN";
		}
}

/* 
 * check for:
 * - start_freq<end_freq	return_code=-1
 * - modes are not 0		return_code=-2
 * - if(rx), low_power,high_power set to -1		return_code=-3
 *     else, power is >0
 * - array is ended by a {0,0,0,0,0} element (before boundary) rc=-4
 * - ranges with same modes do not overlap		rc=-5
 * ->fprintf(stderr,)!
 *
 * TODO: array is sorted in ascending freq order
 */
int range_sanity_check(const struct freq_range_list range_list[], int rx)
{
	int i;

	for (i=0; i<FRQRANGESIZ; i++) {
			if (range_list[i].start == 0 && range_list[i].end == 0)
					break;
			if (range_list[i].start > range_list[i].end)
					return -1;
			if (range_list[i].modes == 0)
					return -2;
			if (rx) {
				if (range_list[i].low_power > 0 && range_list[i].high_power > 0)
						return -3;
			} else {
				if (!(range_list[i].low_power > 0 && range_list[i].high_power > 0))
						return -3;
				if (range_list[i].low_power > range_list[i].high_power)
						return -3;
			}
	}
	if (i == FRQRANGESIZ)
			return -4;

	return 0;
}

/* 
 * check for:
 * - steps sorted in ascending order return_code=-1
 * - modes are not 0		return_code=-2
 * - array is ended by a {0,0,0,0,0} element (before boundary) rc=-4
 *
 * TODO: array is sorted in ascending freq order
 */
int ts_sanity_check(const struct tuning_step_list tuning_step[])
{
	int i;
	shortfreq_t last_ts;
	rmode_t last_modes;

	last_ts = 0;
	last_modes = RIG_MODE_NONE;
	for (i=0; i<TSLSTSIZ; i++) {
			if (RIG_IS_TS_END(tuning_step[i]))
					break;
			if (tuning_step[i].ts != RIG_TS_ANY && tuning_step[i].ts < last_ts && 
							last_modes == tuning_step[i].modes)
					return -1;
			if (tuning_step[i].modes == 0)
					return -2;
			last_ts = tuning_step[i].ts;
			last_modes = tuning_step[i].modes;
	}
	if (i == TSLSTSIZ)
			return -4;

	return 0;
}


static void dump_chan_caps(const channel_cap_t *chan)
{
  if (chan->bank_num) printf("BANK ");
  if (chan->ant) printf("ANT ");
  if (chan->freq) printf("FREQ ");
  if (chan->mode) printf("MODE ");
  if (chan->width) printf("WIDTH ");
  if (chan->tx_freq) printf("TXFREQ ");
  if (chan->tx_mode) printf("TXMODE ");
  if (chan->tx_width) printf("TXWIDTH ");
  if (chan->split) printf("SPLIT ");
  if (chan->rptr_shift) printf("RPTRSHIFT ");
  if (chan->rptr_offs) printf("RPTROFS ");
  if (chan->tuning_step) printf("TS ");
  if (chan->rit) printf("RIT ");
  if (chan->xit) printf("XIT ");
  if (chan->funcs) printf("FUNC ");
  if (chan->levels) printf("LEVEL ");
  if (chan->ctcss_tone) printf("TONE ");
  if (chan->ctcss_sql) printf("CTCSS ");
  if (chan->dcs_code) printf("DCSCODE ");
  if (chan->dcs_sql) printf("DCSSQL ");
  if (chan->scan_group) printf("SCANGRP ");
  if (chan->flags) printf("FLAG ");    /* RIG_CHFLAG's */
  if (chan->channel_desc) printf("NAME ");
  if (chan->ext_levels) printf("EXTLVL ");
}

