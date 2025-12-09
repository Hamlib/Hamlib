#!/bin/bash
#
# FTX-1 CW/Keyer Command Tests
# Tests: KS (Key Speed), SD (Break-in Delay), SBKIN, FBKIN, BI, KP, KM, KY, LM
#
# Source this file from ftx1_test.sh main harness

test_KS() {
    echo "Testing KS (Key Speed)..."
    local orig=$(run_cmd l KEYSPD)
    if [ -z "$orig" ]; then
        log_fail "KS: read failed"
        return
    fi

    # Toggle speed (4-60 WPM)
    local test_val="20"
    if [ "$orig" = "20" ]; then
        test_val="25"
    fi

    run_cmd L KEYSPD $test_val
    local after=$(run_cmd l KEYSPD)
    if [ "$after" = "$test_val" ]; then
        run_cmd L KEYSPD $orig
        log_pass "KS (Key Speed)"
    else
        log_fail "KS: set failed, expected $test_val got $after"
        run_cmd L KEYSPD $orig
    fi
}

test_SD() {
    echo "Testing SD (Break-in Delay)..."
    local orig=$(run_cmd l BKINDL)
    if [ -z "$orig" ]; then
        log_fail "SD: read failed"
        return
    fi

    # Toggle delay (0-30 in 100ms units)
    local test_val="10"
    if [ "$orig" = "10" ]; then
        test_val="5"
    fi

    run_cmd L BKINDL $test_val
    local after=$(run_cmd l BKINDL)
    if [ "$after" = "$test_val" ]; then
        run_cmd L BKINDL $orig
        log_pass "SD (Break-in Delay)"
    else
        log_fail "SD: set failed, expected $test_val got $after"
        run_cmd L BKINDL $orig
    fi
}

test_SBKIN() {
    echo "Testing SBKIN (Semi Break-in)..."
    local orig=$(run_cmd u SBKIN)
    if [ -z "$orig" ]; then
        log_fail "SBKIN: read failed"
        return
    fi

    local test_val=$((1 - orig))
    run_cmd U SBKIN $test_val
    local after=$(run_cmd u SBKIN)
    if [ "$after" = "$test_val" ]; then
        run_cmd U SBKIN $orig
        log_pass "SBKIN (Semi Break-in)"
    else
        log_fail "SBKIN: set failed, expected $test_val got $after"
        run_cmd U SBKIN $orig
    fi
}

test_FBKIN() {
    echo "Testing FBKIN (Full Break-in)..."
    local orig=$(run_cmd u FBKIN)
    if [ -z "$orig" ]; then
        log_fail "FBKIN: read failed"
        return
    fi

    local test_val=$((1 - orig))
    run_cmd U FBKIN $test_val
    local after=$(run_cmd u FBKIN)
    if [ "$after" = "$test_val" ]; then
        run_cmd U FBKIN $orig
        log_pass "FBKIN (Full Break-in)"
    else
        log_fail "FBKIN: set failed, expected $test_val got $after"
        run_cmd U FBKIN $orig
    fi
}

test_BI() {
    echo "Testing BI (Break-in)..."
    local orig=$(raw_cmd "BI")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "BI (Break-in) - command not available"
        return
    fi

    local orig_val="${orig: -1}"
    local test_val=$((1 - orig_val))
    raw_cmd "BI$test_val"
    local after=$(raw_cmd "BI")
    local after_val="${after: -1}"
    if [ "$after_val" = "$test_val" ]; then
        raw_cmd "BI$orig_val"
        log_pass "BI (Break-in)"
    else
        log_fail "BI: set failed, expected $test_val got $after_val"
        raw_cmd "BI$orig_val"
    fi
}

test_KP() {
    echo "Testing KP (Keyer Pitch/Paddle Ratio)..."
    local orig=$(raw_cmd "KP")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "KP (Keyer Pitch) - command not available"
        return
    fi

    local orig_val="${orig: -2}"
    local test_val="20"
    if [ "$orig_val" = "20" ]; then
        test_val="10"
    fi

    raw_cmd "KP$test_val"
    local after=$(raw_cmd "KP")
    local after_val="${after: -2}"
    if [ "$after_val" = "$test_val" ]; then
        raw_cmd "KP$orig_val"
        log_pass "KP (Keyer Pitch)"
    else
        log_fail "KP: set failed, expected $test_val got $after_val"
        raw_cmd "KP$orig_val"
    fi
}

test_KM() {
    echo "Testing KM (Keyer Memory)..."
    # KM: Keyer Memory - Read/Write CW messages to slots 1-5 (up to 50 chars)
    # Verified working 2025-12-09 - both read and write work

    # First read slot 3 to get original value (less likely to be in use)
    local orig=$(raw_cmd "KM3")
    if [ "$orig" = "?" ]; then
        log_skip "KM (Keyer Memory) - firmware returns '?'"
        return
    fi

    # Extract original message (after "KM3" prefix, or empty if just "KM")
    local orig_msg=""
    if [ "${#orig}" -gt 3 ]; then
        orig_msg="${orig:3}"
    fi

    # Write test message to slot 3
    local test_msg="TEST KM"
    raw_cmd "KM3$test_msg"

    # Read back and verify
    local after=$(raw_cmd "KM3")
    if [[ "$after" == "KM3$test_msg"* ]]; then
        # Restore original (or clear if was empty)
        if [ -n "$orig_msg" ]; then
            raw_cmd "KM3$orig_msg"
        else
            raw_cmd "KM3"  # Clear slot
        fi
        log_pass "KM (Keyer Memory) - read/write verified"
    else
        log_fail "KM: set/read mismatch, expected 'KM3$test_msg', got '$after'"
        # Still try to restore
        if [ -n "$orig_msg" ]; then
            raw_cmd "KM3$orig_msg"
        fi
    fi
}

test_KY() {
    echo "Testing KY (Keyer Send)..."
    # WARNING: This causes CW transmission!
    if [ "$TX_TESTS" -eq 0 ]; then
        log_skip "KY (Keyer Send) - TX tests disabled (causes CW transmission)"
        return
    fi
    local resp=$(raw_cmd "KY00")
    if [ "$resp" = "?" ]; then
        log_fail "KY: command returned error"
    else
        log_pass "KY (Keyer Send) - message sent"
    fi
}

test_LM() {
    echo "Testing LM (Load Message)..."
    local resp=$(raw_cmd "LM00")
    if [ "$resp" = "?" ]; then
        log_skip "LM (Load Message) - firmware returns '?'"
    else
        log_pass "LM (Load Message) - set-only command accepted"
    fi
}

run_cw_tests() {
    echo "=== CW/Keyer Tests ==="
    test_KS
    test_SD
    test_SBKIN
    test_FBKIN
    test_BI
    test_KP
    test_KM
    test_KY
    test_LM
    echo ""
}
