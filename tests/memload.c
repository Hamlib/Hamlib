/*
 * memload.c - Copyright (C) 2003 Thierry Leconte
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */


#include <hamlib/config.h>

#include <hamlib/rig.h>
#include "misc.h"

#ifdef HAVE_XML2
#  include <libxml/parser.h>
#  include <libxml/tree.h>

static int set_chan(RIG *rig, channel_t *chan, xmlNodePtr node);
#endif


int xml_load(RIG *my_rig, const char *infilename)
{
#ifdef HAVE_XML2
    xmlDocPtr Doc;
    xmlNodePtr node;

    /* load xlm Doc */
    Doc = xmlParseFile(infilename);

    if (Doc == NULL)
    {
        fprintf(stderr, "xmlParse failed\n");
        exit(2);
    }

    node = xmlDocGetRootElement(Doc);

    if (node == NULL)
    {
        fprintf(stderr, "get root failed\n");
        exit(2);
    }

    if (strcmp((char *) node->name, "hamlib"))
    {
        fprintf(stderr, "no hamlib tag found\n");
        exit(2);
    }

    for (node = node->xmlChildrenNode; node != NULL; node = node->next)
    {
        if (xmlNodeIsText(node))
        {
            continue;
        }

        if (strcmp((char *) node->name, "channels") == 0)
        {
            break;
        }
    }

    if (node == NULL)
    {
        fprintf(stderr, "no channels\n");
        exit(2);
    }

    for (node = node->xmlChildrenNode; node != NULL; node = node->next)
    {
        channel_t chan;
        int status;

        if (xmlNodeIsText(node))
        {
            continue;
        }

        set_chan(my_rig, &chan, node);

        status = rig_set_channel(my_rig, RIG_VFO_NONE, &chan);

        if (status != RIG_OK)
        {
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


int xml_parm_load(RIG *my_rig, const char *infilename)
{
    return -RIG_ENIMPL;
}


#ifdef HAVE_XML2
int set_chan(RIG *rig, channel_t *chan, xmlNodePtr node)
{
    xmlChar *prop;
    int i, n;

    memset(chan, 0, sizeof(channel_t));
    chan->vfo = RIG_VFO_MEM;


    prop = xmlGetProp(node, (unsigned char *) "num");

    if (prop == NULL)
    {
        fprintf(stderr, "no num\n");
        return -1;
    }

    n = chan->channel_num = atoi((char *) prop);

    /* find channel caps */
    for (i = 0; i < HAMLIB_CHANLSTSIZ ; i++)
    {
        if (rig->state.chan_list[i].startc <= n
                && rig->state.chan_list[i].endc >= n)
        {

            break;
        }
    }

    if (i == HAMLIB_CHANLSTSIZ) { return -RIG_EINVAL; }

    fprintf(stderr, "node %d %d\n", n, i);

    if (rig->state.chan_list[i].mem_caps.bank_num)
    {
        prop = xmlGetProp(node, (unsigned char *) "bank_num");

        if (prop != NULL)
        {
            chan->bank_num = atoi((char *) prop);
        }
    }

    if (rig->state.chan_list[i].mem_caps.channel_desc)
    {
        prop = xmlGetProp(node, (unsigned char *) "channel_desc");

        if (prop != NULL)
        {
            strncpy(chan->channel_desc, (char *) prop, 7);
        }
    }

    if (rig->state.chan_list[i].mem_caps.ant)
    {
        prop = xmlGetProp(node, (unsigned char *) "ant");

        if (prop != NULL)
        {
            chan->ant = atoi((char *) prop);
        }
    }

    if (rig->state.chan_list[i].mem_caps.freq)
    {
        prop = xmlGetProp(node, (unsigned char *) "freq");

        if (prop != NULL)
        {
            sscanf((char *) prop, "%"SCNfreq, &chan->freq);
        }
    }

    if (rig->state.chan_list[i].mem_caps.mode)
    {
        prop = xmlGetProp(node, (unsigned char *) "mode");

        if (prop != NULL)
        {
            chan->mode = rig_parse_mode((char *) prop);
        }
    }

    if (rig->state.chan_list[i].mem_caps.width)
    {
        prop = xmlGetProp(node, (unsigned char *) "width");

        if (prop != NULL)
        {
            chan->width = atoi((char *) prop);
        }
    }

    if (rig->state.chan_list[i].mem_caps.tx_freq)
    {
        prop = xmlGetProp(node, (unsigned char *) "tx_freq");

        if (prop != NULL)
        {
            sscanf((char *) prop, "%"SCNfreq, &chan->tx_freq);
        }
    }

    if (rig->state.chan_list[i].mem_caps.tx_mode)
    {
        prop = xmlGetProp(node, (unsigned char *)"tx_mode");

        if (prop != NULL)
        {
            chan->tx_mode = rig_parse_mode((char *) prop);
        }
    }

    if (rig->state.chan_list[i].mem_caps.tx_width)
    {
        prop = xmlGetProp(node, (unsigned char *)"tx_width");

        if (prop != NULL)
        {
            chan->tx_width = atoi((char *) prop);
        }
    }

    if (rig->state.chan_list[i].mem_caps.split)
    {
        chan->split = RIG_SPLIT_OFF;
        prop = xmlGetProp(node, (unsigned char *)"split");

        if (prop != NULL)
        {
            if (strcmp((char *) prop, "on") == 0)
            {
                chan->split = RIG_SPLIT_ON;

                if (rig->state.chan_list[i].mem_caps.tx_vfo)
                {
                    prop = xmlGetProp(node, (unsigned char *)"tx_vfo");

                    if (prop != NULL)
                    {
                        sscanf((char *) prop, "%x", &chan->tx_vfo);
                    }
                }
            }
        }
    }

    if (rig->state.chan_list[i].mem_caps.rptr_shift)
    {
        prop = xmlGetProp(node, (unsigned char *)"rptr_shift");

        if (prop)
        {
            switch (prop[0])
            {
            case '=':
                chan->rptr_shift = RIG_RPT_SHIFT_NONE;
                break;

            case '+':
                chan->rptr_shift = RIG_RPT_SHIFT_PLUS;
                break;

            case '-':
                chan->rptr_shift = RIG_RPT_SHIFT_MINUS;
                break;
            }
        }

        if (rig->state.chan_list[i].mem_caps.rptr_offs
                && chan->rptr_shift != RIG_RPT_SHIFT_NONE)
        {
            prop = xmlGetProp(node, (unsigned char *)"rptr_offs");

            if (prop != NULL)
            {
                chan->rptr_offs = atoi((char *) prop);
            }
        }
    }

    if (rig->state.chan_list[i].mem_caps.tuning_step)
    {
        prop = xmlGetProp(node, (unsigned char *)"tuning_step");

        if (prop != NULL)
        {
            chan->tuning_step = atoi((char *) prop);
        }
    }

    if (rig->state.chan_list[i].mem_caps.rit)
    {
        prop = xmlGetProp(node, (unsigned char *)"rit");

        if (prop != NULL)
        {
            chan->rit = atoi((char *) prop);
        }
    }

    if (rig->state.chan_list[i].mem_caps.xit)
    {
        prop = xmlGetProp(node, (unsigned char *)"xit");

        if (prop != NULL)
        {
            chan->xit = atoi((char *) prop);
        }
    }

    if (rig->state.chan_list[i].mem_caps.funcs)
    {
        prop = xmlGetProp(node, (unsigned char *)"funcs");

        if (prop != NULL)
        {
            sscanf((char *) prop, "%lx", &chan->funcs);
        }
    }

    if (rig->state.chan_list[i].mem_caps.ctcss_tone)
    {
        prop = xmlGetProp(node, (unsigned char *)"ctcss_tone");

        if (prop != NULL)
        {
            chan->ctcss_tone = atoi((char *) prop);
        }
    }

    if (rig->state.chan_list[i].mem_caps.ctcss_sql)
    {
        prop = xmlGetProp(node, (unsigned char *)"ctcss_sql");

        if (prop != NULL)
        {
            chan->ctcss_sql = atoi((char *) prop);
        }
    }

    if (rig->state.chan_list[i].mem_caps.dcs_code)
    {
        prop = xmlGetProp(node, (unsigned char *)"dcs_code");

        if (prop != NULL)
        {
            chan->dcs_code = atoi((char *) prop);
        }
    }

    if (rig->state.chan_list[i].mem_caps.dcs_sql)
    {
        prop = xmlGetProp(node, (unsigned char *)"dcs_sql");

        if (prop != NULL)
        {
            chan->dcs_sql = atoi((char *) prop);
        }
    }

    if (rig->state.chan_list[i].mem_caps.scan_group)
    {
        prop = xmlGetProp(node, (unsigned char *)"scan_group");

        if (prop != NULL)
        {
            chan->scan_group = atoi((char *) prop);
        }
    }

    if (rig->state.chan_list[i].mem_caps.flags)
    {
        prop = xmlGetProp(node, (unsigned char *)"flags");

        if (prop != NULL)
        {
            sscanf((char *) prop, "%x", &chan->flags);
        }
    }


    return 0;
}
#endif
