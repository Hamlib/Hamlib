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
  BS P1 P2P2;      - Band Select (P1=VFO, P2P2=band code 00-10) [WORKING - set-only]
  UP;              - Frequency/Channel Up [WORKING - context-dependent]
  DN;              - Frequency/Channel Down [WORKING - context-dependent]

Frequency Commands (ftx1_freq.c):
  FA P1...P9;      - VFO-A Frequency (9-digit Hz, e.g., FA014250000;) [WORKING]
  FB P1...P9;      - VFO-B Frequency (9-digit Hz) [WORKING]
  CF P1 P2 P3 P4 P5...P8; - Clarifier (RIT/XIT) [WORKING - set offset only]
                     P1=VFO (0/1), P2=RIT on/off, P3=XIT on/off
                     P4=+/- direction, P5-P8=offset (0000-9999 Hz)

Notes:
  - Frequency format: 9 digits, leading zeros required
  - CF sets clarifier offset value only (does not enable clarifier)
  - Format: CF P1 P2 P3 [+/-] PPPP (9 chars after CF)
  - Example: CF001+0500 sets offset to +500Hz
  - P3 must be 1 for command to be accepted; CF010+0500 returns '?'
  - P2 can be 0 or 1 (both work: CF001 and CF011)
  - No CAT command found to enable/disable clarifier (RT, XT, CL all return '?')
  - Use RI for RIT info read

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
  PS P1;           - Power Switch (0=off) [WORKING - read/write, USE WITH CAUTION]
                     NOTE: Only PS0 (power off) works for set. Read returns PS0/PS1.
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
  MC P1;           - Memory Channel Select [WORKING - different format than spec]
  MR P1P2P3P4P5;   - Memory Read (5-digit format!) [WORKING - read-only]
  MW P1P2P3P4P5... - Memory Write (writes data to channel) [WORKING - full implementation]
  MT P1P2P3P4P5;   - Memory Channel Tag/Name (5-digit format) [WORKING - read-only]
  MZ P1P2P3P4P5;   - Memory Zone (5-digit format) [WORKING]
  MA;              - Memory to VFO-A [WORKING - set-only]
  MB;              - Memory to VFO-B [WORKING - set-only]
  AM;              - VFO-A to Memory [WORKING - set-only]
  BM;              - VFO-B to Memory [WORKING - set-only]
  QI;              - QMB Store [WORKING - set-only]
  QR;              - QMB Recall [WORKING - set-only]
  VM P1;           - VFO/Memory mode (P1=0/1 for VFO A/B) [WORKING - partial]
  CH P1;           - Memory Channel Up/Down (P1=0/1) [WORKING - see notes below]

