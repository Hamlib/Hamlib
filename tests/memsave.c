/*
 * memsave.c - Copyright (C) 2003 Thierry Leconte
 *
 *
 *	$Id: memsave.c,v 1.4 2004-01-21 19:47:15 f4dwv Exp $  
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
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <hamlib/rig.h>
#include "misc.h"

#ifdef HAVE_XML2
#include <libxml/parser.h>
#include <libxml/tree.h>

static int dump_xml_chan(RIG *rig, xmlNodePtr root ,int chn, int chan_num);
#endif

int xml_save (RIG *my_rig, const char *outfilename)
{ 
#ifdef HAVE_XML2
	int i,j;
	xmlDocPtr Doc;
	xmlNodePtr root;

	/* create xlm Doc */
	Doc=xmlNewDoc("1.0");
	root=xmlNewNode(NULL,"hamlib");
	xmlDocSetRootElement(Doc, root);
	root=xmlNewChild(root,NULL,"channels",NULL);


 	for (i=0; my_rig->state.chan_list[i].type && i < CHANLSTSIZ; i++) {
		for (j = my_rig->state.chan_list[i].start;
						j <= my_rig->state.chan_list[i].end; j++) {
			dump_xml_chan(my_rig, root, i,j);
		}
	}

	/* write xml File */

	xmlSaveFormatFileEnc(outfilename, Doc, "UTF-8", 1);

        xmlFreeDoc(Doc);
	xmlCleanupParser();
 
	return 0;
#else
	return -RIG_ENAVAIL;
#endif
}

int xml_parm_save (RIG *my_rig, const char *outfilename)
{
	return -RIG_ENIMPL;
}



