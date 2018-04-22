/*
 *  Hamlib Netrigctl backend - main file
 *  Copyright (c) 2001-2010 by Stephane Fillod
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <math.h>
#include <time.h>
#include <errno.h>

#include "hamlib/rig.h"
#include "network.h"
#include "serial.h"
#include "iofunc.h"
#include "misc.h"
#include "num_stdio.h"

#include "dummy.h"

#define CMD_MAX 32
#define BUF_MAX 96

#define CHKSCN1ARG(a) if ((a) != 1) return -RIG_EPROTO; else do {} while(0)

/*
 * Helper function with protocol return code parsing
 */
static int netrigctl_transaction(RIG *rig, char *cmd, int len, char *buf)
{
  int ret;

  /* flush anything in the read buffer before command is sent */
  if (rig->state.rigport.type.rig == RIG_PORT_NETWORK || rig->state.rigport.type.rig == RIG_PORT_UDP_NETWORK) {
        network_flush(&rig->state.rigport);
  } else {
      serial_flush(&rig->state.rigport);
  }

  ret = write_block(&rig->state.rigport, cmd, len);
  if (ret != RIG_OK)
	return ret;

  ret = read_string(&rig->state.rigport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret < 0)
	return ret;

  if (!memcmp(buf, NETRIGCTL_RET, strlen(NETRIGCTL_RET)))
	return atoi(buf+strlen(NETRIGCTL_RET));

  return ret;
}


