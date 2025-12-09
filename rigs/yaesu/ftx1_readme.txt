FTX-1 CAT Command Reference for Hamlib Implementation
======================================================
Copyright (c) 2025 by Terrell Deppe (KJ5HST)

This file documents all FTX-1 CAT commands organized by functional group.
Each group corresponds to a .c implementation file.

Reference: FTX-1 CAT Operation Reference Manual (2508-C)

Legend:
  Set  = Command can set a value
  Read = Command can read/query a value
  Ans  = Answer format when queried
  AI   = Auto-Information (sent automatically when AI mode enabled)

================================================================================
GROUP 1: VFO and Frequency Control (ftx1_vfo.c, ftx1_freq.c)
================================================================================
VFO Commands (ftx1_vfo.c):
  VS P1;           - VFO Select (0=VFO-A/MAIN, 1=VFO-B/SUB) [WORKING]
  ST P1;           - Split on/off (0=off, 1=on) [WORKING]
  FT P1;           - Function TX VFO (0=MAIN TX, 1=SUB TX) [WORKING]
  SV;              - Swap VFO (exchanges A and B) [WORKING]
  AB;              - VFO-A to VFO-B copy [WORKING]
  BA;              - VFO-B to VFO-A copy (read-only query) [WORKING]
  BS P1 P2P2;      - Band Select (P1=VFO, P2P2=band code 00-14) [FIRMWARE BUG: returns ?]
  UP;              - Frequency/Channel Up [WORKING - context-dependent]
  DN;              - Frequency/Channel Down [WORKING - context-dependent]

Frequency Commands (ftx1_freq.c):
  FA P1...P9;      - VFO-A Frequency (9-digit Hz, e.g., FA014250000;) [WORKING]
  FB P1...P9;      - VFO-B Frequency (9-digit Hz) [WORKING]
  FC;              - Sub VFO Frequency [FIRMWARE BUG: returns ?]
  CF P1 P2 P3 P4 P5...P8; - Clarifier (RIT/XIT) [FIRMWARE BUG: returns ?]
                     P1=VFO (0/1), P2=RIT on/off, P3=XIT on/off
                     P4=direction (0=minus, 1=plus), P5-P8=offset (0000-9999 Hz)

Notes:
  - Frequency format: 9 digits, leading zeros required
  - CF command not implemented in firmware v1.08+ - use RI for RIT info

================================================================================
GROUP 2: Mode and Bandwidth (ftx1_mode.c)
================================================================================
Commands:
  MD P1 P2;        - Operating Mode (P1=VFO 0/1, P2=mode code) [WORKING]
  NA P1 P2;        - Narrow Mode (P1=VFO, P2=0 wide/1 narrow) [WORKING]
  SH P1 P2P3;      - Bandwidth/Width (P1=VFO, P2P3=bandwidth code) [WORKING]
  IS P1 P2...P5;   - IF Shift (P1=VFO, P2-P5=offset) [WORKING]

Mode Codes (P2 for MD command):
  1 = LSB          7 = CW-L
  2 = USB          8 = DATA-L (AFSK)
  3 = CW-U         9 = RTTY-U
  4 = FM           A = DATA-FM
  5 = AM           B = FM-N
  6 = RTTY-L       C = DATA-U (AFSK)
                   D = AM-N
                   E = PSK
                   F = DATA-FM-N
                   H = C4FM DN (Digital Narrow)
                   I = C4FM VW (Voice Wide)

Bandwidth Codes (SH command, varies by mode):
  SSB:  00-21 (200Hz to 4000Hz)
  CW:   00-17 (50Hz to 3000Hz)
  RTTY: 00-12 (50Hz to 2400Hz)
  DATA: 00-21 (200Hz to 4000Hz)
  AM:   00-03 (3000Hz to 9000Hz)
  FM:   00 only

