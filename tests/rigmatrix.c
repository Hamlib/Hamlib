/*
 * rigmatric.c - Copyright (C) 2000 Stephane Fillod
 * This program generates the supported rig matrix in HTML format.
 * The code is rather ugly since this is only a try out.
 *
 *
 *    $Id: rigmatrix.c,v 1.4 2001-01-30 22:59:22 f4cfe Exp $  
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
#include <time.h>

#include <gd.h>
#include <gdfontg.h>
#include <gdfonts.h>

#include <hamlib/rig.h>


int create_png_range(const freq_range_t rx_range_list[], const freq_range_t tx_range_list[], int num);

int print_caps_sum(const struct rig_caps *caps, void *data)
{

	printf("<TR><TD>%s</TD><TD>%s</TD><TD>%s</TD><TD>",
					caps->model_name,caps->mfg_name,caps->version
					);

	switch (caps->status) {
	case RIG_STATUS_ALPHA:
			printf("Alpha");
			break;
	case RIG_STATUS_UNTESTED:
			printf("Untested");
			break;
	case RIG_STATUS_BETA:
			printf("Beta");
			break;
	case RIG_STATUS_STABLE:
			printf("Stable");
			break;
	case RIG_STATUS_BUGGY:
			printf("Buggy");
			break;
	case RIG_STATUS_NEW:
			printf("New");
			break;
	default:
			printf("Unknown");
	}
	printf("</TD><TD>");

	switch (caps->rig_type) {
	case RIG_TYPE_TRANSCEIVER:
			printf("Transceiver");
			break;
	case RIG_TYPE_HANDHELD:
			printf("Handheld");
			break;
	case RIG_TYPE_MOBILE:
			printf("Mobile");
			break;
	case RIG_TYPE_RECEIVER:
			printf("Receiver");
			break;
	case RIG_TYPE_SCANNER:
			printf("Scanner");
			break;
	case RIG_TYPE_COMPUTER:
			printf("Computer");
			break;
	default:
			printf("Unknown");
	}
	printf("</TD><TD><A HREF=\"#rng%d\">range</A></TD>"
					"<TD><A HREF=\"#parms%d\">parms</A></TD>"
					"<TD><A HREF=\"#caps%d\">caps</A></TD>"
					"<TD><A HREF=\"#setfunc%d\">func</A></TD>"
					"<TD><A HREF=\"#getlevel%d\">levels</A></TD>"
					"<TD><A HREF=\"#setlevel%d\">set levels</A></TD>"
					"</TR>\n",
					caps->rig_model, caps->rig_model, caps->rig_model,
					caps->rig_model, caps->rig_model, caps->rig_model
					);

	return 1;	/* !=0, we want them all ! */
}

/*
 * IO params et al.
 */
int print_caps_parms(const struct rig_caps *caps, void *data)
{
	printf("<A NAME=\"parms%d\"><TR><TD>%s</TD><TD>", 
					caps->rig_model,
					caps->model_name);
	
	switch (caps->ptt_type) {
	case RIG_PTT_RIG:
			printf("rig");
			break;
	case RIG_PTT_PARALLEL:
			printf("parallel");
			break;
	case RIG_PTT_SERIAL_RTS:
	case RIG_PTT_SERIAL_DTR:
			printf("serial");
			break;
	case RIG_PTT_NONE:
			printf("None");
			break;
	default:
			printf("Unknown");
	}

	printf("</TD><TD>%d</TD><TD>%d</TD><TD>%d%c%d</TD><TD>%s</TD>", 
					caps->serial_rate_min, caps->serial_rate_max,
					caps->serial_data_bits,
					caps->serial_parity==RIG_PARITY_NONE?'N':
					(caps->serial_parity==RIG_PARITY_ODD?'O':'E'),
					caps->serial_stop_bits,
					caps->serial_handshake==RIG_HANDSHAKE_NONE?"none":
					(caps->serial_handshake==RIG_HANDSHAKE_XONXOFF?"XONXOFF":"CTS/RTS")
					);

	printf("<TD>%dms</TD><TD>%dms</TD><TD>%dms</TD><TD>%d</TD></TR></A>\n",
					caps->write_delay, caps->post_write_delay,
					caps->timeout, caps->retry);
	return 1;
}

/* used by print_caps_caps and print_caps_level */
#define print_yn(fn) printf("<TD>%c</TD>", (fn) ? 'Y':'N')

/*
 * backend functions definied
 */