static int netrigctl_open(RIG *rig)
{
  int ret, len, i;
  struct rig_state *rs = &rig->state;
  int prot_ver;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);


  len = sprintf(cmd, "\\dump_state\n");

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  prot_ver = atoi(buf);
#define RIGCTLD_PROT_VER 0
  if (prot_ver < RIGCTLD_PROT_VER)
	  return -RIG_EPROTO;

  ret = read_string(&rig->state.rigport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  ret = read_string(&rig->state.rigport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  rs->itu_region = atoi(buf);

  for (i=0; i<FRQRANGESIZ; i++) {
	ret = read_string(&rig->state.rigport, buf, BUF_MAX, "\n", sizeof("\n"));
	if (ret <= 0)
		return (ret < 0) ? ret : -RIG_EPROTO;

	ret = num_sscanf(buf, "%"SCNfreq"%"SCNfreq"%x%d%d%x%x",
		&rs->rx_range_list[i].start,
		&rs->rx_range_list[i].end,
		&rs->rx_range_list[i].modes,
		&rs->rx_range_list[i].low_power,
		&rs->rx_range_list[i].high_power,
		&rs->rx_range_list[i].vfo,
		&rs->rx_range_list[i].ant
		);
	if (ret != 7)
		return -RIG_EPROTO;
	if (RIG_IS_FRNG_END(rs->rx_range_list[i]))
		break;
  }
  for (i=0; i<FRQRANGESIZ; i++) {
	ret = read_string(&rig->state.rigport, buf, BUF_MAX, "\n", sizeof("\n"));
	if (ret <= 0)
		return (ret < 0) ? ret : -RIG_EPROTO;

	ret = num_sscanf(buf, "%"SCNfreq"%"SCNfreq"%x%d%d%x%x",
		&rs->tx_range_list[i].start,
		&rs->tx_range_list[i].end,
		&rs->tx_range_list[i].modes,
		&rs->tx_range_list[i].low_power,
		&rs->tx_range_list[i].high_power,
		&rs->tx_range_list[i].vfo,
		&rs->tx_range_list[i].ant
		);
	if (ret != 7)
		return -RIG_EPROTO;
	if (RIG_IS_FRNG_END(rs->tx_range_list[i]))
		break;
  }
  for (i=0; i<TSLSTSIZ; i++) {
	ret = read_string(&rig->state.rigport, buf, BUF_MAX, "\n", sizeof("\n"));
  	if (ret <= 0)
		return (ret < 0) ? ret : -RIG_EPROTO;

	ret = sscanf(buf, "%x%ld",
                                &rs->tuning_steps[i].modes,
                                &rs->tuning_steps[i].ts);
	if (ret != 2)
		return -RIG_EPROTO;
	if (RIG_IS_TS_END(rs->tuning_steps[i]))
		break;
  }

  for (i=0; i<FLTLSTSIZ; i++) {
	ret = read_string(&rig->state.rigport, buf, BUF_MAX, "\n", sizeof("\n"));
  	if (ret <= 0)
		return (ret < 0) ? ret : -RIG_EPROTO;

	ret = sscanf(buf, "%x%ld",
                                &rs->filters[i].modes,
                                &rs->filters[i].width);
	if (ret != 2)
		return -RIG_EPROTO;
	if (RIG_IS_FLT_END(rs->filters[i]))
		break;
  }

#if 0
  /* TODO */
chan_t chan_list[CHANLSTSIZ]; /*!< Channel list, zero ended */
#endif

  ret = read_string(&rig->state.rigport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  rs->max_rit = atol(buf);

  ret = read_string(&rig->state.rigport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  rs->max_xit = atol(buf);

  ret = read_string(&rig->state.rigport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  rs->max_ifshift = atol(buf);

  ret = read_string(&rig->state.rigport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  rs->announces = atoi(buf);

  ret = read_string(&rig->state.rigport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  ret = sscanf (buf, "%d%d%d%d%d%d%d",
		  &rs->preamp[0], &rs->preamp[1],
		  &rs->preamp[2], &rs->preamp[3],
		  &rs->preamp[4], &rs->preamp[5],
		  &rs->preamp[6]);
  if (ret < 0 || ret >= MAXDBLSTSIZ)
	  ret = 0;
  rs->preamp[ret] = RIG_DBLST_END;

  ret = read_string(&rig->state.rigport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  ret = sscanf (buf, "%d%d%d%d%d%d%d",
		  &rs->attenuator[0], &rs->attenuator[1],
		  &rs->attenuator[2], &rs->attenuator[3],
		  &rs->attenuator[4], &rs->attenuator[5],
		  &rs->attenuator[6]);
  if (ret < 0 || ret >= MAXDBLSTSIZ)
	  ret = 0;
  rs->attenuator[ret] = RIG_DBLST_END;

  ret = read_string(&rig->state.rigport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  rs->has_get_func = strtol(buf, NULL, 0);

  ret = read_string(&rig->state.rigport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  rs->has_set_func = strtol(buf, NULL, 0);

  ret = read_string(&rig->state.rigport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  rs->has_get_level = strtol(buf, NULL, 0);
  if (rs->has_get_level & RIG_LEVEL_RAWSTR)
    {
      /* include STRENGTH because the remote rig may be able to
         provide a front end emulation, if it can't then an
         -RIG_EINVAL will be returned */
      rs->has_get_level |= RIG_LEVEL_STRENGTH;
    }

  ret = read_string(&rig->state.rigport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  rs->has_set_level = strtol(buf, NULL, 0);

  ret = read_string(&rig->state.rigport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  rs->has_get_parm = strtol(buf, NULL, 0);

  ret = read_string(&rig->state.rigport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  rs->has_set_parm = strtol(buf, NULL, 0);

#if 0
gran_t level_gran[RIG_SETTING_MAX];   /*!< level granularity */
gran_t parm_gran[RIG_SETTING_MAX];    /*!< parm granularity */
#endif

  for (i=0; i<FRQRANGESIZ && !RIG_IS_FRNG_END(rs->rx_range_list[i]); i++) {
	rs->mode_list |= rs->rx_range_list[i].modes;
	rs->vfo_list |= rs->rx_range_list[i].vfo;
  }
  for (i=0; i<FRQRANGESIZ && !RIG_IS_FRNG_END(rs->tx_range_list[i]); i++) {
	rs->mode_list |= rs->tx_range_list[i].modes;
	rs->vfo_list |= rs->tx_range_list[i].vfo;
  }

  return RIG_OK;
}

static int netrigctl_close(RIG *rig)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  /* clean signoff, no read back */
  write_block(&rig->state.rigport, "q\n", 2);

  return RIG_OK;
}

static int netrigctl_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  len = sprintf(cmd, "F %"FREQFMT"\n", freq);

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;
}

static int netrigctl_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "f\n");

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  CHKSCN1ARG(num_sscanf(buf, "%"SCNfreq, freq));

  return RIG_OK;
}


static int netrigctl_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  len = sprintf(cmd, "M %s %li\n",
  		rig_strrmode(mode), width);

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;
}


static int netrigctl_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "m\n");

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  if (ret > 0 && buf[ret-1]=='\n') buf[ret-1] = '\0';	/* chomp */
  *mode = rig_parse_mode(buf);

  ret = read_string(&rig->state.rigport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  *width = atoi(buf);

  return RIG_OK;
}


static int netrigctl_set_vfo(RIG *rig, vfo_t vfo)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  len = sprintf(cmd, "V %s\n", rig_strvfo(vfo));

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;
}


static int netrigctl_get_vfo(RIG *rig, vfo_t *vfo)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "v\n");

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret == -RIG_ENAVAIL) return ret;
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  if (ret > 0 && buf[ret-1]=='\n') buf[ret-1] = '\0';	/* chomp */
  *vfo = rig_parse_vfo(buf);

  return RIG_OK;
}


static int netrigctl_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  len = sprintf(cmd, "T %d\n", ptt);

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;
}


static int netrigctl_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "t\n");

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  *ptt = atoi(buf);

  return RIG_OK;
}

