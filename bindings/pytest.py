#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys

sys.path.append ('.')
sys.path.append ('.libs')
sys.path.append ('/usr/local/hamlib/python')

import Hamlib

def StartUp ():
    print "Python",sys.version[:5],"test,", Hamlib.cvar.hamlib_version

    #Hamlib.rig_set_debug (Hamlib.RIG_DEBUG_TRACE)
    Hamlib.rig_set_debug (Hamlib.RIG_DEBUG_NONE)

    # Init RIG_MODEL_DUMMY
    my_rig = Hamlib.Rig (Hamlib.RIG_MODEL_DUMMY)
    my_rig.set_conf ("rig_pathname","/dev/Rig")
    my_rig.set_conf ("retry","5")

    my_rig.open ()

    # 1073741944 is token value for "itu_region"
    # but using get_conf is much more convenient
    region = my_rig.get_conf(1073741944)
    rpath = my_rig.get_conf("rig_pathname")
    retry = my_rig.get_conf("retry")
    print "status(str):",Hamlib.rigerror(my_rig.error_status)
    print "get_conf: path=",rpath,", retry =",retry,", ITU region=",region

    my_rig.set_freq (5700000000,Hamlib.RIG_VFO_B)
    print "freq:",my_rig.get_freq()
    my_rig.set_freq (145550000)
    my_rig.set_vfo (Hamlib.RIG_VFO_B)
    #my_rig.set_vfo ("VFOA")

    (mode, width) = my_rig.get_mode()
    print "mode:",Hamlib.rig_strrmode(mode),", bandwidth:",width

    print "ITU_region: ",my_rig.state.itu_region
    print "Backend copyright: ",my_rig.caps.copyright

    print "Model:",my_rig.caps.model_name
    print "Manufacturer:",my_rig.caps.mfg_name
    print "Backend version:",my_rig.caps.version
    print "Backend license:",my_rig.caps.copyright
    print "Rig info:", my_rig.get_info()

    my_rig.set_level ("VOX",  1)
    print "VOX level: ",my_rig.get_level_i("VOX")
    my_rig.set_level (Hamlib.RIG_LEVEL_VOX, 5)
    print "VOX level: ", my_rig.get_level_i(Hamlib.RIG_LEVEL_VOX)

    print "strength: ", my_rig.get_level_i(Hamlib.RIG_LEVEL_STRENGTH)
    print "status: ",my_rig.error_status
    print "status(str):",Hamlib.rigerror(my_rig.error_status)

    chan = Hamlib.channel(Hamlib.RIG_VFO_B)

    my_rig.get_channel(chan)
    print "get_channel status: ",my_rig.error_status

    print "VFO: ",Hamlib.rig_strvfo(chan.vfo),", ",chan.freq
    my_rig.close ()

    print "\nSome static functions:"

    err, long1, lat1 = Hamlib.locator2longlat("IN98EC")
    err, long2, lat2 = Hamlib.locator2longlat("DM33DX")
    err, loc1 = Hamlib.longlat2locator(long1, lat1, 3)
    err, loc2 = Hamlib.longlat2locator(long2, lat2, 3)
    print "Loc1: IN98EC -> ",loc1
    print "Loc2: DM33DX -> ",loc2

    # TODO: qrb should normalize?
    err, dist, az = Hamlib.qrb(long1, lat1, long2, lat2)
    if az > 180:
        az -= 360
    longpath = Hamlib.distance_long_path(dist)
    print "Distance: ",dist," km, long path: ",longpath
    err, deg, min, sec, sw = Hamlib.dec2dms(az)
    az2 = Hamlib.dms2dec(deg, min, sec, sw)
    if sw:
        deg = -deg
    print "Bearing: ",az,", ",deg,"° ",min,"' ",sec,", recoded: ",az2


if __name__ == '__main__':
    StartUp ()
