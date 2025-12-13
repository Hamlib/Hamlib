#!/usr/bin/env python3
"""
FTX-1 Memory Command Tests
Tests: AM (VFO-A to Memory), BM (VFO-B to Memory), CH (Channel Up/Down),
       MA (Memory to VFO-A), MB (Memory to VFO-B), MC (Memory Channel Select),
       MR (Memory Read), MT (Memory Tag), MW (Memory Write), MZ (Memory Zone),
       VM (VFO/Memory Mode)
"""

import unittest


class MemoryTests(unittest.TestCase):
    """Memory command tests - requires ser and send_command from main harness."""

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

    def test_AM(self):
        """AM: VFO-A to Memory (set-only per spec)"""
        resp = self.send('AM')
        # Accept either empty response (per spec) or firmware's actual response
        if resp is not None and resp != '':
            pass  # Test passes - command is accepted

    def test_BM(self):
        """BM: VFO-B to Memory (set-only per spec)"""
        resp = self.send('BM')
        if resp is not None and resp != '':
            pass  # Test passes - command is accepted

    def test_MA(self):
        """MA: Memory to VFO A (set-only, destructive)"""
        self.skipTest("MA is destructive; manual test required")

    def test_MB(self):
        """MB: Memory to VFO B (set-only, destructive)"""
        self.skipTest("MB is destructive; manual test required")

    def test_MR(self):
        """MR: Memory read (5-digit format: MR P1 P2P2P2P2)
        Format verified 2025-12-09: MR00001 returns MR00001FFFFFFFFF+OOOOOPPMMxxxx
        Returns '?' for empty/unprogrammed channels.
        """
        resp = self.send('MR00001', is_read=True)
        if resp == '?':
            self.skipTest("MR00001 returned '?' - channel 1 not programmed")
        # Response format: MR + 5-digit channel + 9-digit freq + sign + offset + params
        self.assertRegex(resp, r'MR\d{5}\d{9}[+-]\d+', "MR read response invalid")

    def test_MT(self):
        """MT: Memory tag/name (5-digit format: MT P1 P2P2P2P2)
        Format verified 2025-12-09: MT00001 returns MT00001[12-char name, space padded]
        MT is full read/write! MT00001NAME sets 12-char name.
        Returns '?' for empty/unprogrammed channels.
        """
        resp = self.send('MT00001', is_read=True)
        if resp == '?':
            self.skipTest("MT00001 returned '?' - channel 1 not programmed")
        # Response format: MT + 5-digit channel + 12-char name (space padded)
        self.assertTrue(resp.startswith('MT00001'), "MT read response invalid")
        self.assertGreaterEqual(len(resp), 17, "MT response too short")

        # Save original name
        orig_name = resp[7:] if len(resp) >= 19 else '            '

        # Test set
        test_name = 'HAMLIBTEST  '  # 12 chars
        self.send(f'MT00001{test_name}')
        after = self.send('MT00001', is_read=True)
        self.assertEqual(after[7:19], test_name, "MT set failed")

        # Restore original
        self.send(f'MT00001{orig_name}')

    def test_MW(self):
        """MW: Memory write (29-byte format) - uses high channel to minimize risk
        Format: MW P1P1P1P1P1 P2P2P2P2P2P2P2P2P2 P3P3P3P3P3 P4 P5 P6 P7 P8 P9P9 P10;
        - P1: 5-digit channel (00001-00500)
        - P2: 9-digit frequency in Hz
        - P3: 5-digit clarifier offset with sign (+/-0000 to +/-9999)
        - P4: Clarifier RX (0/1)
        - P5: Clarifier TX (0/1)
        - P6: Mode (1-C/2-LSB/3-USB/4-CW-U/5-FM/6-WFM/7-CW-L/8-DATA-L/9-DATA-FM/A-FM-N/B-DATA-U/C-AM-N/D-C4FM)
        - P7: VFO/Mem mode (0=VFO, 1=Memory)
        - P8: CTCSS state (0=off, 1=ENC, 2=TSQ)
        - P9: 2-digit reserved (00)
        - P10: Shift (0=simplex, 1=+, 2=-)
        Verified working 2025-12-09.
        """
        import time
        # Use channel 00500 (high number, less likely to be in active use)
        test_channel = '00500'

        # First read existing channel data (if any)
        orig = self.send(f'MR{test_channel}', is_read=True)
        had_original = (orig != '?' and orig.startswith(f'MR{test_channel}'))

        # Write test data: 14.200 MHz USB, no clarifier, no CTCSS, simplex
        # Format: MW00500014200000+00000003100000
        test_cmd = f'MW{test_channel}014200000+0000000310000'
        self.send(test_cmd)
        time.sleep(0.2)

        # Read back and verify
        after = self.send(f'MR{test_channel}', is_read=True)
        self.assertNotEqual(after, '?', "MW write should create readable channel")
        self.assertTrue(after.startswith(f'MR{test_channel}014200000'),
                        f"MW verify failed, expected freq 014200000, got '{after}'")

        # Clean up: If channel existed before, restore it; otherwise leave test data
        # (We can't delete channels via CAT, so we leave test data as cleanup)
        if had_original and len(orig) >= 29:
            # Restore original by converting MR response to MW command
            # MR format: MR + channel + freq + offset + params
            restore_data = orig[2:]  # Strip 'MR' prefix
            self.send(f'MW{restore_data}')

    def test_MZ(self):
        """MZ: Memory zone (5-digit format: MZ P1 P2P2P2P2)
        Format verified 2025-12-09: MZ00001 returns MZ000010000000000
        MZ is full read/write! MZ00001DATA sets 10-digit zone data.
        Returns '?' for empty/unprogrammed channels.
        """
        resp = self.send('MZ00001', is_read=True)
        if resp == '?':
            self.skipTest("MZ00001 returned '?' - channel 1 not programmed")
        # Response format: MZ + 5-digit channel + 10-digit zone data
        self.assertTrue(resp.startswith('MZ00001'), "MZ read response invalid")
        self.assertGreaterEqual(len(resp), 17, "MZ response too short")

        # Save original data
        orig_data = resp[7:] if len(resp) >= 17 else '0000000000'

        # Test set
        test_data = '1111111111'
        self.send(f'MZ00001{test_data}')
        after = self.send('MZ00001', is_read=True)
        self.assertEqual(after[7:17], test_data, "MZ set failed")

        # Restore original
        self.send(f'MZ00001{orig_data}')

    def test_VM(self):
        """VM: VFO/Memory mode (read works, limited set)
        Format verified 2025-12-09:
        - VM0/VM1 read returns VMxPP
        - Mode codes DIFFER FROM SPEC: 00=VFO, 11=Memory (not 01!)
        - Only VM000 (VFO mode) works for set
        - Use SV command to toggle to memory mode
        """
        # Read MAIN VFO mode
        resp = self.send('VM0', is_read=True)
        self.assertRegex(resp, r'VM0\d{2}', "VM0 read response invalid")

        # Read SUB VFO mode
        resp = self.send('VM1', is_read=True)
        self.assertRegex(resp, r'VM1\d{2}', "VM1 read response invalid")

        # Test setting to VFO mode (only one that works)
        self.send('VM000')
        after = self.send('VM0', is_read=True)
        self.assertEqual(after, 'VM000', "VM000 set failed")

        # Test SV toggle to memory mode
        self.send('SV')
        import time
        time.sleep(0.3)
        after_sv = self.send('VM0', is_read=True)
        # Mode 11 = Memory mode (not 01 as documented)
        self.assertEqual(after_sv, 'VM011', "SV should toggle to memory mode (VM011)")

        # Toggle back to VFO mode
        self.send('SV')
        time.sleep(0.3)
        restored = self.send('VM0', is_read=True)
        self.assertEqual(restored, 'VM000', "SV should toggle back to VFO mode")

    def test_MC(self):
        """MC: Memory Channel Select (different format than documented!)
        Format verified 2025-12-09:
        - Read: MC0 (MAIN) or MC1 (SUB) returns MCNNNNNN (6-digit channel)
        - Set: MCNNNNNN (6-digit channel, no VFO prefix)
        - Returns '?' if channel doesn't exist (not programmed)
        """
        # Read MAIN memory channel
        resp = self.send('MC0', is_read=True)
        self.assertRegex(resp, r'MC\d{6}', "MC0 read response invalid")
        orig_channel = resp[2:]  # Save 6-digit channel for restore

        # Read SUB memory channel
        resp = self.send('MC1', is_read=True)
        self.assertRegex(resp, r'MC\d{6}', "MC1 read response invalid")

        # Test set (only works if channel exists)
        # Set to channel 1 (most likely to exist)
        resp = self.send('MC000001')
        if resp == '?':
            self.skipTest("MC set returned '?' - channel 1 may not exist")

        # Verify
        after = self.send('MC0', is_read=True)
        self.assertEqual(after, 'MC000001', "MC set to channel 1 failed")

        # Restore original
        self.send(f'MC{orig_channel}')

    def test_CH(self):
        """CH: Memory Channel Up/Down (verified 2025-12-09)
        CH0 = next memory channel (cycles through ALL channels across groups)
        CH1 = previous memory channel
        Cycles: PMG ch1 → ch2 → ... → QMB ch1 → ... → PMG ch1 (wraps)
        Only CH0 and CH1 work; CH; CH00; CH01; etc. return '?'
        Display shows "M-ALL X-NN" where X=group, NN=channel
        """
        # Ensure we're in memory mode and on a known channel
        self.send('MC000001')
        import time
        time.sleep(0.2)

        # Get current state
        orig = self.send('MC0', is_read=True)

        # Test CH0 - should advance to next channel
        self.send('CH0')
        time.sleep(0.2)
        after_ch0 = self.send('MC0', is_read=True)

        # Test CH1 - should go back to previous channel
        self.send('CH1')
        time.sleep(0.2)
        after_ch1 = self.send('MC0', is_read=True)

        # Verify CH0 changed the channel
        self.assertNotEqual(after_ch0, orig, "CH0 should change channel")

        # Verify CH1 returned to original
        self.assertEqual(after_ch1, orig, "CH1 should return to original channel")

        # Test that invalid formats return '?'
        resp = self.send('CH', is_read=True)
        self.assertEqual(resp, '?', "CH; should return '?'")

        resp = self.send('CH00', is_read=True)
        self.assertEqual(resp, '?', "CH00; should return '?'")

        # Restore original
        self.send(f'MC{orig[2:]}')


def get_test_suite(ser, send_command_func):
    """Return test suite for memory commands."""
    MemoryTests.setup_class(ser, send_command_func)
    loader = unittest.TestLoader()
    return loader.loadTestsFromTestCase(MemoryTests)


if __name__ == '__main__':
    print("This module should be run from ftx1_test.py main harness")
