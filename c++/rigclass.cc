/**
 * \file src/rigclass.cc
 * \brief Ham Radio Control Libraries C++ interface
 * \author Stephane Fillod
 * \date 2001-2003
 *
 * Hamlib C++ interface is a frontend implementing wrapper functions.
 */

/*
 *  Hamlib C++ bindings - main file
 *  Copyright (c) 2001-2003 by Stephane Fillod
 *
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <hamlib/config.h>

#include <hamlib/rig.h>
#include <hamlib/rigclass.h>

#define CHECK_RIG(cmd) { int _retval = cmd; if (_retval != RIG_OK) \
							THROW(new RigException (_retval)); }

static int hamlibpp_freq_event(RIG *rig, vfo_t vfo, freq_t freq, rig_ptr_t arg);





static int hamlibpp_freq_event(RIG *rig, vfo_t vfo, freq_t freq, rig_ptr_t arg)
{
	if (!rig || !rig->state.obj)
		return -RIG_EINVAL;

/* assert rig == ((Rig*)rig->state.obj).theRig */
	return (static_cast<Rig*>(rig->state.obj))->FreqEvent(vfo, freq, arg);
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

void Rig::setConf(token_t token, const char *val)
{
	CHECK_RIG( rig_set_conf(theRig, token, val) );
}
void Rig::setConf(const char *name, const char *val)
{
	CHECK_RIG( rig_set_conf(theRig, tokenLookup(name), val) );
}

void Rig::getConf(token_t token, char *val)
{
	CHECK_RIG( rig_get_conf(theRig, token, val) );
}
void Rig::getConf(const char *name, char *val)
{
	CHECK_RIG( rig_get_conf(theRig, tokenLookup(name), val) );
}

