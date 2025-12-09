#!/bin/bash
#
# FTX-1 Frequency Command Tests
# Tests: FA (VFO A), FB (VFO B), BD (Band Down), BU (Band Up), RIT, XIT
#
# Source this file from ftx1_test.sh main harness

test_FA() {
    echo "Testing FA (VFO A Freq)..."
    run_cmd V Main  # Ensure we're on Main
    local orig=$(run_cmd f)
    if [ -z "$orig" ]; then
        log_fail "FA: read failed"
        return
    fi

    # Test with 14.074 MHz or 14.000 MHz
    local test_val=14074000
    if [ "$orig" = "14074000" ]; then
        test_val=14000000
    fi

    run_cmd F $test_val
    local after=$(run_cmd f)
    if [ "$after" = "$test_val" ]; then
        run_cmd F $orig
        local restored=$(run_cmd f)
        if [ "$restored" = "$orig" ]; then
            log_pass "FA (VFO A Freq)"
        else
            log_fail "FA: restore failed"
        fi
    else
        log_fail "FA: set failed, expected $test_val got $after"
    fi
}

test_FB() {
    echo "Testing FB (VFO B Freq)..."
    run_cmd V Sub  # Switch to Sub
    local orig=$(run_cmd f)
    if [ -z "$orig" ]; then
        log_fail "FB: read failed"
        run_cmd V Main
        return
    fi

    # Test with 14.074 MHz or 14.000 MHz
    local test_val=14074000
    if [ "$orig" = "14074000" ]; then
        test_val=14000000
    fi

    run_cmd F $test_val
    local after=$(run_cmd f)
    run_cmd V Main  # Switch back before checking

    if [ "$after" = "$test_val" ]; then
        run_cmd V Sub
        run_cmd F $orig
        run_cmd V Main
        log_pass "FB (VFO B Freq)"
    else
        log_fail "FB: set failed, expected $test_val got $after"
    fi
}

test_BD() {
    echo "Testing BD (Band Down)..."
    run_cmd V Main
    local orig=$(run_cmd f)
    if [ -z "$orig" ]; then
        log_fail "BD: initial freq read failed"
        return
    fi

    # Band down
    raw_cmd "BD0"
    sleep 0.3
    local after=$(run_cmd f)
    if [ "$after" != "$orig" ]; then
        # Band up to restore
        raw_cmd "BU0"
        sleep 0.3
        local restored=$(run_cmd f)
        if [ "$restored" = "$orig" ]; then
            log_pass "BD (Band Down)"
        else
            log_fail "BD: restore failed, got $restored expected $orig"
        fi
    else
        log_fail "BD: freq didn't change, stayed at $orig"
    fi
}

test_BU() {
    echo "Testing BU (Band Up)..."
    run_cmd V Main
    local orig=$(run_cmd f)
    if [ -z "$orig" ]; then
        log_fail "BU: initial freq read failed"
        return
    fi

    # Band up
    raw_cmd "BU0"
    sleep 0.3
    local after=$(run_cmd f)
    if [ "$after" != "$orig" ]; then
        # Band down to restore
        raw_cmd "BD0"
        sleep 0.3
        local restored=$(run_cmd f)
        if [ "$restored" = "$orig" ]; then
            log_pass "BU (Band Up)"
        else
            log_fail "BU: restore failed, got $restored expected $orig"
        fi
    else
        log_fail "BU: freq didn't change, stayed at $orig"
    fi
}

test_RIT() {
    echo "Testing RIT..."
    # First check RIT function status
    local func_orig=$(run_cmd u RIT)

    # Get RIT offset
    local orig=$(run_cmd j)
    if [ -z "$orig" ]; then
        log_fail "RIT: read failed"
        return
    fi

    # Enable RIT and set offset
    run_cmd U RIT 1
    local test_val="100"
    if [ "$orig" = "100" ]; then
        test_val="200"
    fi

    run_cmd J $test_val
    local after=$(run_cmd j)
    if [ "$after" = "$test_val" ]; then
        # Restore
        run_cmd J $orig
        run_cmd U RIT $func_orig
        log_pass "RIT"
    else
        log_fail "RIT: set failed, expected $test_val got $after"
        run_cmd J $orig
        run_cmd U RIT $func_orig
    fi
}

test_XIT() {
    echo "Testing XIT..."
    # First check XIT function status
    local func_orig=$(run_cmd u XIT)

    # Get XIT offset
    local orig=$(run_cmd z)
    if [ -z "$orig" ]; then
        log_fail "XIT: read failed"
        return
    fi

    # Enable XIT and set offset
    run_cmd U XIT 1
    local test_val="100"
    if [ "$orig" = "100" ]; then
        test_val="200"
    fi

    run_cmd Z $test_val
    local after=$(run_cmd z)
    if [ "$after" = "$test_val" ]; then
        # Restore
        run_cmd Z $orig
        run_cmd U XIT $func_orig
        log_pass "XIT"
    else
        log_fail "XIT: set failed, expected $test_val got $after"
        run_cmd Z $orig
        run_cmd U XIT $func_orig
    fi
}

test_RIT_raw() {
    echo "Testing RIT via raw RI command (Hamlib workaround)..."
    local orig=$(raw_cmd "RI0")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "RIT (via RI) - command not available"
        return
    fi

    if [[ "$orig" =~ ^RI0[0-9]{7}$ ]]; then
        local rit_on="${orig:3:1}"
        local xit_on="${orig:4:1}"
        local dir_code="${orig:5:1}"
        local freq="${orig:6:4}"
        local dir="+"
        if [ "$dir_code" = "1" ]; then dir="-"; fi

        log_pass "RIT (via RI raw) - RIT=$rit_on XIT=$xit_on dir=$dir freq=$freq"
    else
        log_fail "RIT_raw: invalid RI format '$orig' (expected RI0 + 7 digits)"
    fi
}

run_freq_tests() {
    echo "=== Frequency Tests ==="
    test_FA
    test_FB
    test_BD
    test_BU
    test_RIT
    test_XIT
    test_RIT_raw
    echo ""
}
