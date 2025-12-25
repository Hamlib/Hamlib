FTX-1 CAT Command Reference for Hamlib Implementation
======================================================
Copyright (c) 2025 by Terrell Deppe (KJ5HST)

This file documents all FTX-1 CAT commands organized by functional group.
Each group corresponds to a .c implementation file.

Reference: FTX-1 CAT Operation Reference Manual (2508-C)

FIRMWARE COMPATIBILITY NOTE:
  RIT/XIT (Clarifier): NOT SUPPORTED in latest firmware (v1.08+).
  The RC/TC commands that worked in earlier firmware are no longer functional.
  Clarifier must be controlled directly from the radio front panel.

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
  OS P1 P2;        - Offset Shift (repeater direction) [WORKING - set/read]
                     P1=VFO (0=MAIN, 1=SUB), P2=Shift (0=Simplex, 1=Plus, 2=Minus, 3=ARS)

Repeater Offset Frequency (ftx1_freq.c):
  Repeater offset values are stored per-band in EX menu items:
    EX010316 (TOK_FM_RPT_SHIFT_28)  - 28 MHz band (0-1000 kHz)
    EX010317 (TOK_FM_RPT_SHIFT_50)  - 50 MHz band (0-4000 kHz)
    EX010318 (TOK_FM_RPT_SHIFT_144) - 144 MHz band (0-1000 kHz, in 10kHz units)
    EX010319 (TOK_FM_RPT_SHIFT_430) - 430 MHz band (0-10000 kHz, in 100kHz units)

  ftx1_set_rptr_offs() and ftx1_get_rptr_offs() automatically detect the current
  band from VFO frequency and access the appropriate menu token.

RIT/XIT Clarifier Commands (ftx1_clarifier.c):
  *** WARNING: NOT SUPPORTED IN LATEST FIRMWARE ***
  The RC/TC commands no longer work in firmware v1.08+.
  The code is retained for reference but will return errors.

  RC P1;           - Receiver Clarifier (RIT offset) [NOT WORKING]
  TC P1;           - Transmit Clarifier (XIT offset) [NOT WORKING]
  RT;              - RIT toggle [NOT WORKING - returns '?']
  XT;              - XIT toggle [NOT WORKING - returns '?']
  IF;              - Information query (may still read clarifier state)

Notes:
  - Frequency format: 9 digits, leading zeros required
  - All RIT/XIT CAT commands return '?' on current FTX-1 firmware
  - Clarifier must be controlled from the radio front panel

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
  ZI P1;           - Zero In (P1=VFO, CW mode only) [WORKING - set-only]

Tuning Step (via EX0306 menu):
  The standard TS command returns '?' on FTX-1. Instead, tuning steps are
  mode-specific and controlled via the EX0306 extended menu:

  EX030601;        - SSB/CW Dial Step [WORKING]
                     0=5Hz, 1=10Hz, 2=20Hz
  EX030602;        - RTTY/PSK Dial Step [WORKING]
                     0=5Hz, 1=10Hz, 2=20Hz
  EX030603;        - FM Dial Step [WORKING]
                     0=5kHz, 1=6.25kHz, 2=10kHz, 3=12.5kHz, 4=20kHz, 5=25kHz, 6=Auto

  Hamlib's get_ts/set_ts automatically selects the correct setting based on
  the current operating mode.

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
  ID;              - Radio ID (always 0840 for all FTX-1 configs) [WORKING]
  IF;              - Information Query (composite status) [WORKING]
  OI;              - Opposite Band Information [WORKING]
  DA P1;           - Date/Dimmer (display brightness) [WORKING]
  DT P1;           - Date/Time [WORKING]
  LK P1;           - Lock (0=off, 1=lock) [WORKING]
  CS P1;           - CW Spot (0=off, 1=on, sidetone for tuning) [WORKING]
  EX040101;        - MY CALL (callsign, up to 10 chars) [WORKING]

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
GROUP 12: Extended Menu and Setup (ftx1_ext.c, ftx1_menu.c)
================================================================================
Commands:
  EX P1 P2...;     - Extended Menu Read/Write [WORKING - verified with SPA-1]
  MN P1P2P3;       - Menu Number Select (for menu navigation) [not fully tested]

