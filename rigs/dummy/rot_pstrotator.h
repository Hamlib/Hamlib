/*
 *  Hamlib Dummy backend - main header
 *  Copyright (c) 2001-2008 by Stephane Fillod
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

#ifndef _ROT_PSTROTATOR_H
#define _ROT_PSTROTATOR_H 1

#include "rotator.h"
#include "token.h"

/* backend conf */
#define TOK_CFG_ROT_MAGICCONF    TOKEN_BACKEND(1)
#define TOK_CFG_ROT_STATIC_DATA  TOKEN_BACKEND(2)

/* ext_level's and ext_parm's tokens */
#define TOK_EL_ROT_MAGICLEVEL    TOKEN_BACKEND(1)
#define TOK_EL_ROT_MAGICFUNC     TOKEN_BACKEND(2)
#define TOK_EL_ROT_MAGICOP       TOKEN_BACKEND(3)
#define TOK_EP_ROT_MAGICPARM     TOKEN_BACKEND(4)
#define TOK_EL_ROT_MAGICCOMBO    TOKEN_BACKEND(5)
#define TOK_EL_ROT_MAGICEXTFUNC  TOKEN_BACKEND(6)

extern struct rot_caps pstrotator_caps;
extern struct rot_caps netrotctl_caps;

#if defined(HAVE_PTHREAD)
typedef struct pstrotator_handler_args_sw
{
    ROT *rot;
    int port; // port for reading PstRotator messages -- always +1 from base port
} pstrotator_handler_args;

typedef struct pstrotator_handler_priv_data_s
{
    pthread_t thread_id;
    pstrotator_handler_args args;
    int pstrotator_handler_thread_run;
    int sockfd2;

} pstrotator_handler_priv_data;
#endif



#endif /* _ROT_PSTROTATOR_H */
