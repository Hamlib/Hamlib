/*
 *  Hamlib Interface - configuration header
 *  Copyright (c) 2000,2001 by Stephane Fillod and Frank Singleton
 *
 *		$Id: rot_conf.h,v 1.1 2001-12-27 21:46:25 fillods Exp $
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

#ifndef _ROT_CONF_H
#define _ROT_CONF_H 1

#include <hamlib/rotator.h>


int frontrot_set_conf(ROT *rot, token_t token, const char *val);
int frontrot_get_conf(ROT *rot, token_t token, char *val);

#define ROT_TOKEN_FRONTEND RIG_TOKEN_FRONTEND

#define TOK_ROT_PATHNAME	ROT_TOKEN_FRONTEND(10)
#define TOK_WRITE_DELAY	ROT_TOKEN_FRONTEND(12)
#define TOK_POST_WRITE_DELAY	ROT_TOKEN_FRONTEND(13)
#define TOK_TIMEOUT	ROT_TOKEN_FRONTEND(14)
#define TOK_RETRY	ROT_TOKEN_FRONTEND(15)

#define TOK_SERIAL_SPEED	ROT_TOKEN_FRONTEND(30)
#define TOK_DATA_BITS	ROT_TOKEN_FRONTEND(31)
#define TOK_STOP_BITS	ROT_TOKEN_FRONTEND(32)
#define TOK_PARITY	ROT_TOKEN_FRONTEND(33)
#define TOK_HANDSHAKE	ROT_TOKEN_FRONTEND(34)


#endif /* _ROT_CONF_H */

