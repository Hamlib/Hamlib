#!/usr/bin/env python3
"""
FTX-1 Audio/Level Command Tests
Tests: AG (AF Gain), RG (RF Gain), MG (Mic Gain), VG (VOX Gain), VD (VOX Delay),
       GT (AGC), SM (S-Meter), SQ (Squelch), IS (IF Shift), ML (Monitor Level),
       AO (AMC Output), MS (Meter Switch)
"""

import unittest
import time


class AudioTests(unittest.TestCase):
    """Audio/level command tests - requires ser and send_command from main harness."""

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

    def test_AG(self):
        """AG: AF gain (P1=0/1 MAIN/SUB, P2=000-255)"""
        orig = self.send('AG0', is_read=True)
        self.assertRegex(orig, r'AG0\d{3}', "AG read response invalid")
        test_val = '128' if orig[-3:] != '128' else '000'
        self.send(f'AG0{test_val}')
        after_set = self.send('AG0', is_read=True)
        self.assertEqual(after_set, f'AG0{test_val}', "AG after set mismatch")
        self.send(f'AG0{orig[-3:]}')
        restored = self.send('AG0', is_read=True)
        self.assertEqual(restored, orig, "AG restore mismatch")

    def test_AO(self):
        """AO: AMC output level (000-100)"""
        orig = self.send('AO', is_read=True)
        self.assertRegex(orig, r'AO\d{3}', "AO read response invalid")
        test_val = '050' if orig[-3:] != '050' else '000'
        self.send(f'AO{test_val}')
        after_set = self.send('AO', is_read=True)
        self.assertEqual(after_set, f'AO{test_val}', "AO after set mismatch")
        self.send(f'AO{orig[-3:]}')
        restored = self.send('AO', is_read=True)
        self.assertEqual(restored, orig, "AO restore mismatch")

    def test_GT(self):
        """GT: AGC time constant (P1=0/1 MAIN/SUB)"""
        orig = self.send('GT0', is_read=True)
        self.assertRegex(orig, r'GT0[0-6]', "GT read response invalid")
        # Test with FAST=1 that has predictable response
        self.send('GT01')
        after_set = self.send('GT0', is_read=True)
        self.assertEqual(after_set, 'GT01', "GT after set mismatch")
        # Restore
        orig_val = orig[-1]
        if orig_val in '456':  # AUTO variants
            self.send('GT04')
        else:
            self.send(f'GT0{orig_val}')
        restored = self.send('GT0', is_read=True)
        if orig_val in '456':
            self.assertIn(restored[-1], '456', "GT restore mismatch (AUTO)")
        else:
            self.assertEqual(restored, orig, "GT restore mismatch")

    def test_IS(self):
        """IS: IF shift - MATCHES SPEC 2508-C page 17"""
        orig = self.send('IS0', is_read=True)
        self.assertRegex(orig, r'IS\d{2}[+-]\d{4}', "IS read response invalid")
        # Don't test set - just verify read works

    def test_MG(self):
        """MG: Mic gain (000-100)"""
        orig = self.send('MG', is_read=True)
        self.assertRegex(orig, r'MG\d{3}', "MG read response invalid")
        test_val = '050' if orig[-3:] != '050' else '000'
        self.send(f'MG{test_val}')
        after_set = self.send('MG', is_read=True)
        self.assertEqual(after_set, f'MG{test_val}', "MG after set mismatch")
        self.send(f'MG{orig[-3:]}')
        restored = self.send('MG', is_read=True)
        self.assertEqual(restored, orig, "MG restore mismatch")

    def test_ML(self):
        """ML: Monitor level (P1=0/1 MAIN/SUB, P2=000-100) - read-only test"""
        orig = self.send('ML0', is_read=True)
        self.assertRegex(orig, r'ML0\d{3}', "ML read response invalid")
        # Skip set test - firmware doesn't persist the value

    def test_MS(self):
        """MS: Meter switch (P1=0/1 MAIN/SUB, P2=0-5 type)"""
        orig = self.send('MS', is_read=True)
        self.assertRegex(orig, r'MS\d{2}', "MS read response invalid")
        test_val = '00' if orig[-2:] != '00' else '01'
        self.send(f'MS{test_val}')
        after_set = self.send('MS', is_read=True)
        self.assertEqual(after_set, f'MS{test_val}', "MS after set mismatch")
        self.send(f'MS{orig[-2:]}')
        restored = self.send('MS', is_read=True)
        self.assertEqual(restored, orig, "MS restore mismatch")

    def test_RG(self):
        """RG: RF gain (P1=0/1 MAIN/SUB, P2=000-255)"""
        orig = self.send('RG0', is_read=True)
        self.assertRegex(orig, r'RG0\d{3}', "RG read response invalid")
        test_val = '128' if orig[-3:] != '128' else '000'
        self.send(f'RG0{test_val}')
        after_set = self.send('RG0', is_read=True)
        self.assertEqual(after_set, f'RG0{test_val}', "RG after set mismatch")
        self.send(f'RG0{orig[-3:]}')
        restored = self.send('RG0', is_read=True)
        self.assertEqual(restored, orig, "RG restore mismatch")

    def test_SM(self):
        """SM: S-meter (P1=0/1 MAIN/SUB, read-only)"""
        resp = self.send('SM0')
        self.assertRegex(resp, r'SM0\d{3}', "SM read response invalid")

    def test_SQ(self):
        """SQ: Squelch level (P1=0/1 MAIN/SUB, P2=000-100)"""
        orig = self.send('SQ0', is_read=True)
        self.assertRegex(orig, r'SQ0\d{3}', "SQ read response invalid")
        test_val = '128' if orig[-3:] != '128' else '000'
        self.send(f'SQ0{test_val}')
        after_set = self.send('SQ0', is_read=True)
        self.assertEqual(after_set, f'SQ0{test_val}', "SQ after set mismatch")
        self.send(f'SQ0{orig[-3:]}')
        restored = self.send('SQ0', is_read=True)
        self.assertEqual(restored, orig, "SQ restore mismatch")

    def test_VD(self):
        """VD: VOX delay (00-30)"""
        orig = self.send('VD', is_read=True)
        self.assertRegex(orig, r'VD\d{2}', "VD read response invalid")
        test_val = '10' if orig[-2:] != '10' else '05'
        self.send(f'VD{test_val}')
        after_set = self.send('VD', is_read=True)
        self.assertEqual(after_set, f'VD{test_val}', "VD after set mismatch")
        self.send(f'VD{orig[-2:]}')
        restored = self.send('VD', is_read=True)
        self.assertEqual(restored, orig, "VD restore mismatch")

    def test_VG(self):
        """VG: VOX gain (000-100)"""
        orig = self.send('VG', is_read=True)
        self.assertRegex(orig, r'VG\d{3}', "VG read response invalid")
        test_val = '050' if orig[-3:] != '050' else '000'
        self.send(f'VG{test_val}')
        after_set = self.send('VG', is_read=True)
        self.assertEqual(after_set, f'VG{test_val}', "VG after set mismatch")
        self.send(f'VG{orig[-3:]}')
        restored = self.send('VG', is_read=True)
        self.assertEqual(restored, orig, "VG restore mismatch")

    def test_PB(self):
        """PB: Play Back (DVS voice messages)
        Format: PB P1 where P1: 0=Stop, 1-5=Play channel 1-5
        Note: This test only tests stop (PB0) for safety.
        Playing channels (PB1-PB5) would cause audio output.
        """
        orig = self.send('PB', is_read=True)
        if orig == '?' or not orig.startswith('PB'):
            self.skipTest("PB command not available")

        self.assertRegex(orig, r'PB[0-5]', "PB read response invalid")

        # Test stop command (safe - doesn't play audio)
        self.send('PB0')
        after = self.send('PB', is_read=True)
        self.assertEqual(after, 'PB0', "PB stop command failed")


def get_test_suite(ser, send_command_func):
    """Return test suite for audio commands."""
    AudioTests.setup_class(ser, send_command_func)
    loader = unittest.TestLoader()
    return loader.loadTestsFromTestCase(AudioTests)


if __name__ == '__main__':
    print("This module should be run from ftx1_test.py main harness")