int print_caps_caps(const struct rig_caps *caps, void *data)
{
	printf("<A NAME=\"caps%d\"><TR><TD>%s</TD>", 
					caps->rig_model,
					caps->model_name);
	
	/* targetable_vfo is not a function, but a boolean */
	print_yn(caps->targetable_vfo);

	print_yn(caps->set_freq);
	print_yn(caps->get_freq);
	print_yn(caps->set_mode);
	print_yn(caps->get_mode);
	print_yn(caps->set_vfo);
	print_yn(caps->get_vfo);
	print_yn(caps->set_ptt);
	print_yn(caps->get_ptt);
	print_yn(caps->set_rptr_shift);
	print_yn(caps->get_rptr_shift);
	print_yn(caps->set_rptr_offs);
	print_yn(caps->get_rptr_offs);
	print_yn(caps->set_split_freq);
	print_yn(caps->get_split_freq);
	print_yn(caps->set_split);
	print_yn(caps->get_split);
	print_yn(caps->set_ts);
	print_yn(caps->get_ts);
	print_yn(caps->set_ctcss);
	print_yn(caps->get_ctcss);
	print_yn(caps->set_dcs);
	print_yn(caps->get_dcs);
	print_yn(caps->set_poweron);
	print_yn(caps->set_poweroff);
	print_yn(caps->set_trn);
	print_yn(caps->set_trn);
	print_yn(caps->decode_event);

	printf("</TR></A>\n");

	return 1;
}

/*
 * Get/Set level abilities
 */
int print_caps_level(const struct rig_caps *caps, void *data)
{
	setting_t level;

	if (!data)
			return 0;

	level = (*(int*)data)? caps->has_set_level : caps->has_level;

	printf("<A NAME=\"%slevel%d\"><TR><TD>%s</TD>", 
					(*(int*)data)? "set":"get",
					caps->rig_model,
					caps->model_name);
	
	print_yn(level & RIG_LEVEL_PREAMP);
	print_yn(level & RIG_LEVEL_ATT);
	print_yn(level & RIG_LEVEL_ANT);
	print_yn(level & RIG_LEVEL_AF);
	print_yn(level & RIG_LEVEL_RF);
	print_yn(level & RIG_LEVEL_SQL);
	print_yn(level & RIG_LEVEL_IF);
	print_yn(level & RIG_LEVEL_APF);
	print_yn(level & RIG_LEVEL_NR);
	print_yn(level & RIG_LEVEL_PBT_IN);
	print_yn(level & RIG_LEVEL_PBT_OUT);
	print_yn(level & RIG_LEVEL_CWPITCH);
	print_yn(level & RIG_LEVEL_RFPOWER);
	print_yn(level & RIG_LEVEL_MICGAIN);
	print_yn(level & RIG_LEVEL_KEYSPD);
	print_yn(level & RIG_LEVEL_NOTCHF);
	print_yn(level & RIG_LEVEL_COMP);
	print_yn(level & RIG_LEVEL_AGC);
	print_yn(level & RIG_LEVEL_BKINDL);
	print_yn(level & RIG_LEVEL_BALANCE);
	print_yn(level & RIG_LEVEL_ANN);

	/* get only levels */
	print_yn(level & RIG_LEVEL_SWR);
	print_yn(level & RIG_LEVEL_ALC);
	print_yn(level & RIG_LEVEL_SQLSTAT);
	print_yn(level & RIG_LEVEL_STRENGTH);

	printf("</TR></A>\n");

	return 1;
}

/*
 * Get/Set func abilities
 */
int print_caps_func(const struct rig_caps *caps, void *data)
{
	setting_t func;

	if (!data)
			return 0;

	func = caps->has_func;

	printf("<A NAME=\"%sfunc%d\"><TR><TD>%s</TD>", 
					(*(int*)data)? "set":"get",
					caps->rig_model,
					caps->model_name);
	
	print_yn(func & RIG_FUNC_FAGC);
	print_yn(func & RIG_FUNC_NB);
	print_yn(func & RIG_FUNC_COMP);
	print_yn(func & RIG_FUNC_VOX);
	print_yn(func & RIG_FUNC_TONE);
	print_yn(func & RIG_FUNC_TSQL);
	print_yn(func & RIG_FUNC_SBKIN);
	print_yn(func & RIG_FUNC_FBKIN);
	print_yn(func & RIG_FUNC_ANF);
	print_yn(func & RIG_FUNC_NR);
	print_yn(func & RIG_FUNC_AIP);
	print_yn(func & RIG_FUNC_APF);
	print_yn(func & RIG_FUNC_MON);
	print_yn(func & RIG_FUNC_MN);
	print_yn(func & RIG_FUNC_RFN);

	printf("</TR></A>\n");

	return 1;
}


