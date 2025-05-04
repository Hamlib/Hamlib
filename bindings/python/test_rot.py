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

    def test_open_close(self):
        """Smoke test"""
        rot = Hamlib.Rot(ROT_MODEL)
        assert rot is not None
        assert rot.open() is None
        assert rot.set_position(0.0, 0.0) is None
        assert rot.get_position() is not None
        assert rot.close() is None
