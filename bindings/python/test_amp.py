#! /bin/env pytest
"""Tests of the Python bindings for Hamlib

Running this script directly will use the installed bindings.
For an in-tree run use "make check", or set PYTHONPATH to point to
the directories containing Hamlib.py and _Hamlib.so.
"""
import Hamlib

Hamlib.rig_set_debug(Hamlib.RIG_DEBUG_NONE)

AMP_MODEL = Hamlib.AMP_MODEL_DUMMY

class TestClass:
    """Container class for tests"""

    def test_open_close(self):
        """Smoke test"""
        amp = Hamlib.Amp(AMP_MODEL)
        assert amp is not None
        assert amp.open() is None
        assert amp.close() is None
