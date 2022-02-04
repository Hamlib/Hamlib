/**
 * \file src/ampclass.cc
 * \brief Ham Radio Control Libraries C++ interface
 * \author Stephane Fillod
 * \date 2002
 *
 * Hamlib C++ interface is a frontend implementing wrapper functions.
 */

/*
 *  Hamlib C++ bindings - main file
 *  Copyright (c) 2002 by Stephane Fillod
 *  Copyright (c) 2019 by Michael Black W9MDB
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

//#include <hamlib/ampator.h>
#include <hamlib/ampclass.h>
#include <hamlib/rigclass.h>

#define CHECK_AMP(cmd) { int _retval = cmd; if (_retval != RIG_OK) \
							THROW(new RigException (_retval)); }



Amplifier::Amplifier(amp_model_t amp_model)
{
	theAmp = amp_init(amp_model);
   	if (!theAmp)
		THROW(new RigException ("Amplifier initialization error"));

	caps = theAmp->caps;
	theAmp->state.obj = (amp_ptr_t)this;
}

Amplifier::~Amplifier()
{
	theAmp->state.obj = NULL;
	CHECK_AMP( amp_cleanup(theAmp) );
	caps = NULL;
}

void Amplifier::open(void) {
	CHECK_AMP( amp_open(theAmp) );
}

void Amplifier::close(void) {
	CHECK_AMP( amp_close(theAmp) );
}

void Amplifier::setConf(token_t token, const char *val)
{
	CHECK_AMP( amp_set_conf(theAmp, token, val) );
}
void Amplifier::setConf(const char *name, const char *val)
{
	CHECK_AMP( amp_set_conf(theAmp, tokenLookup(name), val) );
}

void Amplifier::getConf(token_t token, char *val)
{
	CHECK_AMP( amp_get_conf(theAmp, token, val) );
}
void Amplifier::getConf(const char *name, char *val)
{
	CHECK_AMP( amp_get_conf(theAmp, tokenLookup(name), val) );
}

token_t Amplifier::tokenLookup(const char *name)
{
	return amp_token_lookup(theAmp, name);
}

void Amplifier::reset (amp_reset_t reset)
{
	CHECK_AMP( amp_reset(theAmp, reset) );
}

void Amplifier::setFreq(freq_t freq) {
  CHECK_AMP( amp_set_freq(theAmp, freq) );
}

freq_t Amplifier::getFreq()
{
  freq_t freq;

  CHECK_AMP( amp_get_freq(theAmp, &freq) );

  return freq;
}

