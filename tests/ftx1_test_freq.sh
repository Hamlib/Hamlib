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

test_CF() {
    echo "Testing CF (Clarifier offset)..."
    # CF sets clarifier offset value only (does not enable clarifier)
    # Format: CF P1 P2 P3 [+/-] PPPP where P3 must be 1
    # Verified working 2025-12-09

    # Get current offset from IF command
    local if_resp=$(raw_cmd "IF")
    if [ -z "$if_resp" ] || [ "$if_resp" = "?" ]; then
        log_skip "CF (Clarifier) - cannot read IF response"
        return
    fi

    # Extract offset from IF (positions 16-20, e.g., +0500 or -0300)
    local orig_offset="${if_resp:16:5}"

    # Test setting positive offset
    local test_val="+0500"
    [ "$orig_offset" = "+0500" ] && test_val="+0300"

    local resp=$(raw_cmd "CF001$test_val")
    if [ "$resp" = "?" ]; then
        log_fail "CF: command rejected"
        return
    fi

    # Verify via IF
    sleep 0.2
    local after_if=$(raw_cmd "IF")
    local after_offset="${after_if:16:5}"

    if [ "$after_offset" = "$test_val" ]; then
        # Test negative offset
        raw_cmd "CF001-0250"
        sleep 0.2
        local neg_if=$(raw_cmd "IF")
        local neg_offset="${neg_if:16:5}"

        if [ "$neg_offset" = "-0250" ]; then
            # Restore original
            raw_cmd "CF001$orig_offset"
            log_pass "CF (Clarifier offset) - set-only, value changes verified"
        else
            log_fail "CF: negative offset failed, expected -0250 got $neg_offset"
        fi
    else
        log_fail "CF: set failed, expected $test_val got $after_offset"
    fi
}

test_CF_format() {
    echo "Testing CF format requirements..."
    # P3=0 (RIT only) should return '?'
    local resp=$(raw_cmd "CF010+0100")
    if [ "$resp" = "?" ]; then
        # P3=1 should work
        resp=$(raw_cmd "CF001+0100")
        if [ "$resp" != "?" ]; then
            log_pass "CF format - P3=1 required (P3=0 rejected as expected)"
        else
            log_fail "CF format: P3=1 also rejected"
        fi
    else
        log_fail "CF format: P3=0 should return '?' but was accepted"
    fi
}

test_OS() {
    echo "Testing OS (Offset/Repeater Shift)..."
    # OS: Offset (Repeater Shift) for FM mode
    # Format: OS P1 P2 where P1=VFO (0/1), P2=Shift mode
    # Shift modes: 0=Simplex, 1=Plus, 2=Minus, 3=ARS
    # NOTE: This command only works in FM mode!

    # Save current mode to check if FM
    local mode=$(raw_cmd "MD0")
    if [ -z "$mode" ] || [ "$mode" = "?" ]; then
        log_skip "OS (Repeater Shift) - cannot read mode"
        return
    fi

    # Check if in FM mode (mode 4 = FM, B = FM-N)
    local mode_code="${mode:3:1}"
    if [ "$mode_code" != "4" ] && [ "$mode_code" != "B" ]; then
        log_skip "OS (Repeater Shift) - only works in FM mode (current mode: $mode_code)"
        return
    fi

    local orig=$(raw_cmd "OS0")
    if [ "$orig" = "?" ] || [ -z "$orig" ]; then
        log_skip "OS (Repeater Shift) - command not available"
        return
    fi

    if [[ ! "$orig" =~ ^OS0[0-3]$ ]]; then
        log_fail "OS: invalid read format '$orig'"
        return
    fi

    # Toggle between simplex (0) and plus (1)
    local test_val="1"
    [ "${orig:3:1}" = "1" ] && test_val="0"

    raw_cmd "OS0$test_val"
    local after=$(raw_cmd "OS0")
    if [ "$after" != "OS0$test_val" ]; then
        log_fail "OS: set failed, expected OS0$test_val got $after"
        raw_cmd "OS0${orig:3}"
        return
    fi

    # Restore original
    raw_cmd "OS0${orig:3}"
    local restored=$(raw_cmd "OS0")
    if [ "$restored" = "$orig" ]; then
        log_pass "OS (Repeater Shift) - set/read/restore verified (FM mode)"
    else
        log_fail "OS: restore failed, expected $orig got $restored"
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
    test_CF
    test_CF_format
    test_OS
    echo ""
}
