/*
 * hamlib - (C) 2000 Frank Singleton and Stephane Fillod
 *
 * rigctl.c - (C) Stephane Fillod 2000
 *
 * This program test/control a radio using Hamlib.
 * TODO: be more generic and add command line option to run 
 * 		in non-interactive mode
 *
 * $Id: rigctl.c,v 1.2 2000-11-28 22:34:37 f4cfe Exp $  
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

#include <hamlib/rig.h>


#define SERIAL_PORT "/dev/ttyS0"

#define MAXNAMSIZ 40

struct test_table {
		char cmd;
		char name[MAXNAMSIZ];
        int (*test_func)(RIG *rig);
};

static int set_freq(RIG *rig);
static int get_freq(RIG *rig);
static int set_mode(RIG *rig);
static int get_mode(RIG *rig);
static int set_passband(RIG *rig);
static int get_passband(RIG *rig);
static int set_vfo(RIG *rig);
static int get_vfo(RIG *rig);
static int set_ptt(RIG *rig);
static int get_ptt(RIG *rig);
static int set_rptr_shift(RIG *rig);
static int get_rptr_shift(RIG *rig);
static int set_rptr_offs(RIG *rig);
static int get_rptr_offs(RIG *rig);
static int set_ctcss(RIG *rig);
static int get_ctcss(RIG *rig);
static int set_dcs(RIG *rig);
static int get_dcs(RIG *rig);
static int set_split_freq(RIG *rig);
static int get_split_freq(RIG *rig);
static int set_split(RIG *rig);
static int get_split(RIG *rig);
static int set_ts(RIG *rig);
static int get_ts(RIG *rig);
static int power2mW(RIG *rig);
static int set_level(RIG *rig);
static int get_level(RIG *rig);
static int set_func(RIG *rig);
static int get_func(RIG *rig);
static int set_bank(RIG *rig);
static int set_mem(RIG *rig);
static int get_mem(RIG *rig);
static int mv_ctl(RIG *rig);
static int set_channel(RIG *rig);
static int get_channel(RIG *rig);
static int set_trn(RIG *rig);
static int get_trn(RIG *rig);

struct test_table test_list[] = {
		{ 'F', "rig_set_freq", set_freq },
		{ 'f', "rig_get_freq", get_freq },
		{ 'M', "rig_set_mode", set_mode },
		{ 'm', "rig_get_mode", get_mode },
		{ 'P', "rig_set_passband", set_passband },
		{ 'p', "rig_get_passband", get_passband },
		{ 'V', "rig_set_vfo", set_vfo },
		{ 'v', "rig_get_vfo", get_vfo },
		{ 'T', "rig_set_ptt", set_ptt },
		{ 't', "rig_get_ptt", get_ptt },
		{ 'R', "rig_set_rptr_shift", set_rptr_shift },
		{ 'r', "rig_get_rptr_shift", get_rptr_shift },
		{ 'O', "rig_set_rptr_offs", set_rptr_offs },
		{ 'o', "rig_get_rptr_offs", get_rptr_offs },
		{ 'C', "rig_set_ctcss", set_ctcss },
		{ 'c', "rig_get_ctcss", get_ctcss },
		{ 'D', "rig_set_dcs", set_dcs },
		{ 'd', "rig_get_dcs", get_dcs },
		{ 'I', "rig_set_split_freq", set_split_freq },
		{ 'i', "rig_get_split_freq", get_split_freq },
		{ 'S', "rig_set_split", set_split },
		{ 's', "rig_get_split", get_split },
		{ 'N', "rig_set_ts", set_ts },
		{ 'n', "rig_get_ts", get_ts },
		{ 'W', "rig_set_poweron", rig_set_poweron },
		{ 'w', "rig_set_poweroff", rig_set_poweroff },
		{ '2', "power2mW", power2mW },
		{ 'L', "rig_set_level", set_level },
		{ 'l', "rig_get_level", get_level },
		{ 'U', "rig_set_func", set_func },
		{ 'u', "rig_get_func", get_func },
		{ 'B', "rig_set_bank", set_bank },
		{ 'E', "rig_set_mem", set_mem },
		{ 'e', "rig_get_mem", get_mem },
		{ 'G', "rig_mv_ctl", mv_ctl },
		{ 'H', "rig_set_channel", set_channel },
		{ 'h', "rig_get_channel", get_channel },
		{ 'A', "rig_set_trn", set_trn },
		{ 'a', "rig_get_trn", get_trn },
		{ 0x00, "", NULL },

#if 0
 extern setting_t rig_has_level(RIG *rig, setting_t level);
 extern setting_t rig_has_set_level(RIG *rig, setting_t level);

 extern setting_t rig_has_func(RIG *rig, setting_t func);        /* is part of capabilities? */
																	function(s) */
