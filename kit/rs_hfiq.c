/*
 *  Hamlib rs-hfiq backend - main file
 *  Copyright (c) 2017 by Volker Schroer
 *
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
/*
 *
 * For informations about the controls see
 * https://sites.google.com/site/rshfiqtransceiver/home/technical-data/interface-commands
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */

#include "hamlib/rig.h"
#include "serial.h"


static int rshfiq_open(RIG *rig)
{
 int retval;
 int flag;
 char versionstr[20];
 char stopset[2];
 stopset[0]='\r';
 stopset[1]='\n';

 rig_debug(RIG_DEBUG_TRACE, "%s: Port = %s\n", __func__,rig->state.rigport.pathname );
 rig->state.rigport.timeout=2000;
 rig->state.rigport.retry=1;
 retval=serial_open(&rig->state.rigport);
 if( retval != RIG_OK)
  return retval;
 retval=ser_get_dtr(&rig->state.rigport, &flag);
 if(retval == RIG_OK)
   rig_debug(RIG_DEBUG_TRACE, "%s: DTR: %d\n", __func__,flag);
 else
  rig_debug(RIG_DEBUG_TRACE, "%s: Could not get DTR\n", __func__);
 if(flag == 0)
  {
   flag=1; // Set DTR
   retval=ser_set_dtr(&rig->state.rigport, flag);
   if(retval == RIG_OK)
     rig_debug(RIG_DEBUG_TRACE, "%s: set DTR\n", __func__);
  }
 serial_flush(&rig->state.rigport);

 snprintf(versionstr,sizeof(versionstr),"*w\r");
 rig_debug(RIG_DEBUG_TRACE, "%s: cmdstr = %s\n", __func__, versionstr);
 retval = write_block(&rig->state.rigport, versionstr, strlen(versionstr));
 if (retval != RIG_OK)
  return retval;
 retval =read_string(&rig->state.rigport,versionstr,20,stopset,2);
 if(retval <=0)
  { // Just one retry if the arduino is just booting
   retval = write_block(&rig->state.rigport, versionstr, strlen(versionstr));
   if (retval != RIG_OK)
    return retval;
   retval =read_string(&rig->state.rigport,versionstr,20,stopset,2);
   if(retval <= 0 )
    return retval;
  }
 versionstr[retval]=0;
 rig_debug(RIG_DEBUG_TRACE, "%s: Rigversion = %s\n", __func__, versionstr);
 if(!strstr(versionstr,"RS-HFIQ"))
  {
   rig_debug(RIG_DEBUG_WARN, "%s: Invalid Rigversion: %s\n",__func__, versionstr);
   return -RIG_ECONF;
  }
 return RIG_OK;
}

static int rshfiq_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
  char fstr[9];
  char cmdstr[15];
  int retval;

  snprintf(fstr,sizeof(fstr), "%lu", (unsigned long int)(freq));
  rig_debug(RIG_DEBUG_TRACE,"%s called: %s %s\n", __FUNCTION__,
 			rig_strvfo(vfo), fstr);

  serial_flush(&rig->state.rigport);

  snprintf(cmdstr, sizeof(cmdstr), "*f%lu\r",(unsigned long int)(freq));

  retval = write_block(&rig->state.rigport, cmdstr, strlen(cmdstr));
  if (retval != RIG_OK)
	return retval;

  return RIG_OK;
}
static int rshfiq_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
 char cmdstr[15];
 char stopset[2];
 stopset[0]='\r';
 stopset[1]='\n';
 int retval;
 serial_flush(&rig->state.rigport);

 snprintf(cmdstr, sizeof(cmdstr),"*f?\r");

 rig_debug(RIG_DEBUG_TRACE, "%s: cmdstr = %s\n", __func__, cmdstr);

 retval = write_block(&rig->state.rigport, cmdstr, strlen(cmdstr));
 if (retval != RIG_OK)
  return retval;
 retval =read_string(&rig->state.rigport,cmdstr,9,stopset,2);
 if(retval <=0)
  return retval;
 cmdstr[retval]=0;
 *freq = atoi(cmdstr);
 if( *freq == 0 )  // fldigi interprets zero frequency as error
 	*freq=1;   // so return 1 ( freq= 1Hz )
 return RIG_OK;
}

