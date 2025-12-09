FTX-1 CAT Command Test Harness
===============================

This directory contains two test harnesses for exercising all FTX-1 CAT
commands. Both scripts output results to separate success and failure files
that are overwritten each run.


Output Files
------------
- ftx1_test_success.txt  - All passed tests with timestamp and count
- ftx1_test_failure.txt  - All failed tests with error details

Files are OVERWRITTEN each run to provide fresh results.


Python Test Script (ftx1_test.py)
----------------------------------
Uses direct serial communication to send CAT commands to the radio.
Tests all 80+ CAT commands with set/get/restore pattern.

Requirements:
  - Python 3
  - pyserial (pip install pyserial)

Usage:
  python3 ftx1_test.py <port> [baudrate] [--tx-tests] [--optima] [--commands CMD1,CMD2,...]

Options:
  port              Serial port (required)
  baudrate          Baud rate (default: 38400)
  --tx-tests        Enable TX-related tests (PTT, tuner, keyer)
  --optima, -o      Enable Optima/SPA-1 specific tests (AC tuner command)
  --commands, -c    Comma-separated list of commands to test (default: all)

Examples:
  python3 ftx1_test.py /dev/ttyUSB0
  python3 ftx1_test.py /dev/cu.SLAB_USBtoUART 38400
  python3 ftx1_test.py /dev/cu.SLAB_USBtoUART --tx-tests
  python3 ftx1_test.py /dev/cu.SLAB_USBtoUART --optima --tx-tests  # For SPA-1 configs
  python3 ftx1_test.py /dev/ttyUSB0 --commands IF,MR,PC,SH,IS
  python3 ftx1_test.py /dev/ttyUSB0 -c FA,FB


Bash Test Script (ftx1_test.sh)
--------------------------------
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
  ./ftx1_test.sh <port> [baudrate] [options]

Options:
  -t, --tx-tests     Enable TX-related tests (PTT, tuner, keyer, etc.)
  -o, --optima       Enable Optima/SPA-1 specific tests (AC tuner command)
  -c, --commands CMD Comma-separated list of commands to test (e.g., FA,FB,VS)
  -v, --verbose      Verbose output from rigctl
  -h, --help         Show help message

Examples:
  ./ftx1_test.sh /dev/cu.SLAB_USBtoUART
  ./ftx1_test.sh /dev/cu.SLAB_USBtoUART 38400
  ./ftx1_test.sh /dev/cu.SLAB_USBtoUART --tx-tests
  ./ftx1_test.sh /dev/cu.SLAB_USBtoUART -c FA,FB,VS,MD
  ./ftx1_test.sh /dev/ttyUSB0 38400 -t -o


Test Coverage
-------------
Both scripts test the following CAT command groups:

VFO Commands:
  VS (VFO Select), ST (Split), FT (TX VFO), AB (Swap VFO),
  BA (Copy VFO), SV (Swap VFO/Mem), UP, DN

Frequency Commands:
  FA (VFO A Freq), FB (VFO B Freq), FC (Sub VFO Freq)

Mode Commands:
  MD (Mode), BW (Bandwidth), FN (Filter)

Audio/Level Commands:
  AG (AF Gain), RG (RF Gain), MG (Mic Gain), PC (Power),
  SM (S-Meter), SQ (Squelch), ML (Monitor Level), VG (VOX Gain),
  VD (VOX Delay), GT (AGC)

Filter Commands:
  SH (High Cut), SL (Low Cut), NA (Notch), IS (IF Shift)

Noise/Reduction Commands:
  NB (Noise Blanker), NR (Noise Reduction), BC (Beat Cancel),
  AN (Auto Notch)

Preamp/Attenuator Commands:
  PA (Preamp), RA (Attenuator)

TX Commands:
  TX (PTT), PS (Power Stat), VX (VOX), AC (Tuner)

Memory Commands:
  MC (Memory Channel), MR (Memory Read), MW (Memory Write)

CW Commands:
  KS (Key Speed), KP (Key Pitch), KY (Keyer Send), SD (Break-in Delay)

CTCSS/DCS Commands:
  CN (CTCSS Number), CT (CTCSS Mode)

Info/Display Commands:
  AI (Auto Info), ID (Radio ID), IF (Info), DA (Dimmer),
  LK (Lock), CS (Callsign), NM (Name Display)

Extended Menu Commands:
  EX (Extended Menu), MN (Menu Number), QI (Quick Info)

Scan Commands:
  SC (Scan), BS (Band Select), TS (Tuning Step)


