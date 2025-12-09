#!/bin/bash
#
# FTX-1 Memory Command Tests
# Tests: MR (Memory Read), MT (Memory Tag), MZ (Memory Zone), AM, BM, MA, MB, MW
#
# Source this file from ftx1_test.sh main harness

test_MR() {
    echo "Testing MR (Memory Read)..."
    local resp=$(raw_cmd "MR00001")
    if [ -z "$resp" ] || [ "$resp" = "?" ]; then
        log_skip "MR (Memory Read) - command not available"
        return
    fi
    if [ "${#resp}" -ge 20 ] && [[ "$resp" =~ ^MR ]]; then
        log_pass "MR (Memory Read) - length: ${#resp}"
    else
        log_fail "MR: invalid format '$resp'"
    fi
}

test_MT() {
    echo "Testing MT (Memory Tag)..."
    local orig=$(raw_cmd "MT00001")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "MT (Memory Tag) - command not available"
        return
    fi

    if [[ "$orig" =~ ^MT[0-9]{5} ]]; then
        log_pass "MT (Memory Tag) - value: $orig"
    else
        log_fail "MT: invalid format '$orig'"
    fi
}

test_MZ() {
    echo "Testing MZ (Memory Zone)..."
    local orig=$(raw_cmd "MZ00001")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "MZ (Memory Zone) - command not available"
        return
    fi

    if [[ "$orig" =~ ^MZ[0-9]{5}[01][0-9]{9}$ ]]; then
        log_pass "MZ (Memory Zone) - value: $orig"
    else
        log_fail "MZ: invalid format '$orig'"
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
    log_skip "MW (Memory Write) - destructive operation, skipped for safety"
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

run_memory_tests() {
    echo "=== Memory Tests ==="
    test_MR
    test_MT
    test_MZ
    test_AM
    test_BM
    test_MA
    test_MB
    test_MW
    test_MEM_CH
    test_MEM_raw
    echo ""
}
