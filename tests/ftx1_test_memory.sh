#!/bin/bash
#
# FTX-1 Memory Command Tests
# Tests: AM (VFO-A to Memory), BM (VFO-B to Memory), CH (Channel Up/Down),
#        MA (Memory to VFO-A), MB (Memory to VFO-B), MC (Memory Channel Select),
#        MR (Memory Read), MT (Memory Tag), MW (Memory Write), MZ (Memory Zone),
#        VM (VFO/Memory Mode)
#
# Source this file from ftx1_test.sh main harness
#
# IMPORTANT: MR, MT, MZ use 5-digit format (MR00001 not MR0001) - verified 2025-12-09
# Returns '?' for empty/unprogrammed channels

test_MR() {
    echo "Testing MR (Memory Read)..."
    # 5-digit format: MR P1 P2P2P2P2 (bank + 4-digit channel)
    # Response: MR00001FFFFFFFFF+OOOOOPPMMxxxx
    local resp=$(raw_cmd "MR00001")
    if [ -z "$resp" ] || [ "$resp" = "?" ]; then
        log_skip "MR (Memory Read) - channel 1 not programmed or command unavailable"
        return
    fi
    if [ "${#resp}" -ge 20 ] && [[ "$resp" =~ ^MR00001 ]]; then
        log_pass "MR (Memory Read) - 5-digit format verified, length: ${#resp}"
    else
        log_fail "MR: invalid format '$resp'"
    fi
}

test_MT() {
    echo "Testing MT (Memory Tag)..."
    # 5-digit format: MT P1 P2P2P2P2 [NAME]
    # Response: MT00001[12-char name, space padded]
    # MT is full read/write! Verified 2025-12-09
    local resp=$(raw_cmd "MT00001")
    if [ -z "$resp" ] || [ "$resp" = "?" ]; then
        log_skip "MT (Memory Tag) - channel 1 not programmed or command unavailable"
        return
    fi

    if [[ ! "$resp" =~ ^MT00001 ]] || [ "${#resp}" -lt 17 ]; then
        log_fail "MT: invalid read format '$resp'"
        return
    fi

    # Save original name
    local orig_name="${resp:7}"

    # Test set (12 chars)
    raw_cmd "MT00001HAMLIBTEST  "
    local after=$(raw_cmd "MT00001")
    if [[ "${after:7:12}" == "HAMLIBTEST  " ]]; then
        # Restore original
        raw_cmd "MT00001$orig_name"
        log_pass "MT (Memory Tag) - full R/W verified"
    else
        log_fail "MT: set failed, got '${after:7}'"
        raw_cmd "MT00001$orig_name"
    fi
}

test_MZ() {
    echo "Testing MZ (Memory Zone)..."
    # 5-digit format: MZ P1 P2P2P2P2 [DATA]
    # Response: MZ00001[10-digit zone data]
    # MZ is full read/write! Verified 2025-12-09
    local resp=$(raw_cmd "MZ00001")
    if [ -z "$resp" ] || [ "$resp" = "?" ]; then
        log_skip "MZ (Memory Zone) - channel 1 not programmed or command unavailable"
        return
    fi

    if [[ ! "$resp" =~ ^MZ00001 ]] || [ "${#resp}" -lt 17 ]; then
        log_fail "MZ: invalid read format '$resp'"
        return
    fi

    # Save original data
    local orig_data="${resp:7}"

    # Test set (10 digits)
    raw_cmd "MZ000011111111111"
    local after=$(raw_cmd "MZ00001")
    if [[ "${after:7:10}" == "1111111111" ]]; then
        # Restore original
        raw_cmd "MZ00001$orig_data"
        log_pass "MZ (Memory Zone) - full R/W verified"
    else
        log_fail "MZ: set failed, got '${after:7}'"
        raw_cmd "MZ00001$orig_data"
    fi
}

