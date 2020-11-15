/*
 *  Hamlib Rotator backend - INDI integration
 *  Copyright (c) 2020 by Norbert Varga HA2NON <nonoo@nonoo.hu>
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

#ifndef _INDI_WRAPPER_HPP
#define _INDI_WRAPPER_HPP 1

#include <libindi/basedevice.h>
#include <libindi/baseclient.h>

#include <hamlib/rotator.h>

class RotINDIClient : public INDI::BaseClient
{
public:
    int setSpeed(int speedPercent);
    int move(int direction, int speedPercent);
    int stop();
    int park();
    int unPark();
    void position(azimuth_t *az, elevation_t *el);
    int setPosition(azimuth_t az, elevation_t el);
    const char *getInfo();
    void close(void);

protected:
    virtual void newDevice(INDI::BaseDevice *dp);
    virtual void removeDevice(INDI::BaseDevice *dp);
    virtual void newProperty(INDI::Property *property);
    virtual void removeProperty(INDI::Property *property);
    virtual void newBLOB(IBLOB *bp);
    virtual void newSwitch(ISwitchVectorProperty *svp);
    virtual void newNumber(INumberVectorProperty *nvp);
    virtual void newMessage(INDI::BaseDevice *dp, int messageID);
    virtual void newText(ITextVectorProperty *tvp);
    virtual void newLight(ILightVectorProperty *lvp);
    virtual void serverConnected();
    virtual void serverDisconnected(int exit_code);

private:
	double getPositionDiffBetween(double deg1, double deg2);
	double getPositionDiffOutside(double deg1, double deg2, double minDeg, double maxDeg);
	double getPositionDiff(double deg1, double deg2, double minDeg, double maxDeg);

    INDI::BaseDevice *mTelescope;

    azimuth_t mDstAz;
    elevation_t mDstEl;

    azimuth_t mAz;
    elevation_t mEl;
};

#endif  // _INDI_WRAPPER_HPP
