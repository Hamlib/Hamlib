# Sidecar API Extensibility Analysis

**Date:** 2026-06-20  
**Purpose:** Ensure sidecar API supports all SDR types, not just TCI

---

## Target SDR Backends

### High Priority
1. **TCI (ExpertSDR3)** - ✅ Already implemented
2. **KiwiSDR** - WebSocket protocol, baseband audio + optional IQ
3. **FlexRadio (SmartSDR)** - VITA-49 protocol, IQ + audio
4. **ICOM IC-705/7300** - Native CI-V, audio only (but could add IQ via CI-V extensions)
5. **Apache SDR** - Network protocol, IQ-native
6. **SDRPlay (RSPduo, RSPdx)** - API-based, IQ-native

### Medium Priority
7. **Perseus** - USB, IQ-native
8. **Airspy HF+** - USB, IQ-native
9. **LimeSDR** - SoapySDR via separate backend
10. **PlutoSDR** - SoapySDR via separate backend

---

## Current API Strengths (Already Generic)

### ✅ Protocol-Agnostic Design

1. **No TCI dependencies** - All code is generic
2. **Dynamic sample rate** - Each frame specifies rate (supports runtime changes)
3. **Dynamic format** - Each frame specifies format (int16/int24/int32/float32)
4. **Multiple receivers** - `receiver` field supports multi-channel SDRs
5. **Bidirectional** - RX and TX paths defined
6. **Reserved header space** - 32 bytes for future use

### ✅ Extensible Stream Types

- Types 0-19 defined
- Types 20-4294967295 available
- Unknown-type-ignore rule documented
- No version negotiation needed

---

## Potential Gaps for Non-TCI SDRs

### 1. TX Pacing Models

**TCI Model:** TX_CHRONO (type 3)
- Radio tells client "send N samples now"
- Pull-based (radio requests)

**Alternative Models Needed:**

#### A. **Buffer Level Pacing**
- Radio reports TX buffer level
- Client maintains buffer at target level
- **Use case:** FlexRadio SmartSDR

```c
// Proposed: STREAM_TX_BUFFER_LEVEL (type 20)
// length = current_samples_in_buffer
// channels = buffer_size_samples
```

#### B. **Free-Running TX**
- Client sends TX samples continuously
- No pacing frames needed
- **Use case:** KiwiSDR, simple radios

**Solution:** Already supported - just don't send CHRONO, client free-runs

#### C. **Timestamp-Based TX**
- Client sends samples with timestamps
- Radio schedules transmission
- **Use case:** VITA-49 (FlexRadio)

```c
// Proposed: STREAM_TX_SCHEDULED (type 21)
// reserved[8] = timestamp_lo
// reserved[9] = timestamp_hi
// Payload: TX_AUDIO or TX_IQ with timestamp
```

**Recommendation:** Add types 20-21, document as optional

---

### 2. Metadata / Timestamps

**Current:** No timestamp or metadata support

**Needed For:**
- **GPS-disciplined SDRs** - KiwiSDR, Airspy HF+
- **Signal strength metadata** - Most SDRs provide RSSI
- **Timing precision** - VITA-49, synchronized receivers

#### Proposal: Use Reserved Fields

**Option A - Per-Frame Timestamps (Minimal Change):**

```c
// Use reserved fields in existing frame header:
// reserved[8] = timestamp_seconds (Unix epoch)
// reserved[9] = timestamp_nanoseconds
// reserved[10] = signal_level_dbfs (as float bits)
```

**Pros:** No new stream types, backwards compatible
**Cons:** Always present (8 bytes overhead even when unused)

**Option B - Metadata Stream Type (Clean):**

```c
// STREAM_METADATA (type 22)
// length = metadata_type
// channels = associated_stream_type (0=IQ, 1=RX_AUDIO, etc.)
// reserved[8-11] = metadata values

#define META_TIMESTAMP      1  // GPS timestamp
#define META_SIGNAL_LEVEL   2  // RSSI in dBFS
#define META_FREQUENCY_ERROR 3  // PPM error
#define META_BUFFER_STATS   4  // Over/underruns
```

**Pros:** Clean separation, optional, extensible
**Cons:** More frames to process

**Recommendation:** Option B (STREAM_METADATA type 22)

---

### 3. Sample Rate Negotiation

**Current:** Backend dictates rate, sidecars accept whatever they receive

**Issue:** Some SDRs support limited rates, need negotiation

**Example:**
- KiwiSDR: 12000 Hz fixed
- RSPduo: 2-10 MHz range
- FlexRadio: Multiple fixed rates

#### Proposal: Capability Announcement

```c
// STREAM_CAPABILITY (type 23) - Sent once on connection
// length = capability_type
// channels = value_count
// Payload: array of uint32 values

#define CAP_SUPPORTED_RATES     1  // List of Hz values
#define CAP_SUPPORTED_FORMATS   2  // Bitmask of formats
#define CAP_MAX_CHANNELS        3  // Max simultaneous receivers
#define CAP_FEATURES            4  // Feature bitmask
```

