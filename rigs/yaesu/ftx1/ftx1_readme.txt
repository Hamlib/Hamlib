FTX-1 Hamlib Backend
====================
Author: Terrell Deppe (KJ5HST)
Model: 1051
Status: Complete (91/91 CAT commands implemented)

Reference: FTX-1 CAT Operation Reference Manual (2508-C)
Firmware tested: v1.08+

================================================================================
OVERVIEW
================================================================================

The FTX-1 is a portable HF/VHF/UHF transceiver with modular head design
supporting three configurations:

  Configuration          Power Range    Detection
  ---------------------  -------------  ----------------------------------
  Field Head (Battery)   0.5-6W         PC1xxx format, rejects 8W setting
  Field Head (12V)       0.5-10W        PC1xxx format, accepts 8W setting
  SPA-1/Optima           5-100W         PC2xxx format, VE4 returns version

Note: Radio ID (ID;) always returns 0840 for ALL configurations.

Serial: 38400 baud, 8N1 (supports 4800-115200)

================================================================================
WHAT'S WORKING
================================================================================

CAT Commands (91/91 = 100%):
  All 91 official FTX-1 CAT commands are implemented. See ftx1-cat-commands.txt
  for the complete command list with backend file mappings.

Extended Parameters (ext_parm API):
  329 EX menu parameters exposed via Hamlib's ext_parm API.
  Access with: rigctl -m 1051 p PARAM_NAME   (get)
               rigctl -m 1051 P PARAM_NAME N (set)

  Categories include:
  - Mode settings (SSB/AM/FM/CW/DATA/RTTY AGC, filters, levels)
  - CW keyer settings (type, speed, weight, memories)
  - DSP settings (NB, NR, notch, contour, APF)
  - Display settings (my call, popup, colors)
  - General settings (beep, tuner, dial steps)
  - APRS settings (modem, callsign, beacons, filters)
  - Presets (5 presets x 16 items each)
  - GPS settings (enable, pinning, baudrate) - on head unit
  - SPA-1 settings (11 params: antenna, power limits) - require amplifier

  List all: rigctl -m 1051 --dump-caps | grep -A 500 "Extra parameters:"

Core Features:
  - Frequency get/set (FA, FB)
  - Mode get/set (MD) - all modes including C4FM
  - VFO selection and split operation (VS, ST, FT)
  - Memory channels (MC, MR, MW, MT, MZ) - 5-digit format
  - Power control (PC) - auto-detects head type
  - S-meter, SWR, ALC, power meters (SM, RM)
  - AF/RF/Mic gain (AG, RG, MG)
  - Squelch (SQ) and DCD status via RI command
  - CTCSS/DCS encode/decode (CN, CT, DC)
  - CW keyer with memories (KS, KP, KM, KY)
  - Noise blanker and reduction (NL, RL)
  - Notch filters (BC, BP, CO)
  - Preamp/attenuator (PA, RA)
  - Repeater shift direction (OS)
  - Repeater offset frequency (via per-band EX menu tokens)
  - Band up/down (BU, BD)
  - Antenna tuner control (AC) - SPA-1 only

================================================================================
WHAT'S NOT WORKING
================================================================================

RIT/XIT (Clarifier):
  NOT SUPPORTED in firmware v1.08+. The RC/TC commands that worked in earlier
  firmware versions no longer function. RT/XT commands return '?'.
  Clarifier must be controlled from the radio front panel.

Antenna Tuner Control (AC command):
  AC is STATUS-ONLY. AC; query returns tuner status (P1=on, P2=status, P3=ant):
    AC100 = tuner bypassed (P2=0)
    AC110 = tuner tuning (P2=1)
    AC120 = tuner matched (P2=2)
  AC set commands (AC000, AC100, AC110) return '?' - use GEN_TUNER_SELECT
  ext_parm (EX030104) to control: 0=OFF(bypass), 1=INT, 2=EXT, 3=ATAS.

Signed EX Parameters (cause radio lockup):
  These parameters are NOT exposed - querying them freezes the radio:
  - Audio EQ (SSB/CW/AM/FM/DATA/RTTY AF_TREBLE/MID/BASS)
  - DSP_CONTOUR_LVL (EX030305)
  - EX030601 (DIAL_SSB_CW_STEP) - hang on query
  - EX040108 (DISP_LED_DIMMER) - hang on query

String EX Parameters:
  50 string parameters (callsigns, DTMF memories, status text) are in the
  menu table but not exposed via ext_parm. Use direct EX commands if needed.

================================================================================
KNOWN QUIRKS
================================================================================

Format Differences from Spec:
  Command   Spec Says              Actual Behavior
  -------   --------------------   ------------------------------------
  BS        Read/write             Set-only (no query capability)
  CH        CH; CH00; CH01;        Only CH0 (up) and CH1 (down) work
  MC        4-digit channel        MC0/MC1 read, MCNNNNNN (6-digit) set
  MR/MT/MZ  4-digit format         5-digit format (MR00001 not MR0001)
  PC        3-digit integer        Decimal for sub-watt (PC10.5, PC11.5)
  VM        Mode 01 = Memory       Mode 00=VFO, 11=Memory; VM000 set only

Special Requirements:
  - CT (CTCSS mode) needs VFO param: CT0; not CT; (else returns ?)
  - GP (GP OUT) needs menu: TUN/LIN PORT SELECT = "GPO" (factory="OPTION")
  - ZI (Zero In) only works in CW mode (MD03 or MD07)
  - TS (tuning step) returns ? - use EX0306 mode-specific dial step

