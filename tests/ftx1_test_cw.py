#!/usr/bin/env python3
"""
FTX-1 CW/Keyer Command Tests
Tests: KP (Keyer Pitch), KR (Keyer), KS (Key Speed), KY (Keyer Send),
       SD (Semi Break-in Delay), KM (Keyer Memory), LM (Load Message)
"""

import unittest
import time


class CWTests(unittest.TestCase):
    """CW/Keyer command tests - requires ser and send_command from main harness."""

    ser = None
    send_command = None
    TX_TESTS_ENABLED = False

    @classmethod
    def setup_class(cls, ser, send_command_func, tx_tests=False):
        """Initialize with serial connection and command function."""
        cls.ser = ser
        cls.send_command = send_command_func
        cls.TX_TESTS_ENABLED = tx_tests

    def send(self, cmd, is_read=False):
        """Wrapper for send_command."""
        return self.__class__.send_command(self, cmd, is_read)

    def test_KP(self):
        """KP: Keyer pitch (00-99)"""
        orig = self.send('KP', is_read=True)
        self.assertRegex(orig, r'KP\d{2}', "KP read response invalid")
        test_val = '20' if orig[-2:] != '20' else '10'
        self.send(f'KP{test_val}')
        after_set = self.send('KP', is_read=True)
        self.assertEqual(after_set, f'KP{test_val}', "KP after set mismatch")
        self.send(f'KP{orig[-2:]}')
        restored = self.send('KP', is_read=True)
        self.assertEqual(restored, orig, "KP restore mismatch")

    def test_KR(self):
        """KR: Keyer (0/1 off/on) - read-only test"""
        orig = self.send('KR', is_read=True)
        self.assertRegex(orig, r'KR[01]', "KR read response invalid")
        # Skip set test - firmware doesn't persist the value

    def test_KS(self):
        """KS: Key speed (000-060 wpm)"""
        orig = self.send('KS', is_read=True)
        self.assertRegex(orig, r'KS\d{3}', "KS read response invalid")
        test_val = '020' if orig[-3:] != '020' else '010'
        self.send(f'KS{test_val}')
        after_set = self.send('KS', is_read=True)
        self.assertEqual(after_set, f'KS{test_val}', "KS after set mismatch")
        self.send(f'KS{orig[-3:]}')
        restored = self.send('KS', is_read=True)
        self.assertEqual(restored, orig, "KS restore mismatch")

    def test_KY(self):
        """KY: Keyer send (P1=0-9 message, set-only) - WARNING: causes CW TX!"""
        if not self.__class__.TX_TESTS_ENABLED:
            self.skipTest("TX tests disabled (use --tx-tests to enable)")
        self.send('KY00')  # Send message 0

    def test_SD(self):
        """SD: Semi break-in delay (00-30) - CW TX parameter"""
        # TX Safety: Ensure no TX state
        self.send('MX0')
        self.send('TX0')
        self.send('VX0')
        self.send('FT0')
        self.send('VS0')
        orig = self.send('SD', is_read=True)
        self.assertRegex(orig, r'SD\d{2}', "SD read response invalid")
        test_val = '10' if orig[-2:] != '10' else '05'
        self.send(f'SD{test_val}')
        after_set = self.send('SD', is_read=True)
        self.assertEqual(after_set, f'SD{test_val}', "SD after set mismatch")
        self.send(f'SD{orig[-2:]}')
        restored = self.send('SD', is_read=True)
        self.assertEqual(restored, orig, "SD restore mismatch")

    def test_KM(self):
        """KM: Keyer memory (P1=1-5 message slot)"""
        resp = self.send('KM1', is_read=True)
        if resp == 'KM' or resp == 'KM1':
            self.skipTest("KM read returns empty message (firmware limitation)")

    def test_LM(self):
        """LM: Load message (P1=0/1 start/stop, set-only)"""
        self.send('LM00')  # Example start load


def get_test_suite(ser, send_command_func, tx_tests=False):
    """Return test suite for CW commands."""
    CWTests.setup_class(ser, send_command_func, tx_tests)
    loader = unittest.TestLoader()
    return loader.loadTestsFromTestCase(CWTests)


if __name__ == '__main__':
    print("This module should be run from ftx1_test.py main harness")
