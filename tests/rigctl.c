/*
 * hamlib - (C) 2000 Frank Singleton and Stephane Fillod
 *
 * rigctl.c - (C) Stephane Fillod 2000
 *
 * This program test/control a radio using Hamlib.
 * TODO: be more generic and add command line option to run 
 * 		in non-interactive mode
 *
 * $Id: rigctl.c,v 1.11 2001-04-22 14:47:27 f4cfe Exp $  
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
#include <unistd.h>

/* TODO: autoconf should check for getopt support, include libs otherwise */
#include <getopt.h>

#include <hamlib/rig.h>


#define SERIAL_PORT "/dev/ttyS0"

#define MAXNAMSIZ 40
#define MAXNBOPT 100	/* max number of different options */


struct test_table {
	unsigned char cmd;
	unsigned char name[MAXNAMSIZ];
        int (*test_func)(RIG*, int, const struct test_table*, const char*, const char*, const char*);
	const char *name1;
	const char *name2;
	const char *name3;
};

#define declare_proto_rig(f) static int (f)(RIG *rig, int interactive, const struct test_table *cmd, const char *arg1, const char *arg2, const char *arg3)

declare_proto_rig(set_freq);
declare_proto_rig(get_freq);
declare_proto_rig(set_mode);
declare_proto_rig(get_mode);
declare_proto_rig(set_vfo);
declare_proto_rig(get_vfo);
declare_proto_rig(set_ptt);
declare_proto_rig(get_ptt);
declare_proto_rig(get_ptt);
declare_proto_rig(set_rptr_shift);
declare_proto_rig(get_rptr_shift);
declare_proto_rig(set_rptr_offs);
declare_proto_rig(get_rptr_offs);
declare_proto_rig(set_ctcss);
declare_proto_rig(get_ctcss);
declare_proto_rig(set_dcs);
declare_proto_rig(get_dcs);
declare_proto_rig(set_split_freq);
declare_proto_rig(get_split_freq);
declare_proto_rig(set_split);
declare_proto_rig(get_split);
declare_proto_rig(set_ts);
declare_proto_rig(get_ts);
declare_proto_rig(power2mW);
declare_proto_rig(set_level);
declare_proto_rig(get_level);
declare_proto_rig(set_func);
declare_proto_rig(get_func);
declare_proto_rig(set_bank);
declare_proto_rig(set_mem);
declare_proto_rig(get_mem);
declare_proto_rig(mv_ctl);
declare_proto_rig(set_channel);
declare_proto_rig(get_channel);
declare_proto_rig(set_trn);
declare_proto_rig(get_trn);

/*
 * convention: upper case cmd is set, lowercase is get
 *
 * TODO: add missing rig_set_/rig_get_: rit, ann, ant, dcs, ctcss, dcd, etc.
 * NB: do NOT use -W since it's reserved by POSIX.
 *		'q' 'Q' '?' are also reserved for interface use
 */