token_t Rig::tokenLookup(const char *name)
{
	return rig_token_lookup(theRig, name);
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

void Rig::setParm(setting_t parm, int vali)
{
	value_t val;

	val.i = vali;
	CHECK_RIG( rig_set_parm(theRig, parm, val) );
}

void Rig::setParm(setting_t parm, float valf)
{
	value_t val;

	val.f = valf;
	CHECK_RIG( rig_set_parm(theRig, parm, val) );
}


void Rig::getParm(setting_t parm, int& vali)
{
	value_t val;

	if (RIG_LEVEL_IS_FLOAT(parm))
		THROW(new RigException (-RIG_EINVAL));

	CHECK_RIG( rig_get_parm(theRig, parm, &val) );
	vali = val.i;
}

void Rig::getParm(setting_t parm, float& valf)
{
	value_t val;

	if (!RIG_LEVEL_IS_FLOAT(parm))
		THROW(new RigException (-RIG_EINVAL));

	CHECK_RIG( rig_get_parm(theRig, parm, &val) );
	valf = val.f;
}

int Rig::getParmI(setting_t parm)
{
	value_t val;

	if (RIG_LEVEL_IS_FLOAT(parm))
		THROW(new RigException (-RIG_EINVAL));

	CHECK_RIG( rig_get_parm(theRig, parm, &val) );
	return val.i;
}

float Rig::getParmF(setting_t parm)
{
	value_t val;

	if (!RIG_LEVEL_IS_FLOAT(parm))
		THROW(new RigException (-RIG_EINVAL));

	CHECK_RIG( rig_get_parm(theRig, parm, &val) );
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

void Rig::setSplitFreqMode(freq_t tx_freq, rmode_t mode, pbwidth_t width, vfo_t vfo) {
  CHECK_RIG (rig_set_split_freq_mode (theRig, vfo, tx_freq, mode, width));
}

freq_t Rig::getSplitFreqMode(rmode_t& mode, pbwidth_t& width, vfo_t vfo) {
  freq_t freq;

  CHECK_RIG (rig_get_split_freq_mode (theRig, vfo, &freq, &mode, &width));

  return freq;
}

void Rig::setSplitVFO(split_t split, vfo_t vfo, vfo_t tx_vfo) {
	CHECK_RIG(rig_set_split_vfo(theRig, vfo, split, tx_vfo));
}

split_t Rig::getSplitVFO(vfo_t &tx_vfo, vfo_t vfo) {
	split_t split;

	CHECK_RIG( rig_get_split_vfo(theRig, vfo, &split, &tx_vfo) );

	return split;
}

bool Rig::hasGetLevel (setting_t level)
{
	return rig_has_get_level(theRig, level) == level;
}
bool Rig::hasSetLevel (setting_t level)
{
	return rig_has_set_level(theRig, level) == level;
}

bool Rig::hasGetParm (setting_t parm)
{
	return rig_has_get_parm(theRig, parm) == parm;
}
bool Rig::hasSetParm (setting_t parm)
{
	return rig_has_set_parm(theRig, parm) == parm;
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

void Rig::setRptrShift (rptr_shift_t rptr_shift, vfo_t vfo)
{
	CHECK_RIG( rig_set_rptr_shift(theRig, vfo, rptr_shift) );
}

rptr_shift_t Rig::getRptrShift (vfo_t vfo)
{
	rptr_shift_t rptr_shift;

	CHECK_RIG( rig_get_rptr_shift(theRig, vfo, &rptr_shift) );

	return rptr_shift;
}

void Rig::setRptrOffs (shortfreq_t rptr_offs, vfo_t vfo)
{
	CHECK_RIG( rig_set_rptr_offs(theRig, vfo, rptr_offs) );
}

shortfreq_t Rig::getRptrOffs (vfo_t vfo)
{
	shortfreq_t rptr_offs;

	CHECK_RIG( rig_get_rptr_offs(theRig, vfo, &rptr_offs) );

	return rptr_offs;
}

void Rig::setTs (shortfreq_t ts, vfo_t vfo)
{
	CHECK_RIG( rig_set_ts(theRig, vfo, ts) );
}

shortfreq_t Rig::getTs (vfo_t vfo)
{
	shortfreq_t ts;

	CHECK_RIG( rig_get_ts(theRig, vfo, &ts) );

	return ts;
}

void Rig::setCTCSS (tone_t tone, vfo_t vfo)
{
	CHECK_RIG( rig_set_ctcss_tone(theRig, vfo, tone) );
}

tone_t Rig::getCTCSS (vfo_t vfo)
{
	tone_t tone;

	CHECK_RIG( rig_get_ctcss_tone(theRig, vfo, &tone) );

	return tone;
}

void Rig::setDCS (tone_t code, vfo_t vfo)
{
	CHECK_RIG( rig_set_dcs_code(theRig, vfo, code) );
}

tone_t Rig::getDCS (vfo_t vfo)
{
	tone_t code;

	CHECK_RIG( rig_get_dcs_code(theRig, vfo, &code) );

	return code;
}

void Rig::setCTCSSsql (tone_t tone, vfo_t vfo)
{
	CHECK_RIG( rig_set_ctcss_sql(theRig, vfo, tone) );
}

tone_t Rig::getCTCSSsql (vfo_t vfo)
{
	tone_t tone;

	CHECK_RIG( rig_get_ctcss_sql(theRig, vfo, &tone) );

	return tone;
}

void Rig::setDCSsql (tone_t code, vfo_t vfo)
{
	CHECK_RIG( rig_set_dcs_sql(theRig, vfo, code) );
}

tone_t Rig::getDCSsql (vfo_t vfo)
{
	tone_t code;

	CHECK_RIG( rig_get_dcs_sql(theRig, vfo, &code) );

	return code;
}


void Rig::setFunc (setting_t func, bool status, vfo_t vfo)
{
	CHECK_RIG( rig_set_func(theRig, vfo, func, status? 1:0) );
}

bool Rig::getFunc (setting_t func, vfo_t vfo)
{
	int status;

	CHECK_RIG( rig_get_func(theRig, vfo, func, &status) );

	return status ? true : false;
}

void Rig::VFOop (vfo_op_t op, vfo_t vfo)
{
	CHECK_RIG( rig_vfo_op(theRig, vfo, op) );
}

bool Rig::hasVFOop (vfo_op_t op)
{
	return rig_has_vfo_op(theRig, op)==op;
}

void Rig::scan (scan_t scan, int ch, vfo_t vfo)
{
	CHECK_RIG( rig_scan(theRig, vfo, scan, ch) );
}

bool Rig::hasScan (scan_t scan)
{
	return rig_has_scan(theRig, scan)==scan;
}


void Rig::setRit(shortfreq_t rit, vfo_t vfo)
{
	CHECK_RIG(rig_set_rit(theRig, vfo, rit));
}

shortfreq_t Rig::getRit(vfo_t vfo)
{
	shortfreq_t rit;

	CHECK_RIG( rig_get_rit(theRig, vfo, &rit) );

	return rit;
}

void Rig::setXit(shortfreq_t xit, vfo_t vfo)
{
	CHECK_RIG(rig_set_xit(theRig, vfo, xit));
}

shortfreq_t Rig::getXit(vfo_t vfo)
{
	shortfreq_t xit;

	CHECK_RIG( rig_get_xit(theRig, vfo, &xit) );

	return xit;
}

void Rig::setAnt(const value_t option, ant_t ant, vfo_t vfo)
{
	CHECK_RIG(rig_set_ant(theRig, vfo, ant, option));
}

ant_t Rig::getAnt(ant_t &ant_rx, ant_t &ant_tx, ant_t ant, value_t &option, ant_t &ant_curr, vfo_t vfo)
{
	CHECK_RIG( rig_get_ant(theRig, vfo, ant, &option, &ant_curr, &ant_tx, &ant_rx) );

	return ant;
}

void Rig::sendDtmf(const char *digits, vfo_t vfo)
{
	CHECK_RIG(rig_send_dtmf(theRig, vfo, digits));
}

int Rig::recvDtmf(char *digits, vfo_t vfo)
{
	int len;

	CHECK_RIG( rig_recv_dtmf(theRig, vfo, digits, &len) );

	return len;
}

void Rig::sendMorse(const char *msg, vfo_t vfo)
{
	CHECK_RIG(rig_send_morse(theRig, vfo, msg));
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

void Rig::setBank (int bank, vfo_t vfo)
{
	CHECK_RIG( rig_set_ts(theRig, vfo, bank) );
}

void Rig::setMem (int ch, vfo_t vfo)
{
	CHECK_RIG( rig_set_mem(theRig, vfo, ch) );
}

int Rig::getMem (vfo_t vfo)
{
	int mem;

	CHECK_RIG( rig_get_mem(theRig, vfo, &mem) );

	return mem;
}

void Rig::setChannel (const channel_t *chan, vfo_t vfo)
{
	CHECK_RIG( rig_set_channel(theRig, vfo, chan) );
}

void Rig::getChannel (channel_t *chan,vfo_t vfo, int readOnly)
{
	CHECK_RIG( rig_get_channel(theRig, vfo, chan, readOnly) );
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
	unsigned modes = RIG_MODE_NONE;
	int i;

	for (i=0; i<HAMLIB_FRQRANGESIZ; i++) {
		freq_range_t *rng = &theRig->state.rx_range_list[i];
		if (RIG_IS_FRNG_END(*rng)) {
			return (rmode_t)modes;
		}
		if (freq >= rng->startf && freq <= rng->endf)
			modes |= (unsigned)rng->modes;
	}

	return (rmode_t)modes;
}

rmode_t Rig::RngTxModes (freq_t freq)
{
	unsigned modes = RIG_MODE_NONE;
	int i;

	for (i=0; i<HAMLIB_FRQRANGESIZ; i++) {
	  freq_range_t *rng = &theRig->state.tx_range_list[i];
		if (RIG_IS_FRNG_END(*rng)) {
			return (rmode_t)modes;
		}
		if (freq >= rng->startf && freq <= rng->endf)
			modes |= (unsigned)rng->modes;
	}

	return (rmode_t)modes;
}

