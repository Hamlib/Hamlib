FTX-1 Hamlib Backend
====================
Author: Terrell Deppe (KJ5HST)
Model: 1051
Status: Complete (90/90 CAT commands implemented)

Reference: FTX-1 CAT Operation Reference Manual (2508-C)
Firmware tested: November 2025 Update

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

CAT Commands (90/90 = 100%):
  All 90 official FTX-1 CAT commands are implemented. See ftx1-cat-commands.txt
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
  ftx1_clarifier.c Clarifier (CF command)
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

