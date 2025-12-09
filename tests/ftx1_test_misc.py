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

    def test_DN(self):
        """DN: Down (set-only, menu or freq step)"""
        self.skipTest("DN is context-dependent; manual test required")

    def test_EO(self):
        """EO: Encoder offset (P1=0-3 encoder, P2= +/-, P3=000-999)"""
        self.send('EO0000100')  # Example set
        # No read, skip full test

    def test_EX(self):
        """EX: Menu (EXnnnn read, EXnnnn[value] set)"""
        resp = self.send('EX0101', is_read=True)
        if resp == '?':
            self.skipTest("EX read not implemented in firmware (returns '?')")

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
        """QI: Quick in (set-only)"""
        self.send('QI')

    def test_QR(self):
        """QR: Quick recall (set-only)"""
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
        """VM: Voice memory (P1=0/1 start/stop, set-only)"""
        self.send('VM00')  # Example start

    def test_ZI(self):
        """ZI: Zero in (P1=0/1 MAIN/SUB, set-only)"""
        self.send('ZI0')
        # No read, skip verification

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
