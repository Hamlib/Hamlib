/*  hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * class.cc - Copyright (C) 2001 Stephane Fillod
 * This program defines the Hamlib API and rig capabilities structures that
 * will be used for obtaining rig capabilities. C++ bindings.
 *
 *
 *	$Id: rigclass.cc,v 1.1 2001-06-15 06:56:10 f4cfe Exp $
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


#include <hamlib/rig.h>
#include <hamlib/rigclass.h>

#define CHECK_RIG(cmd) { int _retval = cmd; if (_retval != RIG_OK) \
							THROW(new RigException (_retval)); }

static int hamlibpp_freq_event(RIG *rig, vfo_t vfo, freq_t freq);





static int hamlibpp_freq_event(RIG *rig, vfo_t vfo, freq_t freq)
{
	if (!rig || !rig->state.obj)
		return -RIG_EINVAL;

/* assert rig == ((Rig*)rig->state.obj).thRig */
	return ((Rig*)rig->state.obj)->FreqEvent(vfo, freq);
}


Rig::Rig(rig_model_t rig_model) {
	theRig = rig_init(rig_model);
   	if (!theRig)
		THROW(new RigException ("Rig initialization error"));

	caps = theRig->caps;
	theRig->callbacks.freq_event = &hamlibpp_freq_event;
	theRig->state.obj = (rig_ptr_t)this;
}

Rig::~Rig() {
	theRig->state.obj = NULL;
	CHECK_RIG( rig_cleanup(theRig) );
	caps = NULL;
}

void Rig::open(void) {
	CHECK_RIG( rig_open(theRig) );
}

void Rig::close(void) {
	CHECK_RIG( rig_close(theRig) );
}

void Rig::setFreq(freq_t freq, vfo_t vfo) {
	CHECK_RIG( rig_set_freq(theRig, vfo, freq) );
}

freq_t Rig::getFreq(vfo_t vfo)
{
	freq_t freq;

	CHECK_RIG( rig_get_freq(theRig, vfo, &freq) );

	return freq;
}

void Rig::setMode(rmode_t mode, pbwidth_t width, vfo_t vfo) {
	CHECK_RIG(rig_set_mode(theRig, vfo, mode, width));
}

rmode_t Rig::getMode(pbwidth_t& width, vfo_t vfo) {
	rmode_t mode;

	CHECK_RIG( rig_get_mode(theRig, vfo, &mode, &width) );

	return mode;
}

void Rig::setVFO(vfo_t vfo)
{
	CHECK_RIG( rig_set_vfo(theRig, vfo) );
}

vfo_t Rig::getVFO()
{
	vfo_t vfo;

	CHECK_RIG( rig_get_vfo(theRig, &vfo) );

	return vfo;
}

void Rig::setPTT(ptt_t ptt, vfo_t vfo)
{
	CHECK_RIG( rig_set_ptt(theRig, vfo, ptt) );
}

ptt_t Rig::getPTT(vfo_t vfo)
{
	ptt_t ptt;

	CHECK_RIG( rig_get_ptt(theRig, vfo, &ptt) );

	return ptt;
}

dcd_t Rig::getDCD(vfo_t vfo)
{
	dcd_t dcd;

	CHECK_RIG( rig_get_dcd(theRig, vfo, &dcd) );

	return dcd;
}

void Rig::setLevel(setting_t level, int vali, vfo_t vfo)
{
	value_t val;

	val.i = vali;
	CHECK_RIG( rig_set_level(theRig, vfo, level, val) );
}

void Rig::setLevel(setting_t level, float valf, vfo_t vfo)
{
	value_t val;

	val.f = valf;
	CHECK_RIG( rig_set_level(theRig, vfo, level, val) );
}


void Rig::getLevel(setting_t level, int& vali, vfo_t vfo)
{
	value_t val;

	CHECK_RIG( rig_get_level(theRig, vfo, level, &val) );
	vali = val.i;
}

void Rig::getLevel(setting_t level, float& valf, vfo_t vfo)
{
	value_t val;

	CHECK_RIG( rig_get_level(theRig, vfo, level, &val) );
	valf = val.f;
}


setting_t Rig::hasGetLevel (setting_t level)
{
	return rig_has_get_level(theRig, level);
}
setting_t Rig::hasSetLevel (setting_t level)
{
	return rig_has_set_level(theRig, level);
}

const char *Rig::getInfo (void) {
	return rig_get_info(theRig);
}

pbwidth_t Rig::passbandNormal (rmode_t mode)
{
	return rig_passband_normal(theRig, mode);
}

pbwidth_t Rig::passbandNarrow (rmode_t mode)
{
	return rig_passband_narrow(theRig, mode);
}

pbwidth_t Rig::passbandWide (rmode_t mode)
{
	return rig_passband_wide(theRig, mode);
}

