#!/bin/bash
#
# FTX-1 TX Command Tests
# Tests: PTT, VOX, TUNER, TX, MX, AC
# WARNING: These tests can cause transmission!
#
# Source this file from ftx1_test.sh main harness

test_PTT() {
    if [ "$TX_TESTS" -eq 0 ]; then
        return
    fi
    echo "Testing PTT..."

    local orig=$(run_cmd t)
    if [ -z "$orig" ]; then
        log_fail "PTT: read failed"
        return
    fi

    # Only test turning PTT OFF (safe)
    run_cmd T 0
    local after=$(run_cmd t)
    if [ "$after" = "0" ]; then
        log_pass "PTT (tested OFF only for safety)"
    else
        log_fail "PTT: set to 0 failed, got $after"
    fi
}

test_VOX() {
    if [ "$TX_TESTS" -eq 0 ]; then
        return
    fi
    echo "Testing VOX..."
    local orig=$(run_cmd u VOX)
    if [ -z "$orig" ]; then
        log_fail "VOX: read failed"
        return
    fi

    local test_val=$((1 - orig))
    run_cmd U VOX $test_val
    local after=$(run_cmd u VOX)
    if [ "$after" = "$test_val" ]; then
        run_cmd U VOX $orig
        log_pass "VOX"
    else
        log_fail "VOX: set failed, expected $test_val got $after"
        run_cmd U VOX $orig
    fi
}

test_TUNER() {
    if [ "$TX_TESTS" -eq 0 ]; then
        return
    fi
    echo "Testing TUNER..."
    local orig=$(run_cmd u TUNER)
    if [ -z "$orig" ]; then
        log_fail "TUNER: read failed"
        return
    fi

    local test_val=$((1 - orig))
    run_cmd U TUNER $test_val
    local after=$(run_cmd u TUNER)
    if [ "$after" = "$test_val" ]; then
        run_cmd U TUNER $orig
        log_pass "TUNER"
    else
        log_fail "TUNER: set failed, expected $test_val got $after"
        run_cmd U TUNER $orig
    fi
}

test_TX_cmd() {
    echo "Testing TX (PTT via TX command)..."
    if [ "$TX_TESTS" -eq 0 ]; then
        log_skip "TX (PTT command) - TX tests disabled"
        return
    fi
    local orig=$(raw_cmd "TX")
    if [[ ! "$orig" =~ ^TX[012]$ ]]; then
        log_fail "TX: read failed, got: $orig"
        return
    fi
    # Only test TX0 (RX mode) - safe
    raw_cmd "TX0"
    local after=$(raw_cmd "TX")
    if [ "$after" = "TX0" ]; then
        log_pass "TX (PTT command) - set/read TX0 (RX mode)"
    else
        log_fail "TX: set/read mismatch, expected TX0, got: $after"
    fi
}

test_VX() {
    echo "Testing VX (VOX Enable)..."
    if [ "$TX_TESTS" -eq 0 ]; then
        log_skip "VX (VOX Enable) - TX tests disabled"
        return
    fi
    local orig=$(raw_cmd "VX")
    if [[ ! "$orig" =~ ^VX[01]$ ]]; then
        log_fail "VX: read failed, got: $orig"
        return
    fi
    local test_val="0"
    [ "${orig: -1}" = "0" ] && test_val="1"
    raw_cmd "VX$test_val"
    local after=$(raw_cmd "VX")
    if [ "$after" = "VX$test_val" ]; then
        raw_cmd "VX${orig: -1}"
        log_pass "VX (VOX Enable) - set/read/restore"
    else
        log_fail "VX: set mismatch, expected VX$test_val, got: $after"
    fi
}

test_MX_cmd() {
    echo "Testing MX (MOX)..."
    if [ "$TX_TESTS" -eq 0 ]; then
        log_skip "MX (MOX) - TX tests disabled"
        return
    fi
    local orig=$(raw_cmd "MX")
    if [[ ! "$orig" =~ ^MX[01]$ ]]; then
        log_fail "MX: read failed, got: $orig"
        return
    fi
    # Only test MX0 (off) - safe
    raw_cmd "MX0"
    local after=$(raw_cmd "MX")
    if [ "$after" = "MX0" ]; then
        log_pass "MX (MOX) - verified MOX off"
    else
        log_fail "MX: expected MX0, got: $after"
    fi
}

test_AC() {
    echo "Testing AC (Tuner)..."
    if [ "$OPTIMA_ENABLED" -eq 0 ]; then
        log_skip "AC (Tuner) - Optima tests disabled (use --optima to enable for SPA-1 configurations)"
        return
    fi
    if [ "$TX_TESTS" -eq 0 ]; then
        log_skip "AC (Tuner) - TX tests disabled (tuner start causes transmission)"
        return
    fi
    local orig=$(raw_cmd "AC0")
    if [ "$orig" = "?" ]; then
        log_skip "AC (Tuner) - not available (requires Optima/SPA-1 configuration)"
        return
    fi
    if [[ ! "$orig" =~ ^AC0[0-2]{2}$ ]]; then
        log_fail "AC: read failed, got: $orig"
        return
    fi
    # Only test on/off (00/01), NOT start (02)
    local test_val="00"
    [ "${orig: -2}" = "00" ] && test_val="01"
    raw_cmd "AC0$test_val"
    local after=$(raw_cmd "AC0")
    if [ "$after" = "AC0$test_val" ]; then
        raw_cmd "AC0${orig: -2}"
        log_pass "AC (Tuner) - set/read/restore"
    else
        log_fail "AC: set mismatch, expected AC0$test_val, got: $after"
    fi
}

test_TS() {
    echo "Testing TS (TX Watch)..."
    # TS: TX Watch (monitor SUB VFO during TX)
    # Format: TS P1 where P1: 0=Off, 1=On
    # When enabled, allows monitoring SUB band during transmission

    local orig=$(raw_cmd "TS")
    if [ "$orig" = "?" ] || [ -z "$orig" ]; then
        log_skip "TS (TX Watch) - command not available"
        return
    fi

    if [[ ! "$orig" =~ ^TS[01]$ ]]; then
        log_fail "TS: invalid read format '$orig'"
        return
    fi

    # Toggle TX Watch
    local test_val="1"
    [ "${orig:2:1}" = "1" ] && test_val="0"

    raw_cmd "TS$test_val"
    local after=$(raw_cmd "TS")
    if [ "$after" != "TS$test_val" ]; then
        log_fail "TS: set failed, expected TS$test_val got $after"
        raw_cmd "TS${orig:2}"
        return
    fi

    # Restore original
    raw_cmd "TS${orig:2}"
    local restored=$(raw_cmd "TS")
    if [ "$restored" = "$orig" ]; then
        log_pass "TS (TX Watch) - set/read/restore verified"
    else
        log_fail "TS: restore failed, expected $orig got $restored"
    fi
}

run_tx_tests() {
    echo "=== TX Tests ==="
    test_PTT
    test_VOX
    test_TUNER
    test_TX_cmd
    test_VX
    test_MX_cmd
    test_AC
    test_TS
    echo ""
}
