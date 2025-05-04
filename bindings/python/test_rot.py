#! /bin/env pytest
"""Tests of the Python bindings for Hamlib

Running this script directly will use the installed bindings.
For an in-tree run use "make check", or set PYTHONPATH to point to
the directories containing Hamlib.py and _Hamlib.so.
"""
import Hamlib

Hamlib.rig_set_debug(Hamlib.RIG_DEBUG_NONE)

ROT_MODEL = Hamlib.ROT_MODEL_DUMMY

class TestClass:
    """Container class for tests"""

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
        assert rot.status() is None
        assert rot.stop() is None
        assert rot.park() is None
        assert rot.reset(Hamlib.ROT_RESET_ALL) is None
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