/*
 * inlined PNG graphics
 */
int print_caps_range(const struct rig_caps *caps, void *data)
{
	create_png_range(caps->rx_range_list, caps->tx_range_list, caps->rig_model);

	printf("<A NAME=\"rng%d\"><TR><TD>%s</TD>"
					"<TD><IMG SRC=\"range%d.png\"></TD></TR></A>", 
					caps->rig_model,
					caps->model_name,
					caps->rig_model);
	return 1;
}

#define RANGE_WIDTH 600
#define RANGE_HEIGHT 16
#define RANGE_MIDHEIGHT (RANGE_HEIGHT/2)

#define RX_R 0
#define RX_G 255
#define RX_B 255
#define TX_R 0
#define TX_G 0
#define TX_B 255

#define HF_H 10
#define VHF_H 30
#define UHF_H 50
#define IM_LGD 21

static void draw_range(const freq_range_t range_list[], gdImagePtr im_rng, int h1, int h2, int rgb)
{
	int i;

	for (i=0; i<FRQRANGESIZ; i++) {
		float start_pix, end_pix;

		if (range_list[i].start == 0 && range_list[i].end == 0)
			break;

		start_pix = range_list[i].start;
		end_pix = range_list[i].end;

		/*
		 * HF
		 */
		if (range_list[i].start < MHz(30)) {
			start_pix = start_pix / MHz(30) * RANGE_WIDTH;
			end_pix = end_pix / MHz(30) * RANGE_WIDTH;
			if (end_pix >= RANGE_WIDTH)
					end_pix = RANGE_WIDTH-1;

			start_pix += IM_LGD;
			end_pix += IM_LGD;
			gdImageFilledRectangle(im_rng, start_pix, HF_H+h1, end_pix, HF_H+h2, rgb);
		}

		/*
		 * VHF
		 */
		start_pix = range_list[i].start;
		end_pix = range_list[i].end;
		if ((range_list[i].start > MHz(30) && range_list[i].start < MHz(300)) 
				|| (range_list[i].start < MHz(30) && range_list[i].end > MHz(30))) {
			start_pix = (start_pix-MHz(30)) / MHz(300) * RANGE_WIDTH;
			end_pix = (end_pix-MHz(30)) / MHz(300) * RANGE_WIDTH;
			if (start_pix < 0)
					start_pix = 0;
			if (end_pix >= RANGE_WIDTH)
					end_pix = RANGE_WIDTH-1;

			start_pix += IM_LGD;
			end_pix += IM_LGD;
			gdImageFilledRectangle(im_rng, start_pix, VHF_H+h1, end_pix, VHF_H+h2, rgb);
		}

		/*
		 * UHF
		 */
		start_pix = range_list[i].start;
		end_pix = range_list[i].end;
		if ((range_list[i].start > MHz(300) && range_list[i].start < GHz(3)) 
				|| (range_list[i].start < MHz(300) && range_list[i].end > MHz(300))) {
			start_pix = (start_pix-MHz(300)) / GHz(3) * RANGE_WIDTH;
			end_pix = (end_pix-MHz(300)) / GHz(3) * RANGE_WIDTH;
			if (start_pix < 0)
					start_pix = 0;
			if (end_pix >= RANGE_WIDTH)
					end_pix = RANGE_WIDTH-1;

			start_pix += IM_LGD;
			end_pix += IM_LGD;
			gdImageFilledRectangle(im_rng, start_pix, UHF_H+h1, end_pix, UHF_H+h2, rgb);
		}

	}
}

