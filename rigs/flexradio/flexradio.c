/*
 *  Hamlib Flexradio backend
 *  Copyright (c) 2003-2012 by Stephane Fillod
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

#include "hamlib/rig.h"
#include "flexradio.h"
#include "register.h"

#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif

#if HAVE_NETDB_H
#  include <netdb.h>
#endif

#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif

#if defined (HAVE_SYS_SOCKET_H) && defined (HAVE_SYS_IOCTL_H)
#  include <sys/socket.h>
#  include <sys/ioctl.h>
#endif

DECLARE_INITRIG_BACKEND(flexradio)
{
    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    rig_register(&sdr1k_rig_caps);
    //rig_register(&sdr1krfe_rig_caps);
    rig_register(&dttsp_rig_caps);
    rig_register(&dttsp_udp_rig_caps);
    rig_register(&smartsdr_a_rig_caps);
    rig_register(&smartsdr_b_rig_caps);
    rig_register(&smartsdr_c_rig_caps);
    rig_register(&smartsdr_d_rig_caps);
    rig_register(&smartsdr_e_rig_caps);
    rig_register(&smartsdr_f_rig_caps);
    rig_register(&smartsdr_g_rig_caps);
    rig_register(&smartsdr_h_rig_caps);

    return RIG_OK;
}