Set-Only Commands (no query):
  BS, CF, EO, QI, QR, ZI, MA, MB, AM, BM, SV, CH, DN, UP, BD, BU

================================================================================
HEAD TYPE DETECTION
================================================================================

Auto-detection on rig open uses two-stage process:

Stage 1 - PC Command Format:
  PC1xxx = Field Head (battery or 12V)
  PC2xxx = SPA-1/Optima

Stage 2 - Power Probe (Field Head only):
  Attempt to set 8W:
  - If accepted (power >= 8W) → Field 12V (0.5-10W range)
  - If rejected (stays <= 6W) → Field Battery (0.5-6W range)

  The FTX-1 enforces hardware power limits based on actual power source.
  Probe saves and restores original power setting.

SPA-1 Confirmation:
  VE4 command returns firmware version if SPA-1 present, '?' if not.

================================================================================
EX MENU IMPLEMENTATION
================================================================================

Token Encoding:
  Tokens encode EX command addresses as 0xGGSSII (group/section/item).
  Example: TOK_KEYER_TYPE = 0x020201 = EX020201

Menu Groups (402 items total, 329 exposed via ext_parm):
  EX01  RADIO SETTING     Mode-specific audio (SSB/AM/FM/DATA/RTTY/DIGITAL)
  EX02  CW SETTING        Keyer type, weight, memories, QSK
  EX03  OPERATION         General, scan, DSP, TX audio, power, dial, SPA-1
  EX04  DISPLAY           My call, popup, screen saver, scope, colors
  EX05  EXTENSION         Timezone, GPS time
  EX06  APRS SETTING      Modem, callsign, message templates
  EX07  APRS BEACON       Beacon set, SmartBeac, status text
  EX08  APRS FILTER       Station list, popup, ringer, message filter
  EX09  PRESET            5 presets x 16 items each
  EX11  BLUETOOTH         Enable, audio settings

SPA-1 Guardrails:
  Items marked with FTX1_MENU_FLAG_SPA1 return -RIG_ENAVAIL when SPA-1 not
  present. This includes tuner settings and amplifier power limits.

================================================================================
SOURCE FILES
================================================================================

Header:
  ftx1.h         Shared definitions, macros, extern declarations

Core:
  ftx1.c         Main driver, rig_caps, open/close, SPA-1 detection
  ftx1_menu.c    Complete EX menu (~400 items), get/set, ext_parm dispatch
  ftx1_menu.h    EX menu token definitions, structures, flags

By Function:
  ftx1_vfo.c       VFO select, split, swap (VS, ST, FT, AB, BA, SV, FR)
  ftx1_freq.c      Frequency, repeater shift/offset (FA, FB, OS, EX menu)
  ftx1_audio.c     Gain, power, meters (AG, RG, MG, PC, SM, RM, GT, VG, VD)
  ftx1_tx.c        PTT, VOX, tuner, power switch, DCD (TX, VX, AC, PS, RI)
  ftx1_mem.c       Memory operations (MC, MR, MW, MT, MZ, MA, MB, VM, CH)
  ftx1_cw.c        CW keyer (KS, KP, KR, KY, KM, SD, BI, CS, LM)
  ftx1_ctcss.c     CTCSS/DCS (CN, CT, DC)
  ftx1_filter.c    Notch, contour, APF (BC, BP, CO, FN)
  ftx1_noise.c     Noise blanker/reduction, narrow (NL, RL, NA)
  ftx1_preamp.c    Preamp, attenuator (PA, RA)
  ftx1_scan.c      Scan, band, tuning step (SC, BS, BD, BU, DN, UP, ZI)
  ftx1_info.c      Radio info (AI, ID, IF, OI, DA, DT, IS, LK, GP, SF)
  ftx1_ext.c       Extended menu basics, scope (EX, SS, EO)
  ftx1_clarifier.c RIT/XIT (NOT SUPPORTED in latest firmware)
  ftx1_func.c      Level/function dispatcher

================================================================================
TESTING
================================================================================

Quick test:
  ./tests/rigctl -m 1051 -r /dev/ttyUSB0 -s 38400 f    # get frequency
  ./tests/rigctl -m 1051 -r /dev/ttyUSB0 -s 38400 m    # get mode
  ./tests/rigctl -m 1051 -r /dev/ttyUSB0 -s 38400 p KEYER_TYPE  # get ext_parm

Dump capabilities:
  ./tests/rigctl -m 1051 --dump-caps

Test results (2025-12-10):
  Python direct serial: 93 tests passed, 0 failed
  Hamlib shell test:    89 tests passed, 27 skipped (TX tests, destructive ops)

================================================================================
REVISION HISTORY
================================================================================

2025-12-25  Expanded ext_parm API from 13 to 329 parameters
            - Generated entries for all unsigned EX menu parameters
            - Added 11 SPA-1 parameters (antenna, power limits)
            - Added 3 GPS parameters (GPS is on head, not SPA-1)
            - Excluded: 28 signed (cause lockup), 50 string, 2 hang-causing

2025-12-24  Initial ext_parm API (13 parameters)
            - Discovered signed parameters cause radio lockup
            - Fixed val.f vs val.i issue
            - Fixed set_rptr_offs/get_rptr_offs

2025-12-19  Complete EX menu implementation (~400 items)
            - Added DCD via RI command, CWPITCH, rptr_offs, band up/down

2025-12-12  Finalized head type detection (PC command + power probe)

2025-12-10  All 91 CAT commands verified and documented
