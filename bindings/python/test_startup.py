#! /bin/env pytest
"""Tests of the Python bindings for Hamlib

Running this script directly will use the installed bindings.
For an in-tree run use "make check", or set PYTHONPATH to point to
the directories containing Hamlib.py and _Hamlib.so.
"""
from pytest import approx

import Hamlib

class TestClass:
    """Container class for tests"""

    def test_startup(self):
        """Simple script to test the Hamlib.py module with Python3."""

        assert isinstance(Hamlib.cvar.hamlib_version, str)

        Hamlib.rig_set_debug(Hamlib.RIG_DEBUG_NONE)
        assert Hamlib.rig_get_debug() == Hamlib.RIG_DEBUG_NONE
        assert Hamlib.rig_set_debug_time_stamp(1) is None

        model = Hamlib.RIG_MODEL_DUMMY
        my_rig = Hamlib.Rig(model)
        assert my_rig is not None
        assert my_rig.set_conf("rig_pathname", "/dev/Rig") is None
        assert my_rig.set_conf("retry", "5") is None

        assert my_rig.open() is None

        assert my_rig.get_conf("rig_pathname") == "/dev/Rig"
        assert my_rig.get_conf("retry") == '5'

        assert my_rig.error_status == Hamlib.RIG_OK
        assert Hamlib.rigerror2(my_rig.error_status) == "Command completed successfully\n"

        assert my_rig.set_freq(Hamlib.RIG_VFO_B, 5700000000) is None
        assert my_rig.set_vfo(Hamlib.RIG_VFO_B) is None
        assert my_rig.get_freq() == 5700000000

        assert my_rig.set_freq(Hamlib.RIG_VFO_A, 145550000) is None

        (mode, width) = my_rig.get_mode(Hamlib.RIG_VFO_A)
        assert Hamlib.rig_strrmode(mode) == 'FM'
        assert width == 15000

        assert my_rig.set_mode(Hamlib.RIG_MODE_CW) is None
        (mode, width) = my_rig.get_mode()
        assert Hamlib.rig_strrmode(mode) == 'CW'
        assert width == 0

        assert isinstance(my_rig.caps.copyright, str)
        assert isinstance(my_rig.caps.model_name, str)
        assert isinstance(my_rig.caps.mfg_name, str)
        assert isinstance(my_rig.caps.version, str)
        assert isinstance(Hamlib.rig_strstatus(my_rig.caps.status), str)
        assert isinstance(my_rig.get_info(), str)

        assert my_rig.set_level("VOXDELAY", 1) is None
        assert my_rig.get_level_i("VOXDELAY") == 1

        assert my_rig.set_level(Hamlib.RIG_LEVEL_VOXDELAY, 5) is None
        assert my_rig.get_level_i(Hamlib.RIG_LEVEL_VOXDELAY) == 5

        af = 12.34
        assert my_rig.set_level("AF", af) is None
        assert my_rig.error_status == Hamlib.RIG_OK
        assert my_rig.get_level_f(Hamlib.RIG_LEVEL_AF) == approx(12.34)
        assert my_rig.error_status == Hamlib.RIG_OK

        assert my_rig.get_level_f(Hamlib.RIG_LEVEL_RFPOWER_METER)
        assert my_rig.get_level_f(Hamlib.RIG_LEVEL_RFPOWER_METER_WATTS)
        assert my_rig.get_level_i(Hamlib.RIG_LEVEL_STRENGTH)

        chan = Hamlib.channel(Hamlib.RIG_VFO_B)
        assert chan.vfo == Hamlib.RIG_VFO_B
        assert my_rig.get_channel(chan, 1) is None
        assert my_rig.error_status == Hamlib.RIG_OK

        assert Hamlib.rig_strvfo(chan.vfo) == 'MainB'
        assert chan.freq == 5700000000
        assert my_rig.caps.attenuator == [10, 20, 30, 0, 0, 0, 0, 0]
        # Can't seem to get get_vfo_info to work
        #(freq, width, mode, split) = my_rig.get_vfo_info(Hamlib.RIG_VFO_A,freq,width,mode,split)
        #print("Rig vfo_info:\t\tfreq=%s, mode=%s, width=%s, split=%s" % (freq, mode, width, split))

        assert my_rig.send_morse(Hamlib.RIG_VFO_A, "73") is None
        assert my_rig.close() is None

        # Some static functions

        err, lon1, lat1 = Hamlib.locator2longlat("IN98XC")
        assert err == Hamlib.RIG_OK
        assert lon1 == approx(-0.0417, abs=1.0e-04)  # FIXME why need to override tolerance?
        assert lat1 == approx(48.1042)
        assert Hamlib.longlat2locator(lon1, lat1, 3) == [Hamlib.RIG_OK, 'IN98XC']

        err, lon2, lat2 = Hamlib.locator2longlat("DM33DX")
        assert err == Hamlib.RIG_OK
        assert lon2 == approx(-113.7083)
        assert lat2 == approx(33.9792)
        assert Hamlib.longlat2locator(lon2, lat2, 3) == [Hamlib.RIG_OK, 'DM33DX']

        err, dist, az = Hamlib.qrb(lon1, lat1, lon2, lat2)
        assert err == Hamlib.RIG_OK
        assert dist == approx(8765.814)
        assert az == approx(309.0)
        longpath = Hamlib.distance_long_path(dist)
        assert longpath == approx(31266.186)

        # dec2dms expects values from 180 to -180
        # sw is 1 when deg is negative (west or south) as 0 cannot be signed
        err, deg1, mins1, sec1, sw1 = Hamlib.dec2dms(lon1)
        assert err == Hamlib.RIG_OK
        assert deg1 == 0
        assert mins1 == 2
        assert sec1 == 29
        assert sw1 == 1

        err, deg2, mins2, sec2, sw2 = Hamlib.dec2dms(lat1)
        assert err == Hamlib.RIG_OK
        assert deg2 == 48
        assert mins2 == 6
        assert sec2 == 15
        assert sw2 == 0

        lon3 = Hamlib.dms2dec(deg1, mins1, sec1, sw1)
        lat3 = Hamlib.dms2dec(deg2, mins2, sec2, sw2)
        assert lon3 == approx(-0.0414, abs=1.0e-04)  # FIXME why need to override tolerance?
        assert lat3 == approx(48.1042)

        assert Hamlib.dmmm2dec(48, 60.0 * 0.1042, 0) == approx(-Hamlib.dmmm2dec(-48, 60.0 * -0.1042, 1))
        assert Hamlib.dec2dmmm(48.1042) == [0, 48, 6, 0]

        assert Hamlib.azimuth_long_path(0.0) == 180.0
        assert Hamlib.azimuth_long_path(360.0) == 180.0
        assert Hamlib.azimuth_long_path(92.6) == approx(272.6)
        assert Hamlib.azimuth_long_path(180.0) == 0.0
        assert Hamlib.azimuth_long_path(285.7) == approx(105.7)

        assert my_rig.set_vfo_opt(0) is None
