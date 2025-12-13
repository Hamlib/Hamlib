#!/bin/bash
#
# FTX-1 Info Command Tests (Read-Only)
# Tests: DA (Date), DT (Date/Time), ID (Radio ID), IF (Information),
#        OI (Opposite Band Info), PS (Power Switch), RI (RIT Info), RM (Read Meter)
#
# Source this file from ftx1_test.sh main harness

test_ID() {
    echo "Testing ID (Radio ID)..."
    local resp=$(raw_cmd "ID")
    if [ -z "$resp" ] || [ "$resp" = "?" ]; then
        log_fail "ID: read failed"
        return
    fi
    if [[ "$resp" =~ ^ID[0-9]{4}$ ]]; then
        log_pass "ID (Radio ID) - value: $resp"
    else
        log_fail "ID: invalid format '$resp'"
    fi
}

test_IF() {
    echo "Testing IF (Information)..."
    local resp=$(raw_cmd "IF")
    if [ -z "$resp" ] || [ "$resp" = "?" ]; then
        log_fail "IF: read failed"
        return
    fi
    if [ "${#resp}" -ge 20 ] && [[ "$resp" =~ ^IF ]]; then
        log_pass "IF (Information) - length: ${#resp}"
    else
        log_fail "IF: invalid format '$resp'"
    fi
}

test_OI() {
    echo "Testing OI (Opposite Band Info)..."
    local resp=$(raw_cmd "OI")
    if [ -z "$resp" ] || [ "$resp" = "?" ]; then
        log_fail "OI: read failed"
        return
    fi
    if [ "${#resp}" -ge 20 ] && [[ "$resp" =~ ^OI ]]; then
        log_pass "OI (Opposite Band Info) - length: ${#resp}"
    else
        log_fail "OI: invalid format '$resp'"
    fi
}

test_RI() {
    echo "Testing RI (RIT Information)..."
    local resp=$(raw_cmd "RI0")
    if [ -z "$resp" ] || [ "$resp" = "?" ]; then
        log_fail "RI: read failed"
        return
    fi
    if [[ "$resp" =~ ^RI0[0-9]{7}$ ]]; then
        log_pass "RI (RIT Information) - value: $resp"
    else
        log_fail "RI: invalid format '$resp'"
    fi
}

test_RM() {
    echo "Testing RM (Read Meter)..."
    local resp=$(raw_cmd "RM0")
    if [ -z "$resp" ] || [ "$resp" = "?" ]; then
        log_fail "RM: read failed"
        return
    fi
    if [[ "$resp" =~ ^RM0[0-9]{6,9}$ ]]; then
        log_pass "RM (Read Meter) - value: $resp"
    else
        log_fail "RM: invalid format '$resp'"
    fi
}

test_DA() {
    echo "Testing DA (Date)..."
    local orig=$(raw_cmd "DA")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "DA (Date) - command not available"
        return
    fi
    if [[ "$orig" =~ ^DA00[0-9]{6}$ ]]; then
        log_pass "DA (Date) - value: $orig"
    else
        log_fail "DA: invalid format '$orig'"
    fi
}

test_DT() {
    echo "Testing DT (Date/Time)..."
    local orig=$(raw_cmd "DT0")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "DT (Date/Time) - command not available"
        return
    fi
    if [[ "$orig" =~ ^DT0[0-9]{8}$ ]]; then
        log_pass "DT (Date/Time) - value: $orig"
    else
        log_fail "DT: invalid format '$orig'"
    fi
}

test_PS() {
    echo "Testing PS (Power Switch)..."
    local orig=$(raw_cmd "PS")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "PS (Power Switch) - command not available"
        return
    fi
    if [[ "$orig" =~ ^PS[01]$ ]]; then
        log_pass "PS (Power Switch) - read-only, value: $orig"
    else
        log_fail "PS: invalid format '$orig'"
    fi
}

test_VE() {
    echo "Testing VE (VOX Enable)..."
    local orig=$(raw_cmd "VE0")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "VE (VOX Enable) - command not available"
        return
    fi
    if [[ "$orig" =~ ^VE0[0-9]{4}$ ]]; then
        log_pass "VE (VOX Enable) - read-only, value: $orig"
    else
        log_fail "VE: invalid format '$orig'"
    fi
}

test_SF() {
    echo "Testing SF (Sub Dial/FUNC Knob)..."
    # SF: Sub Dial (FUNC Knob) function assignment
    # Format: SF P1 P2; where P1=VFO (0), P2=single hex char (0-H)
    # 0=None, 7=Mic Gain, D=RF Power, G=CW Pitch, H=BK Delay, etc.
    # NOTE: Read-only test - setting SF may affect SPA-1 amp detection

    local orig=$(raw_cmd "SF0")
    if [ "$orig" = "?" ] || [ -z "$orig" ]; then
        log_skip "SF (Sub Dial/FUNC Knob) - command not available"
        return
    fi

    # Response format: SF0X where X is hex char (0-9, A-H)
    if [[ ! "$orig" =~ ^SF0[0-9A-H]$ ]]; then
        log_fail "SF: invalid read format '$orig' (expected SF0X)"
        return
    fi

    # Read-only test for safety
    log_pass "SF (Sub Dial/FUNC Knob) - read verified: $orig"
}

run_info_tests() {
    echo "=== Info Tests ==="
    test_ID
    test_IF
    test_OI
    test_RI
    test_RM
    test_DA
    test_DT
    test_PS
    test_VE
    test_SF
    echo ""
}
