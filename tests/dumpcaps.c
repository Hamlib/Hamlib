/*
 * dumpcaps.c - Copyright (C) 2000,2001 Stephane Fillod
 * This programs dumps the capabilities of a backend rig.
 *
 *
 *    $Id: dumpcaps.c,v 1.23 2001-05-08 16:41:52 f4cfe Exp $  
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


static char *decode_modes(rmode_t modes);
static const char *decode_mtype(enum chan_type_e type);
int range_sanity_check(const struct freq_range_list range_list[], int rx);
int ts_sanity_check(const struct tuning_step_list tuning_step[]);

int main (int argc, char *argv[])
{ 
	const struct rig_caps *caps;
	int status,i;
	char freqbuf[20];
	int backend_warnings=0;

	if (argc != 2) {
			fprintf(stderr,"%s <rig_num>\n",argv[0]);
			exit(1);
	}

  	status = rig_load_backend("icom");
	status |= rig_load_backend("ft747");
	status |= rig_load_backend("ft847");
	status |= rig_load_backend("kenwood");
	status |= rig_load_backend("aor");
	status |= rig_load_backend("pcr");
	rig_load_backend("winradio");   /* may not be compiled ... */
	rig_load_backend("dummy");

	if (status != RIG_OK ) {
		printf("rig_load_backend: error = %s \n", rigerror(status));
		exit(3);
	}

	caps = rig_get_caps(atoi(argv[1]));
	if (!caps) {
			fprintf(stderr,"Unknown rig num: %d\n",atoi(argv[1]));
			fprintf(stderr,"Please check riglist.h\n");
			exit(2);
	}

	printf("Rig dump for model %d\n",caps->rig_model);
	printf("Model name:\t%s\n",caps->model_name);
	printf("Mfg name:\t%s\n",caps->mfg_name);
	printf("Backend version:\t%s\n",caps->version);
	printf("Backend copyright:\t%s\n",caps->copyright);
	printf("Backend status:\t");
	switch (caps->status) {
	case RIG_STATUS_ALPHA:
			printf("Alpha\n");
			break;
	case RIG_STATUS_UNTESTED:
			printf("Untested\n");
			break;
	case RIG_STATUS_BETA:
			printf("Beta\n");
			break;
	case RIG_STATUS_STABLE:
			printf("Stable\n");
			break;
	case RIG_STATUS_BUGGY:
			printf("Buggy\n");
			break;
	case RIG_STATUS_NEW:
			printf("New\n");
			break;
	default:
			printf("Unknown\n");
			backend_warnings++;
	}
	printf("Rig type:\t");
	switch (caps->rig_type) {
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
	case RIG_TYPE_COMPUTER:
			printf("Computer\n");
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

	printf("Announce: 0x%lx\n", caps->announces);
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

	printf("Get functions: ");
	if (caps->has_get_func!=0) {
		if (caps->has_get_func&RIG_FUNC_FAGC) printf("FAGC ");
		if (caps->has_get_func&RIG_FUNC_NB) printf("NB ");
		if (caps->has_get_func&RIG_FUNC_COMP) printf("COMP ");
		if (caps->has_get_func&RIG_FUNC_VOX) printf("VOX ");
		if (caps->has_get_func&RIG_FUNC_TONE) printf("TONE ");
		if (caps->has_get_func&RIG_FUNC_TSQL) printf("TSQL ");
		if (caps->has_get_func&RIG_FUNC_SBKIN) printf("SBKIN ");
		if (caps->has_get_func&RIG_FUNC_FBKIN) printf("FBKIN ");
		if (caps->has_get_func&RIG_FUNC_ANF) printf("ANF ");
		if (caps->has_get_func&RIG_FUNC_NR) printf("NR ");
		if (caps->has_get_func&RIG_FUNC_AIP) printf("AIP ");
		if (caps->has_get_func&RIG_FUNC_APF) printf("APF ");
		if (caps->has_get_func&RIG_FUNC_MON) printf("MON ");
		if (caps->has_get_func&RIG_FUNC_MN) printf("MN ");
		if (caps->has_get_func&RIG_FUNC_RNF) printf("RNF ");
		printf("\n");
	} else
			printf("none\n");

	printf("Set functions: ");
	if (caps->has_set_func!=0) {
		if (caps->has_set_func&RIG_FUNC_FAGC) printf("FAGC ");
		if (caps->has_set_func&RIG_FUNC_NB) printf("NB ");
		if (caps->has_set_func&RIG_FUNC_COMP) printf("COMP ");
		if (caps->has_set_func&RIG_FUNC_VOX) printf("VOX ");
		if (caps->has_set_func&RIG_FUNC_TONE) printf("TONE ");
		if (caps->has_set_func&RIG_FUNC_TSQL) printf("TSQL ");
		if (caps->has_set_func&RIG_FUNC_SBKIN) printf("SBKIN ");
		if (caps->has_set_func&RIG_FUNC_FBKIN) printf("FBKIN ");
		if (caps->has_set_func&RIG_FUNC_ANF) printf("ANF ");
		if (caps->has_set_func&RIG_FUNC_NR) printf("NR ");
		if (caps->has_set_func&RIG_FUNC_AIP) printf("AIP ");
		if (caps->has_set_func&RIG_FUNC_APF) printf("APF ");
		if (caps->has_set_func&RIG_FUNC_MON) printf("MON ");
		if (caps->has_set_func&RIG_FUNC_MN) printf("MN ");
		if (caps->has_set_func&RIG_FUNC_RNF) printf("RNF ");
		printf("\n");
	} else
			printf("none\n");

	printf("Get level: ");
	if (caps->has_get_level!=0) {
		if (caps->has_get_level&RIG_LEVEL_PREAMP) printf("PREAMP ");
		if (caps->has_get_level&RIG_LEVEL_ATT) printf("ATT ");
		if (caps->has_get_level&RIG_LEVEL_AF) printf("AF ");
		if (caps->has_get_level&RIG_LEVEL_RF) printf("RF ");
		if (caps->has_get_level&RIG_LEVEL_SQL) printf("SQL ");
		if (caps->has_get_level&RIG_LEVEL_IF) printf("IF ");
		if (caps->has_get_level&RIG_LEVEL_APF) printf("APF ");
		if (caps->has_get_level&RIG_LEVEL_NR) printf("NR ");
		if (caps->has_get_level&RIG_LEVEL_PBT_IN) printf("PBT_IN ");
		if (caps->has_get_level&RIG_LEVEL_PBT_OUT) printf("PBT_OUT ");
		if (caps->has_get_level&RIG_LEVEL_CWPITCH) printf("CWPITCH ");
		if (caps->has_get_level&RIG_LEVEL_RFPOWER) printf("RFPOWER ");
		if (caps->has_get_level&RIG_LEVEL_MICGAIN) printf("MICGAIN ");
		if (caps->has_get_level&RIG_LEVEL_KEYSPD) printf("KEYSPD ");
		if (caps->has_get_level&RIG_LEVEL_NOTCHF) printf("NOTCHF ");
		if (caps->has_get_level&RIG_LEVEL_COMP) printf("COMP ");
		if (caps->has_get_level&RIG_LEVEL_AGC) printf("AGC ");
		if (caps->has_get_level&RIG_LEVEL_BKINDL) printf("BKINDL ");
		if (caps->has_get_level&RIG_LEVEL_BALANCE) printf("BALANCE ");

		if (caps->has_get_level&RIG_LEVEL_SWR) printf("SWR ");
		if (caps->has_get_level&RIG_LEVEL_ALC) printf("ALC ");
		if (caps->has_get_level&RIG_LEVEL_SQLSTAT) printf("SQLSTAT ");
		if (caps->has_get_level&RIG_LEVEL_STRENGTH) printf("STRENGTH ");
		printf("\n");
	} else
			printf("none\n");

	printf("Set level: ");
	if (caps->has_set_level!=0) {
		if (caps->has_set_level&RIG_LEVEL_PREAMP) printf("PREAMP ");
		if (caps->has_set_level&RIG_LEVEL_ATT) printf("ATT ");
		if (caps->has_set_level&RIG_LEVEL_AF) printf("AF ");
		if (caps->has_set_level&RIG_LEVEL_RF) printf("RF ");
		if (caps->has_set_level&RIG_LEVEL_SQL) printf("SQL ");
		if (caps->has_set_level&RIG_LEVEL_IF) printf("IF ");
		if (caps->has_set_level&RIG_LEVEL_APF) printf("APF ");
		if (caps->has_set_level&RIG_LEVEL_NR) printf("NR ");
		if (caps->has_set_level&RIG_LEVEL_PBT_IN) printf("PBT_IN ");
		if (caps->has_set_level&RIG_LEVEL_PBT_OUT) printf("PBT_OUT ");
		if (caps->has_set_level&RIG_LEVEL_CWPITCH) printf("CWPITCH ");
		if (caps->has_set_level&RIG_LEVEL_RFPOWER) printf("RFPOWER ");
		if (caps->has_set_level&RIG_LEVEL_MICGAIN) printf("MICGAIN ");
		if (caps->has_set_level&RIG_LEVEL_KEYSPD) printf("KEYSPD ");
		if (caps->has_set_level&RIG_LEVEL_NOTCHF) printf("NOTCHF ");
		if (caps->has_set_level&RIG_LEVEL_COMP) printf("COMP ");
		if (caps->has_set_level&RIG_LEVEL_AGC) printf("AGC ");
		if (caps->has_set_level&RIG_LEVEL_BKINDL) printf("BKINDL ");
		if (caps->has_set_level&RIG_LEVEL_BALANCE) printf("BALANCE ");

		if (caps->has_set_level&RIG_LEVEL_SWR) printf("SWR ");
		if (caps->has_set_level&RIG_LEVEL_ALC) printf("ALC ");
		if (caps->has_set_level&RIG_LEVEL_SQLSTAT) printf("SQLSTAT ");
		if (caps->has_set_level&RIG_LEVEL_STRENGTH) printf("STRENGTH ");
		printf("\n");
		if (caps->has_set_level&RIG_LEVEL_READONLY_LIST) {
				printf("Warning: backend can set readonly levels!\n");
				backend_warnings++;
		}
	} else
			printf("none\n");

	printf("Get parameters: ");
	if (caps->has_get_parm!=0) {
		if (caps->has_get_parm&RIG_PARM_ANN) printf("ANN ");
		if (caps->has_get_parm&RIG_PARM_APO) printf("APO ");
		if (caps->has_get_parm&RIG_PARM_BACKLIGHT) printf("BACKLIGHT ");
		if (caps->has_get_parm&RIG_PARM_BEEP) printf("BEEP ");
		if (caps->has_get_parm&RIG_PARM_TIME) printf("TIME ");
		if (caps->has_get_parm&RIG_PARM_BAT) printf("BAT ");
		printf("\n");
	} else
			printf("none\n");

	printf("Set parameters: ");
	if (caps->has_set_parm!=0) {
		if (caps->has_set_parm&RIG_PARM_ANN) printf("ANN ");
		if (caps->has_set_parm&RIG_PARM_APO) printf("APO ");
		if (caps->has_set_parm&RIG_PARM_BACKLIGHT) printf("BACKLIGHT ");
		if (caps->has_set_parm&RIG_PARM_BEEP) printf("BEEP ");
		if (caps->has_set_parm&RIG_PARM_TIME) printf("TIME ");
		if (caps->has_set_parm&RIG_PARM_BAT) printf("BAT ");
		printf("\n");
	} else
			printf("none\n");


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
	for (i=0; i<TSLSTSIZ && caps->tuning_steps[i].ts; i++) {
			freq_sprintf(freqbuf,caps->tuning_steps[i].ts);
			printf("\n\t%s:   \t%s", freqbuf,
							decode_modes(caps->tuning_steps[i].modes));
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
	for (i=0; i<FLTLSTSIZ && caps->filters[i].modes; i++) {
			freq_sprintf(freqbuf,caps->filters[i].width);
			printf("\n\t%s:   \t%s", freqbuf,
							decode_modes(caps->filters[i].modes));
	}
	if (i==0) {
			printf(" none! This backend might be bogus!");
			backend_warnings++;
	}
	printf("\n");

	printf("Has priv data:\t%c\n",caps->priv!=NULL?'Y':'N');
	/*
	 * TODO: keep me up-to-date with API call list!
	 */
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
	printf("Can set split:\t%c\n",caps->set_split!=NULL?'Y':'N');
	printf("Can get split:\t%c\n",caps->get_split!=NULL?'Y':'N');
	printf("Can set tuning step:\t%c\n",caps->set_ts!=NULL?'Y':'N');
	printf("Can get tuning step:\t%c\n",caps->get_ts!=NULL?'Y':'N');
	printf("Can set RIT:\t%c\n",caps->set_rit!=NULL?'Y':'N');
	printf("Can get RIT:\t%c\n",caps->get_rit!=NULL?'Y':'N');
	printf("Can set XIT:\t%c\n",caps->set_xit!=NULL?'Y':'N');
	printf("Can get XIT:\t%c\n",caps->get_xit!=NULL?'Y':'N');
	printf("Can set CTCSS:\t%c\n",caps->set_ctcss!=NULL?'Y':'N');
	printf("Can get CTCSS:\t%c\n",caps->get_ctcss!=NULL?'Y':'N');
	printf("Can set DCS:\t%c\n",caps->set_dcs!=NULL?'Y':'N');
	printf("Can get DCS:\t%c\n",caps->get_dcs!=NULL?'Y':'N');
	printf("Can set CTCSS squelch:\t%c\n",caps->set_ctcss_sql!=NULL?'Y':'N');
	printf("Can get CTCSS squelch:\t%c\n",caps->get_ctcss_sql!=NULL?'Y':'N');
	printf("Can set DCS squelch:\t%c\n",caps->set_dcs_sql!=NULL?'Y':'N');
	printf("Can get DCS squelch:\t%c\n",caps->get_dcs_sql!=NULL?'Y':'N');
	printf("Can set power stat:\t%c\n",caps->set_powerstat!=NULL?'Y':'N');
	printf("Can get power stat:\t%c\n",caps->get_powerstat!=NULL?'Y':'N');
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
	printf("Can set mem:\t%c\n",caps->set_mem!=NULL?'Y':'N');
	printf("Can get mem:\t%c\n",caps->get_mem!=NULL?'Y':'N');
	printf("Can set channel:\t%c\n",caps->set_channel!=NULL?'Y':'N');
	printf("Can get channel:\t%c\n",caps->get_channel!=NULL?'Y':'N');
	printf("Can ctl mem/vfo:\t%c\n",caps->mv_ctl!=NULL?'Y':'N');
	printf("Can get info:\t%c\n",caps->get_info!=NULL?'Y':'N');
	

	printf("\nOverall backend warnings: %d\n", backend_warnings);

	return backend_warnings;
}


/*
 * NB: this function is not reentrant, because of the static buf.
 * 		but who cares?  --SF
 */
static char *decode_modes(rmode_t modes)
{
	static char buf[80];

	buf[0] = '\0';
	if (modes&RIG_MODE_AM) strcat(buf,"AM ");
	if (modes&RIG_MODE_CW) strcat(buf,"CW ");
	if (modes&RIG_MODE_USB) strcat(buf,"USB ");
	if (modes&RIG_MODE_LSB) strcat(buf,"LSB ");
	if (modes&RIG_MODE_RTTY) strcat(buf,"RTTY ");
	if (modes&RIG_MODE_FM) strcat(buf,"FM ");
#ifdef RIG_MODE_WFM
	if (modes&RIG_MODE_WFM) strcat(buf,"WFM ");
#endif

	return buf;
}

static const char *decode_mtype(enum chan_type_e type)
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
			if (tuning_step[i].modes == 0 && tuning_step[i].ts == 0)
					break;
			if (tuning_step[i].ts < last_ts && 
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


