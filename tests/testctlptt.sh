#!/bin/sh

set -eu

check_ptt()
{
    expected=$1
    shift
    output=$("$program" -m 1 -L -u "$@")
    actual=$(printf '%s\n' "$output" | awk '
        /^ptt_type:/ { getline; sub(/^.*Value: /, ""); print; exit }
    ')

    if test "$actual" != "$expected"
    then
        printf '%s %s: expected PTT %s, got %s\n' \
            "$program" "$*" "$expected" "$actual" >&2
        exit 1
    fi
}

for program in ./rigctl ./rigctld
do
    check_ptt RIG
    check_ptt None -P NONE
    check_ptt RTS -P RTS -C timeout=500
    check_ptt None -P RIG -C ptt_type=None
    check_ptt None -C ptt_type=None -P RIG
    check_ptt RIGMICDATA -P RTS -C ptt_type=RIGMICDATA,timeout=500
    check_ptt RIG -C ptt_type=None,ptt_type=RIG
    check_ptt RTS -p unused-ptt-device
    check_ptt RTS -p unused-ptt-device -C timeout=500
    check_ptt None -p unused-ptt-device -P NONE
    check_ptt None -p unused-ptt-device -C ptt_type=None
    check_ptt RIG -p unused-ptt-device -P RIG

    output=$("$program" -m 1 -P NONE -C ptt_type=None -u)
    capability=$(printf '%s\n' "$output" | awk '
        /^PTT type:/ { sub(/^PTT type:[[:space:]]*/, ""); print; exit }
    ')
    if test "$capability" != "Rig capable"
    then
        printf '%s: PTT override changed model capability to %s\n' \
            "$program" "$capability" >&2
        exit 1
    fi
done
