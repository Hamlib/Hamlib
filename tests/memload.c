/*
 * memload.c - Copyright (C) 2003 Thierry Leconte
 *
 *
 *	$Id: memload.c,v 1.1 2003-12-04 23:15:02 fillods Exp $  
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

static int set_chan(RIG *rig, channel_t *chan ,xmlNodePtr node);
#endif


int xml_load (RIG *my_rig, const char *infilename)
{ 
#ifdef HAVE_XML2
	int c,m=0;
	xmlDocPtr Doc;
	xmlNodePtr node;

	/* load xlm Doc */
	Doc=xmlParseFile(infilename);
	if(Doc==NULL) {
		fprintf(stderr,"xmlParse failed\n");
		exit(2);
	}

	node=xmlDocGetRootElement(Doc);
	if (node == NULL) {
		fprintf(stderr,"get root failed\n");
		exit(2);
        }
	if(strcmp(node->name, (const xmlChar *) "hamlib")) {
		fprintf(stderr,"no hamlib tag found\n");
		exit(2);
	}
	for(node=node->xmlChildrenNode;node!=NULL;node=node->next) {
		if(xmlNodeIsText(node)) continue;
		if(strcmp(node->name, (const xmlChar *) "channels")==0)
			break;
	}
	if(node==NULL) {
		fprintf(stderr,"no channels\n");
		exit(2);
	}
	for(node=node->xmlChildrenNode;node!=NULL;node=node->next) {
		channel_t chan;
		int status;

		if(xmlNodeIsText(node)) continue;

		set_chan(my_rig,&chan,node);

		status=rig_set_channel(my_rig, &chan);

		if (status != RIG_OK ) {
			printf("rig_get_channel: error = %s \n", rigerror(status));
			return status;
		}
	}

        xmlFreeDoc(Doc);
	xmlCleanupParser();

	return 0;
#else
	return -RIG_ENAVAIL;
#endif
}

int xml_parm_load (RIG *my_rig, const char *infilename)
{ 
	return -RIG_ENIMPL;
}

