#!/bin/bash
#
# FTX-1 VFO Command Tests
# Tests: VS (VFO Select), ST (Split), FT (TX VFO), AB, BA, SV
#
# Source this file from ftx1_test.sh main harness

test_VS() {
    echo "Testing VS (VFO Select)..."

    # SAFETY: Ensure TX is off before manipulating VFO
    run_cmd T 0 2>/dev/null  # PTT off

    local orig=$(run_cmd v)
    if [ -z "$orig" ]; then
        log_fail "VS: read failed"
        return
    fi

    # Toggle VFO
    if [ "$orig" = "Main" ]; then
        run_cmd V Sub
        local after=$(run_cmd v)
        if [ "$after" = "Sub" ]; then
            run_cmd V Main
            local restored=$(run_cmd v)
            if [ "$restored" = "Main" ]; then
                log_pass "VS (VFO Select)"
            else
                log_fail "VS: restore failed, got $restored"
            fi
        else
            log_fail "VS: set to Sub failed, got $after"
        fi
    else
        run_cmd V Main
        local after=$(run_cmd v)
        if [ "$after" = "Main" ]; then
            run_cmd V Sub
            local restored=$(run_cmd v)
            if [ "$restored" = "Sub" ]; then
                log_pass "VS (VFO Select)"
            else
                log_fail "VS: restore failed, got $restored"
            fi
        else
            log_fail "VS: set to Main failed, got $after"
        fi
    fi
}

test_ST() {
    echo "Testing ST (Split)..."

    # SAFETY: Ensure TX is off before manipulating split
    run_cmd T 0 2>/dev/null  # PTT off

    local orig=$(run_cmd s | head -1)
    if [ -z "$orig" ]; then
        log_fail "ST: read failed"
        return
    fi

    # Toggle split
    local test_val=$((1 - orig))
    run_cmd S $test_val Main
    local after=$(run_cmd s | head -1)
    if [ "$after" = "$test_val" ]; then
        run_cmd S $orig Main
        local restored=$(run_cmd s | head -1)
        if [ "$restored" = "$orig" ]; then
            log_pass "ST (Split)"
        else
            log_fail "ST: restore failed"
        fi
    else
        log_fail "ST: set failed, expected $test_val got $after"
    fi
}

test_FT() {
    echo "Testing FT (TX VFO)..."

    # SAFETY: Ensure TX is off before manipulating TX VFO
    run_cmd T 0 2>/dev/null  # PTT off

    local orig=$(run_cmd s | tail -1)
    if [ -z "$orig" ]; then
        log_fail "FT: read failed"
        return
    fi

    # Set split on first to test TX VFO
    run_cmd S 1 Sub
    local after=$(run_cmd s | tail -1)
    if [ "$after" = "Sub" ]; then
        run_cmd S 1 Main
        after=$(run_cmd s | tail -1)
        if [ "$after" = "Main" ]; then
            run_cmd S 0 Main
            log_pass "FT (TX VFO)"
        else
            log_fail "FT: set to Main failed"
            run_cmd S 0 Main
        fi
    else
        log_fail "FT: set to Sub failed"
        run_cmd S 0 Main
    fi
}

test_VFO_CPY() {
    echo "Testing VFO CPY (Copy A to B)..."

    # Set VFO A to known frequency
    run_cmd V Main
    local orig_a=$(run_cmd f)
    run_cmd V Sub
    local orig_b=$(run_cmd f)

    # Set A to a distinct frequency
    run_cmd V Main
    run_cmd F 14100000

    # Copy A to B
    run_cmd G CPY
    sleep 0.2

    # Check B frequency
    run_cmd V Sub
    local after_b=$(run_cmd f)

    # Restore
    run_cmd V Main
    run_cmd F $orig_a
    run_cmd V Sub
    run_cmd F $orig_b
    run_cmd V Main

    if [ "$after_b" = "14100000" ]; then
        log_pass "VFO CPY"
    else
        log_fail "VFO CPY: expected 14100000, got $after_b"
    fi
}

test_VFO_XCHG() {
    echo "Testing VFO XCHG (Exchange A and B)..."

    # Set VFOs to known frequencies
    run_cmd V Main
    local orig_a=$(run_cmd f)
    run_cmd V Sub
    local orig_b=$(run_cmd f)

    # Set distinct frequencies
    run_cmd V Main
    run_cmd F 14200000
    run_cmd V Sub
    run_cmd F 14300000

    # Exchange
    run_cmd G XCHG
    sleep 0.2

    # Check frequencies
    run_cmd V Main
    local after_a=$(run_cmd f)
    run_cmd V Sub
    local after_b=$(run_cmd f)

    # Restore
    run_cmd V Main
    run_cmd F $orig_a
    run_cmd V Sub
    run_cmd F $orig_b
    run_cmd V Main

    if [ "$after_a" = "14300000" ] && [ "$after_b" = "14200000" ]; then
        log_pass "VFO XCHG"
    else
        log_fail "VFO XCHG: expected A=14300000,B=14200000 got A=$after_a,B=$after_b"
    fi
}

test_AB() {
    echo "Testing AB (VFO A to B Copy)..."
    local orig_a=$(raw_cmd "FA")
    local orig_b=$(raw_cmd "FB")
    if [ -z "$orig_a" ] || [ -z "$orig_b" ]; then
        log_skip "AB (VFO A→B) - couldn't read VFO frequencies"
        return
    fi

    # Set A to a known value
    raw_cmd "FA014100000"
    raw_cmd "AB"
    sleep 0.2
    local after_b=$(raw_cmd "FB")

    # Restore
    raw_cmd "FA${orig_a:2}"
    raw_cmd "FB${orig_b:2}"

    if [[ "$after_b" == "FB014100000" ]]; then
        log_pass "AB (VFO A→B Copy)"
    else
        log_fail "AB: expected FB014100000, got $after_b"
    fi
}

test_BA() {
    echo "Testing BA (VFO B to A Copy)..."
    local orig_a=$(raw_cmd "FA")
    local orig_b=$(raw_cmd "FB")
    if [ -z "$orig_a" ] || [ -z "$orig_b" ]; then
        log_skip "BA (VFO B→A) - couldn't read VFO frequencies"
        return
    fi

    # Set B to a known value
    raw_cmd "FB014200000"
    raw_cmd "BA"
    sleep 0.2
    local after_a=$(raw_cmd "FA")

    # Restore
    raw_cmd "FA${orig_a:2}"
    raw_cmd "FB${orig_b:2}"

    if [[ "$after_a" == "FA014200000" ]]; then
        log_pass "BA (VFO B→A Copy)"
    else
        log_fail "BA: expected FA014200000, got $after_a"
    fi
}

test_SV() {
    echo "Testing SV (Swap VFO/Memory)..."
    raw_cmd "SV"
    sleep 0.1
    raw_cmd "SV"
    log_pass "SV (Swap VFO/Memory)"
}

run_vfo_tests() {
    echo "=== VFO Tests ==="
    test_VS
    test_ST
    test_FT
    test_VFO_CPY
    test_VFO_XCHG
    test_AB
    test_BA
    test_SV
    echo ""
}
