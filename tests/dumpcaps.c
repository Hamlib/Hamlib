/*
 * dumpcaps.c - Copyright (C) 2000 Stephane Fillod
 * This programs dumps the capabilities of a backend rig.
 *
 *
 *    $Id: dumpcaps.c,v 1.6 2000-11-01 23:27:26 f4cfe Exp $  
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


static char *decode_modes(rmode_t modes);
int range_sanity_check(const struct freq_range_list range_list[], int rx);

int main (int argc, char *argv[])
{ 
	const struct rig_caps *caps;
	int status,i;

	if (argc != 2) {
			fprintf(stderr,"%s <rig_num>\n",argv[0]);
			exit(1);
	}

  	status = rig_load_backend("icom");
	status |= rig_load_backend("ft747");
	status |= rig_load_backend("ft847");
	status |= rig_load_backend("aor");

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
	case RIG_TYPE_SCANNER:
			printf("Scanner\n");
			break;
	case RIG_TYPE_COMPUTER:
			printf("Computer\n");
			break;
	default:
			printf("Unknown\n");
	}

	printf("PTT type:\t");
	switch (caps->ptt_type) {
	case RIG_PTT_RIG:
			printf("rig capable\n");
			break;
	case RIG_PTT_PARALLEL:
			printf("thru parallel port (DATA0)\n");
			break;
	case RIG_PTT_SERIAL:
			printf("thru serial port (CTS/RTS)\n");
			break;
	case RIG_PTT_NONE:
			printf("None\n");
			break;
	default:
			printf("Unknown\n");
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
	printf("Post Write delay %dms \n",
					caps->post_write_delay);


	printf("Functions: ");
	if (caps->has_func!=0) {
		if (caps->has_func&RIG_FUNC_FAGC) printf("FAGC ");
		if (caps->has_func&RIG_FUNC_NB) printf("NB ");
		if (caps->has_func&RIG_FUNC_COMP) printf("COMP ");
		if (caps->has_func&RIG_FUNC_VOX) printf("VOX ");
		if (caps->has_func&RIG_FUNC_TONE) printf("TONE ");
		if (caps->has_func&RIG_FUNC_TSQL) printf("TSQL ");
		if (caps->has_func&RIG_FUNC_SBKIN) printf("SBKIN ");
		if (caps->has_func&RIG_FUNC_FBKIN) printf("FBKIN ");
		if (caps->has_func&RIG_FUNC_ANF) printf("ANF ");
		if (caps->has_func&RIG_FUNC_NR) printf("NR ");
		printf("\n");
	} else
			printf("none\n");

	printf("Number of channels:\t%d\n",caps->chan_qty);

/* TODO: print rx/tx ranges here */
	status = range_sanity_check(caps->tx_range_list,0);
	printf("TX ranges status:\t%s (%d)\n",status?"Bad":"OK",status);
	status = range_sanity_check(caps->rx_range_list,1);
	printf("RX ranges status:\t%s (%d)\n",status?"Bad":"OK",status);

	printf("Tuning steps:\n");
	for (i=0; i<TSLSTSIZ && caps->tuning_steps[i].ts; i++) {
			printf("\t%iHz:\t%s\n",caps->tuning_steps[i].ts,
							decode_modes(caps->tuning_steps[i].modes));
	}

	printf("Can set frequency:\t%c\n",caps->set_freq!=NULL?'Y':'N');
	printf("Can get frequency:\t%c\n",caps->get_freq!=NULL?'Y':'N');
	printf("Can set mode:\t%c\n",caps->set_mode!=NULL?'Y':'N');
	printf("Can get mode:\t%c\n",caps->get_mode!=NULL?'Y':'N');
	printf("Can set vfo:\t%c\n",caps->set_vfo!=NULL?'Y':'N');
	printf("Can get vfo:\t%c\n",caps->get_vfo!=NULL?'Y':'N');
	printf("Can set ptt:\t%c\n",caps->set_ptt!=NULL?'Y':'N');
	printf("Can get ptt:\t%c\n",caps->get_ptt!=NULL?'Y':'N');
	printf("Can set repeater duplex:\t%c\n",caps->set_rptr_shift!=NULL?'Y':'N');
	printf("Can get repeater duplex:\t%c\n",caps->get_rptr_shift!=NULL?'Y':'N');
	printf("Can set repeater offset:\t%c\n",caps->set_rptr_offs!=NULL?'Y':'N');
	printf("Can get repeater offset:\t%c\n",caps->get_rptr_offs!=NULL?'Y':'N');
	printf("Can set tuning step:\t%c\n",caps->set_ts!=NULL?'Y':'N');
	printf("Can get tuning step:\t%c\n",caps->get_ts!=NULL?'Y':'N');
	printf("Can set power on:\t%c\n",caps->set_poweron!=NULL?'Y':'N');
	printf("Can set power off:\t%c\n",caps->set_poweroff!=NULL?'Y':'N');
	printf("Can set transceive:\t%c\n",caps->set_trn!=NULL?'Y':'N');
	printf("Can get transceive:\t%c\n",caps->get_trn!=NULL?'Y':'N');
	printf("Can decode events:\t%c\n",caps->decode_event!=NULL?'Y':'N');
	printf("Can set channel:\t%c\n",caps->set_channel!=NULL?'Y':'N');
	printf("Can get channel:\t%c\n",caps->get_channel!=NULL?'Y':'N');
	

	return 0;
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
#if 0
	if (modes&RIG_MODE_CWR) strcat(buf,"CWR ");
	if (modes&RIG_MODE_NFM) strcat(buf,"NFM ");
	if (modes&RIG_MODE_WFM) strcat(buf,"WFM ");
	if (modes&RIG_MODE_NAM) strcat(buf,"NAM ");
	if (modes&RIG_MODE_WAM) strcat(buf,"WAM ");
	if (modes&RIG_MODE_NCW) strcat(buf,"NCW ");
	if (modes&RIG_MODE_WCW) strcat(buf,"WCW ");
	if (modes&RIG_MODE_NUSB) strcat(buf,"NUSB ");
	if (modes&RIG_MODE_WUSB) strcat(buf,"WUSB ");
	if (modes&RIG_MODE_NLSB) strcat(buf,"NLSB ");
	if (modes&RIG_MODE_WLSB) strcat(buf,"WLSB ");
	if (modes&RIG_MODE_NRTTY) strcat(buf,"NRTTY ");
	if (modes&RIG_MODE_WRTTY) strcat(buf,"WRTTY ");
#endif

	return buf;
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