Extended Menu Numbers:
  The FTX-1 has extensive menu settings accessible via EX command.
  Format: EX GGSSII V; where GG=group, SS=section, II=item, V=value

  Example: EX030104; queries TUNER SELECT (group 03, section 01, item 04)
  Response: EX0301040; means value is 0 (INT)

Hamlib ext_parm API (ftx1.c, ftx1_menu.c):
  The backend exposes 13 EX menu items via Hamlib's ext_parm API.
  These can be accessed using rigctl 'p' (get) and 'P' (set) commands:

  | Parameter        | EX Command | Description              | Range                   |
  |------------------|------------|--------------------------|-------------------------|
  | KEYER_TYPE       | EX020201   | CW keyer type            | 0-5 (OFF/BUG/ELEKEY)    |
  | KEYER_DOT_DASH   | EX020202   | Paddle polarity          | 0=Normal, 1=Reversed    |
  | KEYER_WEIGHT     | EX020203   | Dot/dash weight ratio    | 0-20 (2.5:1 to 4.5:1)   |
  | KEYER_NUM_STYLE  | EX020204   | CW number style          | 0=Normal, 1-6=Cut       |
  | CW_BK_IN_TYPE    | EX020115   | Break-in type            | 0=Semi, 1=Full          |
  | CW_QSK_DELAY     | EX020117   | QSK delay time           | 0=15ms to 3=30ms        |
  | DSP_IF_NOTCH_W   | EX030301   | IF notch filter width    | 0=Narrow, 1=Wide        |
  | DSP_NB_REJECTION | EX030302   | NB rejection level       | 0=Low, 1=Mid, 2=High    |
  | DSP_NB_WIDTH     | EX030303   | Noise blanker width      | 0=1ms, 1=3ms, 2=10ms    |
  | DSP_APF_WIDTH    | EX030304   | APF width                | 0=Narrow, 1=Mid, 2=Wide |
  | DSP_CONTOUR_W    | EX030306   | Contour width            | 1-11                    |
  | GEN_BEEP_LEVEL   | EX030101   | Beep volume level        | 0-100                   |
  | GEN_TUNER_SELECT | EX030104   | Tuner selection          | 0=OFF, 1=INT, 2=EXT, 3=ATAS |

  Usage:
    rigctl -m 1051 -r /dev/ttyUSB0 -s 38400 p KEYER_TYPE     # Get
    rigctl -m 1051 -r /dev/ttyUSB0 -s 38400 P KEYER_TYPE 3   # Set
    rigctl -m 1051 --dump-caps | grep -A 50 "Extra parameters:"

  Implementation notes:
    - Static ftx1_ext_parms[] array in ftx1.c defines available parameters
    - rig_caps.extparms points to this array
    - Uses val->f (float) for RIG_CONF_NUMERIC types per Hamlib API
    - Get/set delegated to ftx1_menu_set_token()/ftx1_menu_get_token()

  Known issues - Signed parameters cause radio lockup:
    - Audio EQ parameters (SSB/CW/AM/FM/DATA/RTTY_AF_TREBLE/MID/BASS) NOT exposed
    - DSP_CONTOUR_LVL (EX030305) NOT exposed - signed value causes hang
    - EX030601 (DIAL_SSB_CW_STEP) causes radio hang on CAT query
    - EX040108 (DISP_LED_DIMMER) causes radio hang on CAT query