#endif

};

int main ()
{ 
	RIG *my_rig;		/* handle to rig (nstance) */

	int retcode;		/* generic return code from functions */
	int i;
	char cmd;

 	/*
	 * allocate memory, setup & open port 
	 */

  	retcode = rig_load_backend("icom");
	retcode |= rig_load_backend("ft747");

	if (retcode != RIG_OK ) {
		printf("rig_load_backend: error = %s \n", rigerror(retcode));
		exit(3);
	}

#if 1
  	my_rig = rig_init(RIG_MODEL_IC706MKIIG);
#else
	my_rig = rig_init(RIG_MODEL_FT747);
#endif

	if (!my_rig)
			exit(1); /* whoops! something went wrong (mem alloc?) */

	strncpy(my_rig->state.rig_path,SERIAL_PORT,FILPATHLEN);

	if ((retcode = rig_open(my_rig)) != RIG_OK) {
	  		fprintf(stderr,"rig_open: error = %s \n", rigerror(retcode));
			exit(2);
	}

	printf("Port %s opened ok\n", SERIAL_PORT);

	while (1) {
			printf("\nRig command: ");
			scanf("%c", &cmd);
			if (cmd == 0x0a)
					scanf("%c", &cmd);

			if (cmd == 'Q' || cmd == 'q')
					break;

			for (i=0; test_list[i].cmd != 0x00; i++)
					if (test_list[i].cmd == cmd)
							break;

			if (test_list[i].cmd == 0x00) {
					printf("Command not found!\n");
					continue;
			}
			retcode = (*test_list[i].test_func)(my_rig);
				
			if (retcode != RIG_OK ) {
	  			printf("%s: error = %s\n",test_list[i].name,rigerror(retcode));
			}
	}

	rig_close(my_rig); /* close port */
	rig_cleanup(my_rig); /* if you care about memory */

	printf("port %s closed ok \n",SERIAL_PORT);

	return 0;
}


static int set_freq(RIG *rig)
{
		freq_t freq;

		printf("Frequency: ");
		scanf("%Ld", &freq);
		return rig_set_freq(rig, freq);
}

static int get_freq(RIG *rig)
{
		int status;
		freq_t freq;

		status = rig_get_freq(rig, &freq);
		printf("Frequency: %Ld\n", freq);
		return status;
}

static int set_mode(RIG *rig)
{
		rmode_t mode;

		printf("Mode: ");
		scanf("%d", &mode);
		return rig_set_mode(rig, mode);
}


static int get_mode(RIG *rig)
{
		int status;
		rmode_t mode;

		status = rig_get_mode(rig, &mode);
		printf("Mode: %d\n", mode);
		return status;
}


static int set_passband(RIG *rig)
{
		pbwidth_t width;

		printf("Passband: ");
		scanf("%d", (int*)&width);
		return rig_set_passband(rig, width);
}


static int get_passband(RIG *rig)
{
		int status;
		pbwidth_t width;

		status = rig_get_passband(rig, &width);
		printf("Passband: %d\n", width);
		return status;
}


static int set_vfo(RIG *rig)
{
		vfo_t vfo;

		printf("VFO: ");
		scanf("%d", (int*)&vfo);
		return rig_set_vfo(rig, vfo);
}


static int get_vfo(RIG *rig)
{
		int status;
		vfo_t vfo;

		status = rig_get_vfo(rig, &vfo);
		printf("VFO: %d\n", vfo);
		return status;
}


static int set_ptt(RIG *rig)
{
		ptt_t ptt;

		printf("PTT: ");
		scanf("%d", (int*)&ptt);
		return rig_set_ptt(rig, ptt);
}


static int get_ptt(RIG *rig)
{
		int status;
		ptt_t ptt;

		status = rig_get_ptt(rig, &ptt);
		printf("PTT: %d\n", ptt);
		return status;
}


static int set_rptr_shift(RIG *rig)
{
		rptr_shift_t rptr_shift;

		printf("Repeater shift: ");
		scanf("%d", (int*)&rptr_shift);
		return rig_set_rptr_shift(rig, rptr_shift);
}


static int get_rptr_shift(RIG *rig)
{
		int status;
		rptr_shift_t rptr_shift;

		status = rig_get_rptr_shift(rig, &rptr_shift);
		printf("Repeater shift: %d\n", rptr_shift);
		return status;
}


