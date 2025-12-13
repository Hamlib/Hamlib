#!/usr/bin/env python3
"""
FTX-1 CAT Command Test Harness (Modular Version)
Main harness that calls 13 test modules organized by command group.

Usage:
  python3 ftx1_test_main.py <port> [baudrate] [options]

Options:
  --tx-tests     Enable TX-related tests (PTT, tuner, keyer, etc.)
  --optima       Enable Optima/SPA-1 specific tests
  --group GROUP  Run only specific test group(s), comma-separated
                 Groups: vfo, freq, mode, filter, audio, function,
                         noise, preamp, cw, tx, memory, info, misc, power

Examples:
  python3 ftx1_test_main.py /dev/cu.SLAB_USBtoUART
  python3 ftx1_test_main.py /dev/cu.SLAB_USBtoUART --tx-tests --optima
  python3 ftx1_test_main.py /dev/cu.SLAB_USBtoUART --group vfo,freq,mode
"""

import unittest
import serial
import time
import argparse
import sys
from datetime import datetime

# Import test modules
import ftx1_test_vfo
import ftx1_test_freq
import ftx1_test_mode
import ftx1_test_filter
import ftx1_test_audio
import ftx1_test_function
import ftx1_test_noise
import ftx1_test_preamp
import ftx1_test_cw
import ftx1_test_tx
import ftx1_test_memory
import ftx1_test_info
import ftx1_test_misc
import ftx1_test_power

# Configuration
PORT = '/dev/cu.SLAB_USBtoUART'
BAUDRATE = 38400
TX_TESTS_ENABLED = False
OPTIMA_ENABLED = False

# Output files
SUCCESS_FILE = "ftx1_test_success.txt"
FAILURE_FILE = "ftx1_test_failure.txt"

# Result collectors
successes = []
failures = []


def confirm_tx_tests():
    """Prompt user to confirm TX tests with dummy load warning."""
    print("")
    print("=" * 60)
    print("WARNING: TX TESTS ENABLED")
    print("=" * 60)
    print("")
    print("TX-related tests can cause the radio to transmit!")
    print("")
    print("Before proceeding, ensure:")
    print("  1. A dummy load is connected, OR")
    print("  2. An appropriate antenna is connected")
    print("")
    print("NEVER transmit without a proper load connected!")
    print("")
    print("=" * 60)
    print("")

    response = input("Type 'YES' to confirm and proceed with TX tests: ")
    if response.strip() != 'YES':
        print("TX tests cancelled. Running without TX tests.")
        return False
    print("")
    print("TX tests confirmed. Proceeding...")
    print("")
    return True


class ResultCollector(unittest.TestResult):
    """Custom result collector to track successes and failures."""

    def addSuccess(self, test):
        super().addSuccess(test)
        successes.append(f"[PASS] {test}")

    def addFailure(self, test, err):
        super().addFailure(test, err)
        failures.append(f"[FAIL] {test}: {err[1]}")

    def addError(self, test, err):
        super().addError(test, err)
        failures.append(f"[ERROR] {test}: {err[1]}")

    def addSkip(self, test, reason):
        super().addSkip(test, reason)
        successes.append(f"[SKIP] {test}: {reason}")