Complete EX Menu Implementation (ftx1_menu.c):
  The backend provides full access to ~250 EX menu items via the internal
  ftx1_menu_set_token()/ftx1_menu_get_token() functions. 13 commonly-used
  items are exposed via Hamlib's ext_parm interface (see above).

  Menu items are organized into 11 groups:

  EX01: RADIO SETTING - Mode-specific audio settings (SSB/AM/FM/DATA/RTTY/DIGITAL)
        AF treble/mid/bass, AGC, LCUT/HCUT, USB levels, mod source, DTMF memories
  EX02: CW SETTING - CW mode and keyer settings
        Keyer type, weight, dot-dash, contest number, CW memories
  EX03: OPERATION SETTING - General, band-scan, RX-DSP, TX audio, TX general,
        key/dial, and SPA-1 option settings
  EX04: DISPLAY SETTING - Display, unit, scope, VFO indicator color
  EX05: EXTENSION SETTING - Date/time, timezone, GPS settings
  EX06: APRS SETTING - General, message templates, symbols, digi path
  EX07: APRS BEACON - Beacon set, auto beacon, SmartBeac, beacon text
  EX08: APRS FILTER - List setting, station list, popup, ringer, message filter
  EX09: PRESET - 5 presets with 16 items each (CAT1 rate, AGC, filters, etc.)
  EX11: BLUETOOTH - Enable, audio settings

  Token Encoding:
    Tokens encode EX command addresses as 0xGGSSII (group/section/item)
    Example: TOK_KEYER_TYPE = 0x020201 = EX020201 (CW SETTING > KEYER > TYPE)

  SPA-1 Guardrails:
    Menu items marked with FTX1_MENU_FLAG_SPA1 are blocked when SPA-1 not present.
    This includes internal tuner settings and amplifier-specific power limits.

  SPA-1 Specific EX Commands:
    EX030104  - TUNER SELECT (0=INT, 1=INT FAST, 2=EXT, 3=ATAS)
    EX0307xx  - OPTION power limits per band (5-100W for HF, 5-50W for V/UHF)

  NOTE: Some EX commands require SPA-1 amplifier to be connected.
  Without SPA-1, SPA-1-specific EX commands return -RIG_ENAVAIL.

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
  RI;              - Radio Information [WORKING - read-only, used for DCD/squelch status]
                     Response: RI P1 P2 P3 P4 P5 P6 P7 P8 P9 P10 P11 P12 P13;
                     P8 = SQL status: 0=Closed (no signal), 1=Open (BUSY/signal present)
                     Used by ftx1_get_dcd() to report squelch/DCD state
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

### 1. Radio ID is Fixed (0840)
| Configuration        | Radio ID  | Power Range  |
|----------------------|-----------|--------------|
| Field Head (Battery) | ID0840    | 0.5-6W       |
| Field Head (12V)     | ID0840    | 0.5-10W      |
| Optima (SPA-1)       | ID0840    | 5-100W       |

The Radio ID command (ID;) always returns ID0840 for all FTX-1 configurations.
Head type detection uses a two-stage process:

Stage 1: PC Command Format
  - PC1xxx = Field Head (battery or 12V)
  - PC2xxx = Optima/SPA-1

Stage 2: Power Probe (Field Head Only)
  For Field Head, probe the power source by attempting to set 8W:
  - If radio accepts 8W → 12V power (0.5-10W range)
  - If radio rejects 8W (stays at 6W or below) → Battery power (0.5-6W range)

  This works because the FTX-1 enforces hardware power limits based on the
  actual power source, regardless of menu settings.

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

RIT/XIT: NOT SUPPORTED - RC/TC commands no longer work in latest firmware

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
  Optima (SPA-1):        5-100W amplifier with internal antenna tuner

Note: Radio ID is always 0840 for all configurations.

The backend uses the newcat framework and implements FTX-1 specific
CAT commands documented in the FTX-1 CAT Operation Reference Manual.

Serial Configuration:
  Default: 38400 baud, 8N1
  Supported rates: 4800, 9600, 19200, 38400, 57600, 115200

Head Type Auto-Detection
------------------------
On rig open, the backend auto-detects the head configuration using a two-stage process:

Stage 1: PC Command Format
  The PC (Power Control) command response identifies the head type:
  - PC1xxx = Field Head
  - PC2xxx = Optima/SPA-1 (5-100W with internal tuner)

Stage 2: Power Probe (Field Head Only)
  For Field Head, probe the actual power source by attempting to set 8W:
  - If radio accepts 8W → 12V power (0.5-10W range)
  - If radio rejects 8W (stays ≤6W) → Battery power (0.5-6W range)

  This works because the FTX-1 enforces hardware power limits based on the
  actual power source, regardless of menu settings. The probe saves and
  restores the original power setting.

