#!/usr/bin/env python3
"""
FTX-1 Power Control Command Tests
Tests: PC (RF Power Control), EX030104 (TUNER SELECT), EX0307xx (OPTION Power)
Note: EX menu tests require SPA-1/Optima configuration
"""

import unittest


class PowerTests(unittest.TestCase):
    """Power control command tests - requires ser and send_command from main harness."""

    ser = None
    send_command = None
    OPTIMA_ENABLED = False

    @classmethod
    def setup_class(cls, ser, send_command_func, optima=False):
        """Initialize with serial connection and command function."""
        cls.ser = ser
        cls.send_command = send_command_func
        cls.OPTIMA_ENABLED = optima

    def send(self, cmd, is_read=False):
        """Wrapper for send_command."""
        return self.__class__.send_command(self, cmd, is_read)

    def test_PC(self):
        """PC: Power control - Format: PC P1 P2
        P1=1 (FTX-1 field head), P1=2 (SPA-1)
        Field head: 0.5-10W, SPA-1: 5-100W
        """
        orig = self.send('PC', is_read=True)
        # Match P1 (1 or 2) followed by either 3-digit int or decimal
        self.assertRegex(orig, r'PC[12](\d{3}|\d\.\d)', "PC read response invalid")
        # Validate power based on mode
        if orig.startswith('PC1'):
            power_str = orig[3:]
            if '.' in power_str:
                power = float(power_str)
            else:
                power = int(power_str)
            self.assertGreaterEqual(power, 0.5, "Field head power below minimum (0.5W)")
            self.assertLessEqual(power, 10.0, "Field head power above maximum (10W)")
        elif orig.startswith('PC2'):
            power_str = orig[3:]
            power = int(power_str)
            self.assertGreaterEqual(power, 5, "SPA-1 power below minimum (5W)")
            self.assertLessEqual(power, 100, "SPA-1 power above maximum (100W)")

    def test_EX_TUNER_SELECT(self):
        """EX030104: TUNER SELECT - controls internal antenna tuner type
        Values: 0=INT, 1=INT(FAST), 2=EXT, 3=ATAS
        GUARDRAIL: Values 0 (INT) and 1 (INT FAST) require SPA-1 amplifier
        """
        if not self.__class__.OPTIMA_ENABLED:
            self.skipTest("Optima tests disabled (use --optima to enable for SPA-1 configurations)")

        resp = self.send('EX030104', is_read=True)
        if resp == '?':
            self.skipTest("EX030104 (TUNER SELECT) not available in firmware")

        self.assertRegex(resp, r'EX030104[0-3]', "EX030104 read response invalid")
        orig_val = resp[-1]

        # With SPA-1, test setting INT (0)
        self.send('EX0301040')
        after = self.send('EX030104', is_read=True)
        self.assertEqual(after, 'EX0301040', "EX030104 set to INT (0) failed with SPA-1")

        # Restore original
        self.send(f'EX030104{orig_val}')
        restored = self.send('EX030104', is_read=True)
        self.assertEqual(restored, f'EX030104{orig_val}', "EX030104 restore mismatch")

    def test_EX_OPTION_POWER(self):
        """EX0307xx: OPTION section - SPA-1 max power settings per band
        GUARDRAIL: These settings require SPA-1 amplifier
        Items: 05=160m, 06=80m, 07=60m, 08=40m, 09=30m, 10=20m, 11=17m
        """
        if not self.__class__.OPTIMA_ENABLED:
            self.skipTest("Optima tests disabled (use --optima to enable for SPA-1 configurations)")

        resp = self.send('EX030705', is_read=True)
        if resp == '?':
            self.skipTest("EX030705 (OPTION 160m power) not available in firmware")

        self.assertRegex(resp, r'EX030705\d{3}', "EX030705 read response invalid")
        orig_val = resp[-3:]

        # Test setting a valid power level (100W is max for SPA-1)
        test_val = '050' if orig_val != '050' else '100'
        self.send(f'EX030705{test_val}')
        after = self.send('EX030705', is_read=True)
        self.assertEqual(after, f'EX030705{test_val}', "EX030705 set failed with SPA-1")

        # Restore original
        self.send(f'EX030705{orig_val}')
        restored = self.send('EX030705', is_read=True)
        self.assertEqual(restored, f'EX030705{orig_val}', "EX030705 restore mismatch")


def get_test_suite(ser, send_command_func, optima=False):
    """Return test suite for power commands."""
    PowerTests.setup_class(ser, send_command_func, optima)
    loader = unittest.TestLoader()
    return loader.loadTestsFromTestCase(PowerTests)


if __name__ == '__main__':
    print("This module should be run from ftx1_test.py main harness")