static int set_rptr_offs(RIG *rig)
{
		unsigned long rptr_offs;

		printf("Repeater shift offset: ");
		scanf("%ld", &rptr_offs);
		return rig_set_rptr_offs(rig, rptr_offs);
}


static int get_rptr_offs(RIG *rig)
{
		int status;
		unsigned long rptr_offs;

		status = rig_get_rptr_offs(rig, &rptr_offs);
		printf("Repeater shift offset: %ld\n", rptr_offs);
		return status;
}


static int set_ctcss(RIG *rig)
{
		unsigned int tone;

		printf("CTCSS tone: ");
		scanf("%d", &tone);
		return rig_set_ctcss(rig, tone);
}


static int get_ctcss(RIG *rig)
{
		int status;
		unsigned int tone;

		status = rig_get_ctcss(rig, &tone);
		printf("CTCSS tone: %d\n", tone);
		return status;
}


static int set_dcs(RIG *rig)
{
		unsigned int code;

		printf("DCS code: ");
		scanf("%d", &code);
		return rig_set_dcs(rig, code);
}


static int get_dcs(RIG *rig)
{
		int status;
		unsigned int code;

		status = rig_get_dcs(rig, &code);
		printf("DCS code: %d\n", code);
		return status;
}


static int set_split_freq(RIG *rig)
{
		freq_t rxfreq,txfreq;

		printf("Receive frequency: ");
		scanf("%Ld", &rxfreq);
		printf("Transmit frequency: ");
		scanf("%Ld", &txfreq);
		return rig_set_split_freq(rig, rxfreq, txfreq);
}


static int get_split_freq(RIG *rig)
{
		int status;
		freq_t rxfreq,txfreq;

		status = rig_get_split_freq(rig, &rxfreq, &txfreq);
		printf("Receive frequency: %Ld\n", rxfreq);
		printf("Transmit frequency: %Ld\n", txfreq);
		return status;
}


static int set_split(RIG *rig)
{
		split_t split;

		printf("Split mode: ");
		scanf("%d", (int*)&split);
		return rig_set_split(rig, split);
}


static int get_split(RIG *rig)
{
		int status;
		split_t split;

		status = rig_get_split(rig, &split);
		printf("Split mode: %d\n", split);
		return status;
}


static int set_ts(RIG *rig)
{
		unsigned long ts;

		printf("Tuning step: ");
		scanf("%ld", &ts);
		return rig_set_ts(rig, ts);
}


static int get_ts(RIG *rig)
{
		int status;
		unsigned long ts;

		status = rig_get_ts(rig, &ts);
		printf("Tuning step: %ld\n", ts);
		return status;
}

static int power2mW(RIG *rig)
{
		int status;
		float power;
		freq_t freq;
		rmode_t mode;
		unsigned int mwp;

		printf("Power [0.0 .. 1.0]: ");
		scanf("%f", &power);
		printf("Frequency: ");
		scanf("%Ld", &freq);
		printf("Mode: ");
		scanf("%d", &mode);
		status = rig_power2mW(rig, &mwp, power, freq, mode);
		printf("Power: %d mW\n", mwp);
		return status;
}


static int set_level(RIG *rig)
{
		return 0;
}


static int get_level(RIG *rig)
{
		return 0;
}


static int set_func(RIG *rig)
{
		return 0;
}


static int get_func(RIG *rig)
{
		return 0;
}


static int set_bank(RIG *rig)
{
		int bank;

		printf("Bank: ");
		scanf("%d", &bank);
		return rig_set_bank(rig, bank);
}


static int set_mem(RIG *rig)
{
		int ch;

		printf("Memory#: ");
		scanf("%d", &ch);
		return rig_set_mem(rig, ch);
}


static int get_mem(RIG *rig)
{
		int status;
		int ch;

		status = rig_get_mem(rig, &ch);
		printf("Memory#: %d\n", ch);
		return status;
}


static int mv_ctl(RIG *rig)
{
		mv_op_t op;

		printf("Mem/VFO op: ");
		scanf("%d", (int*)&op);
		return rig_mv_ctl(rig, op);
}


static int set_channel(RIG *rig)
{
		return 0;
}


static int get_channel(RIG *rig)
{
		return 0;
}


static int set_trn(RIG *rig)
{
		int trn;

		printf("Transceive: ");
		scanf("%d", &trn);
		return rig_set_trn(rig, trn);
}


static int get_trn(RIG *rig)
{
		int status;
		int trn;

		status = rig_get_trn(rig, &trn);
		printf("Transceive: %d\n", trn);
		return status;
}



