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

#include "indi_wrapper.hpp"

#include <math.h>
#include <limits.h>

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

static std::unique_ptr<RotINDIClient> indi_wrapper_client(new RotINDIClient());

int RotINDIClient::setSpeed(int speedPercent)
{
    if (!mTelescope || !mTelescope->isConnected())
    {
        rig_debug(RIG_DEBUG_ERR, "indi: telescope not connected\n");
        return -RIG_EIO;
    }

    ISwitchVectorProperty *switchVector =
        mTelescope->getSwitch("TELESCOPE_SLEW_RATE");

    if (!switchVector)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "indi: unable to find telescope or TELESCOPE_SLEW_RATE switch\n");
        return -RIG_EPROTO;
    }

    if (speedPercent < 0)
    {
        speedPercent = 0;
    }

    if (speedPercent > 100)
    {
        speedPercent = 100;
    }

    unsigned int speed = DIV_ROUND_UP(speedPercent, 10);

    for (unsigned int i = 1; i <= 10; i++)
    {
        char switchName[4];
        snprintf(switchName, sizeof(switchName), "%ux", i);

        ISwitch *slewrate = IUFindSwitch(switchVector, switchName);

        if (slewrate)
        {
            if (speed == i)
            {
                rig_debug(RIG_DEBUG_VERBOSE, "indi: setting speed %s\n", switchName);
                slewrate->s = ISS_ON;
            }
            else
            {
                slewrate->s = ISS_OFF;
            }
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "indi: unable to find switch member %s\n", switchName);
            return -RIG_EPROTO;
        }
    }

    sendNewSwitch(switchVector);

    return RIG_OK;
}

int RotINDIClient::move(int direction, int speedPercent)
{
    if (!mTelescope || !mTelescope->isConnected())
    {
        rig_debug(RIG_DEBUG_ERR, "indi: telescope not connected\n");
        return -RIG_EIO;
    }

    int err = setSpeed(speedPercent);

    if (err != RIG_OK)
    {
        return err;
    }

    ISwitchVectorProperty *switchVector =
        mTelescope->getSwitch("TELESCOPE_MOTION_NS");

    if (!switchVector)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "indi: unable to find telescope or TELESCOPE_MOTION_NS switch\n");
        return -RIG_EPROTO;
    }

    ISwitch *motion_north = IUFindSwitch(switchVector, "MOTION_NORTH");

    if (!motion_north)
    {
        rig_debug(RIG_DEBUG_ERR, "indi: unable to find switch member MOTION_NORTH\n");
        return -RIG_EPROTO;
    }

    if (direction & ROT_MOVE_UP)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "indi: moving up\n");
        motion_north->s = ISS_ON;
    }
    else
    {
        motion_north->s = ISS_OFF;
    }

    ISwitch *motion_south = IUFindSwitch(switchVector, "MOTION_SOUTH");

    if (!motion_south)
    {
        rig_debug(RIG_DEBUG_ERR, "indi: unable to find switch member MOTION_SOUTH\n");
        return -RIG_EPROTO;
    }

    if (direction & ROT_MOVE_DOWN)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "indi: moving down\n");
        motion_south->s = ISS_ON;
    }
    else
    {
        motion_south->s = ISS_OFF;
    }

    sendNewSwitch(switchVector);

    switchVector = mTelescope->getSwitch("TELESCOPE_MOTION_WE");

    if (!switchVector)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "indi: unable to find telescope or TELESCOPE_MOTION_WE switch\n");
        return -RIG_EPROTO;
    }

    ISwitch *motion_west = IUFindSwitch(switchVector, "MOTION_WEST");

    if (!motion_west)
    {
        rig_debug(RIG_DEBUG_ERR, "indi: unable to find switch member MOTION_WEST\n");
        return -RIG_EPROTO;
    }

    if (direction & ROT_MOVE_LEFT)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "indi: moving left\n");
        motion_west->s = ISS_ON;
    }
    else
    {
        motion_west->s = ISS_OFF;
    }

    ISwitch *motion_east = IUFindSwitch(switchVector, "MOTION_EAST");

    if (!motion_east)
    {
        rig_debug(RIG_DEBUG_ERR, "indi: unable to find switch member MOTION_RIGHT\n");
        return -RIG_EPROTO;
    }

    if (direction & ROT_MOVE_RIGHT)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "indi: moving right\n");
        motion_east->s = ISS_ON;
    }
    else
    {
        motion_east->s = ISS_OFF;
    }

    sendNewSwitch(switchVector);

    return RIG_OK;
}

