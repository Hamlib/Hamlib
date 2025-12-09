#!/usr/bin/env python3
"""
FTX-1 TX Command Tests
Tests: AC (Tuner), MX (MOX), TX (PTT), VX (VOX)
WARNING: These tests can cause transmission!
"""

import unittest


class TXTests(unittest.TestCase):
    """TX command tests - requires ser and send_command from main harness."""

    ser = None
    send_command = None
    TX_TESTS_ENABLED = False
    OPTIMA_ENABLED = False

    @classmethod
    def setup_class(cls, ser, send_command_func, tx_tests=False, optima=False):
        """Initialize with serial connection and command function."""
        cls.ser = ser
        cls.send_command = send_command_func
        cls.TX_TESTS_ENABLED = tx_tests
        cls.OPTIMA_ENABLED = optima

    def send(self, cmd, is_read=False):
        """Wrapper for send_command."""
        return self.__class__.send_command(self, cmd, is_read)

    def test_AC(self):
        """AC: Tuner (P1=0/1 MAIN/SUB, P2=0/1/2 off/on/start)
        WARNING: Tuner start (AC0 02) causes transmission!
        NOTE: AC command only works with Optima configuration (SPA-1 amp attached)
        """
        if not self.__class__.OPTIMA_ENABLED:
            self.skipTest("Optima tests disabled (use --optima to enable for SPA-1 configurations)")
        if not self.__class__.TX_TESTS_ENABLED:
            self.skipTest("TX tests disabled (use --tx-tests to enable) - tuner start causes TX")
        orig = self.send('AC0', is_read=True)
        if orig == '?':
            self.skipTest("AC not available (requires Optima/SPA-1 configuration)")
        self.assertRegex(orig, r'AC0\d{2}', "AC read invalid")
        test_val = '00' if orig[-2:] != '00' else '01'  # Toggle off/on (safe, no start)
        self.send(f'AC0{test_val}')
        after = self.send('AC0', is_read=True)
        self.assertEqual(after, f'AC0{test_val}', "AC after set mismatch")
        self.send(f'AC0{orig[-2:]}')  # Restore
        restored = self.send('AC0', is_read=True)
        self.assertEqual(restored, orig, "AC restore mismatch")

    def test_MX(self):
        """MX: MOX (Manual Operator Transmit) (0/1 off/on)
        WARNING: MX1 causes transmission!
        """
        if not self.__class__.TX_TESTS_ENABLED:
            self.skipTest("TX tests disabled (use --tx-tests to enable)")
        orig = self.send('MX', is_read=True)
        self.assertRegex(orig, r'MX[01]', "MX read response invalid")
        # Only test turning MOX OFF (safe)
        self.send('MX0')
        after_set = self.send('MX', is_read=True)
        self.assertEqual(after_set, 'MX0', "MX after set mismatch")
        restored = self.send('MX', is_read=True)
        self.assertEqual(restored, 'MX0', "MX should remain OFF for safety")

    def test_TX(self):
        """TX: PTT (0/1/2 RX/manual/auto)
        WARNING: This test could cause transmission!
        """
        if not self.__class__.TX_TESTS_ENABLED:
            self.skipTest("TX tests disabled (use --tx-tests to enable)")
        orig = self.send('TX', is_read=True)
        self.assertRegex(orig, r'TX[012]', "TX read response invalid")
        # Set to RX (0), safe
        self.send('TX0')
        after_set = self.send('TX', is_read=True)
        self.assertEqual(after_set, 'TX0', "TX after set mismatch")
        self.send(f'TX{orig[-1]}')
        restored = self.send('TX', is_read=True)
        self.assertEqual(restored, orig, "TX restore mismatch")

    def test_VX(self):
        """VX: VOX (0/1 off/on)
        WARNING: VX1 enables VOX which can cause transmission if audio is present!
        """
        if not self.__class__.TX_TESTS_ENABLED:
            self.skipTest("TX tests disabled (use --tx-tests to enable)")
        orig = self.send('VX', is_read=True)
        self.assertRegex(orig, r'VX[01]', "VX read response invalid")
        test_val = '0' if orig[-1] == '1' else '1'
        self.send(f'VX{test_val}')
        after_set = self.send('VX', is_read=True)
        self.assertEqual(after_set, f'VX{test_val}', "VX after set mismatch")
        self.send(f'VX{orig[-1]}')
        restored = self.send('VX', is_read=True)
        self.assertEqual(restored, orig, "VX restore mismatch")

    def test_VE(self):
        """VE: VOX enable info (read-only, P1=0/1 MAIN/SUB)"""
        resp = self.send('VE0')
        self.assertRegex(resp, r'VE0\d{4}', "VE read response invalid")


def get_test_suite(ser, send_command_func, tx_tests=False, optima=False):
    """Return test suite for TX commands."""
    TXTests.setup_class(ser, send_command_func, tx_tests, optima)
    loader = unittest.TestLoader()
    return loader.loadTestsFromTestCase(TXTests)


if __name__ == '__main__':
    print("This module should be run from ftx1_test.py main harness")
