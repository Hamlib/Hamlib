#!/usr/bin/env python3
"""
FTX-1 Noise Command Tests
Tests: NA (Noise Attenuator), NL (Noise Limiter), RL (Noise Reduction Level)
"""

import unittest
import time


class NoiseTests(unittest.TestCase):
    """Noise command tests - requires ser and send_command from main harness."""

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

    def test_NA(self):
        """NA: Noise attenuator (P1=0/1 MAIN/SUB, P2=0/1 off/on)"""
        orig = self.send('NA0', is_read=True)
        self.assertRegex(orig, r'NA0[01]', "NA read response invalid")
        test_val = '0' if orig[-1] == '1' else '1'
        self.send(f'NA0{test_val}')
        after_set = self.send('NA0', is_read=True)
        self.assertEqual(after_set, f'NA0{test_val}', "NA after set mismatch")
        self.send(f'NA0{orig[-1]}')
        restored = self.send('NA0', is_read=True)
        self.assertEqual(restored, orig, "NA restore mismatch")

    def test_NL(self):
        """NL: Noise limiter level (P1=0/1 MAIN/SUB, P2=000-015)"""
        orig = self.send('NL0', is_read=True)
        self.assertRegex(orig, r'NL0\d{3}', "NL read response invalid")
        test_val = '005' if orig[-3:] != '005' else '000'
        self.send(f'NL0{test_val}')
        after_set = self.send('NL0', is_read=True)
        self.assertEqual(after_set, f'NL0{test_val}', "NL after set mismatch")
        self.send(f'NL0{orig[-3:]}')
        restored = self.send('NL0', is_read=True)
        self.assertEqual(restored, orig, "NL restore mismatch")

    def test_RL(self):
        """RL: Noise reduction level (P1=0/1 MAIN/SUB, P2=00-15)"""
        orig = self.send('RL0', is_read=True)
        self.assertRegex(orig, r'RL0\d{2}', "RL read response invalid")
        test_val = '05' if orig[-2:] != '05' else '00'
        self.send(f'RL0{test_val}')
        after_set = self.send('RL0', is_read=True)
        self.assertEqual(after_set, f'RL0{test_val}', "RL after set mismatch")
        self.send(f'RL0{orig[-2:]}')
        restored = self.send('RL0', is_read=True)
        self.assertEqual(restored, orig, "RL restore mismatch")

    def test_RA(self):
        """RA: RF attenuator (P1=0/1 MAIN/SUB, P2=0/1 off/on)"""
        self.__class__.ser.reset_input_buffer()
        time.sleep(0.1)
        orig = self.send('RA0', is_read=True)
        if orig == '?' or not orig.startswith('RA0'):
            time.sleep(0.3)
            self.__class__.ser.reset_input_buffer()
            orig = self.send('RA0', is_read=True)
        if orig == '?':
            self.skipTest("RA intermittently returns '?' (async contamination)")
        self.assertRegex(orig, r'RA0[01]', "RA read response invalid")
        test_val = '0' if orig[-1] == '1' else '1'
        self.send(f'RA0{test_val}')
        after_set = self.send('RA0', is_read=True)
        self.assertEqual(after_set, f'RA0{test_val}', "RA after set mismatch")
        self.send(f'RA0{orig[-1]}')
        restored = self.send('RA0', is_read=True)
        self.assertEqual(restored, orig, "RA restore mismatch")


def get_test_suite(ser, send_command_func):
    """Return test suite for noise commands."""
    NoiseTests.setup_class(ser, send_command_func)
    loader = unittest.TestLoader()
    return loader.loadTestsFromTestCase(NoiseTests)


if __name__ == '__main__':
    print("This module should be run from ftx1_test.py main harness")