static int rshfiq_set_ptt(RIG *rig, vfo_t vfo,ptt_t ptt)
{
 char cmdstr[5];
 cmdstr[0]='*';
 cmdstr[1]='x';
 cmdstr[3]='\r';
 cmdstr[4]=0;
 int retval;
 if(ptt == RIG_PTT_ON)
  cmdstr[2]='1';
 else
  cmdstr[2]='0';
 rig_debug(RIG_DEBUG_TRACE, "%s: cmdstr = %s\n", __func__, cmdstr);

 retval = write_block(&rig->state.rigport, cmdstr, strlen(cmdstr));
 return retval;
}

const struct rig_caps rshfiq_caps = {
  .rig_model =      RIG_MODEL_RSHFIQ,
  .model_name =     "RS-HFIQ",
  .mfg_name =       "HobbyPCB",
  .version =        "0.1",
  .copyright =    	"LGPL",
  .status =         RIG_STATUS_BETA,
  .rig_type =       RIG_TYPE_TRANSCEIVER,
  .ptt_type =       RIG_PTT_RIG,
  .port_type =      RIG_PORT_SERIAL,
  .dcd_type =       RIG_DCD_NONE,
  .serial_rate_min =  57600,
  .serial_rate_max =  57600,
  .serial_data_bits =  8,
  .serial_stop_bits =  1,
  .serial_parity =  RIG_PARITY_NONE,
  .serial_handshake =  RIG_HANDSHAKE_NONE,
  .write_delay =  0,
  .post_write_delay =  1,
  .timeout =  1000,
  .retry =  3,

  .rx_range_list1 =  { {.startf=kHz(3500),.endf=MHz(30),.modes=RIG_MODE_NONE, .low_power=-1,.high_power=-1,RIG_VFO_A},
		    RIG_FRNG_END, },
  .rx_range_list2 =  { {.startf=kHz(3500),.endf=MHz(30),.modes=RIG_MODE_NONE, .low_power=-1,.high_power=-1,RIG_VFO_A},
       RIG_FRNG_END, },
  .tx_range_list1 =   { {.startf=kHz(3500),.endf=kHz(3800),.modes=RIG_MODE_NONE, .low_power=-1,.high_power=-1,RIG_VFO_A},
                        {.startf=kHz(7000),.endf=kHz(7200),.modes=RIG_MODE_NONE, .low_power=-1,.high_power=-1,RIG_VFO_A},
                        {.startf=kHz(10100),.endf=kHz(10150),.modes=RIG_MODE_NONE, .low_power=-1,.high_power=-1,RIG_VFO_A},
                        {.startf=MHz(14),.endf=kHz(14350),.modes=RIG_MODE_NONE, .low_power=-1,.high_power=-1,RIG_VFO_A},
                        {.startf=MHz(21),.endf=kHz(21450),.modes=RIG_MODE_NONE, .low_power=-1,.high_power=-1,RIG_VFO_A},
                        {.startf=kHz(24890),.endf=kHz(24990),.modes=RIG_MODE_NONE, .low_power=-1,.high_power=-1,RIG_VFO_A},
                        {.startf=MHz(28),.endf=kHz(29700),.modes=RIG_MODE_NONE, .low_power=-1,.high_power=-1,RIG_VFO_A},
		    RIG_FRNG_END, },
  .tuning_steps =  { {RIG_MODE_NONE,Hz(1)}, RIG_TS_END, },

  .rig_open =     rshfiq_open,
  .get_freq =     rshfiq_get_freq,
  .set_freq =     rshfiq_set_freq,
  .set_ptt  =     rshfiq_set_ptt,

};