struct test_table test_list[] = {
		{ 'F', "set_freq", set_freq, "Frequency" },
		{ 'f', "get_freq", get_freq, "Frequency" },
		{ 'M', "set_mode", set_mode, "Mode", "Passband" },
		{ 'm', "get_mode", get_mode, "Mode", "Passband" },
		{ 'V', "set_vfo", set_vfo, "VFO" },
		{ 'v', "get_vfo", get_vfo, "VFO" },
		{ 'T', "set_ptt", set_ptt, "PTT" },
		{ 't', "get_ptt", get_ptt, "PTT" },
		{ 'R', "set_rptr_shift", set_rptr_shift, "Rptr shift" },
		{ 'r', "get_rptr_shift", get_rptr_shift, "Rptr shift" },
		{ 'O', "set_rptr_offs", set_rptr_offs, "Rptr offset" },
		{ 'o', "get_rptr_offs", get_rptr_offs, "Rptr offset" },
		{ 'C', "set_ctcss", set_ctcss, "CTCSS tone" },
		{ 'c', "get_ctcss", get_ctcss, "CTCSS tone" },
		{ 'D', "set_dcs", set_dcs, "DCS code" },
		{ 'd', "get_dcs", get_dcs, "DCS code" },
		{ 'I', "set_split_freq", set_split_freq, "Rx frequency", "Tx frequency" },
		{ 'i', "get_split_freq", get_split_freq, "Rx frequency", "Tx frequency" },
		{ 'S', "set_split", set_split, "Split mode" },
		{ 's', "get_split", get_split, "Split mode" },
		{ 'N', "set_ts", set_ts, "Tuning step" },
		{ 'n', "get_ts", get_ts, "Tuning step" },
		{ '2', "power2mW", power2mW },
		{ 'L', "set_level", set_level, "Level", "Value" },
		{ 'l', "get_level", get_level, "Level", "Value" },
		{ 'U', "set_func", set_func, "Func", "Func status" },
		{ 'u', "get_func", get_func, "Func", "Func status" },
		{ 'B', "set_bank", set_bank, "Bank" },
		{ 'E', "set_mem", set_mem, "Memory#" },
		{ 'e', "get_mem", get_mem, "Memory#" },
		{ 'G', "mv_ctl", mv_ctl, "Mem/VFO op" },
		{ 'H', "set_channel", set_channel /* huh! */ },
		{ 'h', "get_channel", get_channel, "Channel" },
		{ 'A', "set_trn", set_trn, "Transceive" },
		{ 'a', "get_trn", get_trn, "Transceive" },
		{ 0x00, "", NULL },

};

struct test_table *find_cmd_entry(int cmd)
{
	int i;
	for (i=0; i<MAXNBOPT && test_list[i].cmd != 0x00; i++)
		if (test_list[i].cmd == cmd)
			break;

	if (i >= MAXNBOPT || test_list[i].cmd == 0x00)
		return NULL;

	return &test_list[i];
}

