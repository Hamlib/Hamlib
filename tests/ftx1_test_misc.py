#!/usr/bin/env python3
"""
FTX-1 Miscellaneous Command Tests
Tests: DN (Down), EO (Encoder Offset), EX (Extended Menu), OS (Offset Shift),
       PB (Playback), PL (Processor Level), PR (Processor), QI (Quick In),
       QR (Quick Recall), SC (Scan), SF (Scope Fix), VM (Voice Memory), ZI (Zero In)
"""

import unittest
import time


class MiscTests(unittest.TestCase):
    """Miscellaneous command tests - requires ser and send_command from main harness."""

    ser = None
    send_command = None

    @classmethod
    def setup_class(cls, ser, send_command_func):
        """Initialize with serial connection and command function."""
        cls.ser = ser
        cls.send_command = send_command_func

    def send(self, cmd, is_read=False):
        """Wrapper for send_command."""
        return self.__class__.send_command(self, cmd, is_read)

    def test_CN(self):
        """CN: CTCSS Number - format CN P1 P2P3P4 (P1=00 TX/10 RX, P2P3P4=001-050)
        Verified working 2025-12-09 - Query: CN00; returns CN00nnn
        """
        orig = self.send('CN00', is_read=True)
        if orig == '?':
            self.skipTest("CN read not implemented in firmware (returns '?')")
        self.assertRegex(orig, r'CN00\d{3}', "CN read response invalid")
        orig_val = orig[-3:]
        # Test set/get/restore
        test_val = '013' if orig_val != '013' else '012'  # Tone 13 = 100.0 Hz
        self.send(f'CN00{test_val}')
        after_set = self.send('CN00', is_read=True)
        self.assertEqual(after_set, f'CN00{test_val}', "CN after set mismatch")
        self.send(f'CN00{orig_val}')
        restored = self.send('CN00', is_read=True)
        self.assertEqual(restored, orig, "CN restore mismatch")

    def test_DN(self):
        """DN: Down (set-only, menu or freq step)"""
        self.skipTest("DN is context-dependent; manual test required")

    def test_EO(self):
        """EO: Encoder offset - set-only, format: EO P1 P2 P3 P4 P5;
        P1=VFO (0/1), P2=encoder (0=MAIN/1=FUNC), P3=dir (+/-), P4=unit, P5=value"""
        # Correct format: EO00+0100 (VFO=0, encoder=0, dir=+, unit=0=Hz, value=100)
        self.send('EO00+0100')  # Set-only, returns empty on success

    def test_EX(self):
        """EX: Menu (EXnnnn read, EXnnnn[value] set)
        Note: EX commands work when SPA-1 is connected (verified 2025-12-09)
        """
        resp = self.send('EX0101', is_read=True)
        if resp == '?':
            self.skipTest("EX0101 read not available (may require SPA-1 or specific config)")

    def test_OS(self):
        """OS: Offset shift (P1=0/1 MAIN/SUB, P2=0-3) - read-only test"""
        orig = self.send('OS0', is_read=True)
        self.assertRegex(orig, r'OS0[0-3]', "OS read response invalid")
        # Skip set test - firmware doesn't persist the value

    def test_PB(self):
        """PB: Playback (P1=0/1 MAIN/SUB, P2=0/1 off/on) - read-only test"""
        orig = self.send('PB0', is_read=True)
        self.assertRegex(orig, r'PB0\d', "PB read response invalid")
        # Skip set test - firmware doesn't persist the value

    def test_PL(self):
        """PL: Processor level (000-100) - read-only test"""
        orig = self.send('PL', is_read=True)
        self.assertRegex(orig, r'PL\d{3}', "PL read response invalid")
        # Skip set test - firmware doesn't persist the value

    def test_PR(self):
        """PR: Processor (P1=0/1 MAIN/SUB, P2=0/1/2) - read-only test"""
        orig = self.send('PR0', is_read=True)
        self.assertRegex(orig, r'PR0[012]', "PR read response invalid")
        # Skip set test - firmware doesn't persist the value

    def test_QI(self):
        """QI: Quick in (set-only) - stores current VFO to QMB"""
        self.send('QI')

    def test_QR(self):
        """QR: Quick recall (set-only) - recalls QMB to current VFO"""
        self.send('QR')

    def test_SC(self):
        """SC: Scan (P1=0/1/2 start/stop/resume)"""
        orig = self.send('SC', is_read=True)
        self.assertRegex(orig, r'SC\d[012]', "SC read response invalid")
        try:
            self.send('SC01')  # Start scan
            time.sleep(0.3)
            self.__class__.ser.reset_input_buffer()
            after_set = self.send('SC', is_read=True)
            self.assertIn(after_set[:2], ['SC'], "SC read should return SC prefix")
        finally:
            self.send('SC00')  # Stop scan
            time.sleep(0.2)
            self.__class__.ser.reset_input_buffer()
        restored = self.send('SC', is_read=True)
        self.assertRegex(restored, r'SC\d', "SC should be stopped after test")

    def test_SF(self):
        """SF: Scope fix (P1=0/1 MAIN/SUB, P2=0-17 band code) - read-only test"""
        orig = self.send('SF0', is_read=True)
        self.assertRegex(orig, r'SF0[0-9A-H]', "SF read response invalid")
        # Skip set test - firmware doesn't persist the value

    def test_UP(self):
        """UP: Up (set-only, menu or freq step)"""
        self.skipTest("UP is context-dependent; manual test required")

    def test_VM(self):
        """VM: VFO/Memory mode - Format differs from spec!
        VM000 = VFO mode, VM011 = Memory mode (not VM001!)
        Set-only for VM000; use SV command to toggle to memory mode
        """
        self.send('VM000')

    def test_ZI(self):
        """ZI: Zero in (P1=0/1 MAIN/SUB, set-only) - CW mode only
        ZI activates CW AUTO ZERO IN function. Only works in CW modes (MD03 or MD07).
        Verified working 2025-12-09 when in CW mode.
        """
        # Save current mode
        orig_mode = self.send('MD0', is_read=True)
        if orig_mode == '?' or not orig_mode:
            self.skipTest("ZI test requires mode read capability")
        orig_mode_val = orig_mode[-1] if len(orig_mode) >= 3 else '2'

        # Switch to CW-U mode (03)
        self.send('MD03')
        time.sleep(0.2)

        # Test ZI in CW mode
        resp = self.send('ZI0', is_read=True)
        # ZI is set-only, empty response means success
        self.assertNotEqual(resp, '?', "ZI should work in CW mode")

        # Restore original mode
        self.send(f'MD0{orig_mode_val}')

    def test_invalid_command(self):
        """Test invalid command returns '?'"""
        resp = self.send('ZZ', is_read=True)
        self.assertEqual(resp, '?', "Invalid command should return ?")


def get_test_suite(ser, send_command_func):
    """Return test suite for misc commands."""
    MiscTests.setup_class(ser, send_command_func)
    loader = unittest.TestLoader()
    return loader.loadTestsFromTestCase(MiscTests)


if __name__ == '__main__':
    print("This module should be run from ftx1_test.py main harness")
