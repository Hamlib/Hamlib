#!/usr/bin/env python3
"""
FTX-1 Frequency Command Tests
Tests: FA (VFO A), FB (VFO B), RIT, XIT, BD (Band Down), BU (Band Up)
"""

import unittest
import time


class FrequencyTests(unittest.TestCase):
    """Frequency command tests - requires ser and send_command from main harness."""

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

    def test_FA(self):
        """FA: VFO A freq (9 digits Hz)"""
        orig = self.send('FA', is_read=True)
        self.assertRegex(orig, r'FA\d{9}', "FA read response invalid")
        test_val = '014074000' if orig[-9:] != '014074000' else '014000000'
        self.send(f'FA{test_val}')
        after_set = self.send('FA', is_read=True)
        self.assertEqual(after_set, f'FA{test_val}', "FA after set mismatch")
        self.send(f'FA{orig[-9:]}')
        restored = self.send('FA', is_read=True)
        self.assertEqual(restored, orig, "FA restore mismatch")

    def test_FB(self):
        """FB: VFO B freq (similar to FA)"""
        orig = self.send('FB', is_read=True)
        self.assertRegex(orig, r'FB\d{9}', "FB read response invalid")
        test_val = '014074000' if orig[-9:] != '014074000' else '014000000'
        self.send(f'FB{test_val}')
        after_set = self.send('FB', is_read=True)
        self.assertEqual(after_set, f'FB{test_val}', "FB after set mismatch")
        self.send(f'FB{orig[-9:]}')
        restored = self.send('FB', is_read=True)
        self.assertEqual(restored, orig, "FB restore mismatch")

    def test_BD(self):
        """BD: Band down (P1=0/1 MAIN/SUB, set-only)"""
        orig_freq = self.send('FA', is_read=True)
        self.send('BD0')  # Band down MAIN
        time.sleep(0.5)
        new_freq = self.send('FA', is_read=True)
        self.assertNotEqual(new_freq, orig_freq, "Frequency should change after BD")
        self.send('BU0')  # Band up to restore
        restored = self.send('FA', is_read=True)
        self.assertEqual(restored, orig_freq, "Frequency restore after BU")

    def test_BU(self):
        """BU: Band up (similar to BD)"""
        orig_freq = self.send('FA', is_read=True)
        self.send('BU0')  # Band up MAIN
        time.sleep(0.5)
        new_freq = self.send('FA', is_read=True)
        self.assertNotEqual(new_freq, orig_freq, "Frequency should change after BU")
        self.send('BD0')  # Band down to restore
        restored = self.send('FA', is_read=True)
        self.assertEqual(restored, orig_freq, "Frequency restore after BD")


def get_test_suite(ser, send_command_func):
    """Return test suite for frequency commands."""
    FrequencyTests.setup_class(ser, send_command_func)
    loader = unittest.TestLoader()
    return loader.loadTestsFromTestCase(FrequencyTests)


if __name__ == '__main__':
    print("This module should be run from ftx1_test.py main harness")