================================================================================
GROUP 3: Audio and Levels (ftx1_audio.c)
================================================================================
Commands:
  AG P1 P2P3P4;    - AF Gain (P1=VFO, P2-P4=000-255) [WORKING]
  RG P1 P2P3P4;    - RF Gain (P1=VFO, P2-P4=000-255) [WORKING]
  MG P1P2P3;       - Mic Gain (000-100) [WORKING]
  GT P1P2P3P4;     - AGC Time Constant (0000-6000ms in modes, or code) [WORKING]
  ML P1P2P3;       - Monitor Level (000-100) [WORKING]
  SM P1;           - S-Meter Read (P1=0 main, returns 0000-0255) [WORKING]
  RM P1;           - Meter Read (P1: 1=COMP, 2=ALC, 3=PO, 4=SWR, 5=ID, 6=VDD) [WORKING]
  PC P1P2P3;       - Power Control (see format notes below) [WORKING]
  VG P1P2P3;       - VOX Gain (000-100) [WORKING]
  VD P1P2P3P4;     - VOX Delay (0030-3000ms) [WORKING]
  SD P1P2P3P4;     - CW Break-in Delay (0030-3000ms) [WORKING]
  SQ P1 P2P3P4;    - Squelch Level (P1=VFO, P2-P4=000-100) [WORKING]

PC Command Format (differs from spec):
  Spec (page 21): PC P1 P2P2P2 where P1=1 (field head) or P1=2 (SPA-1)
  Actual firmware (field head 0.5W to 10W):
    - Whole watts: 3-digit zero-padded (PC1001, PC1005, PC1010)
    - Fractional watts: decimal format (PC10.5, PC11.5, PC15.1)
  SPA-1 (P1=2): 3-digit integers (PC2005 to PC2100)

================================================================================
GROUP 4: DSP Filters and Noise (ftx1_filter.c, ftx1_noise.c)
================================================================================
Filter Commands (ftx1_filter.c):
  BC P1 P2;        - Beat Canceller/Auto Notch (ANF) (P1=VFO, P2=0 off/1 on) [WORKING]
  BP P1 P2 P3P3P3; - Manual Notch (P1=VFO, P2=0 on/off or 1 freq, P3=value) [WORKING]
                     P2=0: P3=000 OFF, 001 ON
                     P2=1: P3=001-320 (x10 Hz = 10-3200 Hz notch freq)
  CO P1 P2 P3P3P3P3; - Contour/APF (P1=VFO, P2=type, P3=4-digit value) [WORKING]
                     P2=0: CONTOUR on/off (P3=0000 OFF, 0001 ON)
                     P2=1: CONTOUR freq (P3=0010-3200 Hz)
                     P2=2: APF on/off (P3=0000 OFF, 0001 ON)
                     P2=3: APF freq (P3=0000-0050, maps to -250 to +250 Hz)
  FN P1 P2;        - Filter Number (P1=VFO, P2=filter slot 1-3) [WORKING]

Noise Commands (ftx1_noise.c):
  NB P1 P2;        - Noise Blanker on/off [WORKING - via raw command]
  NL P1 P2P3P4;    - Noise Blanker Level (P1=VFO, P2-P4=000-010) [WORKING]
  NR P1 P2;        - Noise Reduction on/off [WORKING - via raw command]
  RL P1 P2P3;      - Noise Reduction Level (P1=VFO, P2P3=00-15) [WORKING]

================================================================================
GROUP 5: Preamp and Attenuator (ftx1_preamp.c)
================================================================================
Commands:
  PA P1 P2;        - Preamp (P1=band type, P2=preamp code) [WORKING]
  RA P1 P2;        - Attenuator (P1=0 fixed, P2=0 off/1 on) [WORKING]

Preamp Band Types (P1):
  0 = HF/50MHz
  1 = VHF (144MHz)
  2 = UHF (430MHz)

Preamp Codes (P2):
  HF/50MHz: 0=IPO (bypass), 1=AMP1, 2=AMP2
  VHF/UHF:  0=OFF, 1=ON

Attenuator: 12dB fixed attenuation when enabled

================================================================================
GROUP 6: Transmit Control (ftx1_tx.c)
================================================================================
Commands:
  TX P1;           - Transmit (P1: 0=RX, 1=TX CAT, 2=TX Data) [WORKING - TX test]
  PC P1P2P3;       - Power Control (005-100 watts) [WORKING - also in ftx1_audio.c]
  MX P1;           - MOX (0=off, 1=on) [WORKING - TX test]
  VX P1;           - VOX on/off (0=off, 1=on) [WORKING - TX test]
  AC P1P2P3;       - Antenna Tuner Control (P1: 0=off, 1=on, 2=tune) [OPTIMA ONLY]
  BI P1;           - Break-In (QSK) (0=off, 1=semi, 2=full) [WORKING]
  PS P1;           - Power Switch (0=off, 1=on) - USE WITH CAUTION [SKIP - safety]
  KY P1 P2...;     - CW Message Send (causes TX) [WORKING - TX test]