static int netrigctl_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "\\get_dcd\n");	/* FIXME */

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  *dcd = atoi(buf);

  return RIG_OK;
}


static int netrigctl_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  len = sprintf(cmd, "R %s\n", rig_strptrshift(rptr_shift));

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;
}


static int netrigctl_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "r\n");

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  if (ret > 0 && buf[ret-1]=='\n') buf[ret-1] = '\0';	/* chomp */
  *rptr_shift = rig_parse_rptr_shift(buf);

  return RIG_OK;
}


static int netrigctl_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t rptr_offs)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  len = sprintf(cmd, "O %ld\n", rptr_offs);

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;
}


static int netrigctl_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *rptr_offs)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "o\n");

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  *rptr_offs = atoi(buf);

  return RIG_OK;
}


static int netrigctl_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  len = sprintf(cmd, "C %d\n", tone);

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;
}


static int netrigctl_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "c\n");

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  *tone = atoi(buf);

  return RIG_OK;
}


static int netrigctl_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  len = sprintf(cmd, "D %d\n", code);

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;
}


static int netrigctl_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "d\n");

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  *code = atoi(buf);

  return RIG_OK;
}


static int netrigctl_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  len = sprintf(cmd, "\\set_ctcss_sql %d\n", tone);

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;
}


static int netrigctl_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "\\get_ctcss_sql\n");

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  *tone = atoi(buf);

  return RIG_OK;
}


static int netrigctl_set_dcs_sql(RIG *rig, vfo_t vfo, unsigned int code)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  len = sprintf(cmd, "\\set_dcs_sql %d\n", code);

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;
}


static int netrigctl_get_dcs_sql(RIG *rig, vfo_t vfo, unsigned int *code)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "\\get_dcs_sql\n");

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  *code = atoi(buf);

  return RIG_OK;
}


static int netrigctl_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  len = sprintf(cmd, "I %"FREQFMT"\n", tx_freq);

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;
}


static int netrigctl_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "i\n");

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  CHKSCN1ARG(num_sscanf(buf, "%"SCNfreq, tx_freq));

  return RIG_OK;
}

static int netrigctl_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode, pbwidth_t tx_width)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  len = sprintf(cmd, "X %s %li\n",
  		rig_strrmode(tx_mode), tx_width);

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;
}

static int netrigctl_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode, pbwidth_t *tx_width)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "x\n");

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  if (ret > 0 && buf[ret-1]=='\n') buf[ret-1] = '\0';	/* chomp */
  *tx_mode = rig_parse_mode(buf);

  ret = read_string(&rig->state.rigport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  *tx_width = atoi(buf);

  return RIG_OK;
}

static int netrigctl_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  len = sprintf(cmd, "S %d %s\n", split, rig_strvfo(tx_vfo));

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;
}


static int netrigctl_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "s\n");

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  *split = atoi(buf);

  ret = read_string(&rig->state.rigport, buf, BUF_MAX, "\n", sizeof("\n"));
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;


  if (ret > 0 && buf[ret-1]=='\n') buf[ret-1] = '\0';	/* chomp */
  *tx_vfo = rig_parse_vfo(buf);

  return RIG_OK;
}

static int netrigctl_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  len = sprintf(cmd, "J %ld\n", rit);

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;
}


static int netrigctl_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "j\n");

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  *rit = atoi(buf);

  return RIG_OK;
}


static int netrigctl_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  len = sprintf(cmd, "Z %ld\n", xit);

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;
}


static int netrigctl_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "z\n");

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  *xit = atoi(buf);

  return RIG_OK;
}


static int netrigctl_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  len = sprintf(cmd, "N %ld\n", ts);

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;
}


static int netrigctl_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "n\n");

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  *ts = atoi(buf);

  return RIG_OK;
}


static int netrigctl_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  len = sprintf(cmd, "U %s %i\n", rig_strfunc(func), status);

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;
}


static int netrigctl_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "u %s\n", rig_strfunc(func));

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  *status = atoi(buf);

  return RIG_OK;
}


