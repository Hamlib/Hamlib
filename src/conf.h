/*
 *  Hamlib Interface - configuration header
 *  Copyright (c) 2000,2001 by Stephane Fillod and Frank Singleton
 *
 *		$Id: conf.h,v 1.2 2001-12-26 23:45:57 fillods Exp $
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef _CONF_H
#define _CONF_H 1

#include <hamlib/rig.h>


int frontend_set_conf(RIG *rig, token_t token, const char *val);
int frontend_get_conf(RIG *rig, token_t token, char *val);


#define TOK_RIG_PATHNAME	RIG_TOKEN_FRONTEND(10)
#define TOK_WRITE_DELAY	RIG_TOKEN_FRONTEND(12)
#define TOK_POST_WRITE_DELAY	RIG_TOKEN_FRONTEND(13)
#define TOK_TIMEOUT	RIG_TOKEN_FRONTEND(14)
#define TOK_RETRY	RIG_TOKEN_FRONTEND(15)
#define TOK_ITU_REGION	RIG_TOKEN_FRONTEND(20)
#define TOK_VFO_COMP	RIG_TOKEN_FRONTEND(25)

#define TOK_SERIAL_SPEED	RIG_TOKEN_FRONTEND(30)
#define TOK_DATA_BITS	RIG_TOKEN_FRONTEND(31)
#define TOK_STOP_BITS	RIG_TOKEN_FRONTEND(32)
#define TOK_PARITY	RIG_TOKEN_FRONTEND(33)
#define TOK_HANDSHAKE	RIG_TOKEN_FRONTEND(34)

/*     vfo_comp, rx_range_list/tx_range_list, filters, announces, has(func,lvl,..) */

#endif /* _CONF_H */

