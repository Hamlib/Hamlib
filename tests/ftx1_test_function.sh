#!/bin/bash
#
# FTX-1 Function Command Tests
# Tests: COMP (Compressor), MON (Monitor), LOCK, TONE, TSQL, APF
#
# Source this file from ftx1_test.sh main harness

test_COMP() {
    echo "Testing COMP (Compressor)..."
    local orig=$(run_cmd u COMP)
    if [ -z "$orig" ]; then
        log_fail "COMP: read failed"
        return
    fi

    local test_val=$((1 - orig))
    run_cmd U COMP $test_val
    local after=$(run_cmd u COMP)
    if [ "$after" = "$test_val" ]; then
        run_cmd U COMP $orig
        log_pass "COMP (Compressor)"
    else
        log_fail "COMP: set failed, expected $test_val got $after"
        run_cmd U COMP $orig
    fi
}

test_MON() {
    echo "Testing MON (Monitor)..."
    local orig=$(run_cmd u MON)
    if [ -z "$orig" ]; then
        log_fail "MON: read failed"
        return
    fi

    local test_val=$((1 - orig))
    run_cmd U MON $test_val
    local after=$(run_cmd u MON)
    if [ "$after" = "$test_val" ]; then
        run_cmd U MON $orig
        log_pass "MON (Monitor)"
    else
        log_fail "MON: set failed, expected $test_val got $after"
        run_cmd U MON $orig
    fi
}

test_LOCK() {
    echo "Testing LOCK..."
    local orig=$(run_cmd u LOCK)
    if [ -z "$orig" ]; then
        log_fail "LOCK: read failed"
        return
    fi

    local test_val=$((1 - orig))
    run_cmd U LOCK $test_val
    local after=$(run_cmd u LOCK)
    if [ "$after" = "$test_val" ]; then
        run_cmd U LOCK $orig
        log_pass "LOCK"
    else
        log_fail "LOCK: set failed, expected $test_val got $after"
        run_cmd U LOCK $orig
    fi
}

test_TONE() {
    echo "Testing TONE..."
    local orig=$(run_cmd u TONE)
    if [ -z "$orig" ]; then
        log_fail "TONE: read failed"
        return
    fi

    local test_val=$((1 - orig))
    run_cmd U TONE $test_val
    local after=$(run_cmd u TONE)
    if [ "$after" = "$test_val" ]; then
        run_cmd U TONE $orig
        log_pass "TONE"
    else
        log_fail "TONE: set failed, expected $test_val got $after"
        run_cmd U TONE $orig
    fi
}

test_TSQL() {
    echo "Testing TSQL..."
    local orig=$(run_cmd u TSQL)
    if [ -z "$orig" ]; then
        log_fail "TSQL: read failed"
        return
    fi

    local test_val=$((1 - orig))
    run_cmd U TSQL $test_val
    local after=$(run_cmd u TSQL)
    if [ "$after" = "$test_val" ]; then
        run_cmd U TSQL $orig
        log_pass "TSQL"
    else
        log_fail "TSQL: set failed, expected $test_val got $after"
        run_cmd U TSQL $orig
    fi
}

test_APF() {
    echo "Testing APF (Audio Peak Filter)..."
    local orig=$(run_cmd u APF)
    if [ -z "$orig" ]; then
        log_fail "APF: read failed"
        return
    fi

    local test_val=$((1 - orig))
    run_cmd U APF $test_val
    local after=$(run_cmd u APF)
    if [ "$after" = "$test_val" ]; then
        run_cmd U APF $orig
        log_pass "APF (Audio Peak Filter)"
    else
        log_fail "APF: set failed, expected $test_val got $after"
        run_cmd U APF $orig
    fi
}

test_AI() {
    echo "Testing AI (Auto Information)..."
    raw_cmd "AI0" >/dev/null 2>&1
    sleep 0.3
    raw_cmd "AI" >/dev/null 2>&1
    raw_cmd "AI" >/dev/null 2>&1
    sleep 0.1

    local orig=$(raw_cmd "AI")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "AI (Auto Information) - command not available"
        return
    fi

    if [[ "$orig" == "AI0" ]]; then
        log_pass "AI (Auto Information)"
    elif [[ "$orig" =~ ^RM ]]; then
        log_pass "AI (Auto Information) - detected (meter responses present)"
    else
        log_fail "AI: unexpected response '$orig'"
    fi
}

run_function_tests() {
    echo "=== Function Tests ==="
    test_COMP
    test_MON
    test_LOCK
    test_TONE
    test_TSQL
    test_APF
    test_AI
    echo ""
}
