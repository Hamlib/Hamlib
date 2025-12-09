#!/bin/bash
#
# FTX-1 Miscellaneous Command Tests
# Tests: CN, CT, SC, SF, TS, CS, FR, KR, OS, PB, PL, PR, EO, QI, QR, VM, ZI, DN, UP
#
# Source this file from ftx1_test.sh main harness

test_CN() {
    echo "Testing CN (CTCSS Number)..."
    # CN: CTCSS Number - format CN P1 P2P3P4 (P1=00 TX/10 RX, P2P3P4=001-050)
    # Verified working 2025-12-09 - Query: CN00; returns CN00nnn
    local orig=$(raw_cmd "CN00")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "CN (CTCSS Number) - command not available"
        return
    fi
    if [[ ! "$orig" =~ ^CN00[0-9]{3}$ ]]; then
        log_fail "CN: invalid format '$orig'"
        return
    fi
    local orig_val="${orig: -3}"
    local test_val="013"
    [ "$orig_val" = "013" ] && test_val="012"
    raw_cmd "CN00$test_val"
    local after=$(raw_cmd "CN00")
    if [ "$after" = "CN00$test_val" ]; then
        raw_cmd "CN00$orig_val"
        log_pass "CN (CTCSS Number)"
    else
        log_fail "CN: set failed, expected CN00$test_val got $after"
        raw_cmd "CN00$orig_val"
    fi
}

test_CT() {
    echo "Testing CT (CTCSS Mode)..."
    local orig=$(raw_cmd "CT0")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "CT (CTCSS Mode) - command not available"
        return
    fi

    local orig_val="${orig: -1}"
    local test_val="1"
    if [ "$orig_val" = "1" ]; then
        test_val="0"
    fi

    raw_cmd "CT0$test_val"
    local after=$(raw_cmd "CT0")
    local after_val="${after: -1}"
    if [ "$after_val" = "$test_val" ]; then
        raw_cmd "CT0$orig_val"
        log_pass "CT (CTCSS Mode)"
    else
        log_fail "CT: set failed, expected $test_val got $after_val"
        raw_cmd "CT0$orig_val"
    fi
}

test_SC() {
    echo "Testing SC (Scan)..."
    local orig=$(raw_cmd "SC")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "SC (Scan) - command not available"
        return
    fi

    raw_cmd "SC01"
    sleep 0.3
    raw_cmd "SC00"
    sleep 0.2

    local after=$(raw_cmd "SC")
    if [[ "$after" =~ ^SC ]]; then
        log_pass "SC (Scan)"
    else
        log_fail "SC: invalid response '$after'"
    fi
}

test_SF() {
    echo "Testing SF (Scope Fix)..."
    local orig=$(raw_cmd "SF0")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "SF (Scope Fix) - command not available"
        return
    fi
    if [[ "$orig" =~ ^SF0[0-9A-H]$ ]]; then
        log_pass "SF (Scope Fix) - read-only, value: $orig"
    else
        log_fail "SF: invalid format '$orig'"
    fi
}

test_TS() {
    echo "Testing TS (TX Watch)..."
    local orig=$(raw_cmd "TS")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "TS (TX Watch) - command not available"
        return
    fi
    if [[ "$orig" =~ ^TS[01]$ ]]; then
        log_pass "TS (TX Watch) - read-only, value: $orig"
    else
        log_fail "TS: invalid format '$orig'"
    fi
}

test_CS() {
    echo "Testing CS (CW Spot)..."
    local orig=$(raw_cmd "CS")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "CS (CW Spot) - command not available"
        return
    fi
    if [[ "$orig" =~ ^CS[01]$ ]]; then
        log_pass "CS (CW Spot) - read-only, value: $orig"
    else
        log_fail "CS: invalid format '$orig'"
    fi
}

test_FR() {
    echo "Testing FR (Function RX)..."
    local orig=$(raw_cmd "FR")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "FR (Function RX) - command not available"
        return
    fi
    if [[ "$orig" =~ ^FR[0-9]{2}$ ]]; then
        log_pass "FR (Function RX) - read-only, value: $orig"
    else
        log_fail "FR: invalid format '$orig'"
    fi
}

test_KR() {
    echo "Testing KR (Keyer)..."
    local orig=$(raw_cmd "KR")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "KR (Keyer) - command not available"
        return
    fi
    if [[ "$orig" =~ ^KR[01]$ ]]; then
        log_pass "KR (Keyer) - read-only, value: $orig"
    else
        log_fail "KR: invalid format '$orig'"
    fi
}

test_OS() {
    echo "Testing OS (Offset Shift)..."
    local orig=$(raw_cmd "OS0")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "OS (Offset Shift) - command not available"
        return
    fi
    if [[ "$orig" =~ ^OS0[0-3]$ ]]; then
        log_pass "OS (Offset Shift) - read-only, value: $orig"
    else
        log_fail "OS: invalid format '$orig'"
    fi
}

test_PB() {
    echo "Testing PB (Playback)..."
    local orig=$(raw_cmd "PB0")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "PB (Playback) - command not available"
        return
    fi
    if [[ "$orig" =~ ^PB0[0-9]$ ]]; then
        log_pass "PB (Playback) - read-only, value: $orig"
    else
        log_fail "PB: invalid format '$orig'"
    fi
}

test_PL() {
    echo "Testing PL (Processor Level)..."
    local orig=$(raw_cmd "PL")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "PL (Processor Level) - command not available"
        return
    fi
    if [[ "$orig" =~ ^PL[0-9]{3}$ ]]; then
        log_pass "PL (Processor Level) - read-only, value: $orig"
    else
        log_fail "PL: invalid format '$orig'"
    fi
}

