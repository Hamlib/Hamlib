#!/usr/bin/env python3
"""
FTX-1 Preamp/Attenuator Command Tests
Tests: PA (Preamp)
"""

import unittest


class PreampTests(unittest.TestCase):
    """Preamp command tests - requires ser and send_command from main harness."""

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

    def test_PA(self):
        """PA: Preamp (P1=0/1 MAIN/SUB, P2=0-2 off/amp1/amp2)"""
        orig = self.send('PA0', is_read=True)
        self.assertRegex(orig, r'PA0[0-2]', "PA read response invalid")
        test_val = '0' if orig[-1] == '1' else '1'
        self.send(f'PA0{test_val}')
        after_set = self.send('PA0', is_read=True)
        self.assertEqual(after_set, f'PA0{test_val}', "PA after set mismatch")
        self.send(f'PA0{orig[-1]}')
        restored = self.send('PA0', is_read=True)
        self.assertEqual(restored, orig, "PA restore mismatch")


def get_test_suite(ser, send_command_func):
    """Return test suite for preamp commands."""
    PreampTests.setup_class(ser, send_command_func)
    loader = unittest.TestLoader()
    return loader.loadTestsFromTestCase(PreampTests)


if __name__ == '__main__':
    print("This module should be run from ftx1_test.py main harness")
