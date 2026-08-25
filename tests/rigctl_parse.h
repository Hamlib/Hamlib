/*
 * rigctl_parse.h - (C) Stephane Fillod 2000-2010
 *
 * This program test/control a radio using Hamlib.
 * It takes commands in interactive mode as well as
 * from command line options.
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

#ifndef RIGCTL_PARSE_H
#define RIGCTL_PARSE_H

#include <stdio.h>
#include <pthread.h>
#include "hamlib/rig.h"

#define RIGCTL_PARSE_END 1
#define RIGCTL_PARSE_ERROR 2

/*
 * Temporary disable of rigctld/rigctltcp passwords and their help text, so no expctations
 *   of them working
 * Set this to 1 when they are repaired
 */
#define RIGCTLD_PASSWORDS 1

/* Data that ties each thread to a connection
 * Serves as the initial thread data, then as the state info for individual
 *   connections
 */
struct handle_data
{
    RIG *rig;
    int sock;
    struct sockaddr_storage cli_addr;
    socklen_t clilen;
    int vfo_mode;
    int use_password;
    int is_passwordOK;
    int client_id;      /* Identifies the connection owning a stream */
};

extern pthread_key_t thread_data_key;

/*
 * external prototype
 */

int dumpcaps(RIG *, FILE *);
int dumpstate(RIG *, FILE *);
int dumpconf(RIG *, FILE *);
int dumpconf_list(RIG *, FILE *);

/*
 * Prototypes
 */
void usage_rig(FILE *);
void version();
void list_models();
int dump_chan(FILE *, RIG *, channel_t *);
int print_conf_list(const struct confparams *cfp, rig_ptr_t data);
int print_conf_list2(const struct confparams *cfp, rig_ptr_t data);
int set_conf(RIG *my_rig, char *conf_parms);

void rigctl_parse_init(void);
typedef void (*sync_cb_t)(int);
int rigctl_parse(RIG *my_rig, FILE *fin, FILE *fout, char *argv[], int argc, sync_cb_t sync_cb,
                 int interactive, int prompt, int * vfo_mode, char send_cmd_term,
                 int * ext_resp_ptr, char * resp_sep_ptr, int use_password);

#endif  /* RIGCTL_PARSE_H */
