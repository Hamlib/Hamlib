#! /bin/env pytest
"""Tests of the Python bindings for Hamlib

Running this script directly will use the installed bindings.
For an in-tree run use "make check", or set PYTHONPATH to point to
the directories containing Hamlib.py and _Hamlib.so.
"""
import Hamlib

Hamlib.rig_set_debug(Hamlib.RIG_DEBUG_NONE)

RIG_MODEL = Hamlib.RIG_MODEL_DUMMY

class TestClass:
    """Container class for tests"""

    def test_without_open(self):
        """Call all the methods that do not depend on open()"""
        rig = Hamlib.Rig(RIG_MODEL)
        assert rig is not None
        assert rig.do_exception == 0
        assert rig.error_status == Hamlib.RIG_OK

        assert isinstance(rig.caps.model_name, str)
        assert isinstance(rig.caps.mfg_name, str)
        assert isinstance(rig.caps.version, str)
        assert isinstance(rig.caps.copyright, str)

        assert rig.set_conf("", "") is None
        assert rig.get_conf("") == ""
        assert rig.get_conf(0) == ""
        conf = rig.get_conf("mcfg")
        assert isinstance(conf, str)
        assert rig.set_conf("mcfg", "foo") is None
        conf = rig.get_conf("mcfg")
        assert conf == "foo"

        assert rig.token_lookup("") is None


    def test_with_open(self):
        """Call all the methods that depend on open()"""
        rig = Hamlib.Rig(RIG_MODEL)
        assert rig is not None

        assert rig.state.comm_state == 0
        assert rig.open() is None
        assert rig.state.comm_state == 1
        info = rig.get_info()
        assert isinstance(info, str)
        assert rig.close() is None
        assert rig.state.comm_state == 0
        info = rig.get_info()
        assert info is None


    def test_misc(self):
        """Just call all the methods"""
        rig = Hamlib.Rig(RIG_MODEL)
        assert rig is not None

        rig.caps
        rig.close()
        rig.ext_token_lookup("")
        # FIXME rig.get_ant()
        rig.get_chan_all()
        rig.get_channel(0, 0, 0)
        rig.get_channel(0, 0)
        rig.get_conf("")
        rig.get_ctcss_sql()
        rig.get_ctcss_tone()
        rig.get_dcd()
        rig.get_dcs_code()
        rig.get_dcs_sql()
        rig.get_ext_func(0)
        rig.get_ext_level(0)
        rig.get_ext_parm(0)
        rig.get_freq()
        rig.get_func(0)
        rig.get_info()
        rig.get_level(0)
        rig.get_level_f(0)
        rig.get_level_i(0)
        rig.get_mem()
        rig.get_mode()
        rig.get_parm(0)
        rig.get_parm_f(0)
        rig.get_parm_i(0)
        rig.get_powerstat()
        rig.get_ptt()
        rig.get_rit()
        rig.get_rptr_offs()
        rig.get_rptr_shift()
        rig.get_split_freq()
        rig.get_split_mode()
        # FIXME rig.get_split_vfo()
        rig.get_trn()
        rig.get_ts()
        rig.get_vfo()
        # FIXME rig.get_vfo_info()
        rig.get_xit()
        rig.has_get_level(0)
        rig.has_scan(0)
        rig.has_set_func(0)
        rig.has_set_parm(0)
        rig.has_vfo_op(0)
        rig.lookup_mem_caps(0)
        rig.mem_count()
        rig.open()
        rig.passband_narrow(0)
        rig.passband_normal(0)
        rig.passband_wide(0)
        rig.recv_dtmf()
        rig.reset(Hamlib.RIG_RESET_NONE)
        rig.rig
        rig.scan(0, 0, 0)
        rig.send_dtmf(0, "")
        rig.send_morse(0, "")
        option = Hamlib.value_t()
        option.i = 0  # FIXME use constants RIG_ANT_* when available
        rig.set_ant(Hamlib.RIG_VFO_CURR, option)
        rig.set_bank(0, 0)
        channel = Hamlib.channel()
        rig.set_channel(channel)
        rig.set_conf("", "")
        rig.set_ctcss_sql(0, 0)
        rig.set_ctcss_tone(0, 0)
        rig.set_dcs_code(0, 0)
        rig.set_dcs_sql(0, 0)
        rig.set_ext_func(0, 0)
        # FIXME rig.set_ext_level()
        # FIXME rig.set_ext_parm()
        rig.set_freq(0, 0)
        rig.set_func(0, 0, 0)
        rig.set_level(0, 0, 0)
        rig.set_mem(0, 0)
        rig.set_mode(0, 0, 0)
        rig.set_parm(0, 0)
        rig.set_powerstat(0)
        rig.set_ptt(0, 0)
        rig.set_rit(0, 0)
        rig.set_rptr_offs(0, 0)
        rig.set_rptr_shift(0, 0)
        rig.set_split_freq(0, 0)
        rig.set_split_freq_mode(0, 0)
        rig.set_split_mode(0, 0)
        rig.set_split_vfo(0, 0)
        rig.set_trn(0)
        rig.set_ts(0, 0)
        rig.set_vfo(0)
        rig.set_vfo_opt(0)
        rig.set_xit(0, 0)
        rig.state
        rig.token_lookup("")
        rig.vfo_op(0, 0)