TX Test Safety
--------------
TX-related tests (PTT, tuner, keyer) are DISABLED by default to prevent
accidental transmission. To enable these tests:

1. Use the appropriate flag:
   - Python: --tx-tests
   - Bash: -t

2. When enabled, the script will display a warning and require you to
   type 'YES' to confirm before proceeding.

3. IMPORTANT: Before running TX tests, ensure:
   - A dummy load is connected, OR
   - An appropriate antenna is connected
   - NEVER transmit without a proper load!

TX-protected tests:
   - TX (PTT) - Causes transmission
   - AC (Tuner) - Tuner start causes transmission
   - KY (Keyer) - Sends CW message (causes transmission)
   - MX (MOX) - Manual Operator Transmit (causes transmission)
   - VX (VOX) - Voice-operated TX (can cause transmission if audio present)

TX Safety on Startup:
   The test script now automatically ensures safe TX state before running:
   - MX0 (MOX OFF)
   - TX0 (PTT OFF)
   - FT0 (MAIN as TX VFO, not SUB)
   - VX0 (VOX OFF)

FT (Function TX) Command:
   The FT command selects which VFO is the transmitter:
   - FT0: MAIN-side Transmitter
   - FT1: SUB-side Transmitter
   The test_FT test always restores FT0 for safety to prevent SUB from
   becoming the active TX VFO. Before toggling FT, it sends MX0 to ensure
   MOX is off.

VS (VFO Select) Command:
   The VS command selects the active TX/RX VFO:
   - VS0: MAIN VFO is active TX/RX
   - VS1: SUB VFO is active TX/RX
   The test_VS test ensures all TX states are OFF (MX0, TX0, VX0, FT0) before
   toggling VS, and always restores VS0 for safety. This prevents SUB from
   transmitting if any latent TX state was active.

AB/BA (VFO Copy) Commands:
   The AB and BA commands copy VFO settings between MAIN and SUB:
   - AB: Copy MAIN (A) to SUB (B)
   - BA: Copy SUB (B) to MAIN (A)
   Both test_AB and test_BA ensure all TX states are OFF (MX0, TX0, VX0, FT0, VS0)
   before copying VFO settings. This prevents accidental TX on SUB if TX state
   was latent during VFO manipulation.

SD (Break-in Delay) Command:
   The SD command sets the CW semi break-in delay (00-30).
   Since this is a CW TX parameter, test_SD ensures all TX states are OFF
   (MX0, TX0, VX0, FT0, VS0) before manipulating the delay value.


Optima/SPA-1 Configuration
--------------------------
The FTX-1 "Optima" configuration includes the SPA-1 amplifier attachment.
Some CAT commands only work when the SPA-1 is connected:

Optima-only commands (require --optima flag):
   - AC (Antenna Tuner) - Internal tuner only available with SPA-1
     Per spec page 6: P1=0 is "Internal Antenna Tuner (FTX-1 optima)"
     Without SPA-1, AC read returns '?' error

Commands with Optima-specific parameters:
   - PC (Power Control) - P1=2 for SPA-1 (005-100W) vs P1=1 for field head (005-010W)
   - VE (Firmware Version) - P1=4 returns SPA-1 firmware version

To test Optima-specific features:
   python3 ftx1_test.py /dev/cu.SLAB_USBtoUART --optima --tx-tests

Note: The --optima flag requires --tx-tests because the AC tuner start
command causes transmission.


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

6. TX tests require explicit opt-in (see TX Test Safety above).


Firmware Implementation Notes (as of 2025-12-08)
------------------------------------------------
The following notes document firmware behavior discovered through direct
serial testing with the FTX-1 CAT Operation Reference Manual (2508-C).
Firmware version: MAIN Ver. 1.08+

### Commands Not Implemented in Firmware (return '?')

| Command | Description | Notes |
|---------|-------------|-------|
| BS      | Band Select | Read returns '?' |
| CF      | Clarifier   | Read returns '?' |
| CH      | Channel Up/Down | Returns '?' |
| CN      | CTCSS Number | Read returns '?' |
| EX      | Extended Menu | Read returns '?' |
| FC      | Sub VFO Freq | Read returns '?' |
| GP      | GP OUT      | Read returns '?' |
| MC      | Memory Channel | Read returns '?' |
| SS      | Spectrum Scope | Read returns '?' |

### Commands with Firmware Limitations

| Command | Description | Notes |
|---------|-------------|-------|
| KM      | Keyer Message | Read returns empty (no message content) |