test_PR() {
    echo "Testing PR (Processor)..."
    local orig=$(raw_cmd "PR0")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "PR (Processor) - command not available"
        return
    fi
    if [[ "$orig" =~ ^PR0[012]$ ]]; then
        log_pass "PR (Processor) - read-only, value: $orig"
    else
        log_fail "PR: invalid format '$orig'"
    fi
}

test_EO() {
    echo "Testing EO (Encoder Offset)..."
    # EO: Encoder offset - set-only, format: EO P1 P2 P3 P4 P5 P5 P5;
    # P1=VFO (0=MAIN, 1=SUB), P2=encoder (0=MAIN dial, 1=FUNC knob)
    # P3=direction (+/-), P4=unit (0=Hz, 1=kHz, 2=MHz), P5=value (000-999)
    local resp=$(raw_cmd "EO00+0100")
    if [ "$resp" = "?" ]; then
        log_fail "EO (Encoder Offset) - command returned error"
    else
        log_pass "EO (Encoder Offset) - set-only command accepted"
    fi
}

test_QI() {
    echo "Testing QI (Quick In)..."
    # NOTE: QI command is ACCEPTED but NON-FUNCTIONAL in firmware v1.08+
    # Command is parsed (returns empty) but has no actual effect
    local resp=$(raw_cmd "QI")
    if [ "$resp" = "?" ]; then
        log_fail "QI: command returned error"
    else
        log_pass "QI (Quick In) - set-only command accepted (non-functional in firmware)"
    fi
}

test_QR() {
    echo "Testing QR (Quick Recall)..."
    # NOTE: QR command is ACCEPTED but NON-FUNCTIONAL in firmware v1.08+
    # Command is parsed (returns empty) but has no actual effect
    local resp=$(raw_cmd "QR")
    if [ "$resp" = "?" ]; then
        log_fail "QR: command returned error"
    else
        log_pass "QR (Quick Recall) - set-only command accepted (non-functional in firmware)"
    fi
}

test_VM() {
    echo "Testing VM (Voice Memory)..."
    local resp=$(raw_cmd "VM00")
    if [ "$resp" = "?" ]; then
        log_skip "VM (Voice Memory) - firmware returns '?'"
    else
        log_pass "VM (Voice Memory) - set-only command accepted"
    fi
}

test_ZI() {
    echo "Testing ZI (Zero In)..."
    # ZI only works in CW mode - it's the CW AUTO ZERO IN function
    # Save current mode, switch to CW, test ZI, restore mode
    local orig_mode=$(raw_cmd "MD0")
    if [ -z "$orig_mode" ] || [ "$orig_mode" = "?" ]; then
        log_skip "ZI (Zero In) - cannot read mode"
        return
    fi
    local orig_mode_val="${orig_mode: -1}"

    # Switch to CW-U mode (03)
    raw_cmd "MD03"
    sleep 0.2

    local resp=$(raw_cmd "ZI0")
    if [ "$resp" = "?" ]; then
        log_fail "ZI: command returned error even in CW mode"
    else
        log_pass "ZI (Zero In) - set-only, CW mode only"
    fi

    # Restore original mode
    raw_cmd "MD0$orig_mode_val"
}

test_DN() {
    echo "Testing DN (Down)..."
    log_skip "DN (Down) - context-dependent, manual test required"
}

test_UP() {
    echo "Testing UP (Up)..."
    log_skip "UP (Up) - context-dependent, manual test required"
}

test_SL() {
    echo "Testing SL (Low Cut Filter)..."
    local orig=$(raw_cmd "SL0")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "SL (Low Cut) - command not available"
        return
    fi
    if [[ "$orig" =~ ^SL0[0-9]{3}$ ]]; then
        log_pass "SL (Low Cut Filter) - value: $orig"
    else
        log_fail "SL: invalid format '$orig'"
    fi
}

test_CTCSS() {
    echo "Testing CTCSS Tone..."
    local orig=$(run_cmd c)
    if [ -z "$orig" ]; then
        log_fail "CTCSS: read failed"
        return
    fi

    local test_val="1000"
    if [ "$orig" = "1000" ]; then
        test_val="885"
    fi

    run_cmd C $test_val
    local after=$(run_cmd c)
    if [ "$after" = "$test_val" ]; then
        run_cmd C $orig
        log_pass "CTCSS Tone"
    else
        log_fail "CTCSS: set failed, expected $test_val got $after"
        run_cmd C $orig
    fi
}

test_DCS() {
    echo "Testing DCS Code..."
    local orig=$(run_cmd d)
    if [ -z "$orig" ]; then
        log_fail "DCS: read failed"
        return
    fi

    local test_val="23"
    if [ "$orig" = "23" ]; then
        test_val="25"
    fi

    run_cmd D $test_val
    local after=$(run_cmd d)
    if [ "$after" = "$test_val" ]; then
        run_cmd D $orig
        log_pass "DCS Code"
    else
        log_fail "DCS: set failed, expected $test_val got $after"
        run_cmd D $orig
    fi
}

run_misc_tests() {
    echo "=== Miscellaneous Tests ==="
    test_CN
    test_CT
    test_SC
    test_SF
    test_TS
    test_CS
    test_FR
    test_KR
    test_OS
    test_PB
    test_PL
    test_PR
    test_EO
    test_QI
    test_QR
    test_VM
    test_ZI
    test_DN
    test_UP
    test_SL
    test_CTCSS
    test_DCS
    echo ""
}
