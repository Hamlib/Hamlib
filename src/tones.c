/**
 * \file src/tones.c
 * \ingroup rig
 * \brief CTCSS and DCS interface and tables
 * \author Stephane Fillod
 * \date 2000-2002
 */
/*
 *  Hamlib Interface - CTCSS and DCS interface and tables
 *  Copyright (c) 2000-2002 by Stephane Fillod
 *
 *	$Id: tones.c,v 1.3 2002-11-04 22:21:42 fillods Exp $
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <hamlib/rig.h>

/**
 * 52 CTCSS sub-audible tones
 */
const tone_t full_ctcss_list[] = {
	 600,  670,  693,  719,  744,  770,  797,  825,  854,  885,  915,
	 948,  974, 1000, 1035, 1072, 1109, 1148, 1188, 1200, 1230, 1273,
	1318, 1365, 1413, 1462, 1514, 1567, 1598, 1622, 1655, 1679,
	1713, 1738, 1773, 1799, 1835, 1862, 1899, 1928, 1966, 1995,
	2035, 2065, 2107, 2181, 2257, 2291, 2336, 2418, 2503, 2541,
	0,
};

/**
 * 50 CTCSS sub-audible tones, from 67.0Hz to 254.1Hz
 *
 * \note Don't even think about changing a bit of this array, several
 * backends depend on it. If you need to, create a copy for your 
 * own caps. --SF
 */
const tone_t common_ctcss_list[] = {
	 670,  693,  719,  744,  770,  797,  825,  854,  885,  915,
	 948,  974, 1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273,
	1318, 1365, 1413, 1462, 1514, 1567, 1598, 1622, 1655, 1679,
	1713, 1738, 1773, 1799, 1835, 1862, 1899, 1928, 1966, 1995,
	2035, 2065, 2107, 2181, 2257, 2291, 2336, 2418, 2503, 2541,
	0,
};

/**
 * 106 DCS codes
 */
const tone_t full_dcs_list[] = {
	 17,  23,  25,  26,  31,  32,  36,  43,  47,  50,  51,  53, 
	 54,  65,  71,  72,  73,  74, 114, 115, 116, 122, 125, 131, 
	132, 134, 143, 145, 152, 155, 156, 162, 165, 172, 174, 205, 
	212, 223, 225, 226, 243, 244, 245, 246, 251, 252, 255, 261, 
	263, 265, 266, 271, 274, 306, 311, 315, 325, 331, 332, 343, 
	346, 351, 356, 364, 365, 371, 411, 412, 413, 423, 431, 432, 
	445, 446, 452, 454, 455, 462, 464, 465, 466, 503, 506, 516, 
	523, 526, 532, 546, 565, 606, 612, 624, 627, 631, 632, 654, 
	662, 664, 703, 712, 723, 731, 732, 734, 743, 754, 
	0,
};


#ifndef DOC_HIDDEN

#define CHECK_RIG_ARG(r) (!(r) || !(r)->caps || !(r)->state.comm_state)

#endif /* !DOC_HIDDEN */


/**
 * \brief set CTCSS sub-tone frequency
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param tone	The tone to set to
 *
 *  Sets the current Continuous Tone Controlled Squelch System (CTCSS) 
 *  sub-audible tone frequency.
 *  \note  the \a tone integer is NOT in Hz, but in tenth of Hz! This way,
 *  if you want to set a subaudible tone of 88.5 Hz for example,
 *  then pass 885 to this function.
 *  Also, to disable Tone encoding, set \a tone to 0 (FIXME: not clear in API).
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_ctcss_tone(), rig_set_ctcss_sql()
 */

int rig_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
	const struct rig_caps *caps;
	int retcode;
	vfo_t curr_vfo;

	if (CHECK_RIG_ARG(rig))
		return -RIG_EINVAL;

	caps = rig->caps;

	if (caps->set_ctcss_tone == NULL)
		return -RIG_ENAVAIL;

	if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
				vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
		return caps->set_ctcss_tone(rig, vfo, tone);

	if (!caps->set_vfo)
		return -RIG_ENTARGET;
	curr_vfo = rig->state.current_vfo;
	retcode = caps->set_vfo(rig, vfo);
	if (retcode != RIG_OK)
		return retcode;

	retcode = caps->set_ctcss_tone(rig, vfo, tone);
	caps->set_vfo(rig, curr_vfo);
	return retcode;
}