TX Test Safety Notes:
  - TX, MX, VX, KY commands cause actual transmission
  - AC (tuner) command requires SPA-1 amplifier attachment
  - Test scripts require explicit --tx-tests flag to enable
  - Always use dummy load when testing TX functions

================================================================================
GROUP 7: Memory Operations (ftx1_mem.c)
================================================================================
Commands:
  MC P1P2P3;       - Memory Channel Select (001-117) [FIRMWARE BUG: returns ?]
  MR P1 P2P3P4;    - Memory Read (P1=zone, P2-P4=channel) [WORKING - read-only]
  MW P1 P2P3P4...  - Memory Write (writes data to channel) [SKIP - destructive]
  MT P1 P2P3P4;    - Memory Channel Tag/Name (read name) [WORKING - read-only]
  MZ P1 P2P3P4;    - Memory Zone (P1=zone, P2-P4=channel) [WORKING]
  MA;              - Memory to VFO-A [SKIP - destructive]
  MB;              - Memory to VFO-B [SKIP - destructive]
  AM;              - VFO-A to Memory [WORKING - set-only]
  BM;              - VFO-B to Memory [WORKING - set-only]
  QR P1;           - Quick Memory Recall (P1=0-9) [WORKING - set-only]
  QI;              - Quick In [WORKING - set-only]

Memory Channel Ranges:
  001-099 = Regular memory channels
  100-117 = Special channels (P1-P9, PMS, etc.)

================================================================================
GROUP 8: CW Operations (ftx1_cw.c)
================================================================================
Commands:
  KP P1P2;         - Keyer Pitch (00-75, maps to 300-1050 Hz) [WORKING]
  KR P1;           - Keyer Reverse (0=normal, 1=reverse) [WORKING]
  KS P1P2P3;       - Keyer Speed (004-060 WPM) [WORKING]
  KY P1 P2...;     - CW Message Send (P1=0, P2=message up to 24 chars) [TX test]
  SD P1P2P3P4;     - CW Break-in Delay (0030-3000ms) [WORKING]
  BI P1;           - Break-in (0=off, 1=semi, 2=full) [WORKING]
  KM P1 P2P3;      - Keyer Memory (read message slot) [FIRMWARE: returns empty]
  LM P1 P2P3;      - Load Message to slot [WORKING - set-only]

CW Message Characters:
  Standard letters A-Z, numbers 0-9
  Special: / = slash, ? = question, . = period
  Prosigns: Use standard abbreviations

================================================================================
GROUP 9: CTCSS/DCS Encode/Decode (ftx1_ctcss.c)
================================================================================
Commands:
  CN P1 P2P3;      - CTCSS Tone Number (P1=0 TX/1 RX, P2P3=01-50) [FIRMWARE BUG: returns ?]
  CT P1;           - CTCSS Mode (0=off, 1=ENC, 2=TSQ, 3=DCS) [WORKING]
  TS P1;           - Tone Status Query (composite) [WORKING]
  DC P1 P2P3P4;    - DCS Code (P1=0 TX/1 RX, P2-P4=code number) [not tested]

CTCSS Tone Table (01-50):
  01=67.0   11=94.8   21=131.8  31=177.3  41=225.7
  02=69.3   12=97.4   22=136.5  32=179.9  42=229.1
  03=71.9   13=100.0  23=141.3  33=183.5  43=233.6
  04=74.4   14=103.5  24=146.2  34=186.2  44=241.8
  05=77.0   15=107.2  25=151.4  35=189.9  45=250.3
  06=79.7   16=110.9  26=156.7  36=192.8  46=254.1
  07=82.5   17=114.8  27=159.8  37=196.6  47=CS (Motorola)
  08=85.4   18=118.8  28=162.2  38=199.5  48-50=Reserved
  09=88.5   19=123.0  29=165.5  39=203.5
  10=91.5   20=127.3  30=167.9  40=206.5

