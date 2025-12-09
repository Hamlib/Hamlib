#!/usr/bin/env python3
"""
FTX-1 Mode Command Tests
Tests: MD (Mode), FN (Filter Number)
"""

import unittest


class ModeTests(unittest.TestCase):
    """Mode command tests - requires ser and send_command from main harness."""

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

    def test_MD(self):
        """MD: Mode (P1=0/1 MAIN/SUB, P2=0-9,A-J mode code)"""
        orig = self.send('MD0', is_read=True)
        self.assertRegex(orig, r'MD0[0-9A-J]', "MD read response invalid")
        test_val = '1' if orig[-1] != '1' else '2'
        self.send(f'MD0{test_val}')
        after_set = self.send('MD0', is_read=True)
        self.assertEqual(after_set, f'MD0{test_val}', "MD after set mismatch")
        self.send(f'MD0{orig[-1]}')
        restored = self.send('MD0', is_read=True)
        self.assertEqual(restored, orig, "MD restore mismatch")

    def test_FN(self):
        """FN: Filter number (0/1/2 normal/narrow/wide)"""
        orig = self.send('FN', is_read=True)
        self.assertRegex(orig, r'FN[012]', "FN read response invalid")
        test_val = '0' if orig[-1] == '1' else '1'
        self.send(f'FN{test_val}')
        after_set = self.send('FN', is_read=True)
        self.assertEqual(after_set, f'FN{test_val}', "FN after set mismatch")
        self.send(f'FN{orig[-1]}')
        restored = self.send('FN', is_read=True)
        self.assertEqual(restored, orig, "FN restore mismatch")


def get_test_suite(ser, send_command_func):
    """Return test suite for mode commands."""
    ModeTests.setup_class(ser, send_command_func)
    loader = unittest.TestLoader()
    return loader.loadTestsFromTestCase(ModeTests)


if __name__ == '__main__':
    print("This module should be run from ftx1_test.py main harness")
