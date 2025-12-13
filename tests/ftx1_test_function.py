#!/usr/bin/env python3
"""
FTX-1 Function Command Tests
Tests: AI (Auto Info), BI (Break-in), CT (CTCSS), LK (Lock)
"""

import unittest
import time


class FunctionTests(unittest.TestCase):
    """Function command tests - requires ser and send_command from main harness."""

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

    def test_AI(self):
        """AI: Auto information (0/1 off/on)"""
        orig = self.send('AI', is_read=True)
        self.assertRegex(orig, r'AI[01]', "AI read response invalid")
        try:
            test_val = '0' if orig[-1] == '1' else '1'
            self.send(f'AI{test_val}')
            time.sleep(0.3)
            self.__class__.ser.reset_input_buffer()
            after_set = self.send('AI', is_read=True)
            self.assertEqual(after_set, f'AI{test_val}', "AI after set mismatch")
        finally:
            # ALWAYS restore AI0 to prevent async response contamination
            self.send('AI0')
            time.sleep(0.2)
            self.__class__.ser.reset_input_buffer()
        restored = self.send('AI', is_read=True)
        self.assertEqual(restored, 'AI0', "AI should be AI0 after test")

    def test_BI(self):
        """BI: Break-in (0/1 off/on)"""
        orig = self.send('BI', is_read=True)
        self.assertRegex(orig, r'BI[01]', "BI read response invalid")
        test_val = '0' if orig[-1] == '1' else '1'
        self.send(f'BI{test_val}')
        after_set = self.send('BI', is_read=True)
        self.assertEqual(after_set, f'BI{test_val}', "BI after set mismatch")
        self.send(f'BI{orig[-1]}')
        restored = self.send('BI', is_read=True)
        self.assertEqual(restored, orig, "BI restore mismatch")

    def test_CT(self):
        """CT: CTCSS mode (P1=0/1 MAIN/SUB, P2=0-3 off/enc/dec/encs/decs)"""
        orig = self.send('CT0', is_read=True)
        self.assertRegex(orig, r'CT0[0-3]', "CT read response invalid")
        test_val = '0' if orig[-1] == '1' else '1'
        self.send(f'CT0{test_val}')
        after_set = self.send('CT0', is_read=True)
        self.assertEqual(after_set, f'CT0{test_val}', "CT after set mismatch")
        self.send(f'CT0{orig[-1]}')
        restored = self.send('CT0', is_read=True)
        self.assertEqual(restored, orig, "CT restore mismatch")

    def test_LK(self):
        """LK: Lock (0/1 off/on)"""
        orig = self.send('LK', is_read=True)
        self.assertRegex(orig, r'LK[01]', "LK read response invalid")
        test_val = '0' if orig[-1] == '1' else '1'
        self.send(f'LK{test_val}')
        after_set = self.send('LK', is_read=True)
        self.assertEqual(after_set, f'LK{test_val}', "LK after set mismatch")
        self.send(f'LK{orig[-1]}')
        restored = self.send('LK', is_read=True)
        self.assertEqual(restored, orig, "LK restore mismatch")

    def test_CS(self):
        """CS: CW Spot (0/1 off/on) - read-only test"""
        orig = self.send('CS', is_read=True)
        self.assertRegex(orig, r'CS[01]', "CS read response invalid")
        # Skip set test - firmware doesn't persist the value

    def test_FR(self):
        """FR: Function RX (P1=0/1 MAIN/SUB) - read-only test"""
        orig = self.send('FR', is_read=True)
        self.assertRegex(orig, r'FR\d{2}', "FR read response invalid")
        # Skip set test - firmware doesn't persist the value

    def test_TS(self):
        """TS: TXW (TX Watch) (0/1 off/on) - read-only test"""
        orig = self.send('TS', is_read=True)
        self.assertRegex(orig, r'TS[01]', "TS read response invalid")
        # Skip set test - firmware doesn't persist the value


def get_test_suite(ser, send_command_func):
    """Return test suite for function commands."""
    FunctionTests.setup_class(ser, send_command_func)
    loader = unittest.TestLoader()
    return loader.loadTestsFromTestCase(FunctionTests)


if __name__ == '__main__':
    print("This module should be run from ftx1_test.py main harness")