test_VM() {
    echo "Testing VM (VFO/Memory mode)..."
    # VM P1 where P1=0/1 for MAIN/SUB
    # Response: VM0PP where PP = mode code
    # Mode codes DIFFER FROM SPEC: 00=VFO, 11=Memory (not 01!)
    # Set: Only VM000 (VFO mode) works; use SV to toggle to memory
    # Verified 2025-12-09

    local resp=$(raw_cmd "VM0")
    if [ -z "$resp" ] || [ "$resp" = "?" ]; then
        log_fail "VM: read failed"
        return
    fi

    if [[ ! "$resp" =~ ^VM0[0-9]{2}$ ]]; then
        log_fail "VM: invalid read format '$resp'"
        return
    fi

    # Check SUB VFO too
    local resp1=$(raw_cmd "VM1")
    if [[ ! "$resp1" =~ ^VM1[0-9]{2}$ ]]; then
        log_fail "VM: invalid SUB read format '$resp1'"
        return
    fi

    # Test setting to VFO mode (only working set)
    raw_cmd "VM000"
    local after=$(raw_cmd "VM0")
    if [ "$after" != "VM000" ]; then
        log_fail "VM: set to VM000 failed, got $after"
        return
    fi

    # Test SV toggle to memory mode
    raw_cmd "SV"
    sleep 0.3
    local after_sv=$(raw_cmd "VM0")
    if [ "$after_sv" != "VM011" ]; then
        log_fail "VM: SV should toggle to memory mode (VM011), got $after_sv"
        raw_cmd "SV"  # Try to restore
        return
    fi

    # Toggle back to VFO mode
    raw_cmd "SV"
    sleep 0.3
    local restored=$(raw_cmd "VM0")
    if [ "$restored" = "VM000" ]; then
        log_pass "VM (VFO/Memory) - mode 11=Memory (not 01), SV toggle verified"
    else
        log_fail "VM: SV should toggle back to VFO, got $restored"
    fi
}

test_MC() {
    echo "Testing MC (Memory Channel Select)..."
    # MC format (different from documented spec!):
    # Read: MC0 (MAIN) or MC1 (SUB) returns MCNNNNNN (6-digit channel)
    # Set: MCNNNNNN (6-digit channel, no VFO prefix)
    # Returns '?' if channel doesn't exist
    # Verified 2025-12-09

    # Read MAIN memory channel
    local resp=$(raw_cmd "MC0")
    if [ -z "$resp" ] || [ "$resp" = "?" ]; then
        log_fail "MC: read failed"
        return
    fi

    if [[ ! "$resp" =~ ^MC[0-9]{6}$ ]]; then
        log_fail "MC: invalid read format '$resp'"
        return
    fi

    local orig_channel="${resp:2}"  # Save 6-digit channel for restore

    # Read SUB memory channel
    local resp1=$(raw_cmd "MC1")
    if [[ ! "$resp1" =~ ^MC[0-9]{6}$ ]]; then
        log_fail "MC: invalid SUB read format '$resp1'"
        return
    fi

    # Test set to channel 1 (most likely to exist)
    local set_resp=$(raw_cmd "MC000001")
    if [ "$set_resp" = "?" ]; then
        log_skip "MC set - channel 1 doesn't exist"
        return
    fi

    # Verify
    local after=$(raw_cmd "MC0")
    if [ "$after" = "MC000001" ]; then
        # Restore original
        raw_cmd "MC$orig_channel"
        log_pass "MC (Memory Channel) - read/set verified (6-digit format)"
    else
        log_fail "MC: set failed, expected MC000001 got $after"
        raw_cmd "MC$orig_channel"
    fi
}

test_AM() {
    echo "Testing AM (VFO-A to Memory)..."
    local resp=$(raw_cmd "AM")
    if [ "$resp" = "?" ]; then
        log_fail "AM: command returned error"
    else
        log_pass "AM (VFO-A to Memory) - set-only command accepted"
    fi
}

test_BM() {
    echo "Testing BM (VFO-B to Memory)..."
    local resp=$(raw_cmd "BM")
    if [ "$resp" = "?" ]; then
        log_fail "BM: command returned error"
    else
        log_pass "BM (VFO-B to Memory) - set-only command accepted"
    fi
}

test_MA() {
    echo "Testing MA (Memory to VFO-A)..."
    log_skip "MA (Memory to VFO-A) - destructive operation, skipped for safety"
}

test_MB() {
    echo "Testing MB (Memory to VFO-B)..."
    log_skip "MB (Memory to VFO-B) - destructive operation, skipped for safety"
}

