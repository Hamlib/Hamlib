FTX-1 CAT Command Test Harness (Modular)
========================================

This directory contains modular test harnesses for exercising all FTX-1 CAT
commands. Tests are organized into 14 command groups that can be run
individually or together. Both Python and Bash versions are provided.


File Structure
--------------
Main harnesses:
  ftx1_test_main.py   - Python main harness (unittest framework)
  ftx1_test_main.sh   - Bash main harness (rigctl-based)

Test modules (Python .py and Bash .sh versions):
  ftx1_test_vfo       - VFO select, split, TX VFO, A/B copy, FR (dual receive), GP (GP OUT)
  ftx1_test_freq      - Frequency control, band up/down, RIT/XIT, OS (repeater shift)
  ftx1_test_mode      - Operating mode, filter number
  ftx1_test_filter    - Beat cancel, contour, notch, width
  ftx1_test_audio     - AF/RF gain, mic gain, AGC, squelch, meters, PB (voice playback)
  ftx1_test_function  - Compressor, monitor, lock, tone, TSQL
  ftx1_test_noise     - Noise blanker, noise reduction, attenuator
  ftx1_test_preamp    - Preamp, RF attenuator
  ftx1_test_cw        - Key speed, break-in, keyer memory
  ftx1_test_tx        - PTT, VOX, tuner, MOX, TS (TX watch)
  ftx1_test_memory    - Memory read/write, memory zones
  ftx1_test_info      - Radio ID, IF info, meters (read-only), SF (FUNC knob)
  ftx1_test_misc      - CTCSS/DCS (CN, CT, DC), scan, date/time, misc commands
  ftx1_test_power     - RF power, SPA-1 power settings

Legacy monolithic harnesses (still functional):
  ftx1_test.py        - Original Python harness
  ftx1_test.sh        - Original Bash harness

Output files:
  ftx1_test_success.txt  - All passed tests with timestamp and count
  ftx1_test_failure.txt  - All failed tests with error details

Files are OVERWRITTEN each run to provide fresh results.


Python Test Harness (ftx1_test_main.py)
---------------------------------------
Uses direct serial communication to send CAT commands to the radio.
Tests all 91 CAT commands with set/get/restore pattern.

Requirements:
  - Python 3
  - pyserial (pip install pyserial)

Usage:
  python3 ftx1_test_main.py <port> [baudrate] [options]

Options:
  port                Serial port (required)
  baudrate            Baud rate (default: 38400)
  --tx-tests          Enable TX-related tests (PTT, tuner, keyer)
  --optima, -o        Enable Optima/SPA-1 specific tests
  --group, -g GROUP   Comma-separated list of test groups to run

Examples:
  python3 ftx1_test_main.py /dev/ttyUSB0
  python3 ftx1_test_main.py /dev/cu.SLAB_USBtoUART 38400
  python3 ftx1_test_main.py /dev/cu.SLAB_USBtoUART --tx-tests --optima
  python3 ftx1_test_main.py /dev/cu.SLAB_USBtoUART --group vfo,freq,mode
  python3 ftx1_test_main.py /dev/cu.SLAB_USBtoUART -g audio,filter,noise


Bash Test Harness (ftx1_test_main.sh)
-------------------------------------
Uses rigctl to test via the Hamlib API.
Tests VFO, frequency, mode, memory, audio levels, and function settings.

**NOTE:** This test script requires the FTX-1 Hamlib backend to be fully
functional. If the backend has issues, this script will show failures
even if the underlying CAT commands work. Use the Python script for
direct CAT command verification.

Requirements:
  - rigctl (part of Hamlib)
  - FTX-1 backend compiled and installed

Usage:
  ./ftx1_test_main.sh <port> [baudrate] [options]

Options:
  -t, --tx-tests      Enable TX-related tests (PTT, tuner, keyer, etc.)
  -o, --optima        Enable Optima/SPA-1 specific tests
  -g, --group GROUP   Comma-separated list of test groups to run
  -v, --verbose       Verbose output from rigctl
  -h, --help          Show help message

Examples:
  ./ftx1_test_main.sh /dev/cu.SLAB_USBtoUART
  ./ftx1_test_main.sh /dev/cu.SLAB_USBtoUART 38400
  ./ftx1_test_main.sh /dev/cu.SLAB_USBtoUART --tx-tests --optima
  ./ftx1_test_main.sh /dev/cu.SLAB_USBtoUART -g vfo,freq,mode
  ./ftx1_test_main.sh /dev/ttyUSB0 38400 -t -o -g tx,power


