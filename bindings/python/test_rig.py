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
        rig = Hamlib.Rig(model)
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
        if model == Hamlib.RIG_MODEL_DUMMY:
            conf = rig.get_conf("mcfg")
            assert conf == "DX"
            assert rig.set_conf("mcfg", "foobar") is None
            conf = rig.get_conf("mcfg")
            assert conf == "foobar"
        else:
            conf = rig.get_conf("mcfg")
            assert conf == ""

        assert rig.token_lookup("") is None


    def do_test_frequency(self, rig):
        """Frequency tests"""

        # TODO use a frequency suitable for the VFO
        frequency = 5700000000
        assert rig.set_freq(Hamlib.RIG_VFO_CURR, frequency) is None
        assert rig.get_freq() == 5700000000.0
        frequency = 5700000000.5
        assert rig.set_freq(Hamlib.RIG_VFO_CURR, frequency) is None
        assert isinstance(rig.get_freq(Hamlib.RIG_VFO_CURR), float)
        assert rig.get_freq(Hamlib.RIG_VFO_CURR) == 5700000000.5


    def do_test_vfo(self, rig):
        """VFO tests"""

        assert rig.set_vfo(Hamlib.RIG_VFO_A) is None
        assert rig.get_vfo() == Hamlib.RIG_VFO_A
        assert rig.set_split_vfo(Hamlib.RIG_SPLIT_OFF, Hamlib.RIG_VFO_A) is None
        assert rig.get_split_vfo(Hamlib.RIG_VFO_TX) == [Hamlib.RIG_SPLIT_OFF, Hamlib.RIG_VFO_A]
        assert rig.set_split_vfo(Hamlib.RIG_SPLIT_ON, Hamlib.RIG_VFO_B) is None
        assert rig.get_split_vfo(Hamlib.RIG_VFO_TX) == [Hamlib.RIG_SPLIT_ON, Hamlib.RIG_VFO_B]
        assert rig.set_split_vfo(Hamlib.RIG_SPLIT_OFF, Hamlib.RIG_VFO_CURR) is None
        assert rig.get_split_vfo() == [Hamlib.RIG_SPLIT_OFF, Hamlib.RIG_VFO_B]
        assert rig.get_split_vfo(Hamlib.RIG_VFO_CURR) == [Hamlib.RIG_SPLIT_OFF, Hamlib.RIG_VFO_B]


    def do_test_rit_xit(self, rig):
        """RIT and XIT tests"""

        assert rig.set_rit(Hamlib.RIG_VFO_CURR, 100) is None
        assert rig.get_rit() == 100
        assert rig.get_rit(Hamlib.RIG_VFO_CURR) == 100
        assert rig.set_xit(Hamlib.RIG_VFO_CURR, 200) is None
        assert rig.get_xit() == 200
        assert rig.get_xit(Hamlib.RIG_VFO_CURR) == 200


    def do_test_antenna(self, rig):
        """Antenna tests"""

        # FIXME should use a RIG_ANT_* constant but they aren't available in the bindings
        RIG_ANT_1 = 1<<0
        RIG_ANT_UNKNOWN = 1<<30
        RIG_ANT_CURR = 1<<31
        option = Hamlib.value_t()
        option.i = 0
        expected = [RIG_ANT_UNKNOWN, RIG_ANT_UNKNOWN, RIG_ANT_1, 0]
        assert rig.set_ant(RIG_ANT_1, option) is None
        assert rig.get_ant(RIG_ANT_CURR) == expected
        assert rig.set_ant(RIG_ANT_1, option, Hamlib.RIG_VFO_CURR) is None
        assert rig.get_ant(RIG_ANT_CURR, Hamlib.RIG_VFO_A) == expected


    def do_test_squelch(self, rig):
        """Squelch codes and tones"""

        assert rig.set_ctcss_sql(Hamlib.RIG_VFO_CURR, 885) is None
        assert rig.get_ctcss_sql() == 885
        assert rig.get_ctcss_sql(Hamlib.RIG_VFO_CURR) == 885
        assert rig.set_ctcss_tone(Hamlib.RIG_VFO_CURR, 854) is None
        assert rig.get_ctcss_tone() == 854
        assert rig.get_ctcss_tone(Hamlib.RIG_VFO_CURR) == 854
        assert rig.set_dcs_code(Hamlib.RIG_VFO_CURR, 125) is None
        assert rig.get_dcs_code() == 125
        assert rig.get_dcs_code(Hamlib.RIG_VFO_CURR) == 125
        assert rig.set_dcs_sql(Hamlib.RIG_VFO_CURR, 134) is None
        assert rig.get_dcs_sql() == 134
        assert rig.get_dcs_sql(Hamlib.RIG_VFO_CURR) == 134


    def do_test_callback(self, rig):
        """Callback tests"""

        # Frequency event callback
        def freq_callback(vfo, freq, arg):
            assert (1, 144200000.5, 1234567890) == (vfo, freq, arg)

        assert rig.set_freq_callback(freq_callback, 1234567890) is None
        assert rig.set_freq(Hamlib.RIG_VFO_CURR, 144200000.5) is None
        # TODO assert that freq_callback() is called once
        assert rig.set_freq_callback(None) is None
        assert rig.set_freq(Hamlib.RIG_VFO_CURR, 144210000) is None
        # TODO assert that freq_callback() is called once

        # Mode event callback
        def mode_callback(vfo, mode, pbwidth, arg):
            assert (1, 32, 5000, 2345678901) == (vfo, mode, pbwidth, arg)

        # FIXME should use a Hamlib.RIG_PASSBAND_* constant but they aren't available in the bindings
        RIG_PASSBAND_NOCHANGE = -1
        assert rig.set_mode_callback(mode_callback, 2345678901) is None
        assert rig.set_mode(Hamlib.RIG_MODE_FM, 5000) is None
        # TODO assert that mode_callback() is called once
        assert rig.set_mode_callback(None) is None
        assert rig.set_mode(Hamlib.RIG_MODE_FM, 15000) is None
        # TODO assert that mode_callback() is called once

        # VFO event callback
        def vfo_callback(vfo, arg):
            assert (1, 3456789012) == (vfo, arg)

        assert rig.set_vfo(Hamlib.RIG_VFO_B) is None
        assert rig.set_vfo_callback(vfo_callback, 3456789012) is None
        assert rig.set_vfo(Hamlib.RIG_VFO_A) is None
        # TODO assert that vfo_callback() is called once
        assert rig.set_vfo_callback(None) is None
        assert rig.set_vfo(Hamlib.RIG_VFO_CURR) is None
        # TODO assert that vfo_callback() is called once

        # PTT event callback
        def ptt_callback(vfo, ptt, arg):
            assert (Hamlib.RIG_VFO_CURR, Hamlib.RIG_PTT_ON, 4567890123) == (vfo, ptt, arg)

        assert rig.set_ptt_callback(ptt_callback, 4567890123) is None
        assert rig.set_ptt(Hamlib.RIG_VFO_CURR, Hamlib.RIG_PTT_ON) is None
        # TODO assert that ptt_callback() is called once
        assert rig.set_ptt_callback(None) is None
        assert rig.set_ptt(Hamlib.RIG_VFO_CURR, Hamlib.RIG_PTT_OFF) is None
        # TODO assert that ptt_callback() is called once

        # DCD event callback
        def dcd_callback(vfo, ptt, arg):
            print("dcd_callback", vfo, dcd, arg)
            assert (1, 5000, 2345678901) == (vfo, arg)

        assert rig.set_dcd_callback(dcd_callback, 5678901234) is None
        # TODO simulate dcd events in dummy.c
        assert rig.set_dcd_callback(None) is None

        # PLtune event callback
        def pltune_callback(vfo, ptt, arg):
            print("pltune_callback", vfo, ptt, arg)
            assert (1, 5000, 2345678901) == (vfo, arg)

        assert rig.set_pltune_callback(pltune_callback, 6789012345) is None
        # TODO simulate pltune events in dummy.c
        assert rig.set_pltune_callback(None) is None

        # spectrum event callback
        def spectrum_callback(rig_spectrum_line, arg):
            print("spectrum_callback", rig_spectrum_line, arg)
            assert (1, 5000, 2345678901) == (vfo, arg)

        assert rig.set_spectrum_callback(spectrum_callback, 7890123456) is None
        # TODO simulate spectrum events in dummy.c
        assert rig.set_spectrum_callback(None) is None


    def do_test_send_raw(self, rig):
        """rig_send_raw() tests"""

        # When using the Dummy driver, rig.c replies with a copy of the data
        test_string_1 = "test string 012\n"
        test_string_2 = test_string_1 * 2
        assert rig.send_raw(test_string_1) == test_string_1
        assert rig.send_raw(test_string_2, "\n") == test_string_1
        assert rig.send_raw(test_string_2, bytes([10])) == test_string_1

        test_bytes_1 = bytes("test bytes\x00\x01\x02", "ASCII")
        test_bytes_2 = test_bytes_1 * 2
        assert rig.send_raw(test_bytes_1) == test_bytes_1
        assert rig.send_raw(test_bytes_1, "s") == b"tes"
        assert rig.send_raw(test_bytes_2, b"\x02") == test_bytes_1


    def test_with_open(self, model, rig_file, serial_speed):
        """Call all the methods that depend on open()"""
        rig = Hamlib.Rig(model)
        assert rig is not None

        assert rig.state.comm_state == 0
        assert rig.state.comm_status == Hamlib.RIG_COMM_STATUS_DISCONNECTED
        assert rig.set_conf("rig_pathname", rig_file) is None
        assert rig.set_conf("serial_speed", str(serial_speed)) is None
        assert rig.open() is None
        assert rig.state.comm_state == 1
        assert rig.state.comm_status == Hamlib.RIG_COMM_STATUS_OK
        info = rig.get_info()
        assert isinstance(info, str)

        self.do_test_frequency(rig)
        self.do_test_vfo(rig)
        self.do_test_rit_xit(rig)
        self.do_test_antenna(rig)
        self.do_test_squelch(rig)
        self.do_test_callback(rig)
        # When using the Dummy driver, rig.c replies with a copy of the data
        if model == Hamlib.RIG_MODEL_DUMMY:
            self.do_test_send_raw(rig)

        assert rig.close() is None
        assert rig.state.comm_state == 0
        assert rig.state.comm_status == Hamlib.RIG_COMM_STATUS_DISCONNECTED
        info = rig.get_info()
        assert info is None


    @pytest.mark.skipif('config.getoption("model") != Hamlib.RIG_MODEL_DUMMY')
    def test_misc(self, model):
        """Just call all the methods"""
        rig = Hamlib.Rig(model)
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
        assert isinstance(rig.get_dcd(), int)
        assert isinstance(rig.get_dcd(Hamlib.RIG_VFO_CURR), int)
        assert isinstance(rig.get_ext_func(0), int)
        assert isinstance(rig.get_ext_func(0, 0), int)
        assert rig.get_ext_level(0) is None
        assert rig.get_ext_level(0, 0) is None
        assert rig.get_ext_parm(0) is None
        assert isinstance(rig.get_func(Hamlib.RIG_FUNC_BIT63), int)
        assert isinstance(rig.get_func(Hamlib.RIG_FUNC_BIT63, 0), int)
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
        assert isinstance(rig.get_rptr_offs(), int)
        assert isinstance(rig.get_rptr_offs(Hamlib.RIG_VFO_CURR), int)
        assert isinstance(rig.get_rptr_shift(), int)
        assert isinstance(rig.get_rptr_shift(Hamlib.RIG_VFO_CURR), int)
        assert isinstance(rig.get_split_freq(), float)
        assert isinstance(rig.get_split_freq(Hamlib.RIG_VFO_CURR), float)
        assert len(rig.get_split_mode()) == 2
        assert len(rig.get_split_mode(Hamlib.RIG_VFO_CURR)) == 2
        # assert isinstance(rig.get_trn(), int)  # deprecated
        assert isinstance(rig.get_ts(), int)
        assert isinstance(rig.get_ts(Hamlib.RIG_VFO_CURR), int)
        assert len(rig.get_vfo_info()) == 5
        assert len(rig.get_vfo_info(Hamlib.RIG_VFO_CURR)) == 5
        assert rig.has_get_func(Hamlib.RIG_FUNC_BIT63) is None
        assert rig.has_get_level(0) is None  # FIXME should return setting_t
        assert rig.has_get_parm(0) is None  # FIXME should return setting_t
        assert rig.has_scan(0) is None  # FIXME should return scan_t
        assert rig.has_set_func(0) is None  # FIXME should return setting_t
        # assert rig_has_set_level(0)  FIXME not defined
        assert rig.has_set_parm(0) is None  # FIXME should return setting_t
        assert rig.has_vfo_op(0) is None  # FIXME should return vfo_op_t
        assert rig.lookup_mem_caps() is None
        assert rig.lookup_mem_caps(0) is None
        assert isinstance(rig.mem_count(), int)
        assert rig.open() is None
        assert rig.passband_narrow(Hamlib.RIG_MODE_CW) is None  # FIXME should return passband
        assert rig.passband_normal(Hamlib.RIG_MODE_CW) is None  # FIXME should return passband
        assert rig.passband_wide(Hamlib.RIG_MODE_CW) is None  # FIXME should return passband
        assert isinstance(rig.recv_dtmf(), str)
        assert isinstance(rig.recv_dtmf(Hamlib.RIG_VFO_CURR), str)
        assert rig.reset(Hamlib.RIG_RESET_NONE) is None
        channel = 0
        assert rig.scan(Hamlib.RIG_SCAN_VFO, channel) is None
        assert rig.scan(Hamlib.RIG_SCAN_VFO, channel, Hamlib.RIG_VFO_CURR) is None
        assert rig.send_dtmf(Hamlib.RIG_VFO_CURR, "*0123456789#ABCD") is None
        assert rig.send_morse(Hamlib.RIG_VFO_CURR, "73") is None
        bank = 0
        assert rig.set_bank(Hamlib.RIG_VFO_CURR, bank) is None
        channel = Hamlib.channel()
        channel = Hamlib.channel(0)
        channel = Hamlib.channel(0, Hamlib.RIG_VFO_CURR)
        assert rig.set_channel(channel) is None
        assert rig.set_conf("", "") is None
        assert rig.set_ext_func(0, 0) is None
        assert rig.set_ext_func(0, 0, 0) is None
        level = 0
        value = Hamlib.value_t()
        assert rig.set_ext_level(level, value) is None
        assert rig.set_ext_level(level, value, Hamlib.RIG_VFO_CURR) is None
        value = Hamlib.value_t()
        assert rig.set_ext_parm(0, value) is None
        assert rig.set_func(Hamlib.RIG_FUNC_BIT63, 0, 0) is None
        assert rig.set_level(0, 0, 0) is None
        assert rig.set_mem(0, 0) is None
        assert rig.set_mode(0) is None
        assert rig.set_mode(0, 0) is None
        assert rig.set_mode(0, 0, Hamlib.RIG_VFO_CURR) is None
        assert rig.set_parm(0, 0) is None
        assert rig.set_powerstat(0) is None
        assert rig.set_ptt(0, 0) is None
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
        # assert rig.set_trn(0) is None  # deprecated
        assert rig.set_ts(0, 0) is None
        assert rig.set_vfo_opt(0) is None
        assert rig.token_lookup("") is None
        assert rig.vfo_op(0, 0) is None


    @pytest.mark.skipif('config.getoption("model") != Hamlib.RIG_MODEL_DUMMY')
    def test_object_creation(self, model):
        """Create all objects available"""
        rig = Hamlib.Rig(model)
        assert rig is not None

        assert isinstance(rig.caps, Hamlib.rig_caps)
        assert isinstance(rig.state, Hamlib.rig_state)
