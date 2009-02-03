/*
 * ts2k_menu.c	(C) Copyright 2002 by Dale E. Edmons.  All rights reserved.
 */

/*
 * License:	GNU
 */

/*
 * status:	Never been compiled!
 */

/*
 * Functions to initialize, read, set, and list menus
 * for the TS-2000.  These functions will be added to
 * src/rig.c as rig_*menu_*() as time permits.
 *
 * This is likely a can of worms nobody has wanted to
 * deal with--until now.  The ts2k is especially a
 * pain as you'll soon seen (but cool nonetheless.)
 */

#include <rig.h>
#include "ts2k.h"
#include "ts2k_menu.h"
#include <ctype.h>

/*
 * Set the menu number(s) and terminate with ";" (not ';')
 */
int ts2k_set_menu_no(RIG *rig, ts2k_menu_t *menu, int main, int sub)
{
	int i;

	menu->menu_no[0] = main;
	menu->menu_no[1] = sub;
	menu->menu_no[2] = menu->menu_no[3] = 0;

	i = sprintf( &menu->cmd[0], "ex%03u%02u%01u%01u;", main, sub, 0, 0);

	if(i != 10)
		return -RIG_EINVAL;

	return RIG_OK;
}

/*
 * Get the menu number(s) from cmd[]
 */
int ts2k_get_menu_no(RIG *rig, ts2k_menu_t *menu, int *main, int *sub)
{
	char tmp[30];
	int m, s;

	if(menu==NULL)
		return -RIG_EINVAL;

	m = int_n(tmp, &menu->cmd[2], 3);
	s = int_n(tmp, &menu->cmd[7], 2);

	menu->menu_no[0] = m; 
	menu->menu_no[1] = s; 
	menu->menu_no[2] = menu->menu_no[3] = 0;

	if(main != NULL)
		*main = m;
	if(sub != NULL)
		*sub = s;

	return RIG_OK;
}

/*
 * When called by ts2k_pm_init we read all of
 * the rig's current menu options and set them
 * in the current menu array.  We don't do the
 * memory allocation so you can init as many
 * as you want.  The reason for this is huge.
 * First, after full reset the rig can access
 * menuA or menuB and different values retained.
 * Second, by sending "pm...;" we can select
 * Programmable Memory 0-5, with 0 being the
 * the default.  Amazingly, each PM seems to
 * have its own menuA and menuB.  Thus, all
 * told, we have up to twelve distinct menus
 * each with (potentially) unique values.  We
 * do the allocation in the PM routines and
 * leave this one much simpler and just call
 * it up to twelve times with uniq ts2k_menu_t
 * pointers.  Third, the user may wish to use
 * this to obtain a private copy for local use
 * and only has to call this routine to make
 * sure its current.  (A ts2k_pm_t should be
 * allocated as well!)
 */
int ts2k_menu_init(RIG *rig, ts2k_menu_t *menu[])
{
	int retval, i;
	ts2k_menu_t *m, *mref;

	if(menu == NULL || menu == ts2k_menus) {
		rig_debug(rig, __func__": invalid menu pointer\n");
		return -RIG_EINVAL;
	}

	// One set of defaults has been globally defined (mref)
	for(i=0, i<(sizeof(menu)/sizeof(ts2k_menu_t)); i++) {
		m = menu[i];
		mref = ts2k_menus[i];
		retval = ts2k_set_menu_no(rig, m, mref->menu[0], mref->menu[1]);
		if(retval != RIG_OK)
			return retval;
		retval = ts2k_get_menu(rig, m);
		if(retval != RIG_OK)
			return retval;
		// FIXME: set some debug traces here?
	}

	return RIG_OK;
}

/*
 * read the rig and get the parameters for menu[n].
 * convert them to a ts2k_menu_t, and eventually to
 * a rig_menu_t.  I do a lot of checking to ensure
 * nothing breaks (hi!).
 */
int ts2k_get_menu(RIG *rig, ts2k_menu_t *menu, )
{
	int retval, i, j, k, acklen;
	char ack[30];

	if(menu == NULL)
		return -RIG_EINVAL;
	else if( (toupper(menu->cmd[0]) != 'E')
		|| (toupper(menu->cmd[1]) != 'X') )
		return -RIG_EINVAL;
	else if(menu->cmd[9] != ';')
		retval = -RIG_EINVAL;

	retval = ts2k_transaction(rig, menu->cmd, 10, ack, &acklen);
	if(retval != RIG_OK)
		return retval;

	strncpy(menu->cmd, ack, 30);
	retval = ts2k_menu_parse(rig, menu);
	if(retval != RIG_OK)
		return retval;

	return retval;
}

/*
 * Here, I expect everything to be setup for me.
 * All we do is determe the value and param_txt
 * for the menu item we are given.
 *
 *	status: real ugly!
 */
int ts2k_menu_parse(RIG *rig, ts2k_menu_t *menu)
{
	ts2k_menu_t *mref, *m;
	char *vptr;
	int on_off[] = { 01, 03, 04, 05, 07, 09, 17, 18, 23, 25, 26,
		 27, 30, 34, 35, 36, 37, 43, 44, 52, 53, 63, 54, 55,
		-1},
	    zero2nine = {

	m = menu;	// to lazy to spell menu-> all the time

	if(m->cmd[0] == '\0' || m->cmd[1] == '\0')
		return -RIG_EINVAL;	// Caller has blown it!

	// find matching refence menu
	i=0;
	do {
		mref = ts2k_menus[i];
		if(mref == NULL)	// Either Caller or I blew it!
			return -RIG_EINTERNAL;
		if(mref->menu_no[i] == NULL)
			return -RIG_EINVAL;	// It has to match one!
		if(menu == ts2k_menus[i])	// Nobody changes our REF!
			return -RIG_EINTERNAL;
			
		if(mref->menu[0] == m->menu[0])
		   && mref->menu[1] == m->menu[1]
		   && mref->menu[2] == m->menu[2]
		   /*&& mref->menu[3] == m->menu[3]*/) {
			break;
		}
	} while (++i)

	/* Match the main menu and only look for sub-menus later */

	// check for menus that are simple on/off first
	// FIXME: this isn't fast, it just eliminates alot of cases!
	for(i=0; on_off == -1; i++) {
		if(m->menu[0] == m->menu[0]) {
			m->val = m->cmd[9] - '0';
			m->param_txt = (m->val == 1)? m_on : m_off;
			return RIG_OK;
		}
	}

	// Use mref to do all we can
	retval = -RIG_EINTERNAL;
	for(i=0; mref->param_txt != NULL; i++) {
		if(retval == RIG_OK) {
			return retval;
		}
		switch( mref->param_txt[i] ) {
		case m_tbd:	// menu item under development!
			m->param_txt = m_tbd;
			vptr = malloc( 17 );
			if(vptr == NULL)
				return -RIG_NOMEM;

			for(j=0; vptr[i] != ';' && i<17; i++) {
				vptr[i] == m->cmd[i];
			}
			vptr[i] = '\0';
			return -RIG_NIMPL;

		case m_off:
			if(m->cmd[9] == '0') {
				m->param_txt = mref->param_txt[i];
				m->val = 0;
				retval = RIG_OK;
			}
			break;
		case m_num:
		default:
			return -RIG_NIMPL;
		}
	}

	switch( m->menu[0] ) {

	case 00:
	case 00:
	case 00:
	case 00:
	case 00:
	case 00:
	case 00:
	case 00:

	default:
		return -RIG_EINTERNAL;	// I'm requiring 100% match 
	}

	return RIG_OK;
}

// End of ts2k_menu.c