Test Groups
-----------
Groups can be specified with --group/-g option (comma-separated):

  vfo       VFO select, split, TX VFO, A/B copy, dual receive, GP OUT (VS, ST, FT, AB, BA, SV, FR, GP)
  freq      Frequency control, band up/down, RIT/XIT, repeater shift (FA, FB, BD, BU, OS)
  mode      Operating mode, filter number (MD, FN)
  filter    Beat cancel, contour, notch, width (BC, BP, CO, SH)
  audio     AF/RF gain, mic gain, AGC, squelch, meters, voice playback (AG, RG, MG, PB, etc.)
  function  Compressor, monitor, lock, tone, TSQL (AI, BI, CT, LK, etc.)
  noise     Noise blanker, noise reduction, attenuator (NA, NL, RL, RA)
  preamp    Preamp, RF attenuator (PA)
  cw        Key speed, break-in, keyer memory (KP, KR, KS, KY, SD, KM, LM)
  tx        PTT, VOX, tuner, MOX, TX watch (AC, MX, TX, VX, VE, TS) - requires --tx-tests
  memory    Memory read/write, channel up/down (AM, BM, CH, MA, MB, MC, MR, MT, MW, MZ, VM)
  info      Radio ID, IF info, meters, FUNC knob (DA, DT, ID, IF, OI, PS, RI, RM, SF)
  misc      CTCSS/DCS tones, scan, date/time, misc (CN, CT, DC, SC, CS, etc.)
  power     RF power, SPA-1 settings (PC, EX030104, EX0307xx) - SPA-1 requires --optima

Examples:
  # Run only VFO and frequency tests
  ./ftx1_test_main.sh /dev/cu.SLAB_USBtoUART -g vfo,freq

  # Run audio, filter, and noise tests
  ./ftx1_test_main.sh /dev/cu.SLAB_USBtoUART -g audio,filter,noise

  # Run TX tests (requires confirmation)
  ./ftx1_test_main.sh /dev/cu.SLAB_USBtoUART --tx-tests -g tx

  # Run power tests including SPA-1 specific
  ./ftx1_test_main.sh /dev/cu.SLAB_USBtoUART --optima -g power


TX Test Safety
--------------
TX-related tests (PTT, tuner, keyer) are DISABLED by default to prevent
accidental transmission. To enable these tests:

1. Use the appropriate flag:
   - Python: --tx-tests
   - Bash: -t or --tx-tests

2. When enabled, the script will display a warning and require you to
   type 'YES' to confirm before proceeding.

3. IMPORTANT: Before running TX tests, ensure:
   - A dummy load is connected, OR
   - An appropriate antenna is connected
   - NEVER transmit without a proper load!

TX-protected tests (in tx group):
   - TX (PTT) - Causes transmission
   - AC (Tuner) - Tuner start causes transmission
   - KY (Keyer) - Sends CW message (causes transmission)
   - MX (MOX) - Manual Operator Transmit (causes transmission)
   - VX (VOX) - Voice-operated TX (can cause transmission if audio present)

TX Safety on Startup:
   The test script automatically ensures safe TX state before running:
   - MX0 (MOX OFF)
   - TX0 (PTT OFF)
   - FT0 (MAIN as TX VFO, not SUB)
   - VX0 (VOX OFF)


Optima/SPA-1 Configuration
--------------------------
The FTX-1 "Optima" configuration includes the SPA-1 amplifier attachment.
Some CAT commands only work when the SPA-1 is connected:

Optima-only commands (require --optima flag):
   - AC (Antenna Tuner) - Internal tuner only available with SPA-1
   - EX030104 (TUNER SELECT) - INT and INT FAST require SPA-1
   - EX0307xx (OPTION Power) - Per-band power limits for SPA-1

Commands with Optima-specific parameters:
   - PC (Power Control) - P1=2 for SPA-1 (005-100W) vs P1=1 for field (005-010W)
   - VE (Firmware Version) - P1=4 returns SPA-1 firmware version

To test Optima-specific features:
   python3 ftx1_test_main.py /dev/cu.SLAB_USBtoUART --optima --tx-tests
   ./ftx1_test_main.sh /dev/cu.SLAB_USBtoUART --optima --tx-tests