int main (int argc, char *argv[])
{ 
	RIG *my_rig;		/* handle to rig (nstance) */

	int interactive=1;	/* if no cmd on command line, switch to interactive */
	int retcode;		/* generic return code from functions */
	int i;
	char cmd;
	struct option long_options[MAXNBOPT];
	char opt_string[MAXNBOPT*2], *opt_ptr;
	struct test_table *cmd_entry;

 	/*
	 * allocate memory, setup & open port 
	 */

  	retcode = rig_load_backend("icom");
	retcode |= rig_load_backend("ft747");
	retcode |= rig_load_backend("ft847");
	retcode |= rig_load_backend("kenwood");
	retcode |= rig_load_backend("aor");
	retcode |= rig_load_backend("pcr");
	rig_load_backend("winradio");
	rig_load_backend("dummy");

	if (retcode != RIG_OK ) {
		printf("rig_load_backend: error = %s \n", rigerror(retcode));
		exit(3);
	}

	opt_ptr = opt_string;
	for (i=0; i<MAXNBOPT-1 && test_list[i].cmd; i++) {
		/*
		 * build long_options
		 */
		long_options[i].name = test_list[i].name;
		long_options[i].val = test_list[i].cmd;
		long_options[i].has_arg = test_list[i].name1 ? 
						required_argument : no_argument;
		long_options[i].flag = 0;

		/*
		 * build short_options
		 */
		*opt_ptr++ = test_list[i].cmd;
		if (long_options[i].has_arg)
			*opt_ptr++ = ':';
	}
	memset(long_options+i, 0, sizeof(struct option));
	*opt_ptr = '\0';

printf("%s\n",opt_string);

	/* TODO: add a long option(after they are consumed) or arg to set rig model */
#if 0
  	my_rig = rig_init(RIG_MODEL_DUMMY);
#else
  	my_rig = rig_init(RIG_MODEL_IC706MKIIG);
#endif

	if (!my_rig)
			exit(1); /* whoops! something went wrong (mem alloc?) */

	strncpy(my_rig->state.rig_path,SERIAL_PORT,FILPATHLEN);

	/* TODO: make this a parameter */
	my_rig->state.ptt_type = RIG_PTT_PARALLEL;
	strncpy(my_rig->state.ptt_path,"/dev/parport0",FILPATHLEN);

	if ((retcode = rig_open(my_rig)) != RIG_OK) {
	  		fprintf(stderr,"rig_open: error = %s \n", rigerror(retcode));
			exit(2);
	}

	while(1) {
		int c;
		int option_index = 0;

		c = getopt_long (argc, argv, opt_string,
			long_options, &option_index);
		if (c == -1)
			break;

		printf ("## option '%c' %s\n", c, long_options[option_index].name);

		cmd_entry = find_cmd_entry(c);
		if (!cmd_entry)
			break;	/* TODO: */

		/*
		 * at least one command on command line, 
		 * disable interactive mode
		 */
		interactive = 0;

		retcode = (*cmd_entry->test_func)(my_rig, interactive, cmd_entry, optarg,
					optarg, optarg);
				
		if (retcode != RIG_OK ) {
  			printf("%s: error = %s\n",cmd_entry->name,rigerror(retcode));
			}
	}

	if (!interactive) {
		rig_close(my_rig); /* close port */
		rig_cleanup(my_rig); /* if you care about memory */
		return 0;
	}

	while (1) {
			char arg1[128], *p1;
			char arg2[128], *p2;
			char arg3[128], *p3;

			printf("\nRig command: ");
			scanf("%c", &cmd);
			if (cmd == 0x0a)
					scanf("%c", &cmd);

			if (cmd == 'Q' || cmd == 'q')
					break;

			if (cmd == '?') {
				for (i=0; test_list[i].cmd != 0x00; i++)
						printf("%c: %s\n",test_list[i].cmd,test_list[i].name);
				continue;
			}

			cmd_entry = find_cmd_entry(cmd);
			if (!cmd_entry) {
				fprintf(stderr, "Command not found!\n");
				continue;
			}

			p1 = p2 = p3 = NULL;
			if (cmd_entry->name1) {
				printf("%s: ", cmd_entry->name1);
				scanf("%s", arg1);
				p1 = arg1;
			}
			if (cmd_entry->name2) {
				printf("%s: ", cmd_entry->name2);
				scanf("%s", arg2);
				p2 = arg2;
			}
			if (cmd_entry->name3) {
				printf("%s: ", cmd_entry->name3);
				scanf("%s", arg3);
				p1 = arg3;
			}
			retcode = (*cmd_entry->test_func)(my_rig, interactive, cmd_entry, p1,
					p2, p3);
				
			if (retcode != RIG_OK ) {
	  			printf("%s: error = %s\n",cmd_entry->name,rigerror(retcode));
			}
	}

	rig_close(my_rig); /* close port */
	rig_cleanup(my_rig); /* if you care about memory */

	return 0;
}


/*
 * static int (f)(RIG *rig, int interactive, const void *arg1, const void *arg2, const void *arg3, const void *arg4)
 */

declare_proto_rig(set_freq)
{
		freq_t freq;

		sscanf(arg1, "%lld", &freq);
		return rig_set_freq(rig, RIG_VFO_CURR, freq);
}

declare_proto_rig(get_freq)
{
		int status;
		freq_t freq;

		status = rig_get_freq(rig, RIG_VFO_CURR, &freq);
		if (interactive)
			printf("%s: ", cmd->name1); /* i.e. "Frequency" */
		printf("%lld", freq);
		return status;
}

declare_proto_rig(set_mode)
{
		rmode_t mode;
		pbwidth_t width;

		sscanf(arg1, "%d", &mode);
		sscanf(arg2, "%d", (int*)&width);
		return rig_set_mode(rig, RIG_VFO_CURR, mode, width);
}


declare_proto_rig(get_mode)
{
		int status;
		rmode_t mode;
		pbwidth_t width;

		status = rig_get_mode(rig, RIG_VFO_CURR, &mode, &width);
		if (interactive)
			printf("%s: ", cmd->name1);
		printf("%d\n", mode);
		if (interactive)
			printf("%s: ", cmd->name2);
		printf("%ld", width);
		return status;
}


