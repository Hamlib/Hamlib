#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import sys
# Change this path to match your "make install" path
sys.path.append('/usr/local/lib/python3.10/site-packages')

## Uncomment to run this script from an in-tree build (or adjust to the
## build directory) without installing the bindings.
#sys.path.append ('.')
#sys.path.append ('.libs')

import Hamlib

def StartUp():
    """Simple script to test the Hamlib.py module with Python3."""

    print("%s: Python %s; %s\n" \
          % (sys.argv[0], sys.version.split()[0], Hamlib.cvar.hamlib_version))

    Hamlib.rig_set_debug(Hamlib.RIG_DEBUG_NONE)

    # Init RIG_MODEL_DUMMY
    my_rig = Hamlib.Rig(Hamlib.RIG_MODEL_DUMMY)
    my_rig.set_conf("rig_pathname", "/dev/Rig")
    my_rig.set_conf("retry", "5")

    my_rig.open ()

    rpath = my_rig.get_conf("rig_pathname")
    retry = my_rig.get_conf("retry")

    print("status(str):\t\t%s" % Hamlib.rigerror(my_rig.error_status))
    print("get_conf:\t\tpath = %s, retry = %s" \
          % (rpath, retry))

    my_rig.set_freq(Hamlib.RIG_VFO_B, 5700000000)
    my_rig.set_vfo(Hamlib.RIG_VFO_B)

    print("freq:\t\t\t%s" % my_rig.get_freq())

    my_rig.set_freq(Hamlib.RIG_VFO_A, 145550000)
    (mode, width) = my_rig.get_mode(Hamlib.RIG_VFO_A)

    print("mode:\t\t\t%s\nbandwidth:\t\t%s" % (Hamlib.rig_strrmode(mode), width))

    my_rig.set_mode(Hamlib.RIG_MODE_CW)
    (mode, width) = my_rig.get_mode()

    print("mode:\t\t\t%s\nbandwidth:\t\t%s" % (Hamlib.rig_strrmode(mode), width))

    print("Backend copyright:\t%s" % my_rig.caps.copyright)
    print("Model:\t\t\t%s" % my_rig.caps.model_name)
    print("Manufacturer:\t\t%s" % my_rig.caps.mfg_name)
    print("Backend version:\t%s" % my_rig.caps.version)
    print("Backend status:\t\t%s" % Hamlib.rig_strstatus(my_rig.caps.status))
    print("Rig info:\t\t%s" % my_rig.get_info())

    my_rig.set_level("VOXDELAY",  1)

    print("VOX delay:\t\t%s" % my_rig.get_level_i("VOXDELAY"))

    my_rig.set_level(Hamlib.RIG_LEVEL_VOXDELAY, 5)

    print("VOX delay:\t\t%s" % my_rig.get_level_i(Hamlib.RIG_LEVEL_VOXDELAY))

    af = 12.34

    print("Setting AF to %0.2f...." % (af))

    my_rig.set_level("AF", af)

    print("status:\t\t\t%s - %s" % (my_rig.error_status,
                                    Hamlib.rigerror(my_rig.error_status)))

    print("AF level:\t\t%0.2f" % my_rig.get_level_f(Hamlib.RIG_LEVEL_AF))
    print("strength:\t\t%s" % my_rig.get_level_i(Hamlib.RIG_LEVEL_STRENGTH))
    print("status:\t\t\t%s" % my_rig.error_status)
    print("status(str):\t\t%s" % Hamlib.rigerror(my_rig.error_status))

    chan = Hamlib.channel(Hamlib.RIG_VFO_B)
    my_rig.get_channel(chan,1)

    print("get_channel status:\t%s" % my_rig.error_status)
    print("VFO:\t\t\t%s, %s" % (Hamlib.rig_strvfo(chan.vfo), chan.freq))
    print("Attenuators:\t\t%s" % my_rig.caps.attenuator)
    # Can't seem to get get_vfo_info to work
    #(freq, width, mode, split) = my_rig.get_vfo_info(Hamlib.RIG_VFO_A,freq,width,mode,split)
    #print("Rig vfo_info:\t\tfreq=%s, mode=%s, width=%s, split=%s" % (freq, mode, width, split))
    print("\nSending Morse, '73'")

    my_rig.send_morse(Hamlib.RIG_VFO_A, "73")
    my_rig.close()

    print("\nSome static functions:")

    err, lon1, lat1 = Hamlib.locator2longlat("IN98XC")
    err, lon2, lat2 = Hamlib.locator2longlat("DM33DX")
    err, loc1 = Hamlib.longlat2locator(lon1, lat1, 3)
    err, loc2 = Hamlib.longlat2locator(lon2, lat2, 3)

    print("Loc1:\t\tIN98XC -> %9.4f, %9.4f -> %s" % (lon1, lat1, loc1))
    print("Loc2:\t\tDM33DX -> %9.4f, %9.4f -> %s" % (lon2, lat2, loc2))

    err, dist, az = Hamlib.qrb(lon1, lat1, lon2, lat2)
    longpath = Hamlib.distance_long_path(dist)

    print("Distance:\t%.3f km, azimuth %.2f, long path:\t%.3f km" \
          % (dist, az, longpath))

    # dec2dms expects values from 180 to -180
    # sw is 1 when deg is negative (west or south) as 0 cannot be signed
    err, deg1, mins1, sec1, sw1 = Hamlib.dec2dms(lon1)
    err, deg2, mins2, sec2, sw2 = Hamlib.dec2dms(lat1)

    lon3 = Hamlib.dms2dec(deg1, mins1, sec1, sw1)
    lat3 = Hamlib.dms2dec(deg2, mins2, sec2, sw2)

    print('Longitude:\t%4.4f, %4d° %2d\' %2d" %1s\trecoded: %9.4f' \
        % (lon1, deg1, mins1, sec1, ('W' if sw1 else 'E'), lon3))

    print('Latitude:\t%4.4f, %4d° %2d\' %2d" %1s\trecoded: %9.4f' \
        % (lat1, deg2, mins2, sec2, ('S' if sw2 else 'N'), lat3))

    my_rig.set_vfo_opt(0);

if __name__ == '__main__':
    StartUp()