Memory Command Format Notes:
  IMPORTANT: Memory commands use different formats than documented!

  MC format (Memory Channel Select) - DIFFERENT FROM SPEC:
    Read: MC0 (MAIN) or MC1 (SUB)
    Response: MCNNNNNN (6-digit channel number, no VFO in response)
    Example: MC0 returns MC000001 (channel 1)

    Set: MCNNNNNN (6-digit channel, no VFO prefix)
    Example: MC000001 sets to channel 1
    Returns empty on success, '?' if channel doesn't exist

  MR format: MR P1 P2P2P2P2 (5-digit, not 4-digit as documented)
    Example: MR00001 reads channel 1
    Response: MR00001FFFFFFFFF+OOOOOPPMMxxxx
      FFFFFFFFF = 9-digit frequency in Hz
      +/-OOOOO = clarifier offset
      PP = parameters
      MM = mode
    Returns '?' for empty/unprogrammed channels

  MW format (Memory Write) - SET ONLY:
    Format: MW P1P1P1P1P1 P2P2P2P2P2P2P2P2P2 P3P3P3P3P3 P4 P5 P6 P7 P8 P9P9 P10;
    Total: 29 bytes (command + parameters) + semicolon

    Parameters:
      P1 (5 bytes): Channel number (00001-00999, or P-01L to P-50U for PMS)
      P2 (9 bytes): Frequency in Hz (e.g., 014250000 for 14.250 MHz)
      P3 (5 bytes): Clarifier direction (+/-) + offset (0000-9990 Hz)
      P4 (1 byte): RX CLAR (0=OFF, 1=ON)
      P5 (1 byte): TX CLAR (0=OFF, 1=ON)
      P6 (1 byte): Mode code (1=LSB, 2=USB, 3=CW-U, 4=FM, 5=AM, etc.)
      P7 (1 byte): VFO/Memory mode (0=VFO, 1=Memory, 2=MT, 3=QMB, 5=PMS)
      P8 (1 byte): CTCSS mode (0=OFF, 1=ENC/DEC, 2=ENC, 3=DCS)
      P9 (2 bytes): Fixed "00"
      P10 (1 byte): Shift (0=Simplex, 1=Plus, 2=Minus)

    Examples:
      MW00005014250000+000000210000; = Ch 5, 14.250 MHz USB, no shift
      MW00010007030000+000000310000; = Ch 10, 7.030 MHz CW-U
      MW00017146520000+000000410000; = Ch 17, 146.520 MHz FM

    Mode codes for P6:
      1=LSB, 2=USB, 3=CW-U, 4=FM, 5=AM, 6=RTTY-L, 7=CW-L,
      8=DATA-L, 9=RTTY-U, A=DATA-FM, B=FM-N, C=DATA-U, D=AM-N, E=PSK

  MT format (Memory Tag/Name) - READ/WRITE:
    Read: MT00001 returns MT00001[12-char name, space padded]
    Set: MT00001NAMEHERE sets name (12 chars, space padded)
    Example: MT00001MYSTATION   sets channel 1 name to "MYSTATION"

  MZ format (Memory Zone) - READ/WRITE:
    Read: MZ00001 returns MZ00001[10-digit zone data]
    Set: MZ00001NNNNNNNNNN sets zone data
    Example: MZ000010000000000 sets channel 1 zone data

  VM format (VFO/Memory Mode) - PARTIAL:
    Read: VM0 (MAIN) or VM1 (SUB) returns VMxPP
    Mode codes (DIFFERENT FROM SPEC):
      00 = VFO mode
      11 = Memory mode (spec says 01, but firmware uses 11)
      10 = Memory Tune mode
    Set: Only VM000 works (sets to VFO mode)
    To enter Memory mode: Use SV command to toggle

CH format (Memory Channel Up/Down):
    CH0; = Next memory channel (cycles through ALL channels across groups)
    CH1; = Previous memory channel
    Cycles: PMG ch1 → ch2 → ... → QMB ch1 → ch2 → ... → wraps to PMG ch1
    Note: CH; CH00; CH01; etc. return '?' - only CH0 and CH1 work

    Display shows group indicator:
      Red box with "5" = QMB (Quick Memory Bank)
      "M-ALL NNN" = PMG (Primary Memory Group) channel NNN

    MC response format reflects group: MCGGnnnn where GG=group, nnnn=channel
      MC000001 = Group 0, Channel 1 (PMG - Primary Memory Group)
      MC050001 = Group 5, Channel 1 (QMB - Quick Memory Bank)

Memory Channel Ranges:
  000001-000099 = Regular memory channels (6-digit format for MC)
  000100-000117 = Special channels (P1-P9, PMS, etc.)
  00001-00099 = Regular memory channels (5-digit format for MR/MT/MZ)

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
  KM P1 P2...;     - Keyer Memory Read/Write (P1=1-5 slot, P2=msg up to 50 chars) [WORKING]
  LM P1 P2P3;      - Load Message to slot [WORKING - set-only]

CW Message Characters:
  Standard letters A-Z, numbers 0-9
  Special: / = slash, ? = question, . = period
  Prosigns: Use standard abbreviations