int create_png_range(const freq_range_t rx_range_list[],
						const freq_range_t tx_range_list[], int num)
{
	FILE *out;

	/* Input and output images */
	gdImagePtr im_rng;
	char rng_fname[128];

	/* Color indexes */
	int white, black;
	int rx_rgb,tx_rgb;

	/* Create output image, x by y pixels. */
	im_rng  = gdImageCreate(RANGE_WIDTH+IM_LGD, UHF_H+RANGE_HEIGHT);

	/* First color allocated is background. */
	white = gdImageColorAllocate(im_rng, 255, 255, 255);
	black = gdImageColorAllocate(im_rng, 0, 0, 0);

#if 0
	/* Set transparent color. */
	gdImageColorTransparent(im_rng, white);
#endif

	/* Try to load demoin.png and paste part of it into the
		output image. */

	tx_rgb = gdImageColorAllocate(im_rng, TX_R, TX_G, TX_B);
	rx_rgb = gdImageColorAllocate(im_rng, RX_R, RX_G, RX_B);

	draw_range(rx_range_list, im_rng, 0, RANGE_MIDHEIGHT-1, rx_rgb);
	draw_range(tx_range_list, im_rng, RANGE_MIDHEIGHT, RANGE_HEIGHT-1, tx_rgb);

	gdImageRectangle(im_rng, IM_LGD, HF_H, IM_LGD+RANGE_WIDTH-1, HF_H+RANGE_HEIGHT-1, black);
	gdImageRectangle(im_rng, IM_LGD, VHF_H, IM_LGD+RANGE_WIDTH-1, VHF_H+RANGE_HEIGHT-1, black);
	gdImageRectangle(im_rng, IM_LGD, UHF_H, IM_LGD+RANGE_WIDTH-1, UHF_H+RANGE_HEIGHT-1, black);

	/* gdImageStringUp */
	gdImageString(im_rng, gdFontSmall, 1, HF_H+1, 
		(unsigned char *) "HF", black);
	gdImageString(im_rng, gdFontSmall, 1, VHF_H+1, 
		(unsigned char *) "VHF", black);
	gdImageString(im_rng, gdFontSmall, 1, UHF_H+1, 
		(unsigned char *) "UHF", black);

	/* Make output image interlaced (allows "fade in" in some viewers,
		and in the latest web browsers) */
	gdImageInterlace(im_rng, 1);

	sprintf(rng_fname, "range%d.png", num);
	out = fopen(rng_fname, "wb");
	/* Write PNG */
	gdImagePng(im_rng, out);
	fclose(out);
	gdImageDestroy(im_rng);

	return 0;
}