### Commands with Different Response Formats

Only one command has a response format that differs from spec 2508-C.
Verified by direct serial testing on 2025-12-08.

| Command | Spec Format | Firmware Format | Difference |
|---------|-------------|-----------------|------------|
| PC      | `PC P1 P2P2P2` (P1+3 digits) | `PC1xxx` or `PC1x.x` | P2 uses decimal for fractional watts in field mode |

**PC (Power Control) detail:**
- Spec (page 21): PC P1 P2P2P2 where P1=1 (field head) or P1=2 (SPA-1)
- Field head (P1=1) actual firmware behavior (0.5W to 10W):
  - Whole watts: 3-digit zero-padded (PC1001, PC1005, PC1010)
  - Fractional watts: decimal format (PC10.5, PC11.5, PC15.1)
- SPA-1 (P1=2) uses 3-digit integers (PC2005 to PC2100)

Field head examples (tested):
  | Setting | Response | P1 | P2 (power) |
  |---------|----------|----|----|
  | 0.5W    | PC10.5   | 1  | 0.5 |
  | 1W      | PC1001   | 1  | 001 |
  | 1.5W    | PC11.5   | 1  | 1.5 |
  | 5W      | PC1005   | 1  | 005 |
  | 5.1W    | PC15.1   | 1  | 5.1 |
  | 10W     | PC1010   | 1  | 010 |

### Testing Notes

**RA (Attenuator) command:** May occasionally return '?' due to async response
contamination. Test includes retry logic with buffer clearing.

### Python Test Results Summary (Direct Serial)

**Latest Run:** 91 total tests, 69 passed, 0 failed, 22 skipped

**Working CAT Commands (69 - proven by direct serial):**
AB, AG, AI, AM, AO, BA, BC, BD, BI, BM, BP, BU, CO, CS, CT, DA, DT,
FA, FB, FN, FR, FT, GT, ID, IF, IS, KP, KR, KS, LK, LM, MD, MG, ML, MR,
MS, MT, MZ, NA, NL, OI, OS, PA, PB, PC, PL, PR, QI, QR, RA, RG, RI, RL,
RM, SC, SD, SF, SH, SM, SQ, ST, SV, TS, VD, VE, VG, VS

**Skipped Tests (22 commands):**
| Category | Commands | Reason |
|----------|----------|--------|
| TX tests | KY, MX, TX, VX | Disabled by default (use --tx-tests) |
| Optima only | AC | Requires SPA-1 amp (use --optima --tx-tests) |
| Not implemented | BS, CF, CH, CN, EX, FC, GP, MC, SS | Returns '?' in firmware |
| Firmware limitation | KM, EO, VM, ZI | Returns '?' or empty |
| Manual test | DN, UP | Context-dependent behavior |
| Destructive | MA, MB, MW | Overwrites memory/VFO data |
| Safety | PS | Set command may power off radio |

### Bash/Hamlib Test Results Summary

**Latest Run:** 85 passed, 0 failed, 26 skipped

**Commands Tested via Hamlib (85):**
The shell test uses both native Hamlib API calls and raw CAT command
passthrough (`w` command) to achieve coverage of all working CAT commands.

### CAT Command Support Comparison: Python vs Hamlib

This table shows which CAT commands work on the radio (proven by Python
direct serial test) and their Hamlib support status.

**Legend:**
- PASS = Test passes
- SKIP = Intentionally skipped (TX, destructive, firmware bug)
- N/A = Not applicable to Hamlib (no equivalent API)