declare_proto_rig(set_vfo)
{
		vfo_t vfo;

		sscanf(arg1, "%d", (int*)&vfo);
		return rig_set_vfo(rig, vfo);
}


declare_proto_rig(get_vfo)
{
		int status;
		vfo_t vfo;

		status = rig_get_vfo(rig, &vfo);
		if (interactive)
			printf("%s: ", cmd->name1);
		printf("%d\n", vfo);
		return status;
}


declare_proto_rig(set_ptt)
{
		ptt_t ptt;

		sscanf(arg1, "%d", (int*)&ptt);
		return rig_set_ptt(rig, RIG_VFO_CURR, ptt);
}


declare_proto_rig(get_ptt)
{
		int status;
		ptt_t ptt;

		status = rig_get_ptt(rig, RIG_VFO_CURR, &ptt);
		if (interactive)
			printf("%s: ", cmd->name1);
		printf("%d\n", ptt);
		return status;
}


declare_proto_rig(set_rptr_shift)
{
		rptr_shift_t rptr_shift;

		sscanf(arg1, "%d", (int*)&rptr_shift);
		return rig_set_rptr_shift(rig, RIG_VFO_CURR, rptr_shift);
}


declare_proto_rig(get_rptr_shift)
{
		int status;
		rptr_shift_t rptr_shift;

		status = rig_get_rptr_shift(rig, RIG_VFO_CURR, &rptr_shift);
		if (interactive)
			printf("%s: ", cmd->name1);
		printf("%d\n", rptr_shift);
		return status;
}


declare_proto_rig(set_rptr_offs)
{
		unsigned long rptr_offs;

		sscanf(arg1, "%ld", &rptr_offs);
		return rig_set_rptr_offs(rig, RIG_VFO_CURR, rptr_offs);
}


declare_proto_rig(get_rptr_offs)
{
		int status;
		unsigned long rptr_offs;

		status = rig_get_rptr_offs(rig, RIG_VFO_CURR, &rptr_offs);
		if (interactive)
			printf("%s: ", cmd->name1);
		printf("%ld\n", rptr_offs);
		return status;
}


declare_proto_rig(set_ctcss)
{
		unsigned int tone;

		sscanf(arg1, "%d", &tone);
		return rig_set_ctcss(rig, RIG_VFO_CURR, tone);
}


declare_proto_rig(get_ctcss)
{
		int status;
		unsigned int tone;

		status = rig_get_ctcss(rig, RIG_VFO_CURR, &tone);
		if (interactive)
			printf("%s: ", cmd->name1);
		printf("%d\n", tone);
		return status;
}


declare_proto_rig(set_dcs)
{
		unsigned int code;

		sscanf(arg1, "%d", &code);
		return rig_set_dcs(rig, RIG_VFO_CURR, code);
}


declare_proto_rig(get_dcs)
{
		int status;
		unsigned int code;

		status = rig_get_dcs(rig, RIG_VFO_CURR, &code);
		if (interactive)
			printf("%s: ", cmd->name1);
		printf("%d\n", code);
		return status;
}


declare_proto_rig(set_split_freq)
{
		freq_t rxfreq,txfreq;

		sscanf(arg2, "%lld", &rxfreq);
		sscanf(arg2, "%lld", &txfreq);
		return rig_set_split_freq(rig, RIG_VFO_CURR, rxfreq, txfreq);
}


declare_proto_rig(get_split_freq)
{
		int status;
		freq_t rxfreq,txfreq;

		status = rig_get_split_freq(rig, RIG_VFO_CURR, &rxfreq, &txfreq);
		if (interactive)
			printf("%s: ", cmd->name1);
		printf("%lld\n", rxfreq);
		if (interactive)
			printf("%s: ", cmd->name2);
		printf("%lld\n", txfreq);
		return status;
}


declare_proto_rig(set_split)
{
		split_t split;

		sscanf(arg1, "%d", (int*)&split);
		return rig_set_split(rig, RIG_VFO_CURR, split);
}