Secondary Confirmation (SPA-1):
  VE4 command confirms SPA-1 presence:
  - Returns firmware version string if SPA-1 present
  - Returns '?' if no SPA-1

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
- CW pitch adjustment (300-1050 Hz via KP command)
- VOX with adjustable gain and delay
- Memory channels (1-99) plus PMS (100-117)
- Antenna tuner control
- S-meter, SWR, ALC, power meter readings
- DCD (squelch status) via RI command P8
- Repeater shift direction and offset frequency (per-band via EX menu)

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
1. Rig ID is always 0840 for all configurations (use PC command + power probe)
2. The FTX-1 does not support transceive mode (AI must be queried)
3. Some EX menu items (ARO, FAGC, DUAL_WATCH, DIVERSITY) return '?'
4. CW pitch (KP command) is paddle ratio on FTX-1, not pitch frequency
5. Power level display changes with configuration (Field battery/12V vs SPA-1)
6. CT (CTCSS mode) requires VFO parameter: CT0; not CT; (else returns ?;)
7. TS (tuning step) returns ?; - use EX0306 mode-specific dial step instead
8. EX030601 (DIAL_SSB_CW_STEP) causes radio hang on CAT query
9. EX040108 (DISP_LED_DIMMER) causes radio hang on CAT query
10. Signed EX parameters (Audio EQ, DSP_CONTOUR_LVL) cause radio lockup on query

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
  ftx1_freq.c       - Frequency (FA/FB), repeater shift (OS), repeater offset (EX menu)
  ftx1_mode.c       - Mode documentation (delegates to newcat)
  ftx1_audio.c      - AF/RF/Mic gain (AG/RG/MG), power (PC), meters (SM/RM), AGC (GT)
  ftx1_clarifier.c  - RIT/XIT (NOT SUPPORTED in latest firmware)
  ftx1_func.c       - Central dispatcher for func/level operations
  ftx1_info.c       - Radio info (ID/IF/OI), AI mode, IF shift (IS), date/time (DT)
  ftx1_mem.c        - Memory operations (MC/MR/MW/MT/MZ/MA/MB/AM/BM/VM/CH)
  ftx1_tx.c         - PTT (TX), VOX (VX), tuner (AC), break-in (BI), power (PS), DCD (RI)
  ftx1_cw.c         - CW keyer (KS/KP/KR/KY/KM/SD/CS)
  ftx1_ctcss.c      - CTCSS/DCS tone control (CN/CT/DC)
  ftx1_noise.c      - Noise blanker (NL), noise reduction (RL)
  ftx1_filter.c     - Notch (BP), contour/APF (CO), beat cancel (BC)
  ftx1_preamp.c     - Preamp (PA), attenuator (RA)
  ftx1_scan.c       - Scan (SC), band select (BS), band up/down (BU/BD), tuning step (TS), zero-in (ZI)
  ftx1_ext.c        - Extended menu basics (EX), spectrum scope (SS), encoder offset (EO)
  ftx1_menu.c       - Complete EX menu (~250 items): all 11 groups, set/get, ext_parm
  ftx1_menu.h       - EX menu token definitions (TOK_*), item structures, flags

================================================================================
REVISION HISTORY
================================================================================
2025-12-24  Expanded ext_parm API to 13 parameters, documented signed value issue
            - Expanded ftx1_ext_parms[] from 6 to 13 working parameters
            - Added CW keyer settings: KEYER_DOT_DASH, KEYER_NUM_STYLE
            - Added CW mode settings: CW_BK_IN_TYPE, CW_QSK_DELAY
            - Added DSP settings: DSP_IF_NOTCH_W, DSP_NB_REJECTION, DSP_APF_WIDTH,
              DSP_CONTOUR_W
            - Added GEN_TUNER_SELECT for tuner selection (OFF/INT/EXT/ATAS)
            - CRITICAL: Signed EX parameters cause radio lockup on CAT query
              * Audio EQ parameters (SSB/CW/AM/FM/DATA/RTTY_AF_TREBLE/MID/BASS)
              * DSP_CONTOUR_LVL (EX030305)
              * These are NOT exposed via ext_parm API
            - Fixed set_rptr_offs/get_rptr_offs to use val.f instead of val.i
            - Updated all documentation (CLAUDE.md, ftx1_readme.txt, test issues)