int main (int argc, char *argv[])
{ 
	int status;
	time_t gentime;
	int set_or_get;

	status = rig_load_backend("icom");
	if (status != RIG_OK ) {
		printf("rig_load_backend: error = %s \n", rigerror(status));
		exit(3);
	}
	status = rig_load_backend("ft747");
	if (status != RIG_OK ) {
		printf("rig_load_backend: ft747 error = %s \n", rigerror(status));
		exit(3);
	}
	status = rig_load_backend("ft847");
	if (status != RIG_OK ) {
		printf("rig_load_backend: ft847 error = %s \n", rigerror(status));
		exit(3);
	}
	status = rig_load_backend("kenwood");
	if (status != RIG_OK ) {
		printf("rig_load_backend: kenwood error = %s \n", rigerror(status));
		exit(3);
	}
	status = rig_load_backend("aor");
	if (status != RIG_OK ) {
		printf("rig_load_backend: aor error = %s \n", rigerror(status));
		exit(3);
	}

	printf("<TABLE BORDER=1>");
	printf("<TR><TD>Model</TD><TD>Mfg</TD><TD>Vers.</TD><TD>Status</TD>"
					"<TD>Type</TD><TD>Freq. range</TD><TD>Parameters</TD>"
					"<TD>Capabilities</TD>"
					"<TD>Has func</TD>"
					"<TD>Get level</TD>"
					"<TD>Set level</TD>"
					"</TR>\n");
	status = rig_list_foreach(print_caps_sum,NULL);
	printf("</TABLE>\n");

	printf("<P>");

	printf("<TABLE BORDER=1>\n");
	printf("<TR><TD>Model</TD><TD>PTT</TD><TD>Speed min</TD><TD>Speed max</TD>"
					"<TD>Parm.</TD><TD>Handshake</TD><TD>Write delay</TD>"
					"<TD>Post delay</TD><TD>Timeout</TD><TD>Retry</TD></TR>\n");
	status = rig_list_foreach(print_caps_parms,NULL);
	printf("</TABLE>\n");

	printf("<P>");

	printf("<TABLE BORDER=1>\n");
	printf("<TR><TD>Model</TD><TD>Freq. range</TD></TR>\n");
	status = rig_list_foreach(print_caps_range,NULL);
	printf("</TABLE>\n");

	printf("<P>");

	printf("<TABLE BORDER=1>\n");
	printf("<TR><TD>Model</TD><TD>Target VFO</TD>"
					"<TD>Set freq</TD><TD>Get freq</TD>"
					"<TD>Set mode</TD><TD>Get mode</TD>"
					"<TD>Set VFO</TD><TD>Get VFO</TD>"
					"<TD>Set PTT</TD><TD>Get PTT</TD>"
					"<TD>Set rptr shift</TD><TD>Get rptr shift</TD>"
					"<TD>Set rptr offs</TD><TD>Get rptr offs</TD>"
					"<TD>Set split frq</TD><TD>Get split frq</TD>"
					"<TD>Set split</TD><TD>Get split</TD>"
					"<TD>Set ts</TD><TD>Get ts</TD>"
					"<TD>Set CTCSS</TD><TD>Get CTCSS</TD>"
					"<TD>Set DCS</TD><TD>Get DCS</TD>"
					"<TD>Set PowerON</TD><TD>Set PowerOFF</TD>"
					"<TD>Set trn</TD><TD>Get trn</TD>"
					"<TD>Decode</TD>"
					"</TR>\n");
	status = rig_list_foreach(print_caps_caps,NULL);
	printf("</TABLE>\n");

	printf("<P>");

	printf("Has func");
	printf("<TABLE BORDER=1>\n");
	printf("<TR><TD>Model</TD>"
					"<TD>FAGC</TD><TD>NB</TD>"
					"<TD>COMP</TD><TD>VOX</TD>"
					"<TD>TONE</TD><TD>TSQL</TD>"
					"<TD>SBKIN</TD><TD>FBKIN</TD>"
					"<TD>ANF</TD><TD>NR</TD>"
					"<TD>AIP</TD><TD>APF</TD>"
					"<TD>MON</TD><TD>MN</TD>"
					"<TD>RFN</TD>"
					"</TR>\n");
	set_or_get = 1;
	status = rig_list_foreach(print_caps_func,&set_or_get);
	printf("</TABLE>\n");

	printf("<P>");

	printf("Set level");
	printf("<TABLE BORDER=1>\n");
	printf("<TR><TD>Model</TD>"
					"<TD>Preamp</TD><TD>Att</TD>"
					"<TD>Ant</TD>"
					"<TD>AF</TD><TD>RF</TD>"
					"<TD>SQL</TD><TD>IF</TD>"
					"<TD>APF</TD><TD>NR</TD>"
					"<TD>PBT IN</TD><TD>PBT OUT</TD>"
					"<TD>CW pitch</TD><TD>RF Power</TD>"
					"<TD>Mic gain</TD><TD>Key speed</TD>"
					"<TD>Notch</TD><TD>Comp</TD>"
					"<TD>AGC</TD><TD>BKin delay</TD>"
					"<TD>Bal</TD><TD>Ann</TD>"
					"<TD>SWR</TD><TD>ALC</TD>"
					"<TD>SQL stat</TD><TD>SMeter</TD>"
					"</TR>\n");
	set_or_get = 1;
	status = rig_list_foreach(print_caps_level,&set_or_get);
	printf("</TABLE>\n");

	printf("<P>");

	printf("Get level");
	printf("<TABLE BORDER=1>\n");
	printf("<TR><TD>Model</TD>"
					"<TD>Preamp</TD><TD>Att</TD>"
					"<TD>Ant</TD>"
					"<TD>AF</TD><TD>RF</TD>"
					"<TD>SQL</TD><TD>IF</TD>"
					"<TD>APF</TD><TD>NR</TD>"
					"<TD>PBT IN</TD><TD>PBT OUT</TD>"
					"<TD>CW pitch</TD><TD>RF Power</TD>"
					"<TD>Mic gain</TD><TD>Key speed</TD>"
					"<TD>Notch</TD><TD>Comp</TD>"
					"<TD>AGC</TD><TD>BKin delay</TD>"
					"<TD>Bal</TD><TD>Ann</TD>"
					"<TD>SWR</TD><TD>ALC</TD>"
					"<TD>SQL stat</TD><TD>SMeter</TD>"
					"</TR>\n");
	set_or_get = 0;
	status = rig_list_foreach(print_caps_level,&set_or_get);
	printf("</TABLE>\n");

	printf("<P>");

	time(&gentime);
	printf("Rigmatrix generated %s by %s\n",ctime(&gentime), getenv("USER"));

	printf("</body></html>\n");
	return 0;
}


