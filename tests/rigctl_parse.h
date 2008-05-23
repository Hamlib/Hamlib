/*
 * rigctl_parse.h - (C) Stephane Fillod 2000-2008
 *
 * This program test/control a radio using Hamlib.
 * It takes commands in interactive mode as well as 
 * from command line options.
 *
 * $Id: rigctl_parse.h,v 1.3 2008-05-23 14:26:09 fillods Exp $  
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

#ifndef RIGCTL_PARSE_H
#define RIGCTL_PARSE_H

#include <stdio.h>
#include <hamlib/rig.h>

/* 
 * external prototype
 */

int dumpcaps (RIG *, FILE *);
int dumpconf (RIG *, FILE *);

/* 
 * Prototypes
 */
void usage_rig(FILE *);
void version();
void list_models();
void dump_chan(FILE *, RIG*, channel_t*);
int print_conf_list(const struct confparams *cfp, rig_ptr_t data);
int set_conf(RIG *my_rig, char *conf_parms);

int rigctl_parse(RIG *my_rig, FILE *fin, FILE *fout, char *argv[], int argc);

#endif	/* RIGCTL_PARSE_H */