#ifdef HAVE_XML2
int set_chan(RIG *rig, channel_t *chan, xmlNodePtr node)
{
	xmlChar *prop;
	int i,n;

	memset(chan,0,sizeof(channel_t));
	chan->vfo = RIG_VFO_MEM;


	prop=xmlGetProp(node,"num");
	if(prop==NULL) {
		fprintf(stderr,"no num\n");
		return -1;
	}
	n=chan->channel_num = atoi(prop);

	/* find chanel caps */
	for(i=0;i<CHANLSTSIZ ;i++)
		if (rig->state.chan_list[i].start<=n && rig->state.chan_list[i].end>=n) 
			break;

	fprintf(stderr,"node %d %d\n",n,i);

	if (rig->state.chan_list[i].mem_caps.bank_num) {
		prop=xmlGetProp(node, "bank_num");
		if(prop!=NULL) 
			chan->bank_num = atoi(prop);
	}

	if (rig->state.chan_list[i].mem_caps.channel_desc) {
			prop=xmlGetProp(node, "channel_desc");
			if(prop!=NULL) 
				strncpy(chan->channel_desc,prop,7);
	}

	if (rig->state.chan_list[i].mem_caps.ant) {
		prop=xmlGetProp(node, "ant");
		if(prop!=NULL) 
			chan->ant = atoi(prop);
	}
	if (rig->state.chan_list[i].mem_caps.freq) {
		prop=xmlGetProp(node, "freq");
		if(prop!=NULL) 
			sscanf(prop,"%"FREQFMT,&chan->freq);
	}
	if (rig->state.chan_list[i].mem_caps.mode) {
		prop=xmlGetProp(node, "mode");
		if(prop!=NULL)
			chan->mode = parse_mode(prop);
	}
	if (rig->state.chan_list[i].mem_caps.width) {
		prop=xmlGetProp(node, "width");
		if(prop!=NULL) 
			chan->width = atoi(prop);
	}
	if (rig->state.chan_list[i].mem_caps.tx_freq) {
		prop=xmlGetProp(node, "tx_freq");
		if(prop!=NULL) 
			sscanf(prop,"%"FREQFMT,&chan->tx_freq);
	}
	if (rig->state.chan_list[i].mem_caps.tx_mode) {
		prop=xmlGetProp(node, "tx_mode");
		if(prop!=NULL)
			chan->tx_mode = parse_mode(prop);
	}
	if (rig->state.chan_list[i].mem_caps.tx_width) {
		prop=xmlGetProp(node, "tx_width");
		if(prop!=NULL) 
			chan->tx_width = atoi(prop);
	}
	if (rig->state.chan_list[i].mem_caps.split) {
		prop=xmlGetProp(node, "split");
		if(strcmp(prop,"off")==0) chan->split=RIG_SPLIT_OFF;
		if(strcmp(prop,"on")==0) {
				chan->split=RIG_SPLIT_ON;
				if (rig->state.chan_list[i].mem_caps.tx_vfo) {
					prop=xmlGetProp(node, "tx_vfo");
					if(prop!=NULL)
						sscanf(prop,"%x",&chan->tx_vfo);
				}
		}
	}
	if (rig->state.chan_list[i].mem_caps.rptr_shift) {
		prop=xmlGetProp(node, "rptr_shift");
		if(prop)
		switch(prop[0]) {
		case '=': chan->rptr_shift=RIG_RPT_SHIFT_NONE;
				break;
		case '+': chan->rptr_shift=RIG_RPT_SHIFT_PLUS;
				break;
		case '-': chan->rptr_shift=RIG_RPT_SHIFT_MINUS;
				break;
		}
		if (rig->state.chan_list[i].mem_caps.rptr_offs && chan->rptr_shift!=RIG_RPT_SHIFT_NONE) {
			prop=xmlGetProp(node, "rptr_offs");
			if(prop!=NULL) 
				chan->rptr_offs = atoi(prop);
		}	
	}
	if (rig->state.chan_list[i].mem_caps.tuning_step) {
		prop=xmlGetProp(node, "tuning_step");
		if(prop!=NULL) 
			chan->tuning_step = atoi(prop);
	}
	if (rig->state.chan_list[i].mem_caps.rit) {
		prop=xmlGetProp(node, "rit");
		if(prop!=NULL) 
			chan->rit = atoi(prop);
	}
	if (rig->state.chan_list[i].mem_caps.xit) {
		prop=xmlGetProp(node, "xit");
		if(prop!=NULL) 
			chan->xit = atoi(prop);
	}
	if (rig->state.chan_list[i].mem_caps.funcs) {
		prop=xmlGetProp(node, "funcs");
		if(prop!=NULL) 
			sscanf(prop,"%llx",&chan->funcs);
	}
	if (rig->state.chan_list[i].mem_caps.ctcss_tone) {
		prop=xmlGetProp(node, "ctcss_tone");
		if(prop!=NULL) 
			chan->ctcss_tone = atoi(prop);
	}
	if (rig->state.chan_list[i].mem_caps.ctcss_sql) {
		prop=xmlGetProp(node, "ctcss_sql");
		if(prop!=NULL) 
			chan->ctcss_sql = atoi(prop);
	}
	if (rig->state.chan_list[i].mem_caps.dcs_code) {
		prop=xmlGetProp(node, "dcs_code");
		if(prop!=NULL) 
			chan->dcs_code = atoi(prop);
	}
	if (rig->state.chan_list[i].mem_caps.dcs_sql) {
		prop=xmlGetProp(node, "dcs_sql");
		if(prop!=NULL) 
			chan->dcs_sql = atoi(prop);
	}
	if (rig->state.chan_list[i].mem_caps.scan_group) {
		prop=xmlGetProp(node, "scan_group");
		if(prop!=NULL) 
			chan->scan_group = atoi(prop);
	}
	if (rig->state.chan_list[i].mem_caps.flags) {
		prop=xmlGetProp(node, "flags");
		if(prop!=NULL) 
			sscanf(prop,"%x",&chan->flags);
	}

	
  return 0;
}
#endif

