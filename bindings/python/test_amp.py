#! /bin/env pytest
"""Tests of the Python bindings for Hamlib

Running this script directly will use the installed bindings.
For an in-tree run use "make check", or set PYTHONPATH to point to
the directories containing Hamlib.py and _Hamlib.so.
"""
import pytest

import Hamlib

Hamlib.rig_set_debug(Hamlib.RIG_DEBUG_NONE)

class TestClass:
    """Container class for tests"""

    def test_without_open(self, model):
        """Call all the methods that do not depend on open()"""
        amp = Hamlib.Amp(model)
        assert amp is not None
        assert amp.do_exception == 0
        assert amp.error_status == Hamlib.RIG_OK

        assert isinstance(amp.caps.model_name, str)
        assert isinstance(amp.caps.mfg_name, str)
        assert isinstance(amp.caps.version, str)
        assert isinstance(amp.caps.copyright, str)

        assert amp.set_conf("", "") is None
        assert amp.get_conf("") == ""
        assert amp.get_conf(0) == ""
        if model == Hamlib.AMP_MODEL_DUMMY:
            conf = amp.get_conf("mcfg")
            assert conf == ""
            assert amp.set_conf("mcfg", "foobar") is None
            conf = amp.get_conf("mcfg")
            assert conf == ""  # FIXME: should return "foobar"
        else:
            conf = amp.get_conf("mcfg")
            assert conf == ""

        assert amp.token_lookup("") is None


    @pytest.mark.skipif('config.getoption("model") != Hamlib.AMP_MODEL_DUMMY')
    def test_with_open(self, model, amp_file, serial_speed):
        """Call all the methods that depend on open()"""
        amp = Hamlib.Amp(model)
        assert amp is not None

        assert amp.state.comm_state == 0
        # first try a known string because amp_file is None during "make check"
        assert amp.set_conf("amp_pathname", "foo") is None
        assert amp.get_conf("amp_pathname") == "foo"
        assert amp.set_conf("amp_pathname", amp_file) is None
        assert amp.set_conf("serial_speed", str(serial_speed)) is None
        assert amp.open() is None
        assert amp.state.comm_state == 1
        info = amp.get_info()
        assert isinstance(info, str)
        assert amp.reset(Hamlib.AMP_RESET_FAULT) is None
        assert amp.set_freq(0) is None
        assert amp.set_freq(123.45) is None
        assert amp.get_freq() == 123.45

        assert amp.has_set_level(Hamlib.AMP_LEVEL_SWR) == Hamlib.AMP_LEVEL_NONE
        assert amp.has_get_level(Hamlib.AMP_LEVEL_SWR) == Hamlib.AMP_LEVEL_SWR
        assert amp.get_level(Hamlib.AMP_LEVEL_NONE) is None
        level = amp.get_level(Hamlib.AMP_LEVEL_SWR)
        assert isinstance(level, float)
        level = amp.get_level(Hamlib.AMP_LEVEL_PWR_REFLECTED)
        assert isinstance(level, int)
        level = amp.get_level(Hamlib.AMP_LEVEL_FAULT)
        assert isinstance(level, str)
        level = amp.get_level(123456)
        assert level is None
        value = Hamlib.value_t()
        value.i = 2
        assert amp.set_level(Hamlib.AMP_LEVEL_SWR, value) is None
        assert amp.set_ext_level(Hamlib.AMP_LEVEL_SWR, value) is None
        level = amp.get_ext_level(Hamlib.AMP_LEVEL_SWR)
        assert level is None

        assert amp.set_powerstat(Hamlib.RIG_POWER_ON) is None
        assert amp.get_powerstat() == Hamlib.RIG_POWER_ON

        assert amp.close() is None
        assert amp.state.comm_state == 0
        info = amp.get_info()
        assert info is None


    @pytest.mark.skipif('config.getoption("model") != Hamlib.AMP_MODEL_DUMMY')
    def test_object_creation(self, model):
        """Create all objects available"""
        amp = Hamlib.Rig(model)
        assert amp is not None

        assert isinstance(amp.caps, Hamlib.rig_caps)
        assert isinstance(amp.state, Hamlib.rig_state)
