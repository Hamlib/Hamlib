#!/usr/bin/env python
# -*- coding: <iso-8859-1> -*-

import sys

sys.path.append ('.')
sys.path.append ('.libs')
sys.path.append ('/usr/local/hamlib/python')

import Hamlib

def StartUp ():
    print "Python test, version: ", Hamlib.cvar.hamlib_version

    Hamlib.rig_set_debug (Hamlib.RIG_DEBUG_TRACE)

    # Init RIG_MODEL_DUMMY
    my_rig = Hamlib.Rig (Hamlib.RIG_MODEL_DUMMY)

    my_rig.open ()

    # 1073741944 is token value for "itu_region"
    rpath = my_rig.get_conf("rig_pathname")
    region = my_rig.get_conf(1073741944)
    print "get_conf: path=",rpath,", ITU region=",region

    #my_rig.set_freq (5000000000,Hamlib.RIG_VFO_B)
    print "freq: ",my_rig.get_freq()
    my_rig.set_freq (145550000)

    mode = my_rig.get_mode()
    print "mode: ",mode[0],"bandwidth: ",mode[1],"Hz (0=normal)"

    print "ITU_region: ",my_rig.state.itu_region
    print "Copyright: ",my_rig.caps.copyright

    print "getinfo: ",my_rig.get_info()

    my_rig.set_level ("VOX",  1)
    print "level: ",my_rig.get_level_i("VOX")
    my_rig.set_level (Hamlib.RIG_LEVEL_VOX, 5)
    print "level: ", my_rig.get_level_i(Hamlib.RIG_LEVEL_VOX)

    print "str: ", my_rig.get_level_i(Hamlib.RIG_LEVEL_STRENGTH)
    print "status: ",my_rig.error_status
    print "status(str):",Hamlib.rigerror(my_rig.error_status)

    #chan = Hamlib.Chan(Hamlib.RIG_VFO_A)
    chan = Hamlib.Chan()

    my_rig.get_channel(chan)
    print "get_channel status: ",my_rig.error_status

    print "VFO: ",chan.vfo,", ",chan.freq
    my_rig.close ()


    # TODO:
    err, long1, lat1 = Hamlib.locator2longlat("IN98EC")
    err, long2, lat2 = Hamlib.locator2longlat("DM33DX")
    loc1 = Hamlib.longlat2locator(long1, lat1)
    loc2 = Hamlib.longlat2locator(long2, lat2)
    print "Loc1: IN98EC -> ",loc1
    print "Loc2: DM33DX -> ",loc2

    dist, az = Hamlib.qrb(long1, lat1, long2, lat2)
    longpath = Hamlib.distance_long_path(dist)
    print "Distance: ",dist," km, long path: ",longpath
    deg, min, sec = Hamlib.dec2dms(az)
    az2 = Hamlib.dms2dec(deg, min, sec)
    print "Bearing: ",az,", ",deg,"° ",min,"' ",sec,", recoded: ",az2


if __name__ == '__main__':
    StartUp ()
