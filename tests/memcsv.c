/*
 * memcsv.c - (C) Stephane Fillod 2003-2005
 *
 * This program exercises the backup and restore of a radio
 * using Hamlib. CSV primitives
 *
 * $Id: memcsv.c,v 1.8 2005-04-20 14:47:04 fillods Exp $  
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

#include <getopt.h>

#include <hamlib/rig.h>
#include "misc.h"
#include "sprintflst.h"


/* 
 * external prototype
 */

extern int all;

char csv_sep = ',';	/* CSV separator */

/* 
 * Prototypes
 */
static int dump_csv_chan(RIG *rig, channel_t **chan, int channel_num, const chan_t *chan_list, rig_ptr_t arg);
static void dump_csv_name(const channel_cap_t *mem_caps, FILE *f);

int csv_save (RIG *rig, const char *outfilename);
int csv_load (RIG *rig, const char *infilename);
int csv_parm_save (RIG *rig, const char *outfilename);
int csv_parm_load (RIG *rig, const char *infilename);



int csv_save (RIG *rig, const char *outfilename)
{
	int status;
	FILE *f;

	f = fopen(outfilename, "w");
	if (!f)
		return -1;

	if (rig->caps->clone_combo_get)
		printf("About to save data, enter cloning mode: %s\n",
				rig->caps->clone_combo_get);

	status = rig_get_chan_all_cb (rig, dump_csv_chan, f);

	fclose(f);
	return status;
}

int csv_load (RIG *rig, const char *infilename)
{
	return -RIG_ENIMPL;
	/* for every channel, fscanf, parse, set_chan */
}

static int print_parm_name(RIG *rig, const struct confparams *cfp, rig_ptr_t ptr)
{
	fprintf((FILE*)ptr, "%s%c", cfp->name, csv_sep);

	return 1;	/* process them all */
}

static int print_parm_val(RIG *rig, const struct confparams *cfp, rig_ptr_t ptr)
{
	value_t val;
	FILE *f=(FILE*)ptr;
	rig_get_ext_parm(rig, cfp->token, &val);

	switch (cfp->type) {
		case RIG_CONF_CHECKBUTTON:
		case RIG_CONF_COMBO:
			fprintf(f,"%d%c", val.i, csv_sep);
			break;
		case RIG_CONF_NUMERIC:
			fprintf(f,"%f%c", val.f, csv_sep);
			break;
		case RIG_CONF_STRING:
			fprintf(f,"%s%c", val.s, csv_sep);
			break;
		default:
			fprintf(f,"unknown%c", csv_sep);
       	}

	return 1;	/* process them all */
}


int csv_parm_save (RIG *rig, const char *outfilename)
{
	int i, ret;
	FILE *f;
	setting_t parm, get_parm = all ? 0x7fffffff : rig->state.has_get_parm;

	f = fopen(outfilename, "w");
	if (!f)
		return -1;

	for (i = 0; i < RIG_SETTING_MAX; i++) {
		const char *ms = rig_strparm(get_parm & rig_idx2setting(i));
		if (!ms || !ms[0])
			continue;
		fprintf(f, "%s%c", ms, csv_sep);
	}

	rig_ext_parm_foreach(rig, print_parm_name, f);
	fprintf(f, "\n");

	for (i = 0; i < RIG_SETTING_MAX; i++) {
		const char *ms;
		value_t val;

		parm = get_parm & rig_idx2setting(i);
		ms = rig_strparm(parm);

		if (!ms || !ms[0])
			continue;
		ret = rig_get_parm(rig, parm, &val);
		if (RIG_PARM_IS_FLOAT(parm))
			fprintf(f, "%f%c", val.f, csv_sep);
		else
			fprintf(f, "%d%c", val.i, csv_sep);
	}


	rig_ext_parm_foreach(rig, print_parm_val, f);
	fprintf(f, "\n");
	fclose(f);

	return 0;
}

int csv_parm_load (RIG *rig, const char *infilename)
{
	return -RIG_ENIMPL;
	/* for every parm/ext_parm, fscanf, parse, set_parm's */
}


/* *********************** */