#ifdef HAVE_XML2
int dump_xml_chan(RIG *rig, xmlNodePtr root, int i, int chan_num)
{
	channel_t chan;
	int status;
	char attrbuf[20];
	xmlNodePtr node;

	chan.vfo = RIG_VFO_MEM;
	chan.channel_num = chan_num;
	status=rig_get_channel(rig, &chan);

	if (status != RIG_OK ) {
		printf("rig_get_channel: error = %s \n", rigerror(status));
		return status;
	}

	switch (rig->state.chan_list[i].type) {
		case   RIG_MTYPE_NONE :
			return RIG_OK;
		case  RIG_MTYPE_MEM:
			node=xmlNewChild(root,NULL,"mem",NULL);
			break;
		case  RIG_MTYPE_EDGE:
			node=xmlNewChild(root,NULL,"edge",NULL);
			break;
		case  RIG_MTYPE_CALL:
			node=xmlNewChild(root,NULL,"call",NULL);
			break;
		case  RIG_MTYPE_MEMOPAD:
			node=xmlNewChild(root,NULL,"memopad",NULL);
			break;
		case  RIG_MTYPE_SAT:
			node=xmlNewChild(root,NULL,"sat",NULL);
			break;
	}

	if (rig->state.chan_list[i].mem_caps.bank_num) {
		sprintf(attrbuf,"%d",chan.bank_num);
		xmlNewProp(node, "bank_num", attrbuf);
	}

	sprintf(attrbuf,"%d",chan.channel_num);
	xmlNewProp(node, "num", attrbuf);

	if (rig->state.chan_list[i].mem_caps.channel_desc && chan.channel_desc[0]!='\0') {
			xmlNewProp(node, "channel_desc", chan.channel_desc);
	}
	if (rig->state.chan_list[i].mem_caps.vfo) {
		sprintf(attrbuf,"%d",chan.vfo);
		xmlNewProp(node, "vfo", attrbuf);
	}
	if (rig->state.chan_list[i].mem_caps.ant && chan.ant != RIG_ANT_NONE) {
		sprintf(attrbuf,"%d",chan.ant);
		xmlNewProp(node, "ant", attrbuf);
	}
	if (rig->state.chan_list[i].mem_caps.freq && chan.freq != RIG_FREQ_NONE) {
		sprintf(attrbuf,"%lld",(long long)chan.freq);
		xmlNewProp(node, "freq", attrbuf);
	}
	if (rig->state.chan_list[i].mem_caps.mode && chan.mode != RIG_MODE_NONE) {
		xmlNewProp(node, "mode", strrmode(chan.mode));
	}
	if (rig->state.chan_list[i].mem_caps.width && chan.width != 0) {
		sprintf(attrbuf,"%d",(int)chan.width);
		xmlNewProp(node, "width", attrbuf);
	}
	if (rig->state.chan_list[i].mem_caps.tx_freq && chan.tx_freq != RIG_FREQ_NONE) {
		sprintf(attrbuf,"%lld",(long long)chan.tx_freq);
		xmlNewProp(node, "tx_freq", attrbuf);
	}
	if (rig->state.chan_list[i].mem_caps.tx_mode && chan.tx_mode != RIG_MODE_NONE) {
		xmlNewProp(node, "tx_mode", strrmode(chan.tx_mode));
	}
	if (rig->state.chan_list[i].mem_caps.tx_width && chan.tx_width!=0) {
		sprintf(attrbuf,"%d",(int)chan.tx_width);
		xmlNewProp(node, "tx_width", attrbuf);
	}
	if (rig->state.chan_list[i].mem_caps.split && chan.split!=RIG_SPLIT_OFF) {
		xmlNewProp(node, "split", "on");
		if (rig->state.chan_list[i].mem_caps.tx_vfo) {
				sprintf(attrbuf,"%x",chan.tx_vfo);
				xmlNewProp(node, "tx_vfo", attrbuf);
		}
	}
	if (rig->state.chan_list[i].mem_caps.rptr_shift && chan.rptr_shift!=RIG_RPT_SHIFT_NONE) {
		switch(chan.rptr_shift) {
		case RIG_RPT_SHIFT_NONE:
				xmlNewProp(node, "rptr_shift", "=");
				break;
		case RIG_RPT_SHIFT_PLUS:
				xmlNewProp(node, "rptr_shift", "+");
				break;
		case RIG_RPT_SHIFT_MINUS:
				xmlNewProp(node, "rptr_shift", "-");
				break;
		}
		if (rig->state.chan_list[i].mem_caps.rptr_offs) {
			sprintf(attrbuf,"%d",(int)chan.rptr_offs);
			xmlNewProp(node, "rptr_offs", attrbuf);
		}
	}
	if (rig->state.chan_list[i].mem_caps.tuning_step && chan.tuning_step !=0) {
		sprintf(attrbuf,"%d",(int)chan.tuning_step);
		xmlNewProp(node, "tuning_step", attrbuf);
	}
	if (rig->state.chan_list[i].mem_caps.rit && chan.rit!=0) {
		sprintf(attrbuf,"%d",(int)chan.rit);
		xmlNewProp(node, "rit", attrbuf);
	}
	if (rig->state.chan_list[i].mem_caps.xit && chan.xit !=0) {
		sprintf(attrbuf,"%d",(int)chan.xit);
		xmlNewProp(node, "xit", attrbuf);
	}
	if (rig->state.chan_list[i].mem_caps.funcs) {
		sprintf(attrbuf,"%llx",(long long)chan.funcs);
		xmlNewProp(node, "funcs", attrbuf);
	}
	if (rig->state.chan_list[i].mem_caps.ctcss_tone && chan.ctcss_tone !=0) {
		sprintf(attrbuf,"%d",chan.ctcss_tone);
		xmlNewProp(node, "ctcss_tone", attrbuf);
	}
	if (rig->state.chan_list[i].mem_caps.ctcss_sql && chan.ctcss_sql !=0) {
		sprintf(attrbuf,"%d",chan.ctcss_sql);
		xmlNewProp(node, "ctcss_sql", attrbuf);
	}
	if (rig->state.chan_list[i].mem_caps.dcs_code && chan.dcs_code !=0) {
		sprintf(attrbuf,"%d",chan.dcs_code);
		xmlNewProp(node, "dcs_code", attrbuf);
	}
	if (rig->state.chan_list[i].mem_caps.dcs_sql && chan.dcs_sql !=0) {
		sprintf(attrbuf,"%d",chan.dcs_sql);
		xmlNewProp(node, "dcs_sql", attrbuf);
	}
	if (rig->state.chan_list[i].mem_caps.scan_group) {
		sprintf(attrbuf,"%d",chan.scan_group);
		xmlNewProp(node, "scan_group", attrbuf);
	}
	if (rig->state.chan_list[i].mem_caps.flags) {
		sprintf(attrbuf,"%x",chan.flags);
		xmlNewProp(node, "flags", attrbuf);
	}
	
  return 0;
}
#endif

