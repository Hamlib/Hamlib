# Add Yaesu FTX-1 backend driver

## Summary

This PR adds comprehensive Hamlib backend support for the **Yaesu FTX-1** transceiver, a QRP radio (5-100W HF, 50W 2m, 20W 70cm) using a variant of Yaesu's newcat CAT protocol.

## Acknowledgments

Special thanks to **Jeremy Miller (KO4SSD)** for his invaluable contributions in [PR #1826](https://github.com/Hamlib/Hamlib/pull/1826). Jeremy discovered critical workarounds for FTX-1 firmware limitations:

- **RIT/XIT**: Standard RT/XT commands return `?` on FTX-1. Jeremy figured out that RC (Receiver Clarifier) and TC (Transmit Clarifier) commands work correctly.
- **Tuning Steps**: Mode-specific dial steps via EX0306 extended menu command.

This implementation incorporates Jeremy's discoveries with full attribution in the source code.

## Features Implemented

### Core Functions
- **VFO Control**: VFO A/B selection, frequency set/get, split operation
- **RIT/XIT**: Full ±9999 Hz clarifier control using RC/TC commands (Jeremy Miller's discovery)
- **Filters**: All bandwidth settings via SH/NA commands
- **DSP**: NR, NB, ANF, APF, Notch filter, Contour

### Audio & TX
- **Audio**: AF gain, RF gain, Squelch, Monitor gain
- **TX**: Power control (including 0.5W steps), Mic gain, VOX, Compression
- **CW**: Keyer speed (4-60 WPM), Break-in delay, Paddle ratio, send_morse/stop_morse/wait_morse

### Memory & Scan
- **Memory**: Channel read/write (1-99, PMS, 60m channels) with 5-digit format
- **Scan**: VFO scan, mode-specific tuning steps via EX0306 (Jeremy Miller's discovery)
- **Tones**: CTCSS/DCS encode and decode

### Preamp/ATT
- IPO, AMP1 (10dB), AMP2 (20dB), ATT (12dB)

### Head Type Detection (Unique Feature)
Automatic detection of FTX-1 configuration with appropriate power limits:

| Configuration | Power Range | Detection Method |
|---------------|-------------|------------------|
| Field Head (Battery) | 0.5-6W | PC1xxx + rejects 8W |
| Field Head (12V) | 0.5-10W | PC1xxx + accepts 8W |
| Optima/SPA-1 | 5-100W | PC2xxx format |

## Testing

Tested against actual FTX-1 hardware (firmware v1.08+) with 85+ CAT commands verified. All implemented functions confirmed working.

## File Structure

### Main Files
| File | Description |
|------|-------------|
| `rigs/yaesu/ftx1.c` | Main driver, rig_caps, head detection |
| `rigs/yaesu/ftx1.h` | Shared definitions and constants |
| `rigs/yaesu/Makefile.am` | Build configuration |

### Module Files (in `rigs/yaesu/ftx1/`)
| File | Description |
|------|-------------|
| `ftx1_vfo.c` | VFO and split operations |
| `ftx1_freq.c` | Frequency control |
| `ftx1_mode.c` | Mode operations |
| `ftx1_filter.c` | Filter/bandwidth |
| `ftx1_noise.c` | NR, NB, ANF, Notch |
| `ftx1_func.c` | Function dispatcher |
| `ftx1_preamp.c` | Preamp/Attenuator |
| `ftx1_audio.c` | Audio levels, meters |
| `ftx1_tx.c` | TX power, Mic, VOX, Tuner |
| `ftx1_mem.c` | Memory channels |
| `ftx1_cw.c` | CW keyer, send_morse |
| `ftx1_ctcss.c` | CTCSS/DCS |
| `ftx1_scan.c` | Scan, tuning steps (EX0306) |
| `ftx1_info.c` | Rig info, transceive |
| `ftx1_ext.c` | Extended levels |
| `ftx1_clarifier.c` | RIT/XIT (RC/TC commands) |

### Alternative Build
| File | Description |
|------|-------------|
| `ftx1_mono.c` | Monolithic version (auto-generated) |
| `gen_ftx1_mono.sh` | Generator script |

## Firmware Quirks Documented

During testing, several discrepancies between the CAT manual and actual firmware were found:

| Issue | Details |
|-------|---------|
| RT/XT commands | Return `?` - use RC/TC instead (Jeremy Miller) |
| Memory format | Uses 5-digit (MR00001), not 4-digit as documented |
| MC format | Uses 6-digit format, no VFO prefix |
| CH command | Only CH0/CH1 work, not CH; |
| VM mode codes | 00=VFO, 11=Memory (not 01 as documented) |
| PC command | Uses decimal format for fractional watts (PC10.5;) |

These are documented in code comments.

## Test Plan

- [x] Build completes without errors
- [x] `rigctl -m 1051 -l` lists FTX-1
- [x] Basic frequency get/set works with actual hardware
- [x] Mode switching works
- [x] Power control handles decimal values correctly
- [x] RIT/XIT works using RC/TC commands
- [x] Head type detection works for all configurations
- [x] send_morse/stop_morse/wait_morse work

---
*Tested on macOS with CP2102 USB-serial adapter*
*FTX-1 Firmware: v1.08+*
