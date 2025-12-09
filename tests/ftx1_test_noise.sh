#!/bin/bash
#
# FTX-1 Noise Command Tests
# Tests: NB (Noise Blanker), NR (Noise Reduction), NA (Noise Attenuator),
#        NL (Noise Limiter Level), RL (Noise Reduction Level)
#
# Source this file from ftx1_test.sh main harness

test_NB() {
    echo "Testing NB (Noise Blanker via NL level)..."
    local orig=$(run_cmd u NB)
    if [ -z "$orig" ]; then
        log_fail "NB: read failed"
        return
    fi

    local test_val=$((1 - orig))
    run_cmd U NB $test_val
    local after=$(run_cmd u NB)
    if [ "$after" = "$test_val" ]; then
        run_cmd U NB $orig
        log_pass "NB (Noise Blanker)"
    else
        log_fail "NB: set failed, expected $test_val got $after"
        run_cmd U NB $orig
    fi
}

test_NR() {
    echo "Testing NR (Noise Reduction via RL level)..."
    local orig=$(run_cmd u NR)
    if [ -z "$orig" ]; then
        log_fail "NR: read failed"
        return
    fi

    local test_val=$((1 - orig))
    run_cmd U NR $test_val
    local after=$(run_cmd u NR)
    if [ "$after" = "$test_val" ]; then
        run_cmd U NR $orig
        log_pass "NR (Noise Reduction)"
    else
        log_fail "NR: set failed, expected $test_val got $after"
        run_cmd U NR $orig
    fi
}

test_NA() {
    echo "Testing NA (Noise Attenuator/Narrow)..."
    local orig=$(raw_cmd "NA0")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "NA (Noise Attenuator) - command not available"
        return
    fi

    local orig_val="${orig: -1}"
    local test_val=$((1 - orig_val))

    raw_cmd "NA0$test_val"
    local after=$(raw_cmd "NA0")
    local after_val="${after: -1}"
    if [ "$after_val" = "$test_val" ]; then
        raw_cmd "NA0$orig_val"
        log_pass "NA (Noise Attenuator)"
    else
        log_fail "NA: set failed, expected $test_val got $after_val"
        raw_cmd "NA0$orig_val"
    fi
}

test_NL() {
    echo "Testing NL (Noise Limiter Level)..."
    local orig=$(raw_cmd "NL0")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "NL (Noise Limiter Level) - command not available"
        return
    fi

    local orig_val="${orig: -3}"
    local test_val="005"
    if [ "$orig_val" = "005" ]; then
        test_val="000"
    fi

    raw_cmd "NL0$test_val"
    local after=$(raw_cmd "NL0")
    local after_val="${after: -3}"
    if [ "$after_val" = "$test_val" ]; then
        raw_cmd "NL0$orig_val"
        log_pass "NL (Noise Limiter Level)"
    else
        log_fail "NL: set failed, expected $test_val got $after_val"
        raw_cmd "NL0$orig_val"
    fi
}

test_RL() {
    echo "Testing RL (Noise Reduction Level)..."
    local orig=$(raw_cmd "RL0")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "RL (Noise Reduction Level) - command not available"
        return
    fi

    local orig_val="${orig: -2}"
    local test_val="05"
    if [ "$orig_val" = "05" ]; then
        test_val="00"
    fi

    raw_cmd "RL0$test_val"
    local after=$(raw_cmd "RL0")
    local after_val="${after: -2}"
    if [ "$after_val" = "$test_val" ]; then
        raw_cmd "RL0$orig_val"
        log_pass "RL (Noise Reduction Level)"
    else
        log_fail "RL: set failed, expected $test_val got $after_val"
        raw_cmd "RL0$orig_val"
    fi
}

run_noise_tests() {
    echo "=== Noise Tests ==="
    test_NB
    test_NR
    test_NA
    test_NL
    test_RL
    echo ""
}