| CAT Cmd | Description | Radio Works | Hamlib | Hamlib Method | Notes |
|---------|-------------|-------------|--------|---------------|-------|
| AB | VFO-A to B copy | PASS | PASS | raw `w AB;` | Set-only |
| AG | AF Gain | PASS | PASS | `l AF` / `L AF` | Native API |
| AI | Auto Info | PASS | PASS | raw `w AI;` | Read/set |
| AM | VFO-A to Memory | PASS | PASS | raw `w AM;` | Set-only |
| AO | AMC Output Level | PASS | PASS | raw `w AO;` | Read/set |
| BA | VFO-B to A copy | PASS | PASS | raw `w BA;` | Set-only |
| BC | Beat Canceller | PASS | PASS | `u ANF` / `U ANF` | Native API |
| BD | Band Down | PASS | PASS | raw `w BD0;` | Set-only |
| BI | Break-in | PASS | PASS | raw `w BI;` | Read/set |
| BM | VFO-B to Memory | PASS | PASS | raw `w BM;` | Set-only |
| BP | Break-in Param | PASS | PASS | raw `w BP00;` | Read/set |
| BS | Band Select | SKIP | SKIP | - | Firmware returns '?' |
| BU | Band Up | PASS | PASS | raw `w BU0;` | Set-only |
| CF | Clarifier | SKIP | SKIP | - | Firmware returns '?' |
| CH | Channel Up/Down | SKIP | SKIP | - | Firmware returns '?' |
| CN | CTCSS Number | SKIP | SKIP | - | Firmware returns '?' |
| CO | Contour | PASS | PASS | raw `w CO00;` | Read/set |
| CS | CW Spot | PASS | PASS | raw `w CS;` | Read-only |
| CT | CTCSS Mode | PASS | PASS | raw `w CT0;` | Read/set |
| DA | Date | PASS | PASS | raw `w DA;` | Read/set |
| DN | Down | SKIP | SKIP | - | Context-dependent |
| DT | Date/Time | PASS | PASS | raw `w DT0;` | Read/set |
| EO | Encoder Offset | SKIP | SKIP | - | Firmware returns '?' |
| EX | Extended Menu | SKIP | SKIP | - | Firmware returns '?' |
| FA | VFO-A Frequency | PASS | PASS | `f` / `F` | Native API |
| FB | VFO-B Frequency | PASS | PASS | `i` / `I` | Native API |
| FC | Sub VFO Freq | SKIP | SKIP | - | Firmware returns '?' |
| FN | Filter Number | PASS | PASS | raw `w FN;` | Read/set |
| FR | Function RX | PASS | PASS | raw `w FR;` | Read-only |
| FT | TX VFO Select | PASS | PASS | `s` / `S VFOB` | Native API |
| GP | GP OUT | SKIP | SKIP | - | Firmware returns '?' |
| GT | AGC Time | PASS | PASS | `l AGC` / `L AGC` | Native API |
| ID | Radio ID | PASS | PASS | raw `w ID;` | Read-only |
| IF | Information | PASS | PASS | raw `w IF;` | Read-only |
| IS | IF Shift | PASS | PASS | raw `w IS0;` | Hamlib broken |
| KM | Keyer Memory | SKIP | SKIP | - | Firmware returns empty |
| KP | Keyer Pitch | PASS | PASS | raw `w KP;` | Read/set |
| KR | Keyer | PASS | PASS | raw `w KR;` | Read-only |
| KS | Key Speed | PASS | PASS | `l KEYSPD` / `L KEYSPD` | Native API |
| KY | Keyer Send | SKIP | SKIP | - | TX test (causes CW TX) |
| LK | Lock | PASS | PASS | `u LOCK` / `U LOCK` | Native API |
| LM | Load Message | PASS | PASS | raw `w LM00;` | Set-only |
| MA | Memory to VFO-A | SKIP | SKIP | - | Destructive |
| MB | Memory to VFO-B | SKIP | SKIP | - | Destructive |
| MC | Memory Channel | SKIP | SKIP | - | Firmware returns '?' |
| MD | Mode | PASS | PASS | `m` / `M` | Native API |
| MG | Mic Gain | PASS | PASS | `l MICGAIN` / `L MICGAIN` | Native API |
| ML | Monitor Level | PASS | PASS | raw `w ML0;` | Hamlib broken |
| MR | Memory Read | PASS | PASS | raw `w MR00001;` | Read-only |
| MS | Meter Switch | PASS | PASS | raw `w MS;` | Read/set |
| MT | Memory Tag | PASS | PASS | raw `w MT00001;` | Read-only |
| MW | Memory Write | SKIP | SKIP | - | Destructive |
| MX | MOX | SKIP | SKIP | - | TX test |
| MZ | Memory Zone | PASS | PASS | raw `w MZ00001;` | Read/set |
| NA | Noise Attenuator | PASS | PASS | raw `w NA0;` | Read/set |
| NL | Noise Limiter Level | PASS | PASS | raw `w NL0;` | Read/set |
| OI | Opp Band Info | PASS | PASS | raw `w OI;` | Read-only |
| OS | Offset Shift | PASS | PASS | raw `w OS0;` | Read-only |
| PA | Preamp | PASS | PASS | `l PREAMP` / `L PREAMP` | Native API |
| PB | Playback | PASS | PASS | raw `w PB0;` | Read-only |
| PC | Power Control | PASS | PASS | `l RFPOWER` / `L RFPOWER` | Native API |
| PL | Processor Level | PASS | PASS | raw `w PL;` | Read-only |
| PR | Processor | PASS | PASS | raw `w PR0;` | Read-only |
| PS | Power Switch | SKIP | SKIP | - | May power off radio |
| QI | Quick In | PASS | PASS | raw `w QI;` | Set-only |
| QR | Quick Recall | PASS | PASS | raw `w QR;` | Set-only |
| RA | RF Attenuator | PASS | PASS | `l ATT` / `L ATT` | Native API |
| RG | RF Gain | PASS | PASS | `l RF` / `L RF` | Native API |
| RI | RIT Info | PASS | PASS | raw `w RI0;` | Read-only |
| RL | NR Level | PASS | PASS | raw `w RL0;` | Read/set |
| RM | Read Meter | PASS | PASS | raw `w RM0;` | Read-only |
| SC | Scan | PASS | PASS | raw `w SC;` | Read/set |
| SD | Break-in Delay | PASS | PASS | `l BKINDL` / `L BKINDL` | Native API |
| SF | Scope Fix | PASS | PASS | raw `w SF0;` | Read-only |
| SH | Width/High Cut | PASS | PASS | raw `w SH0;` | Hamlib broken |
| SL | Low Cut | SKIP | SKIP | raw `w SL0;` | Firmware '?' |
| SM | S-Meter | PASS | PASS | `l STRENGTH` | Native API |
| SQ | Squelch | PASS | PASS | `l SQL` / `L SQL` | Native API |
| SS | Spectrum Scope | SKIP | SKIP | - | Firmware returns '?' |
| ST | Split Toggle | PASS | PASS | `s` / `S` | Native API |
| SV | Swap VFO/Mem | PASS | PASS | raw `w SV;` | Set-only |
| TS | TX Watch | PASS | PASS | raw `w TS;` | Read-only |
| TX | PTT | SKIP | SKIP | - | TX test |
| UP | Up | SKIP | SKIP | - | Context-dependent |
| VD | VOX Delay | PASS | PASS | `l VOXDELAY` / `L VOXDELAY` | Native API |
| VE | VOX Enable | PASS | PASS | raw `w VE0;` | Read-only |
| VG | VOX Gain | PASS | PASS | `l VOXGAIN` / `L VOXGAIN` | Native API |
| VM | Voice Memory | SKIP | SKIP | - | Firmware returns '?' |
| VS | VFO Select | PASS | PASS | `v` / `V` | Native API |
| VX | VOX | SKIP | SKIP | - | TX test |
| ZI | Zero In | SKIP | SKIP | - | Firmware returns '?' |

