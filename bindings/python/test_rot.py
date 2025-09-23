#! /bin/env pytest
"""Tests of the Python bindings for Hamlib

Running this script directly will use the installed bindings.
For an in-tree run use "make check", or set PYTHONPATH to point to
the directories containing Hamlib.py and _Hamlib.so.
"""
import pytest
from pytest import raises

import Hamlib

Hamlib.rig_set_debug(Hamlib.RIG_DEBUG_NONE)

class TestClass:
    """Container class for tests"""

    TOK_EL_ROT_MAGICLEVEL = 1
    TOK_EL_ROT_MAGICFUNC = 2
    # TOK_EL_ROT_MAGICOP = 3  # handled by get_ext_level/set_ext_level
    TOK_EP_ROT_MAGICPARM = 4
    # TOK_EL_ROT_MAGICCOMBO = 5  # handled by get_ext_level/set_ext_level
    TOK_EL_ROT_MAGICEXTFUNC = 6

    def test_without_open(self, model):
        """Call all the methods that do not depend on open()"""
        rot = Hamlib.Rot(model)
        assert rot is not None
        assert rot.do_exception == 0
        assert rot.error_status == Hamlib.RIG_OK

        assert isinstance(rot.caps.model_name, str)
        assert isinstance(rot.caps.mfg_name, str)
        assert isinstance(rot.caps.version, str)
        assert isinstance(rot.caps.copyright, str)

        assert rot.set_conf("", "") is None
        assert rot.get_conf("") == ""
        assert rot.get_conf(0) == ""
        if model == Hamlib.ROT_MODEL_DUMMY:
            conf = rot.get_conf("mcfg")
            assert conf == "ROTATOR"
            assert rot.set_conf("mcfg", "foobar") is None
            conf = rot.get_conf("mcfg")
            assert conf == "foobar"
        else:
            conf = rot.get_conf("mcfg")
            assert conf == ""

        assert rot.token_lookup("") is None


    @pytest.mark.skipif('config.getoption("model") != Hamlib.ROT_MODEL_DUMMY')
    def test_with_open(self, model, rot_file, serial_speed):
        """Call all the methods that depend on open()"""
        rot = Hamlib.Rot(model)
        assert rot is not None

        assert rot.state.comm_state == 0
        # first try a known string because rot_file is None during "make check"
        assert rot.set_conf("rot_pathname", "foo") is None
        assert rot.get_conf("rot_pathname") == "foo"
        assert rot.set_conf("rot_pathname", rot_file) is None
        assert rot.set_conf("serial_speed", str(serial_speed)) is None
        assert rot.open() is None
        assert rot.state.comm_state == 1
        info = rot.get_info()
        assert isinstance(info, str)
        assert rot.set_position(1.23, 4.56) is None
        # When the Dummy rotator simulates movement, this test is too fast
        # to see a changed position
        assert rot.get_position() == [0.0, 0.0]  # should be [1.23, 4.56]
        assert rot.move(0, 0) is None
        speed = 4
        assert rot.move(Hamlib.ROT_MOVE_UP, speed) is None
        assert rot.move(Hamlib.ROT_MOVE_LEFT, speed) is None
        # When the Dummy rotator simulates movement, this test is too fast
        # to see a changed position
        assert rot.get_position() == [0.0, 0.0]  # should be [-90.0, 90.0]
        value = Hamlib.value_t()
        value.i = 3
        assert rot.set_level(Hamlib.ROT_LEVEL_SPEED, value) is None
        assert rot.get_level(Hamlib.ROT_LEVEL_NONE) is None
        assert rot.get_level(Hamlib.ROT_LEVEL_SPEED) == 3
        with raises(AttributeError):
            assert rot.status() is None
        assert rot.stop() is None
        assert rot.park() is None
        assert rot.reset(Hamlib.ROT_RESET_ALL) is None

        assert rot.set_ext_func(self.TOK_EL_ROT_MAGICEXTFUNC, 5) is None
        assert rot.get_ext_func(self.TOK_EL_ROT_MAGICEXTFUNC) == 5

        value.i = 6
        assert rot.set_ext_level(self.TOK_EL_ROT_MAGICLEVEL, value) is None
        assert rot.get_ext_level(self.TOK_EL_ROT_MAGICLEVEL) == 6

        value.i = 7
        assert rot.set_ext_parm(self.TOK_EP_ROT_MAGICPARM, value) is None
        assert rot.get_ext_parm(self.TOK_EP_ROT_MAGICPARM) == 7

        # Dummy rotator doesn't support functions
        assert rot.has_set_func(Hamlib.ROT_FUNC_NONE) == Hamlib.ROT_FUNC_NONE
        assert rot.has_get_func(Hamlib.ROT_FUNC_NONE) == Hamlib.ROT_FUNC_NONE
        status = 0
        assert rot.set_func(Hamlib.ROT_FUNC_NONE, status) is None
        assert rot.get_func(Hamlib.ROT_FUNC_NONE) is None

        # Dummy rotator doesn't support parameters
        value.i = 5
        assert rot.set_parm(Hamlib.ROT_PARM_NONE, value) is None
        assert rot.get_parm(Hamlib.ROT_PARM_NONE) is None

        assert rot.close() is None
        assert rot.state.comm_state == 0
        info = rot.get_info()
        assert info is None


    @pytest.mark.skipif('config.getoption("model") != Hamlib.ROT_MODEL_DUMMY')
    def test_object_creation(self, model):
        """Create all objects available"""
        rot = Hamlib.Rig(model)
        assert rot is not None

        assert isinstance(rot.caps, Hamlib.rig_caps)
        assert isinstance(rot.state, Hamlib.rig_state)
