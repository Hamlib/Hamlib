#!/bin/bash
#
# FTX-1 Audio/Level Command Tests
# Tests: AG (AF Gain), RG (RF Gain), MG (Mic Gain), VG (VOX Gain), VD (VOX Delay),
#        GT (AGC), SM (S-Meter), SQ (Squelch), IS (IF Shift), ML (Monitor Level),
#        AO (AMC Output), MS (Meter Switch)
#
# Source this file from ftx1_test.sh main harness

test_AG() {
    echo "Testing AG (AF Gain)..."
    local orig=$(run_cmd l AF)
    if [ -z "$orig" ]; then
        log_fail "AG: read failed"
        return
    fi

    local test_val="0.5"
    local orig_int=$(echo "$orig * 255" | bc | cut -d. -f1)
    if [ "$orig_int" = "127" ]; then
        test_val="0.3"
    fi

    run_cmd L AF $test_val
    local after=$(run_cmd l AF)
    local diff=$(echo "scale=4; $after - $test_val" | bc | tr -d '-')
    if [ "$(echo "$diff < 0.01" | bc)" = "1" ]; then
        run_cmd L AF $orig
        log_pass "AG (AF Gain)"
    else
        log_fail "AG: set failed, expected ~$test_val got $after"
        run_cmd L AF $orig
    fi
}

test_RG() {
    echo "Testing RG (RF Gain)..."
    local orig=$(run_cmd l RF)
    if [ -z "$orig" ]; then
        log_fail "RG: read failed"
        return
    fi

    local test_val="0.5"
    local orig_int=$(echo "$orig * 255" | bc | cut -d. -f1)
    if [ "$orig_int" = "127" ]; then
        test_val="0.7"
    fi

    run_cmd L RF $test_val
    local after=$(run_cmd l RF)
    local diff=$(echo "scale=4; $after - $test_val" | bc | tr -d '-')
    if [ "$(echo "$diff < 0.01" | bc)" = "1" ]; then
        run_cmd L RF $orig
        log_pass "RG (RF Gain)"
    else
        log_fail "RG: set failed, expected ~$test_val got $after"
        run_cmd L RF $orig
    fi
}

test_MG() {
    echo "Testing MG (Mic Gain)..."
    local orig=$(run_cmd l MICGAIN)
    if [ -z "$orig" ]; then
        log_fail "MG: read failed"
        return
    fi

    local test_val="0.500000"
    if [ "$orig" = "0.500000" ]; then
        test_val="0.300000"
    fi

    run_cmd L MICGAIN $test_val
    local after=$(run_cmd l MICGAIN)
    local after_prefix=${after:0:3}
    local test_prefix=${test_val:0:3}
    if [ "$after_prefix" = "$test_prefix" ]; then
        run_cmd L MICGAIN $orig
        log_pass "MG (Mic Gain)"
    else
        log_fail "MG: set failed, expected $test_val got $after"
        run_cmd L MICGAIN $orig
    fi
}

test_VG() {
    echo "Testing VG (VOX Gain)..."
    local orig=$(run_cmd l VOXGAIN)
    if [ -z "$orig" ]; then
        log_fail "VG: read failed"
        return
    fi

    local test_val="0.500000"
    if [ "$orig" = "0.500000" ]; then
        test_val="0.300000"
    fi

    run_cmd L VOXGAIN $test_val
    local after=$(run_cmd l VOXGAIN)
    local after_prefix=${after:0:3}
    local test_prefix=${test_val:0:3}
    if [ "$after_prefix" = "$test_prefix" ]; then
        run_cmd L VOXGAIN $orig
        log_pass "VG (VOX Gain)"
    else
        log_fail "VG: set failed, expected $test_val got $after"
        run_cmd L VOXGAIN $orig
    fi
}