**Recommendation:** Add for complex SDRs (FlexRadio, SDRPlay)

---

### 4. Multiple Simultaneous IQ Streams

**Current:** Single IQ stream per sidecar connection

**Issue:** Multi-receiver SDRs (RSPduo, FlexRadio, Perseus) can have 2-8 parallel receivers

**Solution:** Already supported via `receiver` field!
- receiver=0 → VFO A
- receiver=1 → VFO B
- receiver=2-7 → Additional receivers

**Recommendation:** Document this explicitly, no code change needed ✅

---

### 5. Waterfall / Spectrum Data

**Current:** No spectrum stream type

**Needed For:**
- KiwiSDR has built-in waterfall
- FlexRadio provides spectrum
- Many SDRs have panadapter

#### Proposal: Spectrum Stream Type

```c
// STREAM_SPECTRUM (type 24)
// receiver = receiver index
// sample_rate = FFT rate (updates/sec)
// format = FLOAT32 (dBFS values)
// length = FFT bin count
// channels = 1
// reserved[8] = center_frequency_lo
// reserved[9] = center_frequency_hi
// reserved[10] = span_hz
// Payload: float32 array of dBFS values
```

**Recommendation:** Add, very useful for panadapter applications

---

### 6. Radio-Specific Extensions

**Issue:** FlexRadio, ICOM have vendor-specific features

#### Proposal: Vendor Extension Range

```c
// Stream types 1000-1999: Vendor extensions
// Format: 1000 + vendor_id

#define SIDECAR_VENDOR_FLEXRADIO  1000
#define SIDECAR_VENDOR_ICOM       1001
#define SIDECAR_VENDOR_KIWISDR    1002
#define SIDECAR_VENDOR_SDRPLAY    1003
// etc.

// Sidecars MUST ignore vendor extensions they don't recognize
```

**Recommendation:** Reserve range, document policy

---

## Recommended Changes

### Priority 1: Critical for Multiple Backends

Add these stream types and documentation:

```c
/* Extended TX pacing */
#define SIDECAR_STREAM_TX_BUFFER_LEVEL 20  /* Buffer level pacing */
#define SIDECAR_STREAM_TX_SCHEDULED    21  /* Timestamp-based TX */

/* Metadata */
#define SIDECAR_STREAM_METADATA        22  /* Optional metadata */

/* Capabilities */
#define SIDECAR_STREAM_CAPABILITY      23  /* Backend capabilities */

/* Spectrum data */
#define SIDECAR_STREAM_SPECTRUM        24  /* FFT / panadapter */

/* Reserved ranges */
#define SIDECAR_STREAM_RESERVED_START  100   /* Reserved for future */
#define SIDECAR_STREAM_RESERVED_END    999
#define SIDECAR_STREAM_VENDOR_START    1000  /* Vendor extensions */
#define SIDECAR_STREAM_VENDOR_END      1999
```

### Priority 2: API Functions

Add convenience functions for new stream types:

```c
/* Metadata emission */
int sidecar_emit_timestamp(int fd, uint32_t receiver, 
                           uint64_t timestamp_ns);
int sidecar_emit_signal_level(int fd, uint32_t receiver, 
                              float level_dbfs);

/* Spectrum streaming */
int sidecar_send_spectrum(int fd, uint32_t receiver, 
                          freq_t center_freq, uint32_t span_hz,
                          const float *fft_bins, size_t bin_count,
                          float update_rate);

/* Capability announcement */
int sidecar_send_capabilities(int fd, uint32_t receiver,
                              const uint32_t *supported_rates,
                              size_t rate_count,
                              uint32_t format_mask);
```

### Priority 3: Documentation Updates

1. **docs/sidecar-api.md**
   - Add "Backend-Specific Considerations" section
   - Document TX pacing models
   - Add metadata usage examples

2. **include/hamlib/sidecar.h**
   - Add new stream type constants
   - Document reserved ranges
   - Add vendor extension policy

3. **docs/SIDECAR-BACKEND-GUIDE.md** (NEW)
   - Per-backend integration notes:
     - KiwiSDR: 12kHz fixed rate, optional IQ
     - FlexRadio: VITA-49 timestamps, buffer pacing
     - SDRPlay: Rate negotiation required
     - ICOM: Audio-only, no IQ (unless extended)

---

## Backwards Compatibility Impact

### Existing Sidecars (Python implementations)

**Impact:** NONE - New stream types are OPTIONAL
- Old sidecars ignore unknown types (required by spec)
- Existing types 0-19 unchanged
- No breaking changes

### Existing Backends (tci2.c)

**Impact:** NONE - Can optionally adopt new types
- TCI continues using TX_CHRONO (type 3)
- New backends use appropriate pacing for their hardware
- Both coexist

### ABI Stability

**Impact:** NONE - No header format changes
- 64-byte header unchanged
- New functions added (no signature changes)
- Library version bump: 5.1.0

---

## Implementation Plan

### Phase 1: Define Constants (NOW)

Add to `include/hamlib/sidecar.h`:
- Stream types 20-24
- Reserved ranges (100-999, 1000-1999)
- Vendor extension policy