void dump_csv_name(const channel_cap_t *mem_caps, FILE *f)
{
	fprintf(f, "num%c", csv_sep);

	if (mem_caps->bank_num) {
		fprintf(f, "bank_num%c", csv_sep);
	}

	if (mem_caps->channel_desc) {
			fprintf(f, "channel_desc%c", csv_sep);
	}
	if (mem_caps->vfo) {
		fprintf(f, "vfo%c", csv_sep);
	}
	if (mem_caps->ant) {
		fprintf(f, "ant%c", csv_sep);
	}
	if (mem_caps->freq) {
		fprintf(f, "freq%c", csv_sep);
	}
	if (mem_caps->mode) {
		fprintf(f, "mode%c", csv_sep);
	}
	if (mem_caps->width) {
		fprintf(f, "width%c", csv_sep);
	}
	if (mem_caps->tx_freq) {
		fprintf(f, "tx_freq%c", csv_sep);
	}
	if (mem_caps->tx_mode) {
		fprintf(f, "tx_mode%c", csv_sep);
	}
	if (mem_caps->tx_width) {
		fprintf(f, "tx_width%c", csv_sep);
	}
	if (mem_caps->split) {
		fprintf(f, "split%c", csv_sep);
	}
	if (mem_caps->tx_vfo) {
		fprintf(f, "tx_vfo%c", csv_sep);
	}
	if (mem_caps->rptr_shift) {
		fprintf(f, "rptr_shift%c", csv_sep);
	}
	if (mem_caps->rptr_offs) {
		fprintf(f, "rptr_offs%c", csv_sep);
	}
	if (mem_caps->tuning_step) {
		fprintf(f, "tuning_step%c", csv_sep);
	}
	if (mem_caps->rit) {
		fprintf(f, "rit%c", csv_sep);
	}
	if (mem_caps->xit) {
		fprintf(f, "xit%c", csv_sep);
	}
	if (mem_caps->funcs) {
		fprintf(f, "funcs%c", csv_sep);
	}
	if (mem_caps->ctcss_tone) {
		fprintf(f, "ctcss_tone%c", csv_sep);
	}
	if (mem_caps->ctcss_sql) {
		fprintf(f, "ctcss_sql%c", csv_sep);
	}
	if (mem_caps->dcs_code) {
		fprintf(f, "dcs_code%c", csv_sep);
	}
	if (mem_caps->dcs_sql) {
		fprintf(f, "dcs_sql%c", csv_sep);
	}
	if (mem_caps->scan_group) {
		fprintf(f, "scan_group%c", csv_sep);
	}
	if (mem_caps->flags) {
		fprintf(f, "flags%c", csv_sep);
	}
	fprintf(f, "\n");
}


int dump_csv_chan(RIG *rig, channel_t **chan_pp, int channel_num, const chan_t *chan_list, rig_ptr_t arg)
{
	FILE *f = arg;
	static channel_t chan;
	static int first_time = 1;
	const channel_cap_t *mem_caps = &chan_list->mem_caps;

	if (first_time) {
		dump_csv_name(mem_caps, f);
		first_time = 0;
	}

	if (*chan_pp == NULL) {
		/*
		 * Hamlib frontend demand application an allocated 
		 * channel_t pointer for next round.
		 */
		*chan_pp = &chan;

		return RIG_OK;
	}
	
	fprintf(f,"%d%c",chan.channel_num, csv_sep);

	if (mem_caps->bank_num) {
		fprintf(f,"%d%c",chan.bank_num, csv_sep);
	}

	if (mem_caps->channel_desc) {
		fprintf(f, "%s%c", chan.channel_desc, csv_sep);
	}
	if (mem_caps->vfo) {
		fprintf(f,"%s%c",rig_strvfo(chan.vfo), csv_sep);
	}
	if (mem_caps->ant) {
		fprintf(f,"%d%c",chan.ant, csv_sep);
	}
	if (mem_caps->freq) {
		fprintf(f,"%.0"PRIfreq"%c",chan.freq, csv_sep);
	}
	if (mem_caps->mode) {
		fprintf(f, "%s%c", rig_strrmode(chan.mode), csv_sep);
	}
	if (mem_caps->width) {
		fprintf(f,"%d%c",(int)chan.width, csv_sep);
	}
	if (mem_caps->tx_freq) {
		fprintf(f,"%.0"PRIfreq"%c",chan.tx_freq, csv_sep);
	}
	if (mem_caps->tx_mode) {
		fprintf(f, "%s%c", rig_strrmode(chan.tx_mode), csv_sep);
	}
	if (mem_caps->tx_width) {
		fprintf(f,"%d%c",(int)chan.tx_width, csv_sep);
	}
	if (mem_caps->split) {
		fprintf(f, "%s%c", chan.split==RIG_SPLIT_ON?"on":"off", csv_sep);
	}
	if (mem_caps->tx_vfo) {
		fprintf(f,"%s%c",rig_strvfo(chan.tx_vfo), csv_sep);
	}
	if (mem_caps->rptr_shift) {
		fprintf(f, "%s%c", rig_strptrshift(chan.rptr_shift), csv_sep);
	}
	if (mem_caps->rptr_offs) {
		fprintf(f,"%d%c",(int)chan.rptr_offs, csv_sep);
	}
	if (mem_caps->tuning_step) {
		fprintf(f,"%d%c",(int)chan.tuning_step, csv_sep);
	}
	if (mem_caps->rit) {
		fprintf(f,"%d%c",(int)chan.rit, csv_sep);
	}
	if (mem_caps->xit) {
		fprintf(f,"%d%c",(int)chan.xit, csv_sep);
	}
	if (mem_caps->funcs) {
		fprintf(f,"%lx%c",chan.funcs, csv_sep);
	}
	if (mem_caps->ctcss_tone) {
		fprintf(f,"%d%c",chan.ctcss_tone, csv_sep);
	}
	if (mem_caps->ctcss_sql) {
		fprintf(f,"%d%c",chan.ctcss_sql, csv_sep);
	}
	if (mem_caps->dcs_code) {
		fprintf(f,"%d%c",chan.dcs_code, csv_sep);
	}
	if (mem_caps->dcs_sql) {
		fprintf(f,"%d%c",chan.dcs_sql, csv_sep);
	}
	if (mem_caps->scan_group) {
		fprintf(f,"%d%c",chan.scan_group, csv_sep);
	}
	if (mem_caps->flags) {
		fprintf(f,"%x%c",chan.flags, csv_sep);
	}
	fprintf(f,"\n");

	/*
	 * keep the same *chan_pp for next round, thanks
	 * to chan being static
	 */


	return RIG_OK;
}