/**
 * \brief get the current CTCSS sub-tone frequency
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param tone	The location where to store the current tone
 *
 *  Retrieves the current Continuous Tone Controlled Squelch System (CTCSS) 
 *  sub-audible tone frequency.
 *  \note the \a *tone integer is NOT in Hz, but in tenth of Hz! This way,
 *  if the function rig_get_ctcss_tone() returns a subaudible tone of 885
 *  for example, then the real tone is 88.5 Hz. 
 *  Also, a value of 0 for \a *tone means the Tone encoding is disabled.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_ctcss_tone(), rig_get_ctcss_sql()
 */
int rig_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
	const struct rig_caps *caps;
	int retcode;
	vfo_t curr_vfo;

	if (CHECK_RIG_ARG(rig) || !tone)
		return -RIG_EINVAL;

	caps = rig->caps;

	if (caps->get_ctcss_tone == NULL)
		return -RIG_ENAVAIL;

	if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
				vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
		return caps->get_ctcss_tone(rig, vfo, tone);

	if (!caps->set_vfo)
		return -RIG_ENTARGET;
	curr_vfo = rig->state.current_vfo;
	retcode = caps->set_vfo(rig, vfo);
	if (retcode != RIG_OK)
		return retcode;

	retcode = caps->get_ctcss_tone(rig, vfo, tone);
	caps->set_vfo(rig, curr_vfo);
	return retcode;
}

/**
 * \brief set the current encoding DCS code
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param code	The tone to set to
 *
 * Sets the current encoding Digitally-Coded Squelch code.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_dcs_code(), rig_set_dcs_sql()
 */

int rig_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code)
{
	const struct rig_caps *caps;
	int retcode;
	vfo_t curr_vfo;

	if (CHECK_RIG_ARG(rig))
		return -RIG_EINVAL;

	caps = rig->caps;

	if (caps->set_dcs_code == NULL)
		return -RIG_ENAVAIL;

	if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
				vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
		return caps->set_dcs_code(rig, vfo, code);

	if (!caps->set_vfo)
		return -RIG_ENTARGET;
	curr_vfo = rig->state.current_vfo;
	retcode = caps->set_vfo(rig, vfo);
	if (retcode != RIG_OK)
		return retcode;

	retcode = caps->set_dcs_code(rig, vfo, code);
	caps->set_vfo(rig, curr_vfo);
	return retcode;
}

/**
 * \brief get the current encoding DCS code
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param code	The location where to store the current tone
 *
 * Retrieves the current encoding Digitally-Coded Squelch code. 
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_dcs_code(), rig_get_dcs_sql()
 */
int rig_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code)
{
	const struct rig_caps *caps;
	int retcode;
	vfo_t curr_vfo;

	if (CHECK_RIG_ARG(rig) || !code)
		return -RIG_EINVAL;

	caps = rig->caps;

	if (caps->get_dcs_code == NULL)
		return -RIG_ENAVAIL;

	if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
				vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
		return caps->get_dcs_code(rig, vfo, code);

	if (!caps->set_vfo)
		return -RIG_ENTARGET;
	curr_vfo = rig->state.current_vfo;
	retcode = caps->set_vfo(rig, vfo);
	if (retcode != RIG_OK)
		return retcode;

	retcode = caps->get_dcs_code(rig, vfo, code);
	caps->set_vfo(rig, curr_vfo);
	return retcode;
}

/**
 * \brief set CTCSS squelch
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param tone	The PL tone to set the squelch to
 *
 *  Sets the current Continuous Tone Controlled Squelch System (CTCSS)
 *  sub-audible *squelch* tone.
 *  \note \a tone is NOT in Hz, but in tenth of Hz! This way,
 *  if you want to set subaudible squelch tone of 88.5 Hz for example,
 *  then pass 885 to this function.
 *  Also, to disable Tone squelch, set \a tone to 0 (FIXME: not clear in API).
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_ctcss_sql(), rig_set_ctcss_tone()
 */

