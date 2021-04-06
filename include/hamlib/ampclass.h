/*
 *  Hamlib C++ bindings - amplifier API header
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

#ifndef _AMPCLASS_H
#define _AMPCLASS_H 1

#include <hamlib/amplifier.h>



//! @cond Doxygen_Suppress
class HAMLIB_CPP_IMPEXP Amplifier
{
private:
    AMP *theAmp;  // Global ref. to the amp

protected:
public:
    explicit Amplifier(amp_model_t amp_model);

    virtual ~Amplifier();

#if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201103L) || __cplusplus >= 201103L)
    Amplifier(const Amplifier&) = default;
    Amplifier(Amplifier&&) = default;
    Amplifier& operator=(const Amplifier&) = default;
    Amplifier& operator=(Amplifier&&) = default;
#endif

    const struct amp_caps *caps;

    // This method opens the communication port to the amp
    void open(void);

    // This method closes the communication port to the amp
    void close(void);

    void setConf(token_t token, const char *val);
    void setConf(const char *name, const char *val);
    void getConf(token_t token, char *val);
    void getConf(const char *name, char *val);
    token_t tokenLookup(const char *name);

    void setFreq(freq_t freq);
    freq_t getFreq();

    void reset(amp_reset_t reset);
};
//! @endcond



#endif  // _AMPCLASS_H
