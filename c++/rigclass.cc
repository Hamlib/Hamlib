/**
 * \file src/rigclass.cc
 * \brief Ham Radio Control Libraries C++ interface
 * \author Stephane Fillod
 * \date 2001
 *
 * Hamlib C++ interface is a frontend implementing wrapper functions.
 */

/*
 *  Hamlib C++ bindings - main file
 *  Copyright (c) 2001 by Stephane Fillod
 *
 *		$Id: rigclass.cc,v 1.6 2001-12-20 07:46:12 fillods Exp $
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

	if (RIG_LEVEL_IS_FLOAT(level))
		THROW(new RigException (-RIG_EINVAL));

	CHECK_RIG( rig_get_level(theRig, vfo, level, &val) );
	vali = val.i;
}

void Rig::getLevel(setting_t level, float& valf, vfo_t vfo)
{
	value_t val;

	if (!RIG_LEVEL_IS_FLOAT(level))
		THROW(new RigException (-RIG_EINVAL));

	CHECK_RIG( rig_get_level(theRig, vfo, level, &val) );
	valf = val.f;
}

int Rig::getLevelI(setting_t level, vfo_t vfo)
{
	value_t val;

	if (RIG_LEVEL_IS_FLOAT(level))
		THROW(new RigException (-RIG_EINVAL));

	CHECK_RIG( rig_get_level(theRig, vfo, level, &val) );
	return val.i;
}

float Rig::getLevelF(setting_t level, vfo_t vfo)
{
	value_t val;

	if (!RIG_LEVEL_IS_FLOAT(level))
		THROW(new RigException (-RIG_EINVAL));

	CHECK_RIG( rig_get_level(theRig, vfo, level, &val) );
	return val.f;
}

void Rig::setSplitFreq(freq_t tx_freq, vfo_t vfo) {
	CHECK_RIG( rig_set_split_freq(theRig, vfo, tx_freq) );
}

freq_t Rig::getSplitFreq(vfo_t vfo)
{
	freq_t freq;

	CHECK_RIG( rig_get_split_freq(theRig, vfo, &freq) );

	return freq;
}

void Rig::setSplitMode(rmode_t mode, pbwidth_t width, vfo_t vfo) {
	CHECK_RIG(rig_set_split_mode(theRig, vfo, mode, width));
}

rmode_t Rig::getSplitMode(pbwidth_t& width, vfo_t vfo) {
	rmode_t mode;

	CHECK_RIG( rig_get_split_mode(theRig, vfo, &mode, &width) );

	return mode;
}

void Rig::setSplit(split_t split, vfo_t vfo) {
	CHECK_RIG(rig_set_split(theRig, vfo, split));
}

split_t Rig::getSplit(vfo_t vfo) {
	split_t split;

	CHECK_RIG( rig_get_split(theRig, vfo, &split) );

	return split;
}

setting_t Rig::hasGetLevel (setting_t level)
{
	return rig_has_get_level(theRig, level);
}
setting_t Rig::hasSetLevel (setting_t level)
{
	return rig_has_set_level(theRig, level);
}

const char *Rig::getInfo (void)
{
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

void Rig::setRptrShift (rptr_shift_t rptr_shift, vfo_t vfo = RIG_VFO_CURR)
{
	CHECK_RIG( rig_set_rptr_shift(theRig, vfo, rptr_shift) );
}

rptr_shift_t Rig::getRptrShift (vfo_t vfo = RIG_VFO_CURR)
{
	rptr_shift_t rptr_shift;

	CHECK_RIG( rig_get_rptr_shift(theRig, vfo, &rptr_shift) );

	return rptr_shift;
}

void Rig::setRptrOffs (shortfreq_t rptr_offs, vfo_t vfo = RIG_VFO_CURR)
{
	CHECK_RIG( rig_set_rptr_offs(theRig, vfo, rptr_offs) );
}

shortfreq_t Rig::getRptrOffs (vfo_t vfo = RIG_VFO_CURR)
{
	shortfreq_t rptr_offs;

	CHECK_RIG( rig_get_rptr_offs(theRig, vfo, &rptr_offs) );

	return rptr_offs;
}

void Rig::setTs (shortfreq_t ts, vfo_t vfo = RIG_VFO_CURR)
{
	CHECK_RIG( rig_set_ts(theRig, vfo, ts) );
}

shortfreq_t Rig::getTs (vfo_t vfo = RIG_VFO_CURR)
{
	shortfreq_t ts;

	CHECK_RIG( rig_get_ts(theRig, vfo, &ts) );

	return ts;
}

void Rig::setFunc (setting_t func, bool status, vfo_t vfo = RIG_VFO_CURR)
{
	CHECK_RIG( rig_set_func(theRig, vfo, func, status? 1:0) );
}

bool Rig::getFunc (setting_t func, vfo_t vfo = RIG_VFO_CURR)
{
	int status;

	CHECK_RIG( rig_get_func(theRig, vfo, func, &status) );

	return status ? true : false;
}



shortfreq_t Rig::getResolution (rmode_t mode)
{
	shortfreq_t res;

	res = rig_get_resolution(theRig, mode);
	if (res < 0)
		THROW(new RigException (res));

	return res;
}

void Rig::reset (reset_t reset)
{
	CHECK_RIG( rig_reset(theRig, reset) );
}

bool Rig::hasGetFunc (setting_t func)
{
	return rig_has_get_func(theRig, func)==func;
}
bool Rig::hasSetFunc (setting_t func)
{
	return rig_has_set_func(theRig, func)==func;
}

unsigned int Rig::power2mW (float power, freq_t freq, rmode_t mode)
{
	unsigned int mwpower;

	CHECK_RIG( rig_power2mW(theRig, &mwpower, power, freq, mode) );

	return mwpower;
}

float Rig::mW2power (unsigned int mwpower, freq_t freq, rmode_t mode)
{
	float power;

	CHECK_RIG( rig_mW2power(theRig, &power, mwpower, freq, mode) );

	return power;
}

void Rig::setTrn (int trn)
{
	CHECK_RIG( rig_set_trn(theRig, trn) );
}

int Rig::getTrn ()
{
	int trn;

	CHECK_RIG( rig_get_trn(theRig, &trn) );

	return trn;
}

void Rig::setBank (int bank, vfo_t vfo = RIG_VFO_CURR)
{
	CHECK_RIG( rig_set_ts(theRig, vfo, bank) );
}

void Rig::setMem (int ch, vfo_t vfo = RIG_VFO_CURR)
{
	CHECK_RIG( rig_set_mem(theRig, vfo, ch) );
}

int Rig::getMem (vfo_t vfo = RIG_VFO_CURR)
{
	int mem;

	CHECK_RIG( rig_get_mem(theRig, vfo, &mem) );

	return mem;
}


void Rig::setPowerStat (powerstat_t status)
{
	CHECK_RIG( rig_set_powerstat(theRig, status) );
}

powerstat_t Rig::getPowerStat (void)
{
	powerstat_t status;

	CHECK_RIG( rig_get_powerstat(theRig, &status) );

	return status;
}

rmode_t Rig::RngRxModes (freq_t freq)
{
	rmode_t modes = RIG_MODE_NONE;
	freq_range_t *rng;
	int i;

	for (i=0; i<FRQRANGESIZ; i++) {
		rng = &theRig->state.rx_range_list[i];
		if (RIG_IS_FRNG_END(*rng)) {
			return modes;
		}
		if (freq >= rng->start && freq <= rng->end)
			modes |= rng->modes;
	}

	return modes;
}

rmode_t Rig::RngTxModes (freq_t freq)
{
	rmode_t modes = RIG_MODE_NONE;
	freq_range_t *rng;
	int i;

	for (i=0; i<FRQRANGESIZ; i++) {
		rng = &theRig->state.tx_range_list[i];
		if (RIG_IS_FRNG_END(*rng)) {
			return modes;
		}
		if (freq >= rng->start && freq <= rng->end)
			modes |= rng->modes;
	}

	return modes;
}

