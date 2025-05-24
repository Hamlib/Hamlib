#! /bin/env pytest
"""Tests of the Python bindings for Hamlib

Running this script directly will use the installed bindings.
For an in-tree run use "make check", or set PYTHONPATH to point to
the directories containing Hamlib.py and _Hamlib.so.
"""
import Hamlib

AMP_MODEL = Hamlib.AMP_MODEL_DUMMY

class TestClass:
    """Container class for tests"""

    def test_open_close(self):
        """Smoke test"""
        amp = Hamlib.Amp(AMP_MODEL)
        assert amp is not None
        assert amp.open() is None
        assert amp.close() is None


    def test_all_methods(self):
        """Just call all the methods"""
        amp = Hamlib.Amp(AMP_MODEL)
        assert amp is not None

        # the tests that do not depend on open()
        assert amp.set_conf("", "") is None
        assert amp.get_conf("") == ""
        assert amp.get_conf(0) == ""
        assert amp.get_info() is None
        assert amp.token_lookup("") is None

        # the tests that depend on open()
        assert amp.open() is None
        assert amp.set_freq(0) is None
        # assert amp.get_freq() is None  # FIXME:  AttributeError: 'Amp' object has no attribute 'get_freq'
        assert amp.set_powerstat(Hamlib.RIG_POWER_OFF) is None
        # assert amp.get_powerstat() is None  # FIXME:  AttributeError: 'Amp' object has no attribute 'get_powerstat'
        assert amp.close() is None
