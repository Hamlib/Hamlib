#! /bin/env pytest
"""Tests of the Python bindings for Hamlib

Running this script directly will use the installed bindings.
For an in-tree run use "make check", or set PYTHONPATH to point to
the directories containing Hamlib.py and _Hamlib.so.
"""
from pytest import raises

import Hamlib

Hamlib.rig_set_debug(Hamlib.RIG_DEBUG_NONE)

ROT_MODEL = Hamlib.ROT_MODEL_DUMMY

class TestClass:
    """Container class for tests"""

    TOK_EL_ROT_MAGICLEVEL = 1
    TOK_EL_ROT_MAGICFUNC = 2
    # TOK_EL_ROT_MAGICOP = 3  # handled by get_ext_level/set_ext_level
    TOK_EP_ROT_MAGICPARM = 4
    # TOK_EL_ROT_MAGICCOMBO = 5  # handled by get_ext_level/set_ext_level
    TOK_EL_ROT_MAGICEXTFUNC = 6

    def test_without_open(self):
        """Call all the methods that do not depend on open()"""
        rot = Hamlib.Rot(ROT_MODEL)
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
        conf = rot.get_conf("mcfg")
        assert isinstance(conf, str)
        assert rot.set_conf("mcfg", "foo") is None
        conf = rot.get_conf("mcfg")
        assert conf == "foo"

        assert rot.token_lookup("") is None


    def test_with_open(self):
        """Call all the methods that depend on open()"""
        rot = Hamlib.Rot(ROT_MODEL)
        assert rot is not None

        assert rot.state.comm_state == 0
        assert rot.open() is None
        assert rot.state.comm_state == 1
        info = rot.get_info()
        assert isinstance(info, str)
        assert rot.set_position(0.0, 0.0) is None
        assert rot.get_position() == [0.0, 0.0]
        assert rot.move(0, 0) is None
        speed = 4
        assert rot.move(Hamlib.ROT_MOVE_UP, speed) is None
        assert rot.move(Hamlib.ROT_MOVE_LEFT, speed) is None
        assert rot.get_position() == [0.0, 0.0]  # FIXME
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

        status = 0
        assert rot.set_func(1, status) is None
        assert rot.get_func(1) == status
        status = 1
        assert rot.set_func(self.TOK_EL_ROT_MAGICFUNC, status) is None
        assert rot.get_func(self.TOK_EL_ROT_MAGICFUNC) == 0  # FIXME should return status

        value.i = 5
        assert rot.set_parm(Hamlib.ROT_PARM_NONE, value) is None
        assert rot.get_parm(Hamlib.ROT_PARM_NONE) is None  # FIXME should Dummy support this?

        assert rot.close() is None
        assert rot.state.comm_state == 0
        info = rot.get_info()
        assert info is None


    def test_object_creation(self):
        """Create all objects available"""
        rot = Hamlib.Rig(ROT_MODEL)
        assert rot is not None

        assert isinstance(rot.caps, Hamlib.rig_caps)
        assert isinstance(rot.state, Hamlib.rig_state)