int RotINDIClient::stop()
{
    if (!mTelescope || !mTelescope->isConnected())
    {
        rig_debug(RIG_DEBUG_ERR, "indi: telescope not connected\n");
        return -RIG_EIO;
    }

    ISwitchVectorProperty *switchVector =
        mTelescope->getSwitch("TELESCOPE_ABORT_MOTION");

    if (!switchVector)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "indi: unable to find telescope or TELESCOPE_ABORT_MOTION switch\n");
        return -RIG_EPROTO;
    }

    ISwitch *sw = IUFindSwitch(switchVector, "ABORT");

    if (!sw)
    {
        rig_debug(RIG_DEBUG_ERR, "indi: unable to find switch member ABORT_MOTION\n");
        return -RIG_EPROTO;
    }

    sw->s = ISS_ON;
    sendNewSwitch(switchVector);

    return RIG_OK;
}

int RotINDIClient::park()
{
    if (!mTelescope)
    {
        return -RIG_EIO;
    }

    if (!mTelescope->isConnected())
    {
        rig_debug(RIG_DEBUG_ERR, "indi: telescope not connected\n");
        return -RIG_EIO;
    }

    ISwitchVectorProperty *switchVector = mTelescope->getSwitch("TELESCOPE_PARK");

    if (!switchVector)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "indi: unable to find telescope or TELESCOPE_PARK switch\n");
        return -RIG_EPROTO;
    }

    ISwitch *unpark = IUFindSwitch(switchVector, "UNPARK");

    if (!unpark)
    {
        rig_debug(RIG_DEBUG_ERR, "indi: unable to find switch member UNPARK\n");
        return -RIG_EPROTO;
    }

    unpark->s = ISS_OFF;

    ISwitch *park = IUFindSwitch(switchVector, "PARK");

    if (!park)
    {
        rig_debug(RIG_DEBUG_ERR, "indi: unable to find switch member PARK\n");
        return -RIG_EPROTO;
    }

    park->s = ISS_ON;

    sendNewSwitch(switchVector);

    return RIG_OK;
}

int RotINDIClient::unPark()
{
    if (!mTelescope || !mTelescope->isConnected())
    {
        rig_debug(RIG_DEBUG_ERR, "indi: telescope not connected\n");
        return -RIG_EIO;
    }

    ISwitchVectorProperty *switchVector = mTelescope->getSwitch("TELESCOPE_PARK");

    if (!switchVector)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "indi: unable to find telescope or TELESCOPE_PARK switch\n");
        return -RIG_EPROTO;
    }

    ISwitch *park = IUFindSwitch(switchVector, "PARK");

    if (!park)
    {
        rig_debug(RIG_DEBUG_ERR, "indi: unable to find switch member PARK\n");
        return -RIG_EPROTO;
    }

    park->s = ISS_OFF;

    ISwitch *unpark = IUFindSwitch(switchVector, "UNPARK");

    if (!unpark)
    {
        rig_debug(RIG_DEBUG_ERR, "indi: unable to find switch member UNPARK\n");
        return -RIG_EPROTO;
    }

    unpark->s = ISS_ON;

    sendNewSwitch(switchVector);

    return RIG_OK;
}

void RotINDIClient::position(azimuth_t *az, elevation_t *el)
{
    *az = mAz;
    *el = mEl;
}

double RotINDIClient::getPositionDiffBetween(double deg1, double deg2)
{
    return fabs(deg1 - deg2);
}