test_MW() {
    echo "Testing MW (Memory Write)..."
    # MW: Memory Write (29-byte format) - uses high channel to minimize risk
    # Format: MW P1P1P1P1P1 P2P2P2P2P2P2P2P2P2 P3P3P3P3P3 P4 P5 P6 P7 P8 P9P9 P10;
    # - P1: 5-digit channel (00001-00500)
    # - P2: 9-digit frequency in Hz
    # - P3: 5-digit clarifier offset with sign (+/-0000 to +/-9999)
    # - P4: Clarifier RX (0/1), P5: Clarifier TX (0/1)
    # - P6: Mode (1-C/2-LSB/3-USB/4-CW-U/5-FM/6-WFM/7-CW-L/8-DATA-L/9-DATA-FM/A-FM-N/B-DATA-U/C-AM-N/D-C4FM)
    # - P7: VFO/Mem mode (0=VFO, 1=Memory), P8: CTCSS state (0=off, 1=ENC, 2=TSQ)
    # - P9: 2-digit reserved (00), P10: Shift (0=simplex, 1=+, 2=-)
    # Verified working 2025-12-09

    local test_channel="00500"  # High channel, less likely in use

    # Read existing channel data (if any)
    local orig=$(raw_cmd "MR$test_channel")
    local had_original=0
    if [[ "$orig" =~ ^MR$test_channel ]] && [ "$orig" != "?" ]; then
        had_original=1
    fi

    # Write test data: 14.200 MHz USB, no clarifier, no CTCSS, simplex
    raw_cmd "MW${test_channel}014200000+0000000310000"
    sleep 0.2

    # Read back and verify
    local after=$(raw_cmd "MR$test_channel")
    if [ "$after" = "?" ]; then
        log_fail "MW: write failed, channel not readable"
        return
    fi

    if [[ "$after" =~ ^MR${test_channel}014200000 ]]; then
        # Restore original if it existed
        if [ "$had_original" -eq 1 ] && [ "${#orig}" -ge 29 ]; then
            local restore_data="${orig:2}"  # Strip 'MR' prefix
            raw_cmd "MW$restore_data"
        fi
        log_pass "MW (Memory Write) - 29-byte format verified"
    else
        log_fail "MW: verify failed, expected freq 014200000, got '$after'"
        # Try to restore
        if [ "$had_original" -eq 1 ]; then
            local restore_data="${orig:2}"
            raw_cmd "MW$restore_data"
        fi
    fi
}

test_MEM_CH() {
    echo "Testing Memory Channel Select..."
    local orig=$(run_cmd e)
    if [ -z "$orig" ]; then
        log_fail "MEM_CH: read failed"
        return
    fi

    local test_val="5"
    if [ "$orig" = "5" ]; then
        test_val="10"
    fi

    run_cmd E $test_val
    local after=$(run_cmd e)
    if [ "$after" = "$test_val" ]; then
        run_cmd E $orig
        log_pass "Memory Channel Select"
    else
        log_fail "MEM_CH: set failed, expected $test_val got $after"
        run_cmd E $orig
    fi
}

test_MEM_raw() {
    echo "Testing Memory via raw MR command (Hamlib workaround)..."
    local resp=$(raw_cmd "MR00001")
    if [ -z "$resp" ] || [ "$resp" = "?" ]; then
        log_skip "MEM_raw - MR command not available"
        return
    fi

    if [[ "$resp" =~ ^MR[0-9]{5} ]]; then
        log_pass "Memory (via MR raw) - read channel 00001 works, length: ${#resp}"
    else
        log_fail "MEM_raw: invalid MR format '$resp'"
    fi
}

test_CH() {
    echo "Testing CH (Memory Channel Up/Down)..."
    # CH format (verified 2025-12-09):
    # CH0 = next memory channel (cycles through ALL channels across groups)
    # CH1 = previous memory channel
    # Cycles: PMG ch1 → ch2 → ... → QMB ch1 → ... → PMG ch1 (wraps)
    # Only CH0 and CH1 work; CH; CH00; CH01; etc. return '?'
    # Display shows "M-ALL X-NN" where X=group, NN=channel

    # Ensure we're on a known channel in PMG
    raw_cmd "MC000001"
    sleep 0.2

    # Get current state
    local orig=$(raw_cmd "MC0")

    # Test CH0 - should advance to next channel
    raw_cmd "CH0"
    sleep 0.2
    local after_ch0=$(raw_cmd "MC0")

    # Test CH1 - should go back to previous channel
    raw_cmd "CH1"
    sleep 0.2
    local after_ch1=$(raw_cmd "MC0")

    # Verify CH0 advanced (channel number increased or wrapped to different group)
    if [ "$after_ch0" = "$orig" ]; then
        log_fail "CH: CH0 should change channel, got $after_ch0"
        raw_cmd "MC${orig:2}"
        return
    fi

    # Verify CH1 returned to original
    if [ "$after_ch1" != "$orig" ]; then
        log_fail "CH: CH1 should return to $orig, got $after_ch1"
        raw_cmd "MC${orig:2}"
        return
    fi

    # Test that invalid formats return '?'
    local resp=$(raw_cmd "CH")
    if [ "$resp" != "?" ]; then
        log_fail "CH: CH; should return '?', got $resp"
        raw_cmd "MC${orig:2}"
        return
    fi

    resp=$(raw_cmd "CH00")
    if [ "$resp" != "?" ]; then
        log_fail "CH: CH00; should return '?', got $resp"
        raw_cmd "MC${orig:2}"
        return
    fi

    # Restore original
    raw_cmd "MC${orig:2}"
    log_pass "CH (Memory Channel) - CH0/CH1 cycle through channels"
}

run_memory_tests() {
    echo "=== Memory Tests ==="
    test_MR
    test_MT
    test_MZ
    test_VM
    test_MC
    test_CH
    test_AM
    test_BM
    test_MA
    test_MB
    test_MW
    test_MEM_CH
    test_MEM_raw
    echo ""
}