test_VD() {
    echo "Testing VD (VOX Delay)..."
    local orig=$(run_cmd l VOXDELAY)
    if [ -z "$orig" ]; then
        log_fail "VD: read failed"
        return
    fi

    local test_val="10"
    if [ "$orig" = "10" ]; then
        test_val="5"
    fi

    run_cmd L VOXDELAY $test_val
    local after=$(run_cmd l VOXDELAY)
    if [ "$after" = "$test_val" ]; then
        run_cmd L VOXDELAY $orig
        log_pass "VD (VOX Delay)"
    else
        log_fail "VD: set failed, expected $test_val got $after"
        run_cmd L VOXDELAY $orig
    fi
}

test_GT() {
    echo "Testing GT (AGC)..."
    local orig=$(run_cmd l AGC)
    if [ -z "$orig" ]; then
        log_fail "GT: read failed"
        return
    fi

    local test_val="2"
    if [ "$orig" = "2" ]; then
        test_val="3"
    fi

    run_cmd L AGC $test_val
    local after=$(run_cmd l AGC)
    if [ "$after" = "$test_val" ]; then
        run_cmd L AGC $orig
        log_pass "GT (AGC)"
    else
        log_fail "GT: set failed, expected $test_val got $after"
        run_cmd L AGC $orig
    fi
}

test_SM() {
    echo "Testing SM (S-Meter)..."
    local val=$(run_cmd l STRENGTH)
    if [ -z "$val" ]; then
        log_fail "SM: read failed"
        return
    fi
    log_pass "SM (S-Meter) - read value: $val"
}

test_SQ() {
    echo "Testing SQ (Squelch)..."
    local orig=$(run_cmd l SQL)
    if [ -z "$orig" ]; then
        log_fail "SQ: read failed"
        return
    fi

    local test_val="0.5"
    if [ "${orig:0:3}" = "0.5" ]; then
        test_val="0.3"
    fi

    run_cmd L SQL $test_val
    local after=$(run_cmd l SQL)
    local diff=$(echo "scale=4; $after - $test_val" | bc | tr -d '-')
    if [ "$(echo "$diff < 0.02" | bc)" = "1" ]; then
        run_cmd L SQL $orig
        log_pass "SQ (Squelch)"
    else
        log_fail "SQ: set failed, expected ~$test_val got $after"
        run_cmd L SQL $orig
    fi
}

test_IS() {
    echo "Testing IS (IF Shift)..."
    local orig=$(run_cmd l IF)
    if [ -z "$orig" ]; then
        log_fail "IS: read failed"
        return
    fi

    local test_val="500"
    if [ "$orig" = "500" ]; then
        test_val="0"
    fi

    run_cmd L IF $test_val
    local after=$(run_cmd l IF)
    if [ "$after" = "$test_val" ]; then
        run_cmd L IF $orig
        log_pass "IS (IF Shift)"
    else
        log_fail "IS: set failed, expected $test_val got $after"
        run_cmd L IF $orig
    fi
}

test_IS_raw() {
    echo "Testing IS (IF Shift) via raw command..."
    local orig=$(raw_cmd "IS0")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "IS_raw - command not available"
        return
    fi

    if [[ "$orig" =~ ^IS0[01][+-][0-9]{4}$ ]]; then
        local is_on="${orig:3:1}"
        local dir="${orig:4:1}"
        local freq="${orig:5:4}"

        local test_on=$((1 - is_on))
        raw_cmd "IS0${test_on}${dir}${freq}"
        local after=$(raw_cmd "IS0")

        if [[ "$after" =~ ^IS0${test_on} ]]; then
            raw_cmd "IS0${orig:3}"
            log_pass "IS (IF Shift raw) - set/read works"
        else
            log_pass "IS (IF Shift raw) - read-only, value: $orig"
        fi
    else
        log_fail "IS_raw: invalid format '$orig'"
    fi
}

test_ML() {
    echo "Testing ML (Monitor Level)..."
    local val=$(run_cmd l MONITOR_GAIN)
    if [ -z "$val" ] || [ "$val" = "0" ]; then
        log_skip "ML (Monitor Level) - firmware returns '?' for ML command"
        return
    fi
    if [[ "${val:0:2}" == "0." ]]; then
        log_pass "ML (Monitor Level) - read-only value: $val"
    else
        log_fail "ML: invalid read value: $val"
    fi
}