double RotINDIClient::getPositionDiffOutside(double deg1, double deg2,
        double minDeg, double maxDeg)
{
    if (deg1 < deg2)
    {
        return getPositionDiffBetween(minDeg, deg1) + getPositionDiffBetween(deg2,
                maxDeg);
    }

    return getPositionDiffBetween(minDeg, deg2) + getPositionDiffBetween(deg1,
            maxDeg);
}

double RotINDIClient::getPositionDiff(double deg1, double deg2, double minDeg,
                                      double maxDeg)
{
    double between = getPositionDiffBetween(deg1, deg2);
    double outside = getPositionDiffOutside(deg1, deg2, minDeg, maxDeg);

    if (between < outside)
    {
        return between;
    }

    return outside;
}

int RotINDIClient::setPosition(azimuth_t az, elevation_t el)
{
    if (!mTelescope || !mTelescope->isConnected())
    {
        rig_debug(RIG_DEBUG_ERR, "indi: telescope not connected\n");
        return -RIG_EIO;
    }

    if (fabs(mDstAz - az) < 0.001 && fabs(mDstEl - el) < 0.001)
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "indi: ignoring new position, already approaching\n");
        return RIG_OK;
    }

    // Each coordinate set command stops and restarts the rotator, so we have to avoid unnecessary
    // set calls by defining a range of distance from the new az/el coords which we ignore until
    // we are closer to the new az/el coords than this value.
    const int quickJumpRangeDegrees = 10;

    double currDstNewDstAzDiff = getPositionDiff(mDstAz, az, 0, 360);
    double currDstNewDstElDiff = getPositionDiff(mDstEl, el, -90, 90);
    double currDstNewDstDistance = sqrt(pow(currDstNewDstAzDiff,
                                            2) + pow(currDstNewDstElDiff, 2));
    double currPosNewDstAzDiff = getPositionDiff(mAz, az, 0, 360);
    double currPosNewDstElDiff = getPositionDiff(mEl, el, -90, 90);
    double currPosNewDstDistance = sqrt(pow(currPosNewDstAzDiff,
                                            2) + pow(currPosNewDstElDiff, 2));

    if (currDstNewDstDistance < quickJumpRangeDegrees
            && currPosNewDstDistance > quickJumpRangeDegrees)
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "indi: ignoring new position, approaching quickly, newDst/currDst distance: %f newDst/currPos distance: %f\n",
                  currDstNewDstDistance, currPosNewDstDistance);
        return RIG_OK;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "indi: setting position to az: %f el: %f\n", az,
              el);

    mDstAz = az;
    mDstEl = el;

    ISwitchVectorProperty *switchVector = mTelescope->getSwitch("ON_COORD_SET");

    if (!switchVector)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "indi: unable to find telescope or ON_COORD_SET switch\n");
        return -RIG_EPROTO;
    }

    ISwitch *slew = IUFindSwitch(switchVector, "SLEW");

    if (!slew)
    {
        rig_debug(RIG_DEBUG_ERR, "indi: unable to find switch member SLEW\n");
        return -RIG_EPROTO;
    }

    slew->s = ISS_OFF;

    ISwitch *track = IUFindSwitch(switchVector, "TRACK");

    if (!track)
    {
        rig_debug(RIG_DEBUG_ERR, "indi: unable to find switch member TRACK\n");
        return -RIG_EPROTO;
    }

    track->s = ISS_ON;

    ISwitch *sync = IUFindSwitch(switchVector, "SYNC");

    if (!sync)
    {
        rig_debug(RIG_DEBUG_ERR, "indi: unable to find switch member SYNC\n");
        return -RIG_EPROTO;
    }

    sync->s = ISS_OFF;

    sendNewSwitch(switchVector);

    INumberVectorProperty *property = mTelescope->getNumber("HORIZONTAL_COORD");

    if (!property)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "indi: unable to find telescope or HORIZONTAL_COORD property\n");
        return -RIG_EPROTO;
    }

    property->np[0].value = az;
    property->np[1].value = el;
    sendNewNumber(property);

    return RIG_OK;
}

