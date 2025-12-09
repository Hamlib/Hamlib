#!/usr/bin/env python3
"""
FTX-1 Info Command Tests (Read-Only)
Tests: DA (Date), DT (Date/Time), ID (Radio ID), IF (Information),
       OI (Opposite Band Info), PS (Power Switch), RI (RIT Info), RM (Read Meter)
"""

import unittest


class InfoTests(unittest.TestCase):
    """Info command tests - requires ser and send_command from main harness."""

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

    def test_DA(self):
        """DA: Date (P1=0 set, P2=MMDDYYYY)"""
        orig = self.send('DA', is_read=True)
        self.assertRegex(orig, r'DA00\d{2}\d{2}\d{2}', "DA read response invalid")
        test_val = '00101010' if orig[-8:] != '00101010' else '00000000'
        self.send(f'DA{test_val}')
        after_set = self.send('DA', is_read=True)
        self.assertEqual(after_set, f'DA{test_val}', "DA after set mismatch")
        self.send(f'DA{orig[-8:]}')
        restored = self.send('DA', is_read=True)
        self.assertEqual(restored, orig, "DA restore mismatch")

    def test_DT(self):
        """DT: Date/time (P1=0 date/P1=1 time)"""
        orig = self.send('DT0', is_read=True)
        self.assertRegex(orig, r'DT0\d{8}', "DT read response invalid")
        test_val = '20251201' if orig[-8:] != '20251201' else '20250101'
        self.send(f'DT0{test_val}')
        after_set = self.send('DT0', is_read=True)
        self.assertEqual(after_set, f'DT0{test_val}', "DT after set mismatch")
        self.send(f'DT0{orig[-8:]}')
        restored = self.send('DT0', is_read=True)
        self.assertEqual(restored, orig, "DT restore mismatch")

    def test_ID(self):
        """ID: Identification (read-only)"""
        resp = self.send('ID', is_read=True)
        self.assertRegex(resp, r'ID\d{4}', "ID read response invalid")

    def test_IF(self):
        """IF: Information (read-only)"""
        resp = self.send('IF', is_read=True)
        self.assertRegex(resp, r'IF\d{5}\d{9}[+-]\d{5}\d{7}', "IF read response invalid")

    def test_OI(self):
        """OI: Opp band info (read-only)"""
        resp = self.send('OI')
        self.assertRegex(resp, r'OI\d{5}\d{9}[+-]\d{4}[01][01][0-9A-J][0-5]\d{2}[012]', "OI read response invalid")

    def test_PS(self):
        """PS: Power switch (0/1 off/on)"""
        orig = self.send('PS', is_read=True)
        self.assertRegex(orig, r'PS[01]', "PS read response invalid")
        # Avoid powering off; test read only
        self.skipTest("PS set may power off; read tested")

    def test_RI(self):
        """RI: RIT info (read-only, P1=0/1 MAIN/SUB)"""
        resp = self.send('RI0')
        self.assertRegex(resp, r'RI0\d{7}', "RI read response invalid")

    def test_RM(self):
        """RM: Read meter (P1=0-5 type, read-only)"""
        resp = self.send('RM0')
        self.assertRegex(resp, r'RM0\d{6,9}', "RM read response invalid")


def get_test_suite(ser, send_command_func):
    """Return test suite for info commands."""
    InfoTests.setup_class(ser, send_command_func)
    loader = unittest.TestLoader()
    return loader.loadTestsFromTestCase(InfoTests)


if __name__ == '__main__':
    print("This module should be run from ftx1_test.py main harness")