class TestHarness:
    """Main test harness that manages serial connection and runs test modules."""

    def __init__(self, port, baudrate, tx_tests=False, optima=False):
        self.port = port
        self.baudrate = baudrate
        self.tx_tests = tx_tests
        self.optima = optima
        self.ser = None
        self.max_retries = 3
        self.retry_delay = 0.2

    def connect(self):
        """Establish serial connection."""
        self.ser = serial.Serial(
            port=self.port,
            baudrate=self.baudrate,
            bytesize=8,
            parity='N',
            stopbits=2,
            timeout=1
        )
        time.sleep(1)
        # Initialize radio state
        self._send_command_static('PS1')   # Power on
        self._send_command_static('VS0')   # VFO select MAIN
        self._send_command_static('LK0')   # Unlock
        self._send_command_static('MX0')   # MOX OFF
        self._send_command_static('TX0')   # PTT OFF
        self._send_command_static('FT0')   # MAIN as TX VFO
        self._send_command_static('VX0')   # VOX OFF
        self._send_command_static('AI0')   # Auto Info OFF
        self._send_command_static('SC00')  # Stop scan

    def disconnect(self):
        """Close serial connection."""
        if self.ser:
            self.ser.close()

    def _send_command_static(self, cmd):
        """Send command without retry logic (for initialization)."""
        self.ser.write((cmd + ';').encode())
        time.sleep(0.1)
        return self.ser.read(100).decode().strip(';').strip()

    def send_command(self, instance, cmd, is_read=False, filter_async=True):
        """Send a CAT command and read the response with retries."""
        for attempt in range(self.max_retries):
            try:
                self.ser.write((cmd + ';').encode())
                time.sleep(0.1)
                response = self.ser.read(100).decode().strip(';').strip()

                # Filter out async responses
                if filter_async and response and ';' in response:
                    parts = response.split(';')
                    cmd_prefix = cmd[:2].upper()
                    for part in parts:
                        if part.upper().startswith(cmd_prefix):
                            response = part
                            break
                    else:
                        response = parts[0] if parts else response

                if response or is_read:
                    return response
            except Exception as e:
                if attempt == self.max_retries - 1:
                    raise e
                time.sleep(self.retry_delay)
        return None

    def get_test_groups(self):
        """Return dictionary of available test groups and their modules."""
        return {
            'vfo': ('VFO Tests', ftx1_test_vfo,
                    lambda: ftx1_test_vfo.get_test_suite(self.ser, self.send_command, self.tx_tests)),
            'freq': ('Frequency Tests', ftx1_test_freq,
                     lambda: ftx1_test_freq.get_test_suite(self.ser, self.send_command)),
            'mode': ('Mode Tests', ftx1_test_mode,
                     lambda: ftx1_test_mode.get_test_suite(self.ser, self.send_command)),
            'filter': ('Filter Tests', ftx1_test_filter,
                       lambda: ftx1_test_filter.get_test_suite(self.ser, self.send_command)),
            'audio': ('Audio/Level Tests', ftx1_test_audio,
                      lambda: ftx1_test_audio.get_test_suite(self.ser, self.send_command)),
            'function': ('Function Tests', ftx1_test_function,
                         lambda: ftx1_test_function.get_test_suite(self.ser, self.send_command)),
            'noise': ('Noise Tests', ftx1_test_noise,
                      lambda: ftx1_test_noise.get_test_suite(self.ser, self.send_command)),
            'preamp': ('Preamp/Attenuator Tests', ftx1_test_preamp,
                       lambda: ftx1_test_preamp.get_test_suite(self.ser, self.send_command)),
            'cw': ('CW/Keyer Tests', ftx1_test_cw,
                   lambda: ftx1_test_cw.get_test_suite(self.ser, self.send_command, self.tx_tests)),
            'tx': ('TX Tests', ftx1_test_tx,
                   lambda: ftx1_test_tx.get_test_suite(self.ser, self.send_command, self.tx_tests, self.optima)),
            'memory': ('Memory Tests', ftx1_test_memory,
                       lambda: ftx1_test_memory.get_test_suite(self.ser, self.send_command)),
            'info': ('Info Tests', ftx1_test_info,
                     lambda: ftx1_test_info.get_test_suite(self.ser, self.send_command)),
            'misc': ('Miscellaneous Tests', ftx1_test_misc,
                     lambda: ftx1_test_misc.get_test_suite(self.ser, self.send_command)),
            'power': ('Power Control Tests', ftx1_test_power,
                      lambda: ftx1_test_power.get_test_suite(self.ser, self.send_command, self.optima)),
        }

    def run_tests(self, groups=None):
        """Run test suites for specified groups (or all if None)."""
        all_groups = self.get_test_groups()

        if groups:
            selected = [g.strip().lower() for g in groups.split(',')]
            invalid = [g for g in selected if g not in all_groups]
            if invalid:
                print(f"ERROR: Unknown test group(s): {', '.join(invalid)}")
                print(f"Available groups: {', '.join(all_groups.keys())}")
                return False
        else:
            selected = list(all_groups.keys())

        combined_suite = unittest.TestSuite()

        for group_name in selected:
            name, module, get_suite = all_groups[group_name]
            print(f"\n=== {name} ===")
            suite = get_suite()
            combined_suite.addTests(suite)

        runner = unittest.TextTestRunner(resultclass=ResultCollector, verbosity=2)
        result = runner.run(combined_suite)
        return result.wasSuccessful()


