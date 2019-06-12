/*
 *  Hamlib Netampctl backend - main file
 *  Copyright (c) 2001-2009 by Stephane Fillod
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

#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <math.h>
#include <errno.h>

#include "hamlib/amplifier.h"
#include "iofunc.h"
#include "misc.h"

#include "amp_dummy.h"

#define CMD_MAX 32
#define BUF_MAX 64

/*
 * Helper function with protocol return code parsing
 */
static int netampctl_transaction(AMP *amp, char *cmd, int len, char *buf)
{
  int ret;

  ret = write_block(&amp->state.ampport, cmd, len);

  if (ret != RIG_OK)
  {
    return ret;
  }

  ret = read_string(&amp->state.ampport, buf, BUF_MAX, "\n", sizeof("\n"));

  if (ret < 0)
  {
    return ret;
  }

  if (!memcmp(buf, NETAMPCTL_RET, strlen(NETAMPCTL_RET)))
  {
    return atoi(buf + strlen(NETAMPCTL_RET));
  }

  return ret;
}

static int netampctl_open(AMP *amp)
{
  int ret, len;
  //struct amp_state *rs = &amp->state;
  int pamp_ver;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);


  len = sprintf(cmd, "\\dump_state\n");

  ret = netampctl_transaction(amp, cmd, len, buf);

  if (ret <= 0)
  {
    return (ret < 0) ? ret : -RIG_EPROTO;
  }

  pamp_ver = atoi(buf);
#define AMPCTLD_PAMP_VER 0

  if (pamp_ver < AMPCTLD_PAMP_VER)
  {
    return -RIG_EPROTO;
  }

  ret = read_string(&amp->state.ampport, buf, BUF_MAX, "\n", sizeof("\n"));

  if (ret <= 0)
  {
    return (ret < 0) ? ret : -RIG_EPROTO;
  }

  do
  {
    ret = read_string(&amp->state.ampport, buf, BUF_MAX, "\n", sizeof("\n"));

    if (ret <= 0)
    {
      return (ret < 0) ? ret : -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s called, string=%s\n", __FUNCTION__, buf);
  }
  while (ret > 0);

  return RIG_OK;
}

static int netampctl_close(AMP *amp)
{
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  /* clean signoff, no read back */
  write_block(&amp->state.ampport, "q\n", 2);

  return RIG_OK;
}

static int netampctl_set_freq(AMP *amp, freq_t freq)
{
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);
  return -RIG_ENIMPL;
}

static int netampctl_get_freq(AMP *amp, freq_t *freq)
{
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);
  *freq = 12345;
  return -RIG_ENIMPL;
}

#if 0
static int netampctl_reset(AMP *amp, amp_reset_t reset)
{
  int ret, len;
  char cmd[CMD_MAX];
  char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "R %d\n", reset);

  ret = netampctl_transaction(amp, cmd, len, buf);

  if (ret > 0)
  {
    return -RIG_EPROTO;
  }
  else
  {
    return ret;
  }
}
#endif

static const char *netampctl_get_info(AMP *amp)
{
  int ret, len;
  char cmd[CMD_MAX];
  static char buf[BUF_MAX];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  len = sprintf(cmd, "_\n");

  ret = netampctl_transaction(amp, cmd, len, buf);

  if (ret < 0)
  {
    return NULL;
  }

  buf [ret] = '\0';

  return buf;
}



/*
 * NET ampctl capabilities.
 */

const struct amp_caps netampctl_caps =
{
  .amp_model =      AMP_MODEL_NETAMPCTL,
  .model_name =     "NET ampctl",
  .mfg_name =       "Hamlib",
  .version =        "0.1",
  .copyright =      "LGPL",
  .status =         RIG_STATUS_ALPHA,
  .amp_type =       AMP_TYPE_OTHER,
  .port_type =      RIG_PORT_NETWORK,
  .timeout = 2000,
  .retry =   3,

  .priv =  NULL,  /* priv */

  /* .amp_init =     netampctl_init, */
  /* .amp_cleanup =  netampctl_cleanup, */
  .amp_open =     netampctl_open,
  .amp_close =    netampctl_close,

  .get_freq = netampctl_get_freq,
  .set_freq = netampctl_set_freq,

  .get_info =      netampctl_get_info
};