**Time:** 30 minutes

### Phase 2: Document Extensions (NOW)

Update `docs/sidecar-api.md`:
- TX pacing alternatives
- Metadata usage
- Spectrum streaming
- Backend-specific notes

**Time:** 1 hour

### Phase 3: Implement Helper Functions (LATER)

Add to `src/sidecar.c`:
- Metadata emission functions
- Spectrum streaming function
- Capability announcement function

**Time:** 2-3 hours

### Phase 4: Backend Integration (AS NEEDED)

Per-backend as they're developed:
- KiwiSDR backend: Add metadata support
- FlexRadio backend: Add buffer pacing
- SDRPlay backend: Add capability negotiation

**Time:** Varies per backend

---

## Specific Backend Requirements

### KiwiSDR

**Protocol:** WebSocket (JSON + binary)
**Sample Rate:** 12000 Hz fixed
**Format:** int16
**Special:** GPS timestamps available

**Sidecar API Usage:**
```c
// Fixed 12kHz rate, always report timestamp
sidecar_send_rx_audio(fd, 0, 12000, SIDECAR_FMT_INT16, 1, audio, 512);
sidecar_emit_timestamp(fd, 0, gps_timestamp_ns);  // NEW
```

**Needs:** STREAM_METADATA (type 22) for GPS

---

### FlexRadio SmartSDR

**Protocol:** VITA-49 over TCP/UDP
**Sample Rate:** Multiple (24/48/96/192 kHz)
**Format:** float32 (IQ), int16 (audio)
**Special:** TX buffer level pacing, spectrum data

**Sidecar API Usage:**
```c
// Announce capabilities on connect
uint32_t rates[] = {24000, 48000, 96000, 192000};
sidecar_send_capabilities(fd, 0, rates, 4, 0x08);  // NEW, FLOAT32 only

// Stream IQ with timestamps
sidecar_send_rx_iq(fd, 0, 48000, SIDECAR_FMT_FLOAT32, iq, 1024);
sidecar_emit_timestamp(fd, 0, vita49_timestamp);  // NEW

// Stream spectrum
sidecar_send_spectrum(fd, 0, freq, 100000, fft_bins, 1024, 20);  // NEW

// TX buffer pacing (instead of CHRONO)
// Backend monitors buffer, sends STREAM_TX_BUFFER_LEVEL frames
```

**Needs:** Types 20, 22, 23, 24

---

### SDRPlay RSPduo

**Protocol:** Native API (SoapySDR wrapper)
**Sample Rate:** 2-10 MHz (continuous range)
**Format:** int16 or float32
**Special:** Dual tuner (2 independent receivers)

**Sidecar API Usage:**
```c
// Use receiver field for dual tuner
sidecar_send_rx_iq(fd, 0, 8000000, SIDECAR_FMT_INT16, iq_a, 8192);  // Tuner A
sidecar_send_rx_iq(fd, 1, 8000000, SIDECAR_FMT_INT16, iq_b, 8192);  // Tuner B

// Announce rate range
// (Future: capability negotiation)
```

**Needs:** Type 23 (capability) for rate negotiation

---

### ICOM IC-7300 / IC-705

**Protocol:** CI-V over USB
**Sample Rate:** 48/96/192 kHz (depends on mode)
**Format:** int16 audio (IQ not available via CI-V)
**Special:** Native radio, not true SDR

**Sidecar API Usage:**
```c
// Audio only (no IQ)
sidecar_send_rx_audio(fd, 0, 48000, SIDECAR_FMT_INT16, 1, audio, 512);

// Standard control frames work
sidecar_emit_mode(fd, 0, mode, width);
sidecar_emit_agc_level(fd, 0, agc);
```

**Needs:** Nothing new - existing API sufficient ✅

---

## Decision Matrix

| Feature | Priority | Breaking? | When? |
|---------|----------|-----------|-------|
| Stream types 20-24 | HIGH | No | NOW |
| Reserved ranges | HIGH | No | NOW |
| Documentation | HIGH | No | NOW |
| Helper functions | MEDIUM | No | Later |
| Backend examples | MEDIUM | No | As needed |

---

## Recommendation

**YES - Make changes NOW before first release:**

1. ✅ Add stream type constants 20-24
2. ✅ Document reserved ranges (100-999, 1000-1999)
3. ✅ Update documentation with extension guidelines
4. ⏳ Add helper functions (can be done incrementally)

**Why now:**
- Costs nothing (just constants + docs)
- Prevents future protocol fragmentation
- Makes API immediately useful for non-TCI backends
- Shows forward-thinking design

**Why not break anything:**
- Sidecars already MUST ignore unknown types
- New types are optional
- Existing 0-19 unchanged
- Pure additions, no deletions

---

## Conclusion

**Current API is 90% there!** Just needs:
1. Additional stream type constants (5 types)
2. Reserved range documentation
3. Backend-specific integration notes

All changes are **non-breaking additions**. Existing TCI implementation unaffected.

**Implement NOW** before any backends depend on current range.