### Why Hamlib Can't Test All Commands

1. **Hamlib Abstraction Layer**: Hamlib provides a unified API across many
   radios. Not every FTX-1 CAT command has a direct Hamlib equivalent.

2. **Workaround via Raw Commands**: The shell test uses `rigctl w "CMD;"` to
   send raw CAT commands for functions Hamlib doesn't natively support.

3. **Broken Hamlib Commands**: Some Hamlib commands (IS, ML, SH, RIT/XIT)
   have format mismatches with FTX-1 firmware. Workarounds use raw commands.

4. **Firmware Limitations**: 12 commands return '?' in firmware (BS, CF, CH,
   CN, EO, EX, FC, GP, MC, SL, SS, VM, ZI) - these can't be tested at all.

5. **TX/Destructive Tests**: 9 commands are skipped for safety (AC, KY, MA,
   MB, MW, MX, PS, TX, VX) - require explicit --tx-tests flag.

### Test Methodology

Tests performed using direct serial communication at 38400 baud, 8N2.
Each command was tested using the pattern:
1. Read original value (if applicable)
2. Set new value
3. Read back to verify
4. Restore original value

Test script: ftx1_test.py (Python) / ftx1_test.sh (Hamlib)
Serial port: /dev/cu.SLAB_USBtoUART


Known Issues with Hamlib Backend
---------------------------------
**RIT/XIT Tests:**
The RIT and XIT tests may show "Communication bus collision" errors. This is
a known issue with the newcat framework's handling of these commands, not
specific to the FTX-1 backend implementation. The underlying CAT commands
(RT, XT, RC, RU, RD) work correctly when tested via direct serial (Python
tests), but the newcat framework's validation and retry logic can cause
collisions during rapid command sequences.


Author
------
Terrell Deppe (KJ5HST)
Copyright (c) 2025
