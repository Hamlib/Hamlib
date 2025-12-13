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

    def test_CF(self):
        """CF: Clarifier offset (set-only, does not enable clarifier)
        Format: CF P1 P2 P3 [+/-] PPPP (9 chars after CF)
        P1=VFO, P2=RIT, P3=XIT (P3 must be 1 for command to work)
        Verified working 2025-12-09 - sets offset value only.
        """
        # Read current offset from IF command
        if_resp = self.send('IF', is_read=True)
        if not if_resp.startswith('IF') or len(if_resp) < 21:
            self.skipTest("Cannot read IF response for CF test")

        # Extract current offset from IF (positions 16-20)
        orig_offset = if_resp[16:21] if len(if_resp) >= 21 else '+0000'

        # Test set with different values
        test_val = '+0500' if orig_offset != '+0500' else '+0300'
        resp = self.send(f'CF001{test_val}')
        self.assertNotEqual(resp, '?', "CF command should be accepted")

        # Verify via IF command
        time.sleep(0.2)
        after_if = self.send('IF', is_read=True)
        after_offset = after_if[16:21] if len(after_if) >= 21 else ''
        self.assertEqual(after_offset, test_val, f"CF offset should be {test_val}")

        # Test negative offset
        resp = self.send('CF001-0250')
        self.assertNotEqual(resp, '?', "CF negative offset should be accepted")
        time.sleep(0.2)
        neg_if = self.send('IF', is_read=True)
        neg_offset = neg_if[16:21] if len(neg_if) >= 21 else ''
        self.assertEqual(neg_offset, '-0250', "CF offset should be -0250")

        # Note: Cannot restore - CF only sets value, doesn't clear
        # Set back to something close to original
        self.send(f'CF001{orig_offset}')

    def test_CF_format(self):
        """CF: Test that P3=0 (RIT only) format is rejected"""
        # P3=0 should return '?' - only P3=1 works
        resp = self.send('CF010+0100', is_read=True)
        self.assertEqual(resp, '?', "CF with P3=0 should return '?'")

        # P3=1 should work
        resp = self.send('CF001+0100', is_read=True)
        self.assertNotEqual(resp, '?', "CF with P3=1 should be accepted")

    def test_OS(self):
        """OS: Offset (Repeater Shift) for FM mode
        Format: OS P1 P2 where P1=VFO (0/1), P2=Shift mode
        Shift modes: 0=Simplex, 1=Plus, 2=Minus, 3=ARS
        NOTE: This command only works in FM mode!
        """
        # Check if in FM mode
        mode = self.send('MD0', is_read=True)
        if not mode.startswith('MD0'):
            self.skipTest("Cannot read mode")

        mode_code = mode[3] if len(mode) > 3 else ''
        if mode_code not in ('4', 'B'):  # 4=FM, B=FM-N
            self.skipTest(f"OS only works in FM mode (current mode code: {mode_code})")

        orig = self.send('OS0', is_read=True)
        if orig == '?' or not orig.startswith('OS0'):
            self.skipTest("OS command not available")

        self.assertRegex(orig, r'OS0[0-3]', "OS read response invalid")

        # Toggle between simplex (0) and plus (1)
        test_val = '1' if orig[3] != '1' else '0'
        self.send(f'OS0{test_val}')
        after = self.send('OS0', is_read=True)
        self.assertEqual(after, f'OS0{test_val}', "OS set failed")

        # Restore original state
        self.send(f'OS0{orig[3]}')
        restored = self.send('OS0', is_read=True)
        self.assertEqual(restored, orig, "OS restore failed")


def get_test_suite(ser, send_command_func):
    """Return test suite for frequency commands."""
    FrequencyTests.setup_class(ser, send_command_func)
    loader = unittest.TestLoader()
    return loader.loadTestsFromTestCase(FrequencyTests)


if __name__ == '__main__':
    print("This module should be run from ftx1_test.py main harness")