Note: Some Optima tests (like AC tuner start) require --tx-tests because
they cause transmission.


Firmware Implementation Notes
-----------------------------
Firmware version: MAIN Ver. 1.08+
Radio ID: Firmware returns ID0840; (Hamlib model 1051)

Commands Not Implemented in Firmware (return '?'):
  SL (Low Cut) - NOT IN FTX-1 CAT SPEC (use EX menu or SH for filter settings)

All 91 official FTX-1 CAT commands are tested and documented.

  Set-only commands (no read/query):
    BS (Band Select) - Format: BS P1 P2P2 (P1=VFO, P2P2=band 00-10)
    QI (QMB Store) - Stores current VFO to Quick Memory Bank
    QR (QMB Recall) - Recalls QMB to current VFO
    CF (Clarifier) - Sets offset value only (CF001+0500), doesn't enable
    EO (Encoder Offset) - Format: EO00+0100; (returns empty)
    ZI (Zero In) - CW mode only (P1=0 MAIN/1 SUB) - activates CW AUTO ZERO IN

  Commands with special format requirements:
    CH (Memory Channel Up/Down) - CH0/CH1 cycle through ALL memory channels
                      Cycles: PMG ch1 → ch2 → ... → QMB ch1 → ... → PMG ch1
                      CH; CH00; CH01; etc. return '?' - only CH0 and CH1 work
    CN (CTCSS Number) - Format: CN P1 P2P3P4 (P1=00/10, P2P3P4=001-050)
    GP (GP OUT) - Full R/W, format GP P1P2P3P4 (each controls A/B/C/D, 0=LOW/1=HIGH)
               REQUIRES: Menu [OPERATION SETTING] → [GENERAL] → [TUN/LIN PORT SELECT] = "GPO"
    MC (Memory Channel) - Read: MC0/MC1, Set: MCNNNNNN (6-digit channel)
    MR (Memory Read) - 5-digit format! MR00001 (not MR0001) for channel 1
    MT (Memory Tag) - Full R/W: MT00001NAME sets 12-char name
    MZ (Memory Zone) - Full R/W: MZ00001DATA sets 10-digit zone data
    VM (VFO/Memory) - Mode codes: 00=VFO, 11=Memory; use SV to toggle

  SPA-1 specific commands (require --optima flag):
    AC (Antenna Tuner) - Internal tuner only available with SPA-1
    EX (Extended Menu) - Full access with SPA-1 connected
    EX030104 (TUNER SELECT), EX0307xx (OPTION Power)

Commands with Firmware Limitations:
  KM (Keyer Message) - Read returns empty (no message content)

Memory Command Format (IMPORTANT - different from spec!):
  MR, MT, MZ all use 5-digit format: P1 P2P2P2P2 (1+4 digits)
  - P1 = bank (always 0)
  - P2P2P2P2 = 4-digit channel number (0001-0099, 0100-0117 for special)
  Example: MR00001 reads channel 1, MT00010 reads name of channel 10

PC (Power Control) Format:
  - Field head (P1=1): Uses decimal for fractional watts (PC10.5, PC11.5)
  - SPA-1 (P1=2): Uses 3-digit integers (PC2005 to PC2100)

CN (CTCSS Number) Format:
  - Query TX tone: CN00; returns CN00nnn (nnn=tone number 001-050)
  - Query RX tone: CN10; returns CN10nnn
  - Set TX tone: CN00nnn; (e.g., CN00012 for tone 12 = 97.4 Hz)
  - Set RX tone: CN10nnn;


Notes
-----
1. Some tests are skipped for safety reasons:
   - PS (Power) - Could power off the radio
   - MA/MB (Memory to VFO) - Destructive
   - MW (Memory Write) - Destructive
   - DN/UP - Context-dependent

2. The Python script tests CAT commands directly via serial.
   The Bash script tests through Hamlib's rigctl interface.

3. Both scripts attempt to restore original values after testing.

4. Run tests with the radio in a known state (VFO mode, unlocked).

5. Model number for rigctl: 1051 (FTX-1)


Known Issues with Hamlib Backend
--------------------------------
RIT/XIT Tests:
The RIT and XIT tests may show "Communication bus collision" errors. This is
a known issue with the newcat framework's handling of these commands, not
specific to the FTX-1 backend implementation.


Author
------
Terrell Deppe (KJ5HST)
Copyright (c) 2025
