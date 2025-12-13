#!/bin/bash
#
# FTX-1 Mode Command Tests
# Tests: MD (Mode), FN (Filter Number)
#
# Source this file from ftx1_test.sh main harness

test_MD() {
    echo "Testing MD (Mode)..."
    local orig=$(run_cmd m | head -1)
    if [ -z "$orig" ]; then
        log_fail "MD: read failed"
        return
    fi

    # Toggle between USB and LSB
    local test_mode="USB"
    if [ "$orig" = "USB" ]; then
        test_mode="LSB"
    fi

    run_cmd M $test_mode 0
    local after=$(run_cmd m | head -1)
    if [ "$after" = "$test_mode" ]; then
        run_cmd M $orig 0
        local restored=$(run_cmd m | head -1)
        if [ "$restored" = "$orig" ]; then
            log_pass "MD (Mode)"
        else
            log_fail "MD: restore failed"
        fi
    else
        log_fail "MD: set failed, expected $test_mode got $after"
    fi
}

test_FN() {
    echo "Testing FN (Filter Number)..."
    local orig=$(raw_cmd "FN")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "FN (Filter Number) - command not available"
        return
    fi

    local orig_val="${orig: -1}"
    local test_val="1"
    if [ "$orig_val" = "1" ]; then
        test_val="0"
    fi

    raw_cmd "FN$test_val"
    local after=$(raw_cmd "FN")
    local after_val="${after: -1}"
    if [ "$after_val" = "$test_val" ]; then
        raw_cmd "FN$orig_val"
        log_pass "FN (Filter Number)"
    else
        log_fail "FN: set failed, expected $test_val got $after_val"
        raw_cmd "FN$orig_val"
    fi
}

run_mode_tests() {
    echo "=== Mode Tests ==="
    test_MD
    test_FN
    echo ""
}
