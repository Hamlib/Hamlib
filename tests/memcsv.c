/*
 * memcsv.c - (C) Stephane Fillod 2003-2004
 *
 * This program exercises the backup and restore of a radio
 * using Hamlib. CSV primitives
 *
 * $Id: memcsv.c,v 1.4 2004-05-17 21:09:45 fillods Exp $  
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

/* 
 * Prototypes
 */
static void dump_csv_chan(const channel_cap_t *mem_caps, const channel_t *chan, FILE *f);
static void dump_csv_name(const channel_cap_t *mem_caps, FILE *f);

int csv_save (RIG *rig, const char *outfilename);
int csv_load (RIG *rig, const char *infilename);
int csv_parm_save (RIG *rig, const char *outfilename);
int csv_parm_load (RIG *rig, const char *infilename);



int csv_save (RIG *rig, const char *outfilename)
{
	int i,j,status;
	FILE *f;
	channel_t chan;

	f = fopen(outfilename, "w");
	if (!f)
		return -1;


 	for (i=0; rig->state.chan_list[i].type && i < CHANLSTSIZ; i++) {

		dump_csv_name(&rig->state.chan_list[i].mem_caps, f);

		for (j = rig->state.chan_list[i].start;
						j <= rig->state.chan_list[i].end; j++) {
			chan.vfo = RIG_VFO_MEM;
			chan.channel_num = j;
			status=rig_get_channel(rig, &chan);

			if (status != RIG_OK ) {
				printf("rig_get_channel: error = %s \n", rigerror(status));
				return status;
			}

			dump_csv_chan(&rig->state.chan_list[i].mem_caps, &chan, f);
		}
	}


	fclose(f);
	return 0;
}

int csv_load (RIG *rig, const char *infilename)
{
	return -RIG_ENIMPL;
	/* for every channel, fscanf, parse, set_chan */
}

static int print_parm_name(RIG *rig, const struct confparams *cfp, rig_ptr_t ptr)
{
	fprintf((FILE*)ptr, "%s;", cfp->name);

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
                                  fprintf(f,"%d;", val.i);
                                  break;
                          case RIG_CONF_NUMERIC:
                                  fprintf(f,"%f;", val.f);
                                  break;
                          case RIG_CONF_STRING:
                                  fprintf(f,"%s;", val.s);
                                  break;
                          default:
                                  fprintf(f,"unknown;");
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
		fprintf(f, "%s;", ms);
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
			fprintf(f, "%f;", val.f);
		else
			fprintf(f, "%d;", val.i);
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
	fprintf(f, "num;");

	if (mem_caps->bank_num) {
		fprintf(f, "bank_num;");
	}

	if (mem_caps->channel_desc) {
			fprintf(f, "channel_desc;");
	}
	if (mem_caps->vfo) {
		fprintf(f, "vfo;");
	}
	if (mem_caps->ant) {
		fprintf(f, "ant;");
	}
	if (mem_caps->freq) {
		fprintf(f, "freq;");
	}
	if (mem_caps->mode) {
		fprintf(f, "mode;");
	}
	if (mem_caps->width) {
		fprintf(f, "width;");
	}
	if (mem_caps->tx_freq) {
		fprintf(f, "tx_freq;");
	}
	if (mem_caps->tx_mode) {
		fprintf(f, "tx_mode;");
	}
	if (mem_caps->tx_width) {
		fprintf(f, "tx_width;");
	}
	if (mem_caps->split) {
		fprintf(f, "split;");
	}
	if (mem_caps->tx_vfo) {
		fprintf(f, "tx_vfo;");
	}
	if (mem_caps->rptr_shift) {
		fprintf(f, "rptr_shift;");
	}
	if (mem_caps->rptr_offs) {
		fprintf(f, "rptr_offs;");
	}
	if (mem_caps->tuning_step) {
		fprintf(f, "tuning_step;");
	}
	if (mem_caps->rit) {
		fprintf(f, "rit;");
	}
	if (mem_caps->xit) {
		fprintf(f, "xit;");
	}
	if (mem_caps->funcs) {
		fprintf(f, "funcs;");
	}
	if (mem_caps->ctcss_tone) {
		fprintf(f, "ctcss_tone;");
	}
	if (mem_caps->ctcss_sql) {
		fprintf(f, "ctcss_sql;");
	}
	if (mem_caps->dcs_code) {
		fprintf(f, "dcs_code;");
	}
	if (mem_caps->dcs_sql) {
		fprintf(f, "dcs_sql;");
	}
	if (mem_caps->scan_group) {
		fprintf(f, "scan_group;");
	}
	if (mem_caps->flags) {
		fprintf(f, "flags;");
	}
	fprintf(f, "\n");
}


void dump_csv_chan(const channel_cap_t *mem_caps, const channel_t *chan, FILE *f)
{
	fprintf(f,"%d;",chan->channel_num);

	if (mem_caps->bank_num) {
		fprintf(f,"%d;",chan->bank_num);
	}

	if (mem_caps->channel_desc) {
		fprintf(f, "%s;", chan->channel_desc);
	}
	if (mem_caps->vfo) {
		fprintf(f,"%s;",rig_strvfo(chan->vfo));
	}
	if (mem_caps->ant) {
		fprintf(f,"%d;",chan->ant);
	}
	if (mem_caps->freq) {
		fprintf(f,"%"FREQFMT";",chan->freq);
	}
	if (mem_caps->mode) {
		fprintf(f, "%s;", rig_strrmode(chan->mode));
	}
	if (mem_caps->width) {
		fprintf(f,"%d;",(int)chan->width);
	}
	if (mem_caps->tx_freq) {
		fprintf(f,"%"FREQFMT";",chan->tx_freq);
	}
	if (mem_caps->tx_mode) {
		fprintf(f, "%s;", rig_strrmode(chan->tx_mode));
	}
	if (mem_caps->tx_width) {
		fprintf(f,"%d;",(int)chan->tx_width);
	}
	if (mem_caps->split) {
		fprintf(f, "%s;", chan->split==RIG_SPLIT_ON?"on":"off");
	}
	if (mem_caps->tx_vfo) {
		fprintf(f,"%s;",rig_strvfo(chan->tx_vfo));
	}
	if (mem_caps->rptr_shift) {
		fprintf(f, "%s;", rig_strptrshift(chan->rptr_shift));
	}
	if (mem_caps->rptr_offs) {
		fprintf(f,"%d",(int)chan->rptr_offs);
	}
	if (mem_caps->tuning_step) {
		fprintf(f,"%d;",(int)chan->tuning_step);
	}
	if (mem_caps->rit) {
		fprintf(f,"%d;",(int)chan->rit);
	}
	if (mem_caps->xit) {
		fprintf(f,"%d;",(int)chan->xit);
	}
	if (mem_caps->funcs) {
		fprintf(f,"%llx;",(long long)chan->funcs);
	}
	if (mem_caps->ctcss_tone) {
		fprintf(f,"%d;",chan->ctcss_tone);
	}
	if (mem_caps->ctcss_sql) {
		fprintf(f,"%d;",chan->ctcss_sql);
	}
	if (mem_caps->dcs_code) {
		fprintf(f,"%d;",chan->dcs_code);
	}
	if (mem_caps->dcs_sql) {
		fprintf(f,"%d;",chan->dcs_sql);
	}
	if (mem_caps->scan_group) {
		fprintf(f,"%d;",chan->scan_group);
	}
	if (mem_caps->flags) {
		fprintf(f,"%x;",chan->flags);
	}
	fprintf(f,"\n");
}

