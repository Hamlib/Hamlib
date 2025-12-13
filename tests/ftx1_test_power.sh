#!/bin/bash
#
# FTX-1 Power Control Command Tests
# Tests: PC (RF Power), EX030104 (TUNER SELECT), EX0307xx (OPTION Power)
# Note: EX menu tests require SPA-1/Optima configuration
#
# Source this file from ftx1_test.sh main harness

test_PC() {
    echo "Testing PC (RF Power)..."
    local orig=$(run_cmd l RFPOWER)
    if [ -z "$orig" ]; then
        log_fail "PC: read failed"
        return
    fi

    # RF Power is 0.0-1.0, FTX-1 field head is 0.5-10W (0.05-1.0)
    local test_val="0.5"
    if [ "${orig:0:3}" = "0.5" ]; then
        test_val="0.3"
    fi

    run_cmd L RFPOWER $test_val
    local after=$(run_cmd l RFPOWER)
    # Allow tolerance for power level quantization
    local diff=$(echo "scale=4; $after - $test_val" | bc | tr -d '-')
    if [ "$(echo "$diff < 0.05" | bc)" = "1" ]; then
        run_cmd L RFPOWER $orig
        log_pass "PC (RF Power)"
    else
        log_fail "PC: set failed, expected ~$test_val got $after"
        run_cmd L RFPOWER $orig
    fi
}

test_EX_TUNER_SELECT() {
    echo "Testing EX030104 (TUNER SELECT)..."
    # EX030104: TUNER SELECT - controls internal antenna tuner type
    # Values: 0=INT, 1=INT(FAST), 2=EXT, 3=ATAS
    # GUARDRAIL: Values 0 (INT) and 1 (INT FAST) require SPA-1 amplifier
    if [ "$OPTIMA_ENABLED" -eq 0 ]; then
        log_skip "EX030104 (TUNER SELECT) - Optima tests disabled (use --optima for SPA-1)"
        return
    fi
    local orig=$(raw_cmd "EX030104")
    if [ "$orig" = "?" ]; then
        log_skip "EX030104 (TUNER SELECT) - not available in firmware"
        return
    fi
    if [[ ! "$orig" =~ ^EX030104[0-3]$ ]]; then
        log_fail "EX030104: read failed, got: $orig"
        return
    fi
    local orig_val="${orig: -1}"
    # With SPA-1, test setting INT (0)
    raw_cmd "EX0301040"
    local after=$(raw_cmd "EX030104")
    if [ "$after" = "EX0301040" ]; then
        raw_cmd "EX030104$orig_val"
        log_pass "EX030104 (TUNER SELECT INT) - set/read/restore with SPA-1"
    else
        log_fail "EX030104: set to INT failed with SPA-1, got: $after"
        raw_cmd "EX030104$orig_val"
    fi
}

test_EX_OPTION_POWER() {
    echo "Testing EX030705 (OPTION 160m Power)..."
    # EX0307xx: OPTION section - SPA-1 max power settings per band
    # GUARDRAIL: These settings require SPA-1 amplifier
    if [ "$OPTIMA_ENABLED" -eq 0 ]; then
        log_skip "EX030705 (OPTION Power) - Optima tests disabled (use --optima for SPA-1)"
        return
    fi
    local orig=$(raw_cmd "EX030705")
    if [ "$orig" = "?" ]; then
        log_skip "EX030705 (OPTION 160m power) - not available in firmware"
        return
    fi
    if [[ ! "$orig" =~ ^EX030705[0-9]{3}$ ]]; then
        log_fail "EX030705: read failed, got: $orig"
        return
    fi
    local orig_val="${orig: -3}"
    # Test setting a valid power level (100W is max for SPA-1)
    local test_val="050"
    [ "$orig_val" = "050" ] && test_val="100"
    raw_cmd "EX030705$test_val"
    local after=$(raw_cmd "EX030705")
    if [ "$after" = "EX030705$test_val" ]; then
        raw_cmd "EX030705$orig_val"
        log_pass "EX030705 (OPTION Power) - set/read/restore with SPA-1"
    else
        log_fail "EX030705: set failed with SPA-1, got: $after"
        raw_cmd "EX030705$orig_val"
    fi
}

run_power_tests() {
    echo "=== Power Control Tests ==="
    test_PC
    test_EX_TUNER_SELECT
    test_EX_OPTION_POWER
    echo ""
}
