/*
 * memsave.c - Copyright (C) 2003-2005 Thierry Leconte
 *
 *
 *	$Id: memsave.c,v 1.10 2006-10-07 19:56:57 csete Exp $  
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

static int dump_xml_chan(RIG *rig, channel_t **chan, int channel_num, const chan_t *chan_list, rig_ptr_t arg);
#endif

int xml_save (RIG *rig, const char *outfilename)
{ 
#ifdef HAVE_XML2
	int retval;
	xmlDocPtr Doc;
	xmlNodePtr root;

	/* create xlm Doc */
	Doc=xmlNewDoc((unsigned char *) "1.0");
	root=xmlNewNode(NULL,(unsigned char *) "hamlib");
	xmlDocSetRootElement(Doc, root);
	root=xmlNewChild(root,NULL,(unsigned char *) "channels",NULL);


	if (rig->caps->clone_combo_get)
		printf("About to save data, enter cloning mode: %s\n",
				rig->caps->clone_combo_get);

	retval = rig_get_chan_all_cb (rig, dump_xml_chan, root);
	if (retval != RIG_OK)
		return retval;

	/* write xml File */

	xmlSaveFormatFileEnc(outfilename, Doc, "UTF-8", 1);

        xmlFreeDoc(Doc);
	xmlCleanupParser();
 
	return 0;
#else
	return -RIG_ENAVAIL;
#endif
}

int xml_parm_save (RIG *rig, const char *outfilename)
{
	return -RIG_ENIMPL;
}