def write_results():
    """Write test results to output files."""
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    with open(SUCCESS_FILE, 'w') as f:
        f.write(f"FTX-1 CAT Command Test Results - Successes\n")
        f.write(f"Timestamp: {timestamp}\n")
        f.write(f"Total Passed: {len(successes)}\n")
        f.write("=" * 50 + "\n\n")
        for entry in successes:
            f.write(entry + "\n")

    with open(FAILURE_FILE, 'w') as f:
        f.write(f"FTX-1 CAT Command Test Results - Failures\n")
        f.write(f"Timestamp: {timestamp}\n")
        f.write(f"Total Failed: {len(failures)}\n")
        f.write("=" * 50 + "\n\n")
        for entry in failures:
            f.write(entry + "\n")

    print(f"\nResults written to {SUCCESS_FILE} and {FAILURE_FILE}")
    print(f"Summary: {len(successes)} passed, {len(failures)} failed")


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='FTX-1 CAT Command Test Harness (Modular)',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Test Groups:
  vfo      - VFO select, split, TX VFO, A/B copy
  freq     - Frequency control, band up/down, RIT/XIT
  mode     - Operating mode, filter number
  filter   - Beat cancel, contour, notch, width
  audio    - AF/RF gain, mic gain, AGC, squelch, meters
  function - Compressor, monitor, lock, tone, TSQL
  noise    - Noise blanker, noise reduction, attenuator
  preamp   - Preamp, RF attenuator
  cw       - Key speed, break-in, keyer memory
  tx       - PTT, VOX, tuner, MOX (requires --tx-tests)
  memory   - Memory read/write, memory zones
  info     - Radio ID, IF info, meters (read-only)
  misc     - CTCSS, scan, date/time, misc commands
  power    - RF power, SPA-1 power settings (requires --optima)

Examples:
  python3 ftx1_test_main.py /dev/ttyUSB0
  python3 ftx1_test_main.py /dev/cu.SLAB_USBtoUART --tx-tests --optima
  python3 ftx1_test_main.py /dev/ttyUSB0 --group vfo,freq,mode
        '''
    )
    parser.add_argument('port', help='Serial port (required)')
    parser.add_argument('baudrate', nargs='?', type=int, default=BAUDRATE,
                        help=f'Baud rate (default: {BAUDRATE})')
    parser.add_argument('--tx-tests', action='store_true',
                        help='Enable TX-related tests (PTT, tuner, keyer)')
    parser.add_argument('--optima', '-o', action='store_true',
                        help='Enable Optima/SPA-1 specific tests')
    parser.add_argument('--group', '-g', type=str, default=None,
                        help='Comma-separated list of test groups to run')

    args = parser.parse_args()

    print("=" * 60)
    print("FTX-1 CAT Command Test Harness (Modular)")
    print("=" * 60)
    print(f"Port: {args.port}")
    print(f"Baudrate: {args.baudrate}")
    print(f"TX Tests: {'ENABLED' if args.tx_tests else 'DISABLED'}")
    print(f"Optima (SPA-1): {'ENABLED' if args.optima else 'DISABLED'}")
    if args.group:
        print(f"Groups: {args.group}")
    else:
        print("Groups: ALL")
    print("=" * 60)

    # Handle TX tests confirmation
    tx_enabled = args.tx_tests and confirm_tx_tests() if args.tx_tests else False

    # Create and run test harness
    harness = TestHarness(args.port, args.baudrate, tx_enabled, args.optima)

    try:
        harness.connect()
        success = harness.run_tests(args.group)
    finally:
        harness.disconnect()

    write_results()
    sys.exit(0 if success else 1)
