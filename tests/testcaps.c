/*
 * testcaps.c - Copyright (C) 2001 Stephane Fillod
 * This programs test the capabilities of a backend rig,
 * like the passband info..
 *
 *
 *    $Id: testcaps.c,v 1.2 2001-06-04 21:17:53 f4cfe Exp $  
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


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <hamlib/rig.h>
#include "misc.h"


static char *decode_modes(rmode_t modes);

int main (int argc, char *argv[])
{ 
	const struct rig_caps *caps;
	int status, i;
	char freqbuf[20];
	RIG *pbrig;

	if (argc != 2) {
			fprintf(stderr,"%s <rig_num>\n",argv[0]);
			exit(1);
	}

#if 0
  	status = rig_load_backend("icom");
	status |= rig_load_backend("ft747");
	status |= rig_load_backend("ft847");
	status |= rig_load_backend("kenwood");
	status |= rig_load_backend("aor");
	status |= rig_load_backend("pcr");
	rig_load_backend("winradio");   /* may not be compiled ... */
	rig_load_backend("dummy");

	if (status != RIG_OK ) {
		printf("rig_load_backend: error = %s \n", rigerror(status));
		exit(3);
	}
#endif

	pbrig = rig_init(atoi(argv[1]));
	if (!pbrig) {
			fprintf(stderr,"Unknown rig num: %d\n",atoi(argv[1]));
			fprintf(stderr,"Please check riglist.h\n");
			exit(2);
	}

	caps = pbrig->caps;

	printf("Rig dump for model %d\n",caps->rig_model);
	printf("Model name:\t%s\n",caps->model_name);
	printf("Mfg name:\t%s\n",caps->mfg_name);
	printf("Backend version:\t%s\n",caps->version);


	for (i=1; i < 1<<10; i<<=1) {
		const char *mode = decode_modes(i);

		pbwidth_t pbnorm = rig_passband_normal(pbrig, i);
		if (pbnorm == 0)
				continue;

		freq_sprintf(freqbuf, pbnorm);
		printf("%s normal: %s\n", mode, freqbuf);

		freq_sprintf(freqbuf, rig_passband_narrow(pbrig, i));
		printf("%s narrow: %s\n", mode, freqbuf);

		freq_sprintf(freqbuf, rig_passband_wide(pbrig, i));
		printf("%s wide: %s\n", mode, freqbuf);
	}

	rig_cleanup(pbrig);

	return 0;
}


/*
 * NB: this function is not reentrant, because of the static buf.
 * 		but who cares?  --SF
 */
static char *decode_modes(rmode_t modes)
{
	static char buf[80];

	buf[0] = '\0';
	if (modes&RIG_MODE_AM) strcat(buf,"AM ");
	if (modes&RIG_MODE_CW) strcat(buf,"CW ");
	if (modes&RIG_MODE_USB) strcat(buf,"USB ");
	if (modes&RIG_MODE_LSB) strcat(buf,"LSB ");
	if (modes&RIG_MODE_RTTY) strcat(buf,"RTTY ");
	if (modes&RIG_MODE_FM) strcat(buf,"FM ");
#ifdef RIG_MODE_WFM
	if (modes&RIG_MODE_WFM) strcat(buf,"WFM ");
#endif

	return buf;
}


