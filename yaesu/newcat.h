/*
 * hamlib - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *              and the Hamlib Group (hamlib-developer at lists.sourceforge.net)
 *
 * newcat.h - (C) Nate Bargmann 2007 (n0nb at arrl.net)
 *
 * This shared library provides the backend API for communicating
 * via serial interface to any Yaesu radio using the new "CAT"
 * interface commands that are similar to the Kenwood command set.
 *
 * Models this code aims to support are FTDX-9000*, FT-2000,
 * FT-950, FT-450.  Much testing remains.  -N0NB
 *
 *
 *    $Id: newcat.h,v 1.7 2008-11-27 07:46:00 mrtembry Exp $
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 *
 */


#ifndef _NEWCAT_H
#define _NEWCAT_H 1


/* Handy constants */

#ifndef TRUE
#define TRUE 1
#endif
#define ON TRUE
#ifndef FALSE
#define FALSE 0
#endif
#define OFF FALSE

typedef char ncboolean;

/* shared function version */
#define NEWCAT_VER "0.2"

/* Hopefully large enough for future use, 128 chars plus '\0' */
#define NEWCAT_DATA_LEN                 129

/* arbitrary value for now.  11 bits (8N2+1) == 2.2917 mS @ 4800 bps */
#define NEWCAT_DEFAULT_READ_TIMEOUT     (NEWCAT_DATA_LEN * 5)


#define NEWCAT_MEM_CAP {    \
	.freq = 1,      \
	.mode = 1,      \
	.width = 1,     \
	.rit = 1,       \
	.xit = 1,       \
	.rptr_offs = 1, \
	.ctcss_tone = 1,\
	.ctcss_sql = 1,\
	.funcs = (RIG_FUNC_TONE|RIG_FUNC_TSQL), \
}

/*
 * Functions considered to be Stable:
 *
 * Functions considered to be Beta:
 *
 * Functions considered to be Alpha:
 *      newcat_set_freq
 *      newcat_get_freq
 *      newcat_set_vfo
 *      newcat_get_vfo
 *
 * Functions not yet implemented
 * most everything at this time.
 *
 * At this time, CAT documentation for the FT-450 can be obtained from
 * the Yaesu website at: http://www.yaesu.com/downloadFile.cfm?FileID=2600&FileCatID=158&FileName=FT%2D450%5FCAT%5FOperation%5FReference%5FBook.pdf&FileContentType=application%2Fpdf
 *
 */


/*
 * newcat function definitions.
 *
 */

int newcat_init(RIG *rig);
int newcat_cleanup(RIG *rig);
int newcat_open(RIG *rig);
int newcat_close(RIG *rig);

int newcat_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int newcat_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);

int newcat_set_vfo(RIG *rig, vfo_t vfo);
int newcat_get_vfo(RIG *rig, vfo_t *vfo);

ncboolean newcat_valid_command(RIG *rig, char *command);

int newcat_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int newcat_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);

int newcat_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
int newcat_get_ptt(RIG * rig, vfo_t vfo, ptt_t * ptt);
int newcat_set_ant(RIG * rig, vfo_t vfo, ant_t ant);
int newcat_get_ant(RIG * rig, vfo_t vfo, ant_t * ant);
int newcat_set_level(RIG * rig, vfo_t vfo, setting_t level, value_t val);
int newcat_get_level(RIG * rig, vfo_t vfo, setting_t level, value_t * val);
int newcat_set_func(RIG * rig, vfo_t vfo, setting_t func, int status);
int newcat_get_func(RIG * rig, vfo_t vfo, setting_t func, int *status);
int newcat_set_mem(RIG * rig, vfo_t vfo, int ch);
int newcat_get_mem(RIG * rig, vfo_t vfo, int *ch);
int newcat_vfo_op(RIG * rig, vfo_t vfo, vfo_op_t op);
const char *newcat_get_info(RIG * rig);


#endif /* _NEWCAT_H */