declare_proto_rig(get_split)
{
		int status;
		split_t split;

		status = rig_get_split(rig, RIG_VFO_CURR, &split);
		if (interactive)
			printf("%s: ", cmd->name1);
		printf("%d\n", split);
		return status;
}


declare_proto_rig(set_ts)
{
		unsigned long ts;

		sscanf(arg1, "%ld", &ts);
		return rig_set_ts(rig, RIG_VFO_CURR, ts);
}


declare_proto_rig(get_ts)
{
		int status;
		unsigned long ts;

		status = rig_get_ts(rig, RIG_VFO_CURR, &ts);
		if (interactive)
			printf("%s: ", cmd->name1);
		printf("%ld\n", ts);
		return status;
}

declare_proto_rig(power2mW)
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


declare_proto_rig(set_level)
{
		setting_t level;
		value_t val;

		sscanf(arg1, "%ld", &level);
		if (RIG_LEVEL_IS_FLOAT(level))
			sscanf(arg2, "%f", &val.f);
		else
			sscanf(arg2, "%d", &val.i);

		return rig_set_level(rig, RIG_VFO_CURR, level, val);
}


declare_proto_rig(get_level)
{
		int status;
		setting_t level;
		value_t val;

		sscanf(arg1, "%ld", &level);
		status = rig_get_level(rig, RIG_VFO_CURR, level, &val);
		if (interactive)
			printf("%s: ", cmd->name2);
		if (RIG_LEVEL_IS_FLOAT(level))
			printf("%f\n", val.f);
		else
			printf("%d\n", val.i);

		return status;
}


declare_proto_rig(set_func)
{
		setting_t func;
		int func_stat;

		sscanf(arg1, "%ld", &func);
		sscanf(arg2, "%d", (int*)&func_stat);
		return rig_set_func(rig, RIG_VFO_CURR, func, func_stat);
}


/*
 * TODO: if rig_get_func fails, do not printf
 *		fix all other get_* calls in rigctl..
 */
declare_proto_rig(get_func)
{
		int status;
		setting_t func;
		int func_stat;

		sscanf(arg1, "%ld", &func);
		status = rig_get_func(rig, RIG_VFO_CURR, func, &func_stat);
		if (interactive)
			printf("%s: ", cmd->name2);
		printf("%d\n", func_stat);
		return status;
}


declare_proto_rig(set_bank)
{
		int bank;

		sscanf(arg1, "%d", &bank);
		return rig_set_bank(rig, RIG_VFO_CURR, bank);
}


declare_proto_rig(set_mem)
{
		int ch;

		sscanf(arg1, "%d", &ch);
		return rig_set_mem(rig, RIG_VFO_CURR, ch);
}


declare_proto_rig(get_mem)
{
		int status;
		int ch;

		status = rig_get_mem(rig, RIG_VFO_CURR, &ch);
		if (interactive)
			printf("%s: ", cmd->name1);
		printf("%d\n", ch);
		return status;
}


declare_proto_rig(mv_ctl)
{
		mv_op_t op;

		sscanf(arg1, "%d", (int*)&op);
		return rig_mv_ctl(rig, RIG_VFO_CURR, op);
}


declare_proto_rig(set_channel)
{
		fprintf(stderr,"rigctl_set_channel not implemented yet!\n");
		return 0;
}


declare_proto_rig(get_channel)
{
		int status;
		channel_t chan;

		sscanf(arg1, "%d", &chan.channel_num);
		status = rig_get_channel(rig, &chan);
		return status;
}


declare_proto_rig(set_trn)
{
		int trn;

		sscanf(arg1, "%d", &trn);
		return rig_set_trn(rig, RIG_VFO_CURR, trn);
}


declare_proto_rig(get_trn)
{
		int status;
		int trn;

		status = rig_get_trn(rig, RIG_VFO_CURR, &trn);
		if (interactive)
			printf("%s: ", cmd->name1);
		printf("%d\n", trn);
		return status;
}