static int netrigctl_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];
  char lstr[32];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  if (RIG_LEVEL_IS_FLOAT(level))
	sprintf(lstr, "%f", val.f);
  else
	sprintf(lstr, "%d", val.i);

  len = snprintf(cmd, sizeof(cmd), "L %s %s\n", rig_strlevel(level), lstr);

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;

}


static int netrigctl_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "l %s\n", rig_strlevel(level));

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  if (RIG_LEVEL_IS_FLOAT(level))
  	val->f = atof(buf);
  else
  	val->i = atoi(buf);

  return RIG_OK;
}


static int netrigctl_set_powerstat(RIG *rig, powerstat_t status)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  len = sprintf(cmd, "\\set_powerstat %d\n", status);

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;
}


static int netrigctl_get_powerstat(RIG *rig, powerstat_t *status)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "\\get_powerstat\n");

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  *status = atoi(buf);

  return RIG_OK;
}


static int netrigctl_set_parm(RIG *rig, setting_t parm, value_t val)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];
  char pstr[32];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  if (RIG_PARM_IS_FLOAT(parm))
	sprintf(pstr, "%f", val.f);
  else
	sprintf(pstr, "%d", val.i);
  len = snprintf(cmd, sizeof(cmd), "P %s %s\n", rig_strparm(parm), pstr);

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;
}


static int netrigctl_get_parm(RIG *rig, setting_t parm, value_t *val)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "p %s\n", rig_strparm(parm));

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  if (RIG_PARM_IS_FLOAT(parm))
  	val->f = atoi(buf);
  else
  	val->i = atoi(buf);

  return RIG_OK;
}


static int netrigctl_set_ant(RIG *rig, vfo_t vfo, ant_t ant)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  len = sprintf(cmd, "Y %d\n", ant);

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;
}


static int netrigctl_get_ant(RIG *rig, vfo_t vfo, ant_t *ant)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "y\n");

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  *ant = atoi(buf);

  return RIG_OK;
}


static int netrigctl_set_bank(RIG *rig, vfo_t vfo, int bank)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  len = sprintf(cmd, "B %d\n", bank);

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;
}


static int netrigctl_set_mem(RIG *rig, vfo_t vfo, int ch)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  len = sprintf(cmd, "E %d\n", ch);

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;
}


static int netrigctl_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "e\n");

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  *ch = atoi(buf);

  return RIG_OK;
}

static int netrigctl_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  len = sprintf(cmd, "g %s %d\n", rig_strscan(scan), ch);

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;
}

static int netrigctl_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  len = sprintf(cmd, "J %s\n", rig_strvfop(op));

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;
}

static int netrigctl_set_channel(RIG *rig, const channel_t *chan)
{
  return -RIG_ENIMPL;
}


static int netrigctl_get_channel(RIG *rig, channel_t *chan)
{
  return -RIG_ENIMPL;
}


static const char *netrigctl_get_info(RIG *rig)
{
  int ret, len;
  char cmd[CMD_MAX];
  static char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "_\n");

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret < 0)
	return NULL;

  buf [ret] = '\0';

  return buf;
}



static int netrigctl_send_dtmf(RIG *rig, vfo_t vfo, const char *digits)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  len = sprintf(cmd, "\\send_dtmf %s\n", digits);

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;
}

static int netrigctl_recv_dtmf(RIG *rig, vfo_t vfo, char *digits, int *length)
{
  int ret, len;
  char cmd[CMD_MAX];
  static char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "\\recv_dtmf\n");

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret <= 0)
	return (ret < 0) ? ret : -RIG_EPROTO;

  if (ret > *length)
	  ret = *length;
  strncpy(digits, buf, ret);
  *length = ret;
  digits [ret] = '\0';

  return RIG_OK;
}

static int netrigctl_send_morse(RIG *rig, vfo_t vfo, const char *msg)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  len = sprintf(cmd, "\\send_morse %s\n", msg);

  ret = netrigctl_transaction(rig, cmd, len, buf);
  if (ret > 0)
	return -RIG_EPROTO;
  else
  	return ret;
}



/*
 * Netrigctl rig capabilities.
 */