void RotINDIClient::removeDevice(INDI::BaseDevice *dp) {}
void RotINDIClient::newProperty(INDI::Property *property)
{
    std::string name(property->getName());

    if (!mTelescope && name == "TELESCOPE_INFO")
    {
        mTelescope = property->getBaseDevice();
        rig_debug(RIG_DEBUG_VERBOSE, "indi: using device %s\n",
                  mTelescope->getDeviceName());
        watchDevice(mTelescope->getDeviceName());

        if (!mTelescope->isConnected())
        {
            connectDevice(mTelescope->getDeviceName());
        }

        mDstAz = INT_MAX;
        mDstEl = INT_MAX;
    }

    if (name == "HORIZONTAL_COORD")
    {
        mAz = property->getNumber()->np[0].value;
        mEl = property->getNumber()->np[1].value;
    }
}
void RotINDIClient::removeProperty(INDI::Property *property) {}
void RotINDIClient::newBLOB(IBLOB *bp) {}
void RotINDIClient::newSwitch(ISwitchVectorProperty *svp) {}
void RotINDIClient::newNumber(INumberVectorProperty *nvp)
{
    std::string name(nvp->name);

    if (name == "HORIZONTAL_COORD")
    {
        mAz = nvp->np[0].value;
        mEl = nvp->np[1].value;
    }
}
void RotINDIClient::newMessage(INDI::BaseDevice *dp, int messageID) {}
void RotINDIClient::newText(ITextVectorProperty *tvp) {}
void RotINDIClient::newLight(ILightVectorProperty *lvp) {}
void RotINDIClient::newDevice(INDI::BaseDevice *dp) {}

void RotINDIClient::serverConnected()
{
    rig_debug(RIG_DEBUG_VERBOSE, "indi: server connected\n");
}

void RotINDIClient::serverDisconnected(int exit_code)
{
    rig_debug(RIG_DEBUG_VERBOSE, "indi: server disconnected\n");
}

const char *RotINDIClient::getInfo(void)
{
    if (mTelescope && mTelescope->isConnected())
    {
        static char info[128];
        snprintf(info, sizeof(info), "using INDI device %s",
                 mTelescope->getDeviceName());
        return info;
    }

    return "no INDI device connected";
}

void RotINDIClient::close(void)
{
    if (mTelescope && mTelescope->isConnected())
    {
        disconnectDevice(mTelescope->getDeviceName());
    }

    disconnectServer();
}

extern "C" int indi_wrapper_set_position(ROT *rot, azimuth_t az,
        elevation_t el)
{
    int res;

    rig_debug(RIG_DEBUG_TRACE, "%s called: az=%f el=%f\n", __func__, az, el);

    res = indi_wrapper_client->unPark();

    if (res != RIG_OK)
    {
        return res;
    }

    return indi_wrapper_client->setPosition(az, el);
}

extern "C" int indi_wrapper_get_position(ROT *rot, azimuth_t *az,
        elevation_t *el)
{
    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    indi_wrapper_client->position(az, el);

    return RIG_OK;
}

extern "C" int indi_wrapper_stop(ROT *rot)
{
    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    return indi_wrapper_client->stop();
}

extern "C" int indi_wrapper_park(ROT *rot)
{
    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    return indi_wrapper_client->park();
}

extern "C" int indi_wrapper_move(ROT *rot, int direction, int speed)
{
    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    return indi_wrapper_client->move(direction, speed);
}

extern "C" const char *indi_wrapper_get_info(ROT *rot)
{
    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    return indi_wrapper_client->getInfo();
}

extern "C" int indi_wrapper_open(ROT *rot)
{
    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    indi_wrapper_client->setServer("localhost", 7624);

    if (!indi_wrapper_client->connectServer())
    {
        rig_debug(RIG_DEBUG_ERR, "indi: server refused connection\n");
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

extern "C" int indi_wrapper_close(ROT *rot)
{
    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    indi_wrapper_client->close();

    return RIG_OK;
}