#ifdef HAVE_XML2
int dump_xml_chan(RIG *rig, channel_t **chan_pp, int chan_num, const chan_t *chan_list, rig_ptr_t arg)
{
	char attrbuf[20];
	xmlNodePtr root = arg;
	xmlNodePtr node = NULL;

	static channel_t chan;
	const channel_cap_t *mem_caps = &chan_list->mem_caps;


	if (*chan_pp == NULL) {
		/*
		 * Hamlib frontend demand application an allocated 
		 * channel_t pointer for next round.
		 */
		*chan_pp = &chan;

		return RIG_OK;
	}
	
	switch (chan_list->type) {
		case   RIG_MTYPE_NONE :
			return RIG_OK;
		case  RIG_MTYPE_MEM:
			node=xmlNewChild(root,NULL,(unsigned char *) "mem",NULL);
			break;
		case  RIG_MTYPE_EDGE:
			node=xmlNewChild(root,NULL,(unsigned char *) "edge",NULL);
			break;
		case  RIG_MTYPE_CALL:
			node=xmlNewChild(root,NULL,(unsigned char *) "call",NULL);
			break;
		case  RIG_MTYPE_MEMOPAD:
			node=xmlNewChild(root,NULL,(unsigned char *) "memopad",NULL);
			break;
		case  RIG_MTYPE_SAT:
			node=xmlNewChild(root,NULL,(unsigned char *) "sat",NULL);
			break;
	}

	if (mem_caps->bank_num) {
		sprintf(attrbuf,"%d",chan.bank_num);
		xmlNewProp(node, (unsigned char *) "bank_num", (unsigned char *) attrbuf);
	}

	sprintf(attrbuf,"%d",chan.channel_num);
	xmlNewProp(node, (unsigned char *) "num", (unsigned char *) attrbuf);

	if (mem_caps->channel_desc && chan.channel_desc[0]!='\0') {
			xmlNewProp(node,
                                   (unsigned char *) "channel_desc",
                                   (unsigned char *) chan.channel_desc);
	}
	if (mem_caps->vfo) {
		sprintf(attrbuf,"%d",chan.vfo);
		xmlNewProp(node, (unsigned char *) "vfo", (unsigned char *) attrbuf);
	}
	if (mem_caps->ant && chan.ant != RIG_ANT_NONE) {
		sprintf(attrbuf,"%d",chan.ant);
		xmlNewProp(node, (unsigned char *) "ant", (unsigned char *) attrbuf);
	}
	if (mem_caps->freq && chan.freq != RIG_FREQ_NONE) {
		sprintf(attrbuf,"%"PRIll,(long long)chan.freq);
		xmlNewProp(node, (unsigned char *) "freq", (unsigned char *) attrbuf);
	}
	if (mem_caps->mode && chan.mode != RIG_MODE_NONE) {
		xmlNewProp(node, (unsigned char *) "mode", (unsigned char *) rig_strrmode(chan.mode));
	}
	if (mem_caps->width && chan.width != 0) {
		sprintf(attrbuf,"%d",(int)chan.width);
		xmlNewProp(node, (unsigned char *) "width", (unsigned char *) attrbuf);
	}
	if (mem_caps->tx_freq && chan.tx_freq != RIG_FREQ_NONE) {
		sprintf(attrbuf,"%"PRIll,(long long)chan.tx_freq);
		xmlNewProp(node, (unsigned char *) "tx_freq", (unsigned char *) attrbuf);
	}
	if (mem_caps->tx_mode && chan.tx_mode != RIG_MODE_NONE) {
		xmlNewProp(node,
                           (unsigned char *) "tx_mode",
                           (unsigned char *) rig_strrmode(chan.tx_mode));
	}
	if (mem_caps->tx_width && chan.tx_width!=0) {
		sprintf(attrbuf,"%d",(int)chan.tx_width);
		xmlNewProp(node, (unsigned char *) "tx_width", (unsigned char *) attrbuf);
	}
	if (mem_caps->split && chan.split!=RIG_SPLIT_OFF) {
		xmlNewProp(node, (unsigned char *) "split", (unsigned char *) "on");
		if (mem_caps->tx_vfo) {
				sprintf(attrbuf,"%x",chan.tx_vfo);
				xmlNewProp(node,
                                           (unsigned char *) "tx_vfo",
                                           (unsigned char *) attrbuf);
		}
	}
	if (mem_caps->rptr_shift && chan.rptr_shift!=RIG_RPT_SHIFT_NONE) {
		switch(chan.rptr_shift) {
		case RIG_RPT_SHIFT_NONE:
				xmlNewProp(node,
                                           (unsigned char *) "rptr_shift",
                                           (unsigned char *) "=");
				break;
		case RIG_RPT_SHIFT_PLUS:
				xmlNewProp(node,
                                           (unsigned char *) "rptr_shift",
                                           (unsigned char *) "+");
				break;
		case RIG_RPT_SHIFT_MINUS:
				xmlNewProp(node,
                                           (unsigned char *) "rptr_shift",
                                           (unsigned char *) "-");
				break;
		}
		if (mem_caps->rptr_offs && (int)chan.rptr_offs!=0) {
			sprintf(attrbuf,"%d",(int)chan.rptr_offs);
			xmlNewProp(node,
                                   (unsigned char *) "rptr_offs",
                                   (unsigned char *) attrbuf);
		}
	}
	if (mem_caps->tuning_step && chan.tuning_step !=0) {
		sprintf(attrbuf,"%d",(int)chan.tuning_step);
		xmlNewProp(node, (unsigned char *) "tuning_step", (unsigned char *) attrbuf);
	}
	if (mem_caps->rit && chan.rit!=0) {
		sprintf(attrbuf,"%d",(int)chan.rit);
		xmlNewProp(node, (unsigned char *) "rit", (unsigned char *) attrbuf);
	}
	if (mem_caps->xit && chan.xit !=0) {
		sprintf(attrbuf,"%d",(int)chan.xit);
		xmlNewProp(node, (unsigned char *) "xit", (unsigned char *) attrbuf);
	}
	if (mem_caps->funcs) {
		sprintf(attrbuf,"%llx",(long long)chan.funcs);
		xmlNewProp(node, (unsigned char *) "funcs", (unsigned char *) attrbuf);
	}
	if (mem_caps->ctcss_tone && chan.ctcss_tone !=0) {
		sprintf(attrbuf,"%d",chan.ctcss_tone);
		xmlNewProp(node, (unsigned char *) "ctcss_tone", (unsigned char *) attrbuf);
	}
	if (mem_caps->ctcss_sql && chan.ctcss_sql !=0) {
		sprintf(attrbuf,"%d",chan.ctcss_sql);
		xmlNewProp(node, (unsigned char *) "ctcss_sql", (unsigned char *) attrbuf);
	}
	if (mem_caps->dcs_code && chan.dcs_code !=0) {
		sprintf(attrbuf,"%d",chan.dcs_code);
		xmlNewProp(node, (unsigned char *) "dcs_code", (unsigned char *) attrbuf);
	}
	if (mem_caps->dcs_sql && chan.dcs_sql !=0) {
		sprintf(attrbuf,"%d",chan.dcs_sql);
		xmlNewProp(node, (unsigned char *) "dcs_sql", (unsigned char *) attrbuf);
	}
	if (mem_caps->scan_group) {
		sprintf(attrbuf,"%d",chan.scan_group);
		xmlNewProp(node, (unsigned char *) "scan_group", (unsigned char *) attrbuf);
	}
	if (mem_caps->flags) {
		sprintf(attrbuf,"%x",chan.flags);
		xmlNewProp(node, (unsigned char *) "flags", (unsigned char *) attrbuf);
	}
	
  return 0;
}
#endif
