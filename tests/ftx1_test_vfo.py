#!/usr/bin/env python3
"""
FTX-1 VFO Command Tests
Tests: VS (VFO Select), ST (Split), FT (TX VFO)
"""

import unittest
import time


class VFOTests(unittest.TestCase):
    """VFO command tests - requires ser and send_command from main harness."""

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

    def test_VS(self):
        """VS: VFO select (0/1 MAIN/SUB)"""
        # SAFETY: Ensure TX is off before VFO manipulation
        self.send('MX0')
        self.send('TX0')
        self.send('VX0')
        self.send('FT0')
        orig = self.send('VS', is_read=True)
        self.assertRegex(orig, r'VS[01]', "VS read response invalid")
        test_val = '0' if orig[-1] == '1' else '1'
        self.send(f'VS{test_val}')
        after_set = self.send('VS', is_read=True)
        self.assertEqual(after_set, f'VS{test_val}', "VS after set mismatch")
        # Always restore to VS0 (MAIN) for safety
        self.send('VS0')
        restored = self.send('VS', is_read=True)
        self.assertEqual(restored, 'VS0', "VS should be VS0 for safety")

    def test_ST(self):
        """ST: Split toggle (0/1 off/on)"""
        # SAFETY: Ensure TX is off before split manipulation
        self.send('MX0')
        self.send('TX0')
        self.send('VX0')
        self.send('FT0')
        orig = self.send('ST', is_read=True)
        self.assertRegex(orig, r'ST[01]', "ST read response invalid")
        test_val = '0' if orig[-1] == '1' else '1'
        self.send(f'ST{test_val}')
        after_set = self.send('ST', is_read=True)
        self.assertEqual(after_set, f'ST{test_val}', "ST after set mismatch")
        # Always restore to ST0 (split OFF) for safety
        self.send('ST0')
        restored = self.send('ST', is_read=True)
        self.assertEqual(restored, 'ST0', "ST should be ST0 for safety")

    def test_FT(self):
        """FT: TX select (0/1 MAIN/SUB)"""
        # SAFETY: Ensure MOX is OFF before changing TX VFO
        self.send('MX0')
        orig = self.send('FT', is_read=True)
        self.assertRegex(orig, r'FT[01]', "FT read response invalid")
        test_val = '0' if orig[-1] == '1' else '1'
        self.send(f'FT{test_val}')
        after_set = self.send('FT', is_read=True)
        self.assertEqual(after_set, f'FT{test_val}', "FT after set mismatch")
        # Always restore to FT0 (MAIN as TX) for safety
        self.send('FT0')
        restored = self.send('FT', is_read=True)
        self.assertEqual(restored, 'FT0', "FT should be FT0 for safety")

    def test_AB(self):
        """AB: VFO-A/B - Copy VFO-A freq to VFO-B (set-only)"""
        # SAFETY: Ensure no TX state before VFO manipulation
        self.send('MX0')
        self.send('TX0')
        self.send('VX0')
        self.send('FT0')
        self.send('VS0')
        orig_fa = self.send('FA', is_read=True)
        self.assertRegex(orig_fa, r'FA\d{9}', "Original FA read invalid")
        orig_fb = self.send('FB', is_read=True)
        self.assertRegex(orig_fb, r'FB\d{9}', "Original FB read invalid")
        # Send AB command - copies VFO-A to VFO-B
        self.send('AB')
        new_fb = self.send('FB', is_read=True)
        self.assertEqual(new_fb, f'FB{orig_fa[2:]}', "After AB, FB should be original FA (copy)")
        # Restore VFO-B to original
        self.send(f'FB{orig_fb[2:]}')
        restored_fb = self.send('FB', is_read=True)
        self.assertEqual(restored_fb, orig_fb, "FB restore mismatch")

    def test_BA(self):
        """BA: Copy VFO (set-only) - copies SUB to MAIN"""
        # SAFETY: Ensure no TX state before VFO manipulation
        self.send('MX0')
        self.send('TX0')
        self.send('VX0')
        self.send('FT0')
        self.send('VS0')
        orig_main = self.send('FA', is_read=True)
        self.assertRegex(orig_main, r'FA\d{9}', "Original MAIN freq invalid")
        orig_sub = self.send('FB', is_read=True)
        self.assertRegex(orig_sub, r'FB\d{9}', "Original SUB freq invalid")
        self.send('BA')  # Copy SUB to MAIN (B→A)
        new_main = self.send('FA', is_read=True)
        expected_main = f'FA{orig_sub[2:]}'
        self.assertEqual(new_main, expected_main, f"After BA, MAIN should match original SUB")
        # Restore MAIN to original
        self.send(f'FA{orig_main[2:]}')
        restored_main = self.send('FA', is_read=True)
        self.assertEqual(restored_main, orig_main, "MAIN restore mismatch")

    def test_SV(self):
        """SV: Swap VFO/mem (set-only)"""
        # SAFETY: Ensure no TX state before swapping
        self.send('MX0')
        self.send('TX0')
        self.send('VX0')
        self.send('FT0')
        self.send('VS0')
        self.send('SV')
        self.send('SV')  # Double to restore


def get_test_suite(ser, send_command_func, tx_tests=False):
    """Return test suite for VFO commands."""
    VFOTests.setup_class(ser, send_command_func, tx_tests)
    loader = unittest.TestLoader()
    return loader.loadTestsFromTestCase(VFOTests)


if __name__ == '__main__':
    print("This module should be run from ftx1_test.py main harness")