================================================================================
GROUP 10: Scan and Band Operations (ftx1_scan.c)
================================================================================
Commands:
  SC P1;           - Scan Control (0=stop, 1=up, 2=down) [WORKING]
  BS P1P2;         - Band Select (P1P2=band code) [FIRMWARE BUG: returns ?]
  UP;              - Frequency/Channel Up [WORKING - context-dependent]
  DN;              - Frequency/Channel Down [WORKING - context-dependent]

Band Codes (BS command - not implemented in firmware):
  00 = 160m (1.8MHz)     08 = 15m (21MHz)
  01 = 80m (3.5MHz)      09 = 12m (24MHz)
  02 = 60m (5MHz)        10 = 10m (28MHz)
  03 = 40m (7MHz)        11 = 6m (50MHz)
  04 = 30m (10MHz)       12 = GEN (general coverage)
  05 = 20m (14MHz)       13 = MW (AM broadcast)
  06 = 17m (18MHz)       14 = AIR (airband)
  07 = (reserved)

================================================================================
GROUP 11: Display and Information (ftx1_info.c)
================================================================================
Commands:
  AI P1;           - Auto Information (0=off, 1=on) [WORKING]
  ID;              - Radio ID (returns 0763 for FTX-1) [WORKING]
  IF;              - Information Query (composite status) [WORKING]
  OI;              - Opposite Band Information [WORKING]
  DA P1;           - Date/Dimmer (display brightness) [WORKING]
  DT P1;           - Date/Time [WORKING]
  LK P1;           - Lock (0=off, 1=lock) [WORKING]
  CS P1;           - CW Spot (stored callsign, read-only) [WORKING]

IF Command Response Format (27+ characters):
  Position  Content
  01-02     "IF"
  03-11     Frequency (9 digits)
  12-16     Clarifier offset (5 chars, signed)
  17        RX clarifier status
  18        TX clarifier status
  19        Mode
  20        VFO Memory status
  21        CTCSS status
  22        Simplex/Split
  23-24     Tone number
  25        Shift direction
  ...       Additional fields

================================================================================
GROUP 12: Extended Menu and Setup (ftx1_ext.c)
================================================================================
Commands:
  EX P1 P2...;     - Extended Menu Read/Write [FIRMWARE BUG: returns ?]
  MN P1P2P3;       - Menu Number Select (for menu navigation) [not fully tested]

Extended Menu Numbers:
  The FTX-1 has extensive menu settings accessible via EX command.
  Format: EX MMNN PPPP; where MM=menu, NN=submenu, PPPP=parameter

  NOTE: EX command returns '?' in firmware v1.08+ - menu access via CAT not
  implemented. Use front panel for menu configuration.

================================================================================
GROUP 13: Miscellaneous Commands
================================================================================
Commands:
  AO P1 P2P3P4;    - AMC Output Level (P1=VFO, P2-P4=000-100) [WORKING]
  BD P1;           - Band Down (P1=0) [WORKING - set-only]
  BU P1;           - Band Up (P1=0) [WORKING - set-only]
  FR P1;           - Function RX (shows RX VFO) [WORKING - read-only]
  GP P1P2;         - GP OUT [FIRMWARE BUG: returns ?]
  MS P1;           - Meter Switch (select meter type) [WORKING]
  OS P1;           - Offset Shift (repeater offset) [WORKING - read-only]
  PB P1;           - Playback [WORKING - read-only]
  PL P1;           - Processor Level [WORKING - read-only]
  PR P1;           - Processor on/off [WORKING - read-only]
  RI P1;           - RIT Information [WORKING - read-only]
  SF P1;           - Scope Fix [WORKING - read-only]
  SS P1;           - Spectrum Scope [FIRMWARE BUG: returns ?]
  SV;              - Swap VFO/Memory [WORKING - set-only]
  VE P1;           - VOX Enable [WORKING - read-only]
  VM P1;           - Voice Memory [FIRMWARE BUG: returns ?]
  EO P1;           - Encoder Offset [FIRMWARE BUG: returns ?]
  ZI;              - Zero In [FIRMWARE BUG: returns ?]