int rig_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
	const struct rig_caps *caps;
	int retcode;
	vfo_t curr_vfo;

	if (CHECK_RIG_ARG(rig))
		return -RIG_EINVAL;

	caps = rig->caps;

	if (caps->set_ctcss_sql == NULL)
		return -RIG_ENAVAIL;

	if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
				vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
		return caps->set_ctcss_sql(rig, vfo, tone);

	if (!caps->set_vfo)
		return -RIG_ENTARGET;
	curr_vfo = rig->state.current_vfo;
	retcode = caps->set_vfo(rig, vfo);
	if (retcode != RIG_OK)
		return retcode;

	retcode = caps->set_ctcss_sql(rig, vfo, tone);
	caps->set_vfo(rig, curr_vfo);
	return retcode;
}

/**
 * \brief get the current CTCSS squelch
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param tone	The location where to store the current tone
 *
 *  Retrieves the current Continuous Tone Controlled Squelch System (CTCSS) 
 *  sub-audible *squelch* tone.
 *  \note \a *tone is NOT in Hz, but in tenth of Hz! This way,
 *  if the function rig_get_ctcss_sql() returns a subaudible tone of 885
 *  for example, then the real tone is 88.5 Hz. 
 *  Also, a value of 0 for \a tone means the Tone squelch is disabled.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_ctcss_sql(), rig_get_ctcss_tone()
 */
int rig_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
	const struct rig_caps *caps;
	int retcode;
	vfo_t curr_vfo;

	if (CHECK_RIG_ARG(rig) || !tone)
		return -RIG_EINVAL;

	caps = rig->caps;

	if (caps->get_ctcss_sql == NULL)
		return -RIG_ENAVAIL;

	if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) ||
			vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
		return caps->get_ctcss_sql(rig, vfo, tone);

	if (!caps->set_vfo)
		return -RIG_ENTARGET;
	curr_vfo = rig->state.current_vfo;
	retcode = caps->set_vfo(rig, vfo);
	if (retcode != RIG_OK)
		return retcode;

	retcode = caps->get_ctcss_sql(rig, vfo, tone);
	caps->set_vfo(rig, curr_vfo);
	return retcode;
}

/**
 * \brief set the current DCS code
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param code	The tone to set to
 *
 * Sets the current Digitally-Coded *Squelch* code.
 *
 * \return returns RIG_OK if the operation has been sucessful, ortherwise
 * a negative value if an error occured (in which case, cause is set
 * appropriately).
 *
 * \sa rig_get_dcs_sql(), rig_set_dcs_code()
 */

int rig_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code)
{
	const struct rig_caps *caps;
	int retcode;
	vfo_t curr_vfo;

	if (CHECK_RIG_ARG(rig))
		return -RIG_EINVAL;

	caps = rig->caps;

	if (caps->set_dcs_sql == NULL)
		return -RIG_ENAVAIL;

	if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) ||
			vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
		return caps->set_dcs_sql(rig, vfo, code);

	if (!caps->set_vfo)
		return -RIG_ENTARGET;
	curr_vfo = rig->state.current_vfo;
	retcode = caps->set_vfo(rig, vfo);
	if (retcode != RIG_OK)
		return retcode;

	retcode = caps->set_dcs_sql(rig, vfo, code);
	caps->set_vfo(rig, curr_vfo);
	return retcode;
}

/**
 * \brief get the current DCS code
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param code	The location where to store the current tone
 *
 * Retrieves the current Digitally-Coded *Squelch* code. 
 *
 * \return RIG_OK if the operation has been sucessful, ortherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_dcs_sql(), rig_get_dcs_code()
 */
int rig_get_dcs_sql(RIG *rig, vfo_t vfo, tone_t *code)
{
	const struct rig_caps *caps;
	int retcode;
	vfo_t curr_vfo;

	if (CHECK_RIG_ARG(rig) || !code)
		return -RIG_EINVAL;

	caps = rig->caps;

	if (caps->get_dcs_sql == NULL)
		return -RIG_ENAVAIL;

	if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) ||
			vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
		return caps->get_dcs_sql(rig, vfo, code);

	if (!caps->set_vfo)
		return -RIG_ENTARGET;
	curr_vfo = rig->state.current_vfo;
	retcode = caps->set_vfo(rig, vfo);
	if (retcode != RIG_OK)
		return retcode;

	retcode = caps->get_dcs_sql(rig, vfo, code);
	caps->set_vfo(rig, curr_vfo);
	return retcode;
}