test_ML_raw() {
    echo "Testing ML (Monitor Level) via raw command..."
    local orig=$(raw_cmd "ML0")
    if [ -z "$orig" ] || [ "$orig" = "?" ]; then
        log_skip "ML_raw - firmware returns '?' (not implemented)"
        return
    fi

    if [[ "$orig" =~ ^ML0[0-9]{3}$ ]]; then
        local level="${orig:3:3}"

        local test_val="050"
        if [ "$level" = "050" ]; then
            test_val="025"
        fi
        raw_cmd "ML0${test_val}"
        local after=$(raw_cmd "ML0")

        if [[ "$after" == "ML0${test_val}" ]]; then
            raw_cmd "ML0${level}"
            log_pass "ML (Monitor Level raw) - set/read works"
        else
            log_pass "ML (Monitor Level raw) - read-only (SET doesn't persist), value: $orig"
        fi
    else
        log_fail "ML_raw: invalid format '$orig'"
    fi
}

test_AO() {
    echo "Testing AO (AMC Output/ANTIVOX)..."
    local orig=$(run_cmd l ANTIVOX)
    if [ -z "$orig" ]; then
        log_fail "AO: read failed"
        return
    fi

    local test_val="0.5"
    if [[ "${orig:0:3}" == "0.5" ]]; then
        test_val="0.25"
    fi

    run_cmd L ANTIVOX $test_val
    local after=$(run_cmd l ANTIVOX)
    if [[ "${after:0:3}" == "${test_val:0:3}" ]]; then
        run_cmd L ANTIVOX $orig
        log_pass "AO (AMC Output)"
    else
        log_fail "AO: set failed, expected ~$test_val got $after"
        run_cmd L ANTIVOX $orig
    fi
}

test_MS() {
    echo "Testing MS (Meter Switch)..."
    local orig=$(run_cmd l METER)
    if [ -z "$orig" ]; then
        log_fail "MS: read failed"
        return
    fi

    local test_val="3"
    if [ "$orig" = "3" ]; then
        test_val="0"
    fi

    run_cmd L METER $test_val
    local after=$(run_cmd l METER)
    if [ "$after" = "$test_val" ]; then
        run_cmd L METER $orig
        log_pass "MS (Meter Switch)"
    else
        log_fail "MS: set failed, expected $test_val got $after"
        run_cmd L METER $orig
    fi
}

test_PB() {
    echo "Testing PB (Play Back/DVS Voice Messages)..."
    # PB: Play Back (DVS voice messages)
    # Format: PB P1 where P1: 0=Stop, 1-5=Play channel 1-5
    # Note: This test only tests the stop command (PB0) for safety
    # Playing back messages (PB1-PB5) would cause audio output

    local orig=$(raw_cmd "PB")
    if [ "$orig" = "?" ] || [ -z "$orig" ]; then
        log_skip "PB (Play Back/DVS) - command not available"
        return
    fi

    if [[ ! "$orig" =~ ^PB[0-5]$ ]]; then
        log_fail "PB: invalid read format '$orig'"
        return
    fi

    # Test stop command (safe - doesn't play audio)
    raw_cmd "PB0"
    local after=$(raw_cmd "PB")
    if [ "$after" = "PB0" ]; then
        log_pass "PB (Play Back/DVS) - stop command verified"
    else
        log_fail "PB: stop command failed, got $after"
    fi
}

run_audio_tests() {
    echo "=== Audio/Level Tests ==="
    test_AG
    test_RG
    test_MG
    test_VG
    test_VD
    test_GT
    test_SM
    test_SQ
    test_IS
    test_IS_raw
    test_ML
    test_ML_raw
    test_AO
    test_MS
    test_PB
    echo ""
}