2025-12-24  Initial Hamlib ext_parm API for EX menu access
            - Added static ftx1_ext_parms[] array in ftx1.c with 6 parameters
            - Wired rig_caps.extparms to expose parameters via Hamlib API
            - Parameters: KEYER_TYPE, KEYER_WEIGHT, SSB_AF_TREBLE, CW_AF_TREBLE,
              DSP_NB_WIDTH, GEN_BEEP_LEVEL
            - Fixed value type: uses val->f (float) not val->i for RIG_CONF_NUMERIC
            - Fixed KEYER_WEIGHT range from 25-45 to 0-20 (actual FTX-1 range)
            - Fixed CT command to include VFO parameter (CT0; instead of CT;)
            - Documented EX030601/EX040108 cause radio hang - not exposed
            - Updated ftx1.c version to 20251224.0

2025-12-19  Gap fixes and additional capability support
            - Added DCD (squelch status) support via RI command P8
              * Implemented ftx1_get_dcd() in ftx1_tx.c
              * Changed dcd_type from RIG_DCD_NONE to RIG_DCD_RIG
              * RI command returns 13 parameters, P8 is SQL status (0=closed, 1=open)
            - Added RIG_LEVEL_CWPITCH to has_get_level/has_set_level
              * Implementation already existed via KP command (300-1050 Hz)
              * Now properly advertised in rig_caps
            - Added repeater offset frequency support (set_rptr_offs/get_rptr_offs)
              * Implemented in ftx1_freq.c using per-band EX menu tokens
              * TOK_FM_RPT_SHIFT_28/50/144/430 for band-specific offsets
              * Auto-detects band from VFO frequency
            - Implemented RIG_OP_BAND_UP and RIG_OP_BAND_DOWN
              * ftx1_band_up() uses BU0; command
              * ftx1_band_down() uses BD0; command
              * Cycles through amateur bands on MAIN VFO
            - Note: NA (narrow filter) helpers exist but Hamlib has no RIG_FUNC_NAR
              * ftx1_set_na_helper() and ftx1_get_na_helper() available internally
              * Cannot be exposed via standard Hamlib func interface

2025-12-19  Implemented complete EX menu support (~250 menu items)
            - Created ftx1_menu.h with token definitions for all EX commands
              Token encoding: 0xGGSSII maps directly to EX group/section/item
            - Created ftx1_menu.c with menu table and get/set functions
              Covers all 11 EX groups from CAT manual Table 3 (pages 10-16)
            - Groups implemented:
              * EX01 RADIO SETTING (SSB/AM/FM/DATA/RTTY/DIGITAL mode settings)
              * EX02 CW SETTING (keyer type, weight, memories, QSK)
              * EX03 OPERATION SETTING (general, scan, DSP, TX audio, power limits)
              * EX04 DISPLAY SETTING (my call, popup, screen saver, scope)
              * EX05 EXTENSION SETTING (timezone, GPS time)
              * EX06 APRS SETTING (modem, callsign, message templates)
              * EX07 APRS BEACON (beacon set, SmartBeac, status text)
              * EX08 APRS FILTER (station list, popup, ringer, message filter)
              * EX09 PRESET (5 presets with 16 items each)
              * EX11 BLUETOOTH (enable, audio)
            - Added set_ext_parm/get_ext_parm to rig_caps for Hamlib API access
            - SPA-1 guardrails for amplifier-specific settings (FTX1_MENU_FLAG_SPA1)
            - Signed value support for parameters like AF treble/bass (-20 to +10)
            - String parameter support (callsigns, DTMF memories, status text)
            - Convenience functions for common operations (keyer_type, weight, etc.)
2025-12-12  Confirmed Radio ID is fixed (0840), finalized power probe detection
            - Radio ID is always 0840 for ALL FTX-1 configurations (confirmed)
            - Head type detection uses two-stage process:
              * Stage 1: PC command format (PC1xxx=Field, PC2xxx=SPA-1)
              * Stage 2: Power probe for Field Head battery vs 12V
            - Power probe method: attempt to set 8W
              * If accepted (power ≥8W) → Field 12V (0.5-10W)
              * If rejected (stays ≤6W) → Field Battery (0.5-6W)
            - Probe saves and restores original power setting
            - Updated all documentation (README files, code comments)
2025-12-11  Head type auto-detection update
            - Discovered Radio ID is always 0840 for all configurations
            - Changed head type detection to use PC command response format
              (PC1xxx=Field, PC2xxx=SPA-1 instead of ID command)
            - Added Field Head power source probing (battery vs 12V)
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