================================================================================
GROUP 9: CTCSS/DCS Encode/Decode (ftx1_ctcss.c)
================================================================================
Commands:
  CN P1 P2P3P4;    - CTCSS Tone Number (P1=00 TX/10 RX, P2P3P4=001-050) [WORKING]
                     Query: CN00; or CN10; returns CNppnnn; (pp=00/10, nnn=tone#)
                     Set: CN00nnn; (TX) or CN10nnn; (RX) where nnn=001-050
  CT P1;           - CTCSS Mode (0=off, 1=ENC, 2=TSQ, 3=DCS) [WORKING]
  TS P1;           - Tone Status Query (composite) [WORKING]
  DC P1 P2P3P4;    - DCS Code (P1=0 TX/1 RX, P2-P4=code number) [WORKING]

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
  BS P1 P2P2;      - Band Select (P1=VFO, P2P2=band code) [WORKING - set-only]
  UP;              - Frequency/Channel Up [WORKING - context-dependent]
  DN;              - Frequency/Channel Down [WORKING - context-dependent]

Band Codes (BS command):
  00 = 160m (1.8MHz)     06 = 17m (18MHz)
  01 = 80m (3.5MHz)      07 = 15m (21MHz)
  02 = 60m (5MHz)        08 = 12m (24MHz)
  03 = 40m (7MHz)        09 = 10m (28MHz)
  04 = 30m (10MHz)       10 = 6m (50MHz)
  05 = 20m (14MHz)

Note: BS is set-only (no read/query). Bands 11+ map to GEN/MW/AIR modes.

================================================================================
GROUP 11: Display and Information (ftx1_info.c)
================================================================================
Commands:
  AI P1;           - Auto Information (0=off, 1=on) [WORKING]
  ID;              - Radio ID (Field=0763, Optima=0840) [WORKING]
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
  EX P1 P2...;     - Extended Menu Read/Write [WORKING - verified with SPA-1]
  MN P1P2P3;       - Menu Number Select (for menu navigation) [not fully tested]

Extended Menu Numbers:
  The FTX-1 has extensive menu settings accessible via EX command.
  Format: EX GGSSII V; where GG=group, SS=section, II=item, V=value

  Example: EX030104; queries TUNER SELECT (group 03, section 01, item 04)
  Response: EX0301040; means value is 0 (INT)

  SPA-1 Specific EX Commands:
    EX030104  - TUNER SELECT (0=INT, 1=INT FAST, 2=EXT, 3=ATAS)
    EX030705  - OPTION 160m Power (005-100)
    EX030706  - OPTION 80m Power (005-100)
    EX030707  - OPTION 60m Power (005-100)
    EX030708  - OPTION 40m Power (005-100)
    EX030709  - OPTION 30m Power (005-100)
    EX030710  - OPTION 20m Power (005-100)
    EX030711  - OPTION 17m Power (005-100)
    EX030712  - OPTION 15m Power (005-100)
    EX030713  - OPTION 12m Power (005-100)
    EX030714  - OPTION 10m Power (005-100)
    EX030715  - OPTION 6m Power (005-100)

  NOTE: EX commands require SPA-1 amplifier to be connected for full access.
  Without SPA-1, some EX commands may return '?'.

================================================================================
GROUP 13: Miscellaneous Commands
================================================================================
Commands:
  AO P1 P2P3P4;    - AMC Output Level (P1=VFO, P2-P4=000-100) [WORKING]
  BD P1;           - Band Down (P1=0) [WORKING - set-only]
  BU P1;           - Band Up (P1=0) [WORKING - set-only]
  FR P1P2;         - Function RX (dual/single receive) [WORKING - set/read]
                     P1P2: 00=Dual Receive, 01=Single Receive
                     Dual Receive: both VFOs active (audio from both)
                     Single Receive: only selected VFO active
  GP P1P2P3P4;     - GP OUT A/B/C/D (0=LOW, 1=HIGH) [WORKING - see menu note]
  MS P1;           - Meter Switch (select meter type) [WORKING]
  OS P1 P2;        - Offset Shift (repeater offset) [WORKING - set/read]
                     P1=VFO (0=MAIN, 1=SUB)
                     P2=Shift mode: 0=Simplex, 1=Plus, 2=Minus, 3=ARS
                     NOTE: Only works in FM mode
  PB P1 P2;        - Playback (DVS voice messages) [WORKING - set/read]
                     P1=VFO (0), P2: 0=Stop, 1-5=Play channel 1-5
                     Read: PB0; Response: PB0n (n=channel or 0 if stopped)
  PL P1;           - Processor Level [WORKING - read-only]
  PR P1;           - Processor on/off [WORKING - read-only]
  RI P1;           - RIT Information [WORKING - read-only]
  SF P1 P2;        - Sub Dial (FUNC Knob) assignment [WORKING - read-only recommended]
                     P1=VFO (0), P2=single hex char (0-H)
                     0=None, 7=Mic Gain, D=RF Power, G=CW Pitch, H=BK Delay
                     Read: SF0; Response: SF0X (X=hex char)
                     WARNING: Setting SF may affect SPA-1 amp detection
  SS P1 P2;        - Spectrum Scope [WORKING - P2=0-7 selects parameter]
  SV;              - Swap VFO/Memory [WORKING - set-only]
  TS P1;           - TX Watch (monitor SUB during TX) [WORKING - set/read]
                     P1: 0=Off, 1=On
  VE P1;           - VOX Enable [WORKING - read-only]
  VM P1;           - Voice Memory [FIRMWARE BUG: returns ?]
  EO P1 P2 P3 P4 P5; - Encoder Offset [WORKING - set-only, e.g. EO00+0100]
  ZI P1;           - Zero In (CW mode only, P1=0 MAIN/1 SUB) [WORKING - set-only]

================================================================================
IMPLEMENTATION STATUS (Verified 2025-12-10)
================================================================================
Test methodology: Direct serial communication via Python and Hamlib rigctl
Firmware version: MAIN Ver. 1.08+
Serial settings: 38400 baud, 8N1

Test Results Summary (Latest Run 2025-12-10):
  Python direct serial: 93 tests passed, 0 failed
  Hamlib shell test:    89 tests passed, 27 skipped (TX tests, destructive ops)

All 91 official FTX-1 CAT commands are accounted for:
  - 79 commands pass active testing
  - 12 commands require special flags or manual testing:
    - 6 manual test (DN, UP, MA, MB, MW, PS)
    - 5 TX commands (AC, KY, MX, TX, VX) - require --tx-tests
    - 1 SPA-1 only (EX) - requires --optima

File                 Status      Notes
-------------------- ----------- ---------------------------------------------
ftx1.c               Complete    Main backend, rig_caps, SPA-1 detection
ftx1_vfo.c           Complete    VS, ST, FT, FR, AB, BA, SV, BD, BU
ftx1_freq.c          Complete    FA, FB, OS
ftx1_func.c          Complete    Central dispatcher for func/level operations
ftx1_audio.c         Complete    AG, RG, MG, VG, VD, GT, SM, SH, MS, ML, AO, PC
ftx1_filter.c        Complete    BC, BP, CO, FN
ftx1_noise.c         Complete    NA, NL, RL
ftx1_preamp.c        Complete    PA, RA
ftx1_tx.c            Complete    TX, VX, MX, AC, BI, PS, SQ, PR, TS
ftx1_mem.c           Complete    MC, MR, MW, MT, MZ, MA, MB, AM, BM, QI, QR, VM, CH
ftx1_cw.c            Complete    KP, KR, KS, KY, KM, SD, CT, LM
ftx1_ctcss.c         Complete    CT, CN, DC
ftx1_info.c          Complete    AI, DA, DT, ID, IF, OI, RI, RM, SF, LK, IS
ftx1_scan.c          Complete    SC
ftx1_ext.c           Complete    EX, SS

================================================================================
SPEC VS FIRMWARE DISCREPANCIES
================================================================================
Comparison of CAT Operation Reference Manual (2508-C) vs actual firmware v1.08+
behavior, verified by Python direct serial and Hamlib rigctl testing.

### 1. Radio ID is Always 0840
| Item            | Spec (Page 17) | All Configs  |
|-----------------|----------------|--------------|
| ID command      | Returns 0840   | Returns 0840 |

The Radio ID is ALWAYS 0840 regardless of head type or power source:
  - Field Head on battery: ID0840
  - Field Head on 12V: ID0840
  - Field Head with SPA-1: ID0840

To distinguish configurations, use the PC command response format:
  - PC1xxx = Field Head
  - PC2xxx = SPA-1

To distinguish battery vs 12V for Field Head, probe max power:
  - Set 10W, if accepted = 12V, if clamped to 6W = battery

### 2. Commands with Format Differences from Spec

These commands work but use different formats than documented in the spec:

| Command | Spec Page | Function              | Actual Behavior                     |
|---------|-----------|----------------------|--------------------------------------|
| BS      | 7         | Band Select          | Set-only (no read capability)        |
| CF      | 8         | Clarifier (RIT/XIT)  | Set-only, sets offset (P3=1 required)|
| CH      | 8         | Memory Channel Up/Dn | CH0/CH1 only (CH; CH00; etc. fail)   |
| CN      | 8         | CTCSS Tone Number    | 3-digit tone number format           |
| EX      | 9-16      | Extended Menu        | Requires SPA-1 for full access       |
| GP      | 17        | GP OUT A/B/C/D       | Requires menu: TUN/LIN PORT = "GPO"  |
| MC      | 19        | Memory Channel       | MC0/MC1 read, MCNNNNNN set           |
| SS      | 25        | Spectrum Scope       | SS0X; (X=0-7 selects parameter)      |

### 3. Commands Not in Official 91-Command Spec List (Page 5)

These commands are used by Hamlib but are NOT part of the official 91 FTX-1 CAT commands:
- NB (Noise Blanker on/off) - Hamlib alias, firmware accepts it
- NR (Noise Reduction on/off) - Hamlib alias, firmware accepts it
- AF (AF Gain alias) - Hamlib alias for AG
- RF (RF Gain alias) - Hamlib alias for RG
- SL (Low Cut) - Hamlib uses this, firmware returns '?'

Note: EO (Encoder Offset) IS in the 91 commands - documented on page 9, works as set-only.

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

Note: NB/NR are Hamlib aliases not in official spec (see section 3).

================================================================================
KNOWN FIRMWARE LIMITATIONS
================================================================================
Commands NOT in official FTX-1 CAT spec (return '?'):

| Command | Description           | Notes                              |
|---------|-----------------------|------------------------------------|
| SL      | Low Cut filter        | NOT IN SPEC - use EX menu or SH    |

Commands that are set-only (no read/query capability):

| Command | Description           | Notes                              |
|---------|-----------------------|------------------------------------|
| BS      | Band Select           | BS P1 P2P2 - works, just no read   |
| CF      | Clarifier (RIT/XIT)   | Sets offset only (not enable)      |
| EO      | Encoder Offset        | EO00+0100; (returns empty)         |
| QI      | QMB Store             | Stores VFO to Quick Memory Bank    |
| QR      | QMB Recall            | Recalls QMB to current VFO         |
| ZI      | Zero In               | CW mode only (activates AUTO ZERO IN) |

Commands with Special Format Requirements:

| Command | Description           | Notes                              |
|---------|-----------------------|------------------------------------|
| CH      | Memory Channel Up/Dn  | CH0/CH1 cycle through ALL channels |
| CN      | CTCSS Number          | 3-digit tone number format         |
| EX      | Extended Menu         | Full R/W access with SPA-1         |
| GP      | GP OUT                | Full R/W, requires menu config     |
| KM      | Keyer Memory          | Full R/W: slots 1-5, 50 chars      |
| MC      | Memory Channel        | MC0/MC1 read, MCNNNNNN set         |
| MR      | Memory Read           | 5-digit format (MR00001)           |
| MT      | Memory Tag            | Full R/W: 12-char name             |
| MZ      | Memory Zone           | Full R/W: 10-digit zone data       |
| SS      | Spectrum Scope        | SS0X; where X=0-7 for params       |
| VM      | VFO/Memory mode       | Mode 00=VFO, 11=Memory; use SV     |

Special format requirements:
- CH cycles through ALL channels: CH0=next, CH1=previous; CH; CH00; etc. return '?'
- MC uses different format: read with MC0/MC1, set with MCNNNNNN (returns '?' if channel empty)
- CN works directly with 3-digit tone number
- EX requires SPA-1 connected for full access
- GP requires menu: [OPERATION SETTING] → [GENERAL] → [TUN/LIN PORT SELECT] = "GPO"
  Factory default is "OPTION" which causes GP to return '?'
- MR, MT, MZ use 5-digit format (MR00001) not 4-digit (MR0001) as documented
- VM mode codes differ from spec: 00=VFO, 11=Memory (not 01)
- VM set only works for VM000 (VFO mode); use SV to toggle to memory mode
- ZI only works in CW mode (MD03 or MD07)

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
HAMLIB BACKEND OVERVIEW
================================================================================
Backend: ftx1 (Model 1051)
Author: Terrell Deppe (KJ5HST)
Status: Beta

The FTX-1 is a portable HF/VHF/UHF transceiver with modular head design.
It supports three configurations:

  Field Head (Battery):  0.5-6W portable configuration on internal battery
  Field Head (12V):      0.5-10W portable configuration on external 12V
  SPA-1 (Optima):        5-100W amplifier with internal antenna tuner

The backend uses the newcat framework and implements FTX-1 specific
CAT commands documented in the FTX-1 CAT Operation Reference Manual.

Serial Configuration:
  Default: 38400 baud, 8N1
  Supported rates: 4800, 9600, 19200, 38400, 57600, 115200

Head Type Auto-Detection
------------------------
On rig open, the backend auto-detects the head configuration:

1. PC command response format indicates head type:
   - PC1xxx = Field Head (battery or 12V)
   - PC2xxx = SPA-1

2. For Field Head, power probing distinguishes battery vs 12V:
   - Try setting 10W
   - If accepted (reads back ~10W): 12V power source
   - If clamped to 6W: Battery power source

3. VE4 command - confirms SPA-1 presence:
   - Returns firmware version string if SPA-1 present
   - Returns '?' if no SPA-1

IMPORTANT: Radio ID is always 0840 regardless of configuration.
The ID command does NOT distinguish between configurations.

This detection enables:
  - Correct power range enforcement for each mode
  - Guardrails for amplifier-specific commands (AC tuner control)

SPA-1 Guardrails
----------------
Certain EX menu commands require SPA-1 hardware:

EX030104 (TUNER SELECT):
  Values 0 (INT) and 1 (INT FAST) require SPA-1's internal tuner.
  Setting these without SPA-1 would fail or cause undefined behavior.
  The backend blocks these values when SPA-1 is not detected.

EX0307xx (OPTION Power Settings):
  These set maximum power per band for the SPA-1 amplifier.
  Only accessible when SPA-1 is present.

Supported Features
------------------
- Dual VFO (Main/Sub) with split operation
- All amateur HF bands (160m-10m) plus 6m, 2m, 70cm
- 60m band channels (500-503)
- Modes: LSB, USB, CW, CW-R, AM, FM, FM-N, DATA, DATA-R
- CTCSS/DCS encode and decode
- RIT/XIT with +/-9999 Hz range
- Noise blanker and noise reduction
- Manual/auto notch filter
- Beat cancel and contour controls
- CW keyer with memories (1-5)
- VOX with adjustable gain and delay
- Memory channels (1-99) plus PMS (100-117)
- Antenna tuner control
- S-meter, SWR, ALC, power meter readings

Tuner Control
-------------
The AC (Antenna Tuner Control) command supports:
- AC000 = Tuner off
- AC001 = Tuner on (hold from previous tune)
- AC002 = Start tune cycle

Notes:
- AC003 (forced tune) not implemented (radio does not support it)
- Tune start (AC002) automatically enables the tuner
- Internal tuner (INT, INT FAST) requires SPA-1

Known Quirks
------------
1. Rig ID varies: Field returns ID0763, Optima returns ID0840
2. The FTX-1 does not support transceive mode (AI must be queried)
3. Some EX menu items (ARO, FAGC, DUAL_WATCH, DIVERSITY) return '?'
4. CW pitch (KP command) is paddle ratio on FTX-1, not pitch frequency
5. Power level display changes with head type (field vs SPA-1)

================================================================================
TEST SCRIPTS
================================================================================
See tests/ftx1_test_readme.txt for full test harness documentation.

================================================================================
SOURCE FILE ORGANIZATION
================================================================================
The FTX-1 backend is organized into modular source files by function group.
All files share common definitions via ftx1.h header.

Header File:
  ftx1.h            - Shared definitions (macros, constants, extern declarations)
                      Defines: NC_RIGID_FTX1, FTX1_HEAD_*, FTX1_VFO_TO_P1,
                               FTX1_CTCSS_MODE_*, ftx1_has_spa1(), ftx1_get_head_type()

Source Files:
  ftx1.c            - Main driver, rig_caps, open/close, SPA-1 detection
  ftx1_vfo.c        - VFO select (VS), split (ST/FT), VFO operations (AB/BA/SV)
  ftx1_freq.c       - Frequency get/set (FA/FB)
  ftx1_mode.c       - Mode documentation (delegates to newcat)
  ftx1_audio.c      - AF/RF/Mic gain (AG/RG/MG), power (PC), meters (SM/RM), AGC (GT)
  ftx1_func.c       - Central dispatcher for func/level operations
  ftx1_info.c       - Radio info (ID/IF/OI), AI mode, IF shift (IS), date/time (DT)
  ftx1_mem.c        - Memory operations (MC/MR/MW/MT/MZ/MA/MB/AM/BM/VM/CH)
  ftx1_tx.c         - PTT (TX), VOX (VX), tuner (AC), break-in (BI), power (PS)
  ftx1_cw.c         - CW keyer (KS/KP/KR/KY/KM/SD)
  ftx1_ctcss.c      - CTCSS/DCS tone control (CN/CT/DC)
  ftx1_noise.c      - Noise blanker (NL), noise reduction (RL)
  ftx1_filter.c     - Notch (BP), contour/APF (CO), beat cancel (BC)
  ftx1_preamp.c     - Preamp (PA), attenuator (RA)
  ftx1_scan.c       - Scan (SC), band select (BS), tuning step (TS), zero-in (ZI)
  ftx1_ext.c        - Extended menu (EX), spectrum scope (SS)

================================================================================
REVISION HISTORY
================================================================================
2025-12-11  Head type auto-detection update
            - Discovered Radio ID is always 0840 for all configurations
            - Changed head type detection to use PC command response format
              (PC1xxx=Field, PC2xxx=SPA-1 instead of ID command)
            - Added Field Head power source probing (battery vs 12V)
              by setting 10W and checking if clamped to 6W
            - Updated power ranges: FIELD_BATTERY (0.5-6W), FIELD_12V (0.5-10W)
            - Added FTX1_HEAD_FIELD_BATTERY and FTX1_HEAD_FIELD_12V constants
2025-12-10  Documentation and test verification complete
            - All 91 official FTX-1 CAT commands verified and documented
            - Test results: 93 passed (Python), 89 passed (shell), 0 failed
            - Backend implementation is complete and consistent
2025-12-10  Implemented remaining 6 commands for 100% coverage
            - FR (Function RX) - dual/single receive mode
            - GP (GP OUT) - 4-pin digital output control
            - OS (Offset/Repeater Shift) - FM repeater shift
            - PB (Play Back/DVS) - voice message playback
            - SF (Sub Dial/FUNC Knob) - knob function assignment
            - TS (TX Watch) - monitor SUB during TX
            Backend now covers 91/91 CAT commands (100%)
2025-12-09  Code cleanup and consolidation
            - Created ftx1.h header with shared definitions
            - Consolidated FTX1_VFO_TO_P1 macro (was duplicated in 5 files)
            - Added FTX1_CTCSS_MODE_* constants for proper CTCSS mode handling
            - Fixed ftx1_ctcss.c to use proper mode values instead of RIG_FUNC_* flags
            - Added debug logging in ftx1_preamp.c for band detection failures
            - Moved NC_RIGID_FTX1 and FTX1_HEAD_* constants to shared header
2025-12-09  Added KM (Keyer Memory) read/write support
            - KM command verified working with full read/write (slots 1-5, 50 chars)
            - Added ftx1_set_keyer_memory() function
            - Updated tests (shell and Python) with read/write verification
2025-12-09  Verified CN (CTCSS) and EX (Extended Menu) commands working
            - CN command confirmed working with 3-digit tone number format
            - EX commands verified with SPA-1 connected
            - Added SPA-1 specific EX commands list (TUNER SELECT, OPTION Power)
            - Marked DC (DCS) command as working
            - Fixed ftx1_ctcss.c CN command format (was 2-digit, now 3-digit)
2025-12-09  Added SPEC VS FIRMWARE DISCREPANCIES section
            - Documented Radio ID (Field=0763, Optima=0840)
            - Documented commands with format differences from spec
            - Documented PC command decimal format for fractional watts
2025-12-09  Initial creation based on CAT Manual 2508-C
            - Grouped commands by function for modular implementation
