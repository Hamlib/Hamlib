/*
 *  Hamlib Interface - token header
 *  Copyright (c) 2000,2001,2002 by Stephane Fillod
 *
 *		$Id: token.h,v 1.1 2002-01-27 14:48:49 fillods Exp $
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

#ifndef _TOKEN_H
#define _TOKEN_H 1

#include <hamlib/rig.h>

#define TOKEN_BACKEND(t) (t)
#define TOKEN_FRONTEND(t) ((t)|(1<<30))
#define IS_TOKEN_FRONTEND(t) ((t)&(1<<30))

#define TOK_FRONTEND_NONE	TOKEN_FRONTEND(0)
#define TOK_BACKEND_NONE	TOKEN_BACKEND(0)

/*
 * tokens shared among rig and rotator,
 * Numbers go from TOKEN_FRONTEND(1) to TOKEN_FRONTEND(99)
 */
#define TOK_PATHNAME	TOKEN_FRONTEND(10)
#define TOK_WRITE_DELAY	TOKEN_FRONTEND(12)
#define TOK_POST_WRITE_DELAY	TOKEN_FRONTEND(13)
#define TOK_TIMEOUT		TOKEN_FRONTEND(14)
#define TOK_RETRY		TOKEN_FRONTEND(15)

#define TOK_SERIAL_SPEED	TOKEN_FRONTEND(20)
#define TOK_DATA_BITS	TOKEN_FRONTEND(21)
#define TOK_STOP_BITS	TOKEN_FRONTEND(22)
#define TOK_PARITY		TOKEN_FRONTEND(23)
#define TOK_HANDSHAKE	TOKEN_FRONTEND(24)

/*
 * rig specific tokens
 */
/* rx_range_list/tx_range_list, filters, announces, has(func,lvl,..) */

#define TOK_VFO_COMP	TOKEN_FRONTEND(110)
#define TOK_ITU_REGION	TOKEN_FRONTEND(120)

/*
 * rotator specific tokens
 */
#define TOK_MIN_AZ	TOKEN_FRONTEND(110)
#define TOK_MAX_AZ	TOKEN_FRONTEND(111)
#define TOK_MIN_EL	TOKEN_FRONTEND(112)
#define TOK_MAX_EL	TOKEN_FRONTEND(113)


#endif /* _TOKEN_H */
