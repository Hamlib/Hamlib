#!/bin/sh

set -eu

check_rotctl()
{
    input=$1
    expected=$2
    actual=$(./rotctl -m 1 d "$input" | tr -d '\015')

    if test "$actual" != "$expected"; then
        printf 'rotctl d %s: unexpected output:\n%s\n' "$input" "$actual" >&2
        exit 1
    fi
}

check_rotctl 10.2 '10
12
0.000000
0'
check_rotctl 10.1999999999 '10
12
0.000000
0'
check_rotctl 10.1999999998 '10
11
59.999999
0'
check_rotctl -12.9999999999 '13
0
0.000000
1'
check_rotctl -179.9999999999 '180
0
0.000000
1'

expected='Degrees: 10
Minutes: 12
Seconds: 0.000000
S/W: 0'
actual=$(printf 'd 10.2\nq\n' | ./rotctl -m 1 | tr -d '\015')
response=$(printf '%s\n' "$actual" | awk '
    /(^|: )Degrees: / { sub(/^.*Degrees: /, "Degrees: "); print; next }
    /^(Minutes|Seconds|S\/W): / { print }
')

if test "$response" != "$expected"; then
    printf 'rotctl interactive response: unexpected output:\n%s\n' "$actual" >&2
    exit 1
fi

actual=$(./testloc EM79UT96LW 5 2>/dev/null | tr -d '\000\015')
printf '%s\n' "$actual" | awk '
    /Longitude:/ { displayed = ($5 == "15.62\"") }
    /Recoded lon:/ { recoded = ($3 == "-84.254340") }
    END { exit !(displayed && recoded) }
'
