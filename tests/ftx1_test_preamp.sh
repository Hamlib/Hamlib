#!/bin/bash
#
# FTX-1 Preamp/Attenuator Command Tests
# Tests: PA (Preamp), RA (Attenuator)
#
# Source this file from ftx1_test.sh main harness

test_PA() {
    echo "Testing PA (Preamp)..."
    local orig=$(run_cmd l PREAMP)
    if [ -z "$orig" ]; then
        log_fail "PA: read failed"
        return
    fi

    # Toggle preamp: 0=IPO, 10=AMP1, 20=AMP2
    local test_val="10"
    if [ "$orig" = "10" ]; then
        test_val="0"
    fi

    run_cmd L PREAMP $test_val
    local after=$(run_cmd l PREAMP)
    if [ "$after" = "$test_val" ]; then
        run_cmd L PREAMP $orig
        log_pass "PA (Preamp)"
    else
        log_fail "PA: set failed, expected $test_val got $after"
        run_cmd L PREAMP $orig
    fi
}

test_RA() {
    echo "Testing RA (Attenuator)..."
    local orig=$(run_cmd l ATT)
    if [ -z "$orig" ]; then
        log_fail "RA: read failed"
        return
    fi

    # Toggle attenuator: 0=OFF, 12=ON (12dB)
    local test_val="12"
    if [ "$orig" = "12" ]; then
        test_val="0"
    fi

    run_cmd L ATT $test_val
    local after=$(run_cmd l ATT)
    if [ "$after" = "$test_val" ]; then
        run_cmd L ATT $orig
        log_pass "RA (Attenuator)"
    else
        log_fail "RA: set failed, expected $test_val got $after"
        run_cmd L ATT $orig
    fi
}

run_preamp_tests() {
    echo "=== Preamp/Attenuator Tests ==="
    test_PA
    test_RA
    echo ""
}