const struct rig_caps netrigctl_caps = {
  .rig_model =      RIG_MODEL_NETRIGCTL,
  .model_name =     "NET rigctl",
  .mfg_name =       "Hamlib",
  .version =        "0.3",
  .copyright =      "LGPL",
  .status =         RIG_STATUS_BETA,
  .rig_type =       RIG_TYPE_OTHER,
  .targetable_vfo = 	 0,
  .ptt_type =       RIG_PTT_RIG_MICDATA,
  .dcd_type =       RIG_DCD_RIG,
  .port_type =      RIG_PORT_NETWORK,
  .timeout = 2000,	/* enough for a network */
  .retry =   3,

  /* following fields updated in rig_state at openning time */
  .has_get_func =   RIG_FUNC_NONE,
  .has_set_func =   RIG_FUNC_NONE,
  .has_get_level =  RIG_LEVEL_NONE,
  .has_set_level =  RIG_LEVEL_NONE,
  .has_get_parm = 	 RIG_PARM_NONE,
  .has_set_parm = 	 RIG_PARM_NONE,

  .level_gran =		 { },
  .ctcss_list = 	 NULL,
  .dcs_list =   	 NULL,
  .chan_list = 	 { },
  .transceive =     RIG_TRN_OFF,
  .attenuator =     { },
  .preamp = 		 { },
  .rx_range_list2 =  { RIG_FRNG_END, },
  .tx_range_list2 =  { RIG_FRNG_END, },
  .tuning_steps =  { },
  .filters =  { RIG_FLT_END, },
  .max_rit = 0,
  .max_xit = 0,
  .max_ifshift = 0,
  .priv =  NULL,

  .rig_init =     NULL,
  .rig_cleanup =  NULL,
  .rig_open =     netrigctl_open,
  .rig_close =    netrigctl_close,

  .set_freq =     netrigctl_set_freq,
  .get_freq =     netrigctl_get_freq,
  .set_mode =     netrigctl_set_mode,
  .get_mode =     netrigctl_get_mode,
  .set_vfo =      netrigctl_set_vfo,
  .get_vfo =      netrigctl_get_vfo,

  .set_powerstat =  netrigctl_set_powerstat,
  .get_powerstat =  netrigctl_get_powerstat,
  .set_level =     netrigctl_set_level,
  .get_level =     netrigctl_get_level,
  .set_func =      netrigctl_set_func,
  .get_func =      netrigctl_get_func,
  .set_parm =      netrigctl_set_parm,
  .get_parm =      netrigctl_get_parm,

  .get_info =      netrigctl_get_info,


  .set_ptt = 	netrigctl_set_ptt,
  .get_ptt = 	netrigctl_get_ptt,
  .get_dcd = 	netrigctl_get_dcd,
  .set_rptr_shift = 	netrigctl_set_rptr_shift,
  .get_rptr_shift = 	netrigctl_get_rptr_shift,
  .set_rptr_offs = 	netrigctl_set_rptr_offs,
  .get_rptr_offs = 	netrigctl_get_rptr_offs,
  .set_ctcss_tone = 	netrigctl_set_ctcss_tone,
  .get_ctcss_tone = 	netrigctl_get_ctcss_tone,
  .set_dcs_code = 	netrigctl_set_dcs_code,
  .get_dcs_code = 	netrigctl_get_dcs_code,
  .set_ctcss_sql = 	netrigctl_set_ctcss_sql,
  .get_ctcss_sql = 	netrigctl_get_ctcss_sql,
  .set_dcs_sql = 	netrigctl_set_dcs_sql,
  .get_dcs_sql = 	netrigctl_get_dcs_sql,
  .set_split_freq = 	netrigctl_set_split_freq,
  .get_split_freq = 	netrigctl_get_split_freq,
  .set_split_mode = 	netrigctl_set_split_mode,
  .get_split_mode = 	netrigctl_get_split_mode,
  .set_split_vfo = 	netrigctl_set_split_vfo,
  .get_split_vfo = 	netrigctl_get_split_vfo,
  .set_rit = 	netrigctl_set_rit,
  .get_rit = 	netrigctl_get_rit,
  .set_xit = 	netrigctl_set_xit,
  .get_xit = 	netrigctl_get_xit,
  .set_ts = 	netrigctl_set_ts,
  .get_ts = 	netrigctl_get_ts,
  .set_ant = 	netrigctl_set_ant,
  .get_ant = 	netrigctl_get_ant,
  .set_bank = 	netrigctl_set_bank,
  .set_mem = 	netrigctl_set_mem,
  .get_mem = 	netrigctl_get_mem,
  .vfo_op = 	netrigctl_vfo_op,
  .scan = 		netrigctl_scan,
  .send_dtmf =  netrigctl_send_dtmf,
  .recv_dtmf =  netrigctl_recv_dtmf,
  .send_morse =  netrigctl_send_morse,
  .set_channel = 	netrigctl_set_channel,
  .get_channel = 	netrigctl_get_channel,
};
