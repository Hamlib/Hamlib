/**
 * \file src/rotclass.cc
 * \brief Ham Radio Control Libraries C++ interface
 * \author Stephane Fillod
 * \date 2002
 *
 * Hamlib C++ interface is a frontend implementing wrapper functions.
 */

/*
 *  Hamlib C++ bindings - main file
 *  Copyright (c) 2002 by Stephane Fillod
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

#include <hamlib/rotator.h>
#include <hamlib/rotclass.h>
#include <hamlib/rigclass.h>

#define CHECK_ROT(cmd) { int _retval = cmd; if (_retval != RIG_OK) \
							THROW(new RigException (_retval)); }



Rotator::Rotator(rot_model_t rot_model)
{
	theRot = rot_init(rot_model);
   	if (!theRot)
		THROW(new RigException ("Rotator initialization error"));

	caps = theRot->caps;
	theRot->state.obj = (rig_ptr_t)this;
}

Rotator::~Rotator()
{
	theRot->state.obj = NULL;
	CHECK_ROT( rot_cleanup(theRot) );
	caps = NULL;
}

void Rotator::open(void) {
	CHECK_ROT( rot_open(theRot) );
}

void Rotator::close(void) {
	CHECK_ROT( rot_close(theRot) );
}

void Rotator::setConf(token_t token, const char *val)
{
	CHECK_ROT( rot_set_conf(theRot, token, val) );
}
void Rotator::setConf(const char *name, const char *val)
{
	CHECK_ROT( rot_set_conf(theRot, tokenLookup(name), val) );
}

void Rotator::getConf(token_t token, char *val)
{
	CHECK_ROT( rot_get_conf(theRot, token, val) );
}
void Rotator::getConf(const char *name, char *val)
{
	CHECK_ROT( rot_get_conf(theRot, tokenLookup(name), val) );
}

token_t Rotator::tokenLookup(const char *name)
{
	return rot_token_lookup(theRot, name);
}

void Rotator::setPosition(azimuth_t az, elevation_t el)
{
	CHECK_ROT( rot_set_position(theRot, az, el) );
}

void Rotator::getPosition(azimuth_t& az, elevation_t& el)
{
	CHECK_ROT( rot_get_position(theRot, &az, &el) );
}

void Rotator::stop()
{
	CHECK_ROT(rot_stop(theRot));
}

void Rotator::park()
{
	CHECK_ROT(rot_park(theRot));
}

void Rotator::reset (rot_reset_t reset)
{
	CHECK_ROT( rot_reset(theRot, reset) );
}

void Rotator::move (int direction, int speed)
{
	CHECK_ROT( rot_move(theRot, direction, speed) );
}