================================================================================
IMPLEMENTATION STATUS (Verified 2025-12-08)
================================================================================
Test methodology: Direct serial communication via Python and Hamlib rigctl
Firmware version: MAIN Ver. 1.08+
Serial settings: 38400 baud, 8N2

Test Results Summary:
  Python direct serial: 69 commands working, 22 skipped
  Hamlib shell test:    85 tests passed, 26 skipped

File                 Status      Notes
-------------------- ----------- ---------------------------------------------
ftx1.c               Complete    Main backend implementation
ftx1_vfo.c           -           (integrated into ftx1.c)
ftx1_freq.c          -           (integrated into ftx1.c)
ftx1_mode.c          -           (integrated into ftx1.c)
ftx1_audio.c         -           (integrated into ftx1.c)
ftx1_filter.c        -           (integrated into ftx1.c)
ftx1_noise.c         -           (integrated into ftx1.c)
ftx1_preamp.c        -           (integrated into ftx1.c)
ftx1_tx.c            -           (integrated into ftx1.c)
ftx1_mem.c           -           (integrated into ftx1.c)
ftx1_cw.c            -           (integrated into ftx1.c)
ftx1_ctcss.c         -           (integrated into ftx1.c)
ftx1_scan.c          -           (integrated into ftx1.c)
ftx1_info.c          -           (integrated into ftx1.c)
ftx1_ext.c           -           (integrated into ftx1.c)

================================================================================
SPEC VS FIRMWARE DISCREPANCIES (Verified 2025-12-09)
================================================================================
Comparison of CAT Operation Reference Manual (2508-C) vs actual firmware v1.08+
behavior, verified by Python direct serial and Hamlib rigctl testing.

### 1. Radio ID Mismatch
| Item            | Spec (Page 17) | Actual Firmware |
|-----------------|----------------|-----------------|
| ID command      | Returns 0840   | Returns 0763    |

The spec says `ID;` returns `ID0840;` but the radio returns `ID0763;`.
This is a SPEC ERROR - the radio ID 0763 is correct for FTX-1.

### 2. Commands Documented But Return '?' in Firmware

These commands are fully documented in the spec but return '?' on firmware v1.08+:

| Command | Spec Page | Function              | Status                      |
|---------|-----------|----------------------|------------------------------|
| BS      | 7         | Band Select          | Read/Answer documented, returns '?' |
| CF      | 8         | Clarifier (RIT/XIT)  | Full R/W documented, returns '?' |
| CH      | 8         | Channel Up/Down      | Set documented, returns '?'  |
| CN      | 8         | CTCSS Tone Number    | Full R/W documented, returns '?' |
| EX      | 9-16      | Extended Menu        | 7 pages of docs!, returns '?' |
| GP      | 17        | GP OUT A/B/C/D       | Full R/W documented, returns '?' |
| MC      | 19        | Memory Channel       | Full R/W documented, returns '?' |
| SS      | 25        | Spectrum Scope       | Extensive docs, returns '?'  |

### 3. Commands Missing from Spec Command List (Page 5)

The spec's command list on page 5 is missing some commands that appear later:
- EO (Encoder Offset) - documented on page 9 but missing from list
- FC (Sub VFO Frequency) - not documented at all, firmware returns '?'
- NB (Noise Blanker on/off) - not in list, but NL (level) is
- NR (Noise Reduction on/off) - not in list, but RL (level) is
- AF (AF Gain alias) - not documented (Hamlib uses this)
- RF (RF Gain alias) - not documented (Hamlib uses this)
- SL (Low Cut) - not documented, firmware returns '?'

### 4. PC (Power Control) Format Discrepancy

| Item       | Spec (Page 21)                        | Actual Firmware                    |
|------------|---------------------------------------|-------------------------------------|
| Format     | `PC P1 P2P2P2;` (P1=1/2, P2=3 digits) | Fractional watts use decimal       |
| Field head | `PC1005` - `PC1010`                   | `PC10.5` for 0.5W, `PC1001` for 1W |

The spec shows 3-digit integer format for P2, but firmware uses decimal notation
for sub-watt values on field head (e.g., PC10.5, PC11.5, PC15.1).

### 5. VM Command Dual Definition

Page 26 shows TWO different VM commands:
1. `VM;` - MAIN-SIDE TO MEMORY CHANNEL (set-only)
2. `VM P1 P2P2;` - VFO/MEMORY CHANNEL (read/write)

