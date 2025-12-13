#!/bin/bash
#
# FTX-1 Filter Command Tests
# Tests: BC (Beat Cancel), BP (Manual Notch Position), CO (Contour), SH (Width)
#
# Source this file from ftx1_test.sh main harness

test_BC() {
    echo "Testing BC (Beat Cancel/ANF)..."
    local orig=$(run_cmd u ANF)
    if [ -z "$orig" ]; then
        log_fail "BC: read failed"
        return
    fi

    local test_val=$((1 - orig))
    run_cmd U ANF $test_val
    local after=$(run_cmd u ANF)
    if [ "$after" = "$test_val" ]; then
        run_cmd U ANF $orig
        local restored=$(run_cmd u ANF)
        if [ "$restored" = "$orig" ]; then
            log_pass "BC (Beat Cancel/ANF)"
        else
            log_fail "BC: restore failed"
        fi
    else
        log_fail "BC: set failed, expected $test_val got $after"
    fi
}

test_BP() {
    echo "Testing BP (Manual Notch Position)..."
    local orig=$(raw_cmd "BP00")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "BP (Manual Notch Position) - command not available"
        return
    fi

    local orig_val="${orig: -3}"
    local test_val="001"
    if [ "$orig_val" = "001" ]; then
        test_val="000"
    fi

    raw_cmd "BP00$test_val"
    local after=$(raw_cmd "BP00")
    local after_val="${after: -3}"
    if [ "$after_val" = "$test_val" ]; then
        raw_cmd "BP00$orig_val"
        log_pass "BP (Manual Notch Position)"
    else
        log_fail "BP: set failed, expected $test_val got $after_val"
        raw_cmd "BP00$orig_val"
    fi
}

test_CO() {
    echo "Testing CO (Contour)..."
    local orig=$(raw_cmd "CO00")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "CO (Contour) - command not available"
        return
    fi

    local orig_val="${orig: -4}"
    local test_val="0001"
    if [ "$orig_val" = "0001" ]; then
        test_val="0000"
    fi

    raw_cmd "CO00$test_val"
    local after=$(raw_cmd "CO00")
    local after_val="${after: -4}"
    if [ "$after_val" = "$test_val" ]; then
        raw_cmd "CO00$orig_val"
        log_pass "CO (Contour)"
    else
        log_fail "CO: set failed, expected $test_val got $after_val"
        raw_cmd "CO00$orig_val"
    fi
}

test_SH() {
    echo "Testing SH (Filter Width/PBT)..."
    local orig=$(run_cmd l PBT_IN)
    if [ -z "$orig" ]; then
        log_fail "SH: read failed"
        return
    fi

    local test_val="0.5"
    if [[ "${orig:0:3}" == "0.4" ]] || [[ "${orig:0:3}" == "0.5" ]]; then
        test_val="0.25"
    fi

    run_cmd L PBT_IN $test_val
    local after=$(run_cmd l PBT_IN)
    if [[ "$after" != "$orig" ]] && [[ "${after:0:2}" == "0." ]]; then
        run_cmd L PBT_IN $orig
        log_pass "SH (Filter Width)"
    else
        log_fail "SH: set failed, expected change from $orig, got $after"
        run_cmd L PBT_IN $orig
    fi
}

test_SH_raw() {
    echo "Testing SH (Width/High Cut) via raw command..."
    local orig=$(raw_cmd "SH0")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "SH_raw - command not available"
        return
    fi

    if [[ "$orig" =~ ^SH0[0-9]{3}$ ]]; then
        local width="${orig:3:3}"
        log_pass "SH (Width raw) - current width code: $width"
    else
        log_fail "SH_raw: invalid format '$orig'"
    fi
}

test_MN() {
    echo "Testing MN (Manual Notch)..."
    local orig=$(run_cmd u MN)
    if [ -z "$orig" ]; then
        log_fail "MN: read failed"
        return
    fi

    local test_val=$((1 - orig))
    run_cmd U MN $test_val
    local after=$(run_cmd u MN)
    if [ "$after" = "$test_val" ]; then
        run_cmd U MN $orig
        log_pass "MN (Manual Notch)"
    else
        log_fail "MN: set failed, expected $test_val got $after"
        run_cmd U MN $orig
    fi
}

test_NOTCHF() {
    echo "Testing NOTCHF (Notch Frequency)..."
    local orig=$(run_cmd l NOTCHF)
    if [ -z "$orig" ]; then
        log_fail "NOTCHF: read failed"
        return
    fi

    local test_val="1500"
    if [ "$orig" = "1500" ]; then
        test_val="1000"
    fi

    run_cmd L NOTCHF $test_val
    local after=$(run_cmd l NOTCHF)
    local diff=$((after - test_val))
    if [ "$diff" -lt 0 ]; then diff=$((-diff)); fi
    if [ "$diff" -lt 50 ]; then
        run_cmd L NOTCHF $orig
        log_pass "NOTCHF (Notch Frequency)"
    else
        log_fail "NOTCHF: set failed, expected ~$test_val got $after"
        run_cmd L NOTCHF $orig
    fi
}

run_filter_tests() {
    echo "=== Filter Tests ==="
    test_BC
    test_BP
    test_CO
    test_SH
    test_SH_raw
    test_MN
    test_NOTCHF
    echo ""
}
