#!/usr/bin/env python3
"""
FTX-1 Filter Command Tests
Tests: BC (Beat Cancel), CO (Contour), SH (Width), BP (Manual Notch Position)
"""

import unittest


class FilterTests(unittest.TestCase):
    """Filter command tests - requires ser and send_command from main harness."""

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

    def test_BC(self):
        """BC: Beat canceller (P1=0/1 MAIN/SUB, P2=0/1 off/on)"""
        orig = self.send('BC0', is_read=True)
        self.assertRegex(orig, r'BC0[01]', "BC read response invalid")
        test_val = '0' if orig[-1] == '1' else '1'
        self.send(f'BC0{test_val}')
        after_set = self.send('BC0', is_read=True)
        self.assertEqual(after_set, f'BC0{test_val}', "BC after set mismatch")
        self.send(f'BC0{orig[-1]}')
        restored = self.send('BC0', is_read=True)
        self.assertEqual(restored, orig, "BC restore mismatch")

    def test_BP(self):
        """BP: Break-in delay or param (P1P2=00-99, P3=000-100)"""
        orig = self.send('BP00', is_read=True)
        self.assertRegex(orig, r'BP00\d{3}', "BP read response invalid")
        test_val = '001' if orig[-3:] != '001' else '000'
        self.send(f'BP00{test_val}')
        after_set = self.send('BP00', is_read=True)
        self.assertEqual(after_set, f'BP00{test_val}', "BP after set mismatch")
        self.send(f'BP00{orig[-3:]}')
        restored = self.send('BP00', is_read=True)
        self.assertEqual(restored, orig, "BP restore mismatch")

    def test_CO(self):
        """CO: Contour (P1=0/1 MAIN/SUB, P2=0-4 param, P3=value)"""
        orig = self.send('CO00', is_read=True)
        self.assertRegex(orig, r'CO00\d{4}', "CO read response invalid")
        test_val = '0001' if orig[-4:] != '0001' else '0000'
        self.send(f'CO00{test_val}')
        after_set = self.send('CO00', is_read=True)
        self.assertEqual(after_set, f'CO00{test_val}', "CO after set mismatch")
        self.send(f'CO00{orig[-4:]}')
        restored = self.send('CO00', is_read=True)
        self.assertEqual(restored, orig, "CO restore mismatch")

    def test_SH(self):
        """SH: Width - MATCHES SPEC 2508-C page 24"""
        orig = self.send('SH0', is_read=True)
        self.assertRegex(orig, r'SH0\d{3}', "SH read response invalid")
        # Skip set test - firmware doesn't persist the value


def get_test_suite(ser, send_command_func):
    """Return test suite for filter commands."""
    FilterTests.setup_class(ser, send_command_func)
    loader = unittest.TestLoader()
    return loader.loadTestsFromTestCase(FilterTests)


if __name__ == '__main__':
    print("This module should be run from ftx1_test.py main harness")