Both documented as "VM" which is confusing. Python tests show `VM;` (set-only)
returns '?' in firmware.

### 6. Commands That Work But Spec Implies Read-Only

| Command | Spec Says   | Actual Behavior                     |
|---------|-------------|--------------------------------------|
| NL      | Read/Answer | Actually settable via raw command    |
| RL      | Read/Answer | Actually settable via raw command    |
| NB      | Not listed  | Works for on/off via raw command     |
| NR      | Not listed  | Works for on/off via raw command     |

================================================================================
KNOWN FIRMWARE LIMITATIONS (Verified 2025-12-09)
================================================================================
Commands that return '?' (not implemented in firmware v1.08+):

| Command | Description           | Notes                              |
|---------|-----------------------|------------------------------------|
| BS      | Band Select           | Read returns '?'                   |
| CF      | Clarifier (RIT/XIT)   | Read returns '?' - use RI instead  |
| CH      | Channel Up/Down       | Returns '?'                        |
| CN      | CTCSS Number          | Read returns '?'                   |
| EO      | Encoder Offset        | Returns '?'                        |
| EX      | Extended Menu         | Read returns '?' (7 pages in spec!)|
| FC      | Sub VFO Freq          | Read returns '?'                   |
| GP      | GP OUT                | Read returns '?'                   |
| KM      | Keyer Memory          | Returns empty (no message content) |
| MC      | Memory Channel        | Read returns '?'                   |
| SL      | Low Cut filter        | Returns '?' (use SH instead)       |
| SS      | Spectrum Scope        | Read returns '?'                   |
| VM      | Voice Memory (set)    | Returns '?'                        |
| ZI      | Zero In               | Returns '?'                        |

These limitations should be reported to Yaesu for potential firmware updates.
The EX command alone has 7 pages of documentation in the spec (pages 9-16)
covering extensive menu access that is completely non-functional.

================================================================================
HAMLIB BACKEND NOTES
================================================================================
Some Hamlib commands have format mismatches with FTX-1 firmware and require
workarounds using raw CAT command passthrough (`rigctl w "CMD;"`):

| Hamlib Cmd | Issue                                           |
|------------|-------------------------------------------------|
| IS (IFShift)| Hamlib format doesn't match FTX-1 IS command   |
| ML (MonLvl)| Hamlib expects different parameter format       |
| SH (Width) | Hamlib width codes don't match FTX-1 codes      |
| RIT/XIT    | newcat framework collision issues               |

These are worked around in the test harness using raw CAT commands.

================================================================================
TEST SCRIPTS
================================================================================
Location: tests/ftx1_test.py (Python) and tests/ftx1_test.sh (Bash/Hamlib)

Python test (direct serial):
  python3 ftx1_test.py <port> [baud] [--tx-tests] [--optima] [-c CMD1,CMD2]

Bash/Hamlib test:
  ./ftx1_test.sh <port> [baud] [-t] [-o] [-c CMD1,CMD2] [-v]

Options:
  --tx-tests / -t   Enable TX tests (PTT, tuner, keyer)
  --optima / -o     Enable SPA-1 amplifier tests (AC tuner)
  --commands / -c   Test specific commands only
  --verbose / -v    Verbose output

See tests/ftx1_test_readme.txt for full documentation.

================================================================================
REVISION HISTORY
================================================================================
2025-12-09  Added comprehensive SPEC VS FIRMWARE DISCREPANCIES section
            - Documented Radio ID mismatch (spec says 0840, firmware returns 0763)
            - Listed 8 commands fully documented but return '?' in firmware
            - Noted commands missing from spec command list (EO, FC, NB, NR, SL)
            - Documented PC command decimal format for fractional watts
            - Noted VM command dual definition confusion
            - Listed commands that work despite spec implying read-only
2025-12-09  Updated with verified test results from Python and Hamlib testing
            - Corrected firmware limitation list (14 commands return '?')
            - Updated implementation status to reflect ftx1.c integration
            - Added Hamlib backend notes for workaround commands
            - Updated reference to CAT Manual 2508-C
2025-xx-xx  Initial creation based on CAT Manual 2507-B
            Grouped commands by function for modular implementation
