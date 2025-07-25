#! /bin/env pytest
"""Tests of the Python bindings for Hamlib

Running this script directly will use the installed bindings.
For an in-tree run use "make check", or set PYTHONPATH to point to
the directories containing Hamlib.py and _Hamlib.so.
"""
from pytest import raises

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

        assert rig.set_split_vfo(Hamlib.RIG_SPLIT_OFF, Hamlib.RIG_VFO_A) is None
        assert rig.get_split_vfo(Hamlib.RIG_VFO_TX) == [Hamlib.RIG_SPLIT_OFF, Hamlib.RIG_VFO_A]
        assert rig.set_split_vfo(Hamlib.RIG_SPLIT_ON, Hamlib.RIG_VFO_B) is None
        assert rig.get_split_vfo(Hamlib.RIG_VFO_TX) == [Hamlib.RIG_SPLIT_ON, Hamlib.RIG_VFO_B]
        assert rig.set_split_vfo(Hamlib.RIG_SPLIT_OFF, Hamlib.RIG_VFO_CURR) is None
        assert rig.get_split_vfo() == [Hamlib.RIG_SPLIT_OFF, Hamlib.RIG_VFO_B]
        assert rig.get_split_vfo(Hamlib.RIG_VFO_CURR) == [Hamlib.RIG_SPLIT_OFF, Hamlib.RIG_VFO_B]

        # FIXME should use a RIG_ANT_* constant but it isn't available in the bindings
        RIG_ANT_UNKNOWN = 1<<30
        assert rig.get_ant(Hamlib.RIG_VFO_A) == [RIG_ANT_UNKNOWN, RIG_ANT_UNKNOWN, Hamlib.RIG_VFO_A, 0]
        assert rig.get_ant(Hamlib.RIG_VFO_A, Hamlib.RIG_VFO_A) == [RIG_ANT_UNKNOWN, RIG_ANT_UNKNOWN, Hamlib.RIG_VFO_A, 0]

        assert rig.close() is None
        assert rig.state.comm_state == 0
        info = rig.get_info()
        assert info is None


    def test_misc(self):
        """Just call all the methods"""
        rig = Hamlib.Rig(RIG_MODEL)
        assert rig is not None

        assert rig.close() is None
        assert rig.ext_token_lookup("") is None
        assert rig.get_chan_all() is None
        channel = 0
        readonly = 0
        assert isinstance(rig.get_channel(channel), Hamlib.channel)
        assert isinstance(rig.get_channel(channel, Hamlib.RIG_VFO_CURR), Hamlib.channel)
        assert isinstance(rig.get_channel(channel, Hamlib.RIG_VFO_CURR, readonly), Hamlib.channel)
        assert isinstance(rig.get_conf(""), str)
        assert isinstance(rig.get_ctcss_sql(), int)
        assert isinstance(rig.get_ctcss_sql(0), int)
        assert isinstance(rig.get_ctcss_tone(), int)
        assert isinstance(rig.get_ctcss_tone(0), int)
        assert isinstance(rig.get_dcd(), int)
        assert isinstance(rig.get_dcd(Hamlib.RIG_VFO_CURR,), int)
        assert isinstance(rig.get_dcs_code(), int)
        assert isinstance(rig.get_dcs_code(0), int)
        assert isinstance(rig.get_dcs_sql(), int)
        assert isinstance(rig.get_dcs_sql(0), int)
        assert isinstance(rig.get_ext_func(0), int)
        assert isinstance(rig.get_ext_func(0, 0), int)
        assert rig.get_ext_level(0) is None
        assert rig.get_ext_level(0, 0) is None
        assert rig.get_ext_parm(0) is None
        assert isinstance(rig.get_freq(), float)
        assert isinstance(rig.get_freq(Hamlib.RIG_VFO_CURR), float)
        assert isinstance(rig.get_func(0), int)
        assert isinstance(rig.get_func(0, 0), int)
        assert rig.get_info() is None
        assert rig.get_level(0) is None
        assert isinstance(rig.get_level_f(0), float)
        assert isinstance(rig.get_level_i(0), int)
        assert isinstance(rig.get_mem(), int)
        assert isinstance(rig.get_mem(Hamlib.RIG_VFO_CURR), int)
        assert len(rig.get_mode()) == 2
        assert len(rig.get_mode(Hamlib.RIG_VFO_CURR)) == 2
        assert rig.get_parm(0) is None
        assert isinstance(rig.get_parm_f(0), float)
        assert isinstance(rig.get_parm_i(0), int)
        assert isinstance(rig.get_powerstat(), int)
        assert isinstance(rig.get_ptt(), int)
        assert isinstance(rig.get_ptt(Hamlib.RIG_VFO_CURR), int)
        assert isinstance(rig.get_rit(), int)
        assert isinstance(rig.get_rit(Hamlib.RIG_VFO_CURR), int)
        assert isinstance(rig.get_rptr_offs(), int)
        assert isinstance(rig.get_rptr_offs(Hamlib.RIG_VFO_CURR), int)
        assert isinstance(rig.get_rptr_shift(), int)
        assert isinstance(rig.get_rptr_shift(Hamlib.RIG_VFO_CURR), int)
        assert isinstance(rig.get_split_freq(), float)
        assert isinstance(rig.get_split_freq(Hamlib.RIG_VFO_CURR), float)
        assert len(rig.get_split_mode()) == 2
        assert len(rig.get_split_mode(Hamlib.RIG_VFO_CURR)) == 2
        assert isinstance(rig.get_trn(), int)  # deprecated
        assert isinstance(rig.get_ts(), int)
        assert isinstance(rig.get_ts(Hamlib.RIG_VFO_CURR), int)
        assert isinstance(rig.get_vfo(), int)
        assert len(rig.get_vfo_info()) == 5
        assert len(rig.get_vfo_info(Hamlib.RIG_VFO_CURR)) == 5
        assert isinstance(rig.get_xit(), int)
        assert isinstance(rig.get_xit(Hamlib.RIG_VFO_CURR), int)
        # assert rig_has_get_func(0)  FIXME not defined
        assert rig.has_get_level(0) is None  # FIXME should return setting_t
        # assert rig_has_get_parm(0)  FIXME not defined
        assert rig.has_scan(0) is None  # FIXME should return scan_t
        assert rig.has_set_func(0) is None  # FIXME should return setting_t
        # assert rig_has_set_level(0)  FIXME not defined
        assert rig.has_set_parm(0) is None  # FIXME should return setting_t
        assert rig.has_vfo_op(0) is None  # FIXME should return vfo_op_t
        assert rig.lookup_mem_caps() is None
        assert rig.lookup_mem_caps(0) is None
        assert isinstance(rig.mem_count(), int)
        assert rig.open() is None
        assert rig.passband_narrow(0) is None
        assert rig.passband_normal(0) is None
        assert rig.passband_wide(0) is None
        assert isinstance(rig.recv_dtmf(), str)
        assert isinstance(rig.recv_dtmf(0), str)
        assert rig.reset(Hamlib.RIG_RESET_NONE) is None
        assert rig.scan(0, 0) is None
        assert rig.scan(0, 0, 0) is None
        assert rig.send_dtmf(0, "") is None
        assert rig.send_morse(0, "") is None
        # FIXME should use a RIG_ANT_* constant but it isn't available in the bindings
        RIG_ANT_1 = 1<<0
        option = Hamlib.value_t()
        option.i = 0
        assert rig.set_ant(Hamlib.RIG_VFO_CURR, option) is None
        assert rig.set_ant(Hamlib.RIG_VFO_CURR, option, RIG_ANT_1) is None
        assert rig.set_bank(0, 0) is None
        channel = Hamlib.channel()
        channel = Hamlib.channel(0)
        channel = Hamlib.channel(0, 0)
        assert rig.set_channel(channel) is None
        assert rig.set_conf("", "") is None
        assert rig.set_ctcss_sql(0, 0) is None
        assert rig.set_ctcss_tone(0, 0) is None
        assert rig.set_dcs_code(0, 0) is None
        assert rig.set_dcs_sql(0) is None
        assert rig.set_dcs_sql(0, 0) is None
        assert rig.set_ext_func(0, 0) is None
        assert rig.set_ext_func(0, 0, 0) is None
        level = 0
        value = Hamlib.value_t()
        assert rig.set_ext_level(level, value) is None
        assert rig.set_ext_level(level, value, Hamlib.RIG_VFO_CURR) is None
        value = Hamlib.value_t()
        assert rig.set_ext_parm(0, value) is None
        assert rig.set_freq(0, 0) is None
        assert rig.set_func(0, 0, 0) is None
        assert rig.set_level(0, 0, 0) is None
        assert rig.set_mem(0, 0) is None
        assert rig.set_mode(0) is None
        assert rig.set_mode(0, 0) is None
        assert rig.set_mode(0, 0, 0) is None
        assert rig.set_parm(0, 0) is None
        assert rig.set_powerstat(0) is None
        assert rig.set_ptt(0, 0) is None
        assert rig.set_rit(0, 0) is None
        assert rig.set_rptr_offs(0, 0) is None
        assert rig.set_rptr_shift(0, 0) is None
        assert rig.set_split_freq(0, 0) is None
        assert rig.set_split_freq_mode(0, 0) is None
        assert rig.set_split_freq_mode(0, 0, 0) is None
        assert rig.set_split_freq_mode(0, 0, 0, 0) is None
        assert rig.set_split_mode(0) is None
        assert rig.set_split_mode(0, 0) is None
        assert rig.set_split_mode(0, 0, 0) is None
        assert rig.set_split_vfo(0, 0) is None
        assert rig.set_split_vfo(0, 0, 0) is None
        assert rig.set_trn(0) is None  # deprecated
        assert rig.set_ts(0, 0) is None
        assert rig.set_vfo(0) is None
        assert rig.set_vfo_opt(0) is None
        assert rig.set_xit(0, 0) is None
        assert rig.token_lookup("") is None
        assert rig.vfo_op(0, 0) is None


    def test_object_creation(self):
        """Create all objects available"""
        rig = Hamlib.Rig(RIG_MODEL)
        assert rig is not None

        assert isinstance(rig.caps, Hamlib.rig_caps)
        assert isinstance(rig.state, Hamlib.rig_state)
