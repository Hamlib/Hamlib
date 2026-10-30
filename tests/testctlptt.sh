#!/bin/sh

set -eu

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/testctlptt.XXXXXX")
trap 'rm -f "$tmp_dir/stdout" "$tmp_dir/stderr"; rmdir "$tmp_dir"' 0
trap 'exit 1' 1 2 3 15

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

check_invalid_ptt()
{
    invalid=$1
    shift

    if "$program" -m 1 -L -u "$@" >"$tmp_dir/stdout" 2>"$tmp_dir/stderr"
    then
        status=0
    else
        status=$?
    fi

    if test "$status" -ne 2 || test -s "$tmp_dir/stdout" ||
       test "$(cat "$tmp_dir/stderr")" != "Unrecognised PTT type '$invalid'"
    then
        printf '%s %s: expected status 2, empty stdout, and invalid PTT diagnostic; got status %s\n' \
            "$program" "$*" "$status" >&2
        cat "$tmp_dir/stdout" "$tmp_dir/stderr" >&2
        exit 1
    fi
}

for program in ./rigctl ./rigctld
do
    check_ptt RIG
    check_ptt None -P NONE
    check_ptt RTS -P RTS -C timeout=500
    check_ptt RTS -P RIG -P RTS
    check_ptt None -P RIG -C ptt_type=None
    check_ptt None -C ptt_type=None -P RIG
    check_ptt RIGMICDATA -P RTS -C ptt_type=RIGMICDATA,timeout=500
    check_ptt RIG -C ptt_type=None,ptt_type=RIG
    check_ptt RTS -p unused-ptt-device
    check_ptt RTS -p unused-ptt-device -C timeout=500
    check_ptt None -p unused-ptt-device -P NONE
    check_ptt None -p unused-ptt-device -C ptt_type=None
    check_ptt RIG -p unused-ptt-device -P RIG

    for invalid in bogus rig CAT RIGMICDATA Parallel
    do
        check_invalid_ptt "$invalid" -P "$invalid"
    done
    check_invalid_ptt bogus --ptt-type=bogus
    check_invalid_ptt bogus -P RTS -P bogus
    check_invalid_ptt bogus -P bogus -P RTS
    check_invalid_ptt bogus -p unused-ptt-device -P bogus
    check_invalid_ptt bogus -P bogus -p unused-ptt-device
    check_invalid_ptt bogus -C ptt_type=RIG -P bogus
    check_invalid_ptt bogus -P bogus -C ptt_type=RIG

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
