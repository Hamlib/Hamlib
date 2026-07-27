#!/bin/sh

set -eu

printf '\n' | ./rigctl -m 1 >/dev/null
printf '\n' | ./rotctl -m 1 >/dev/null
printf '\n' | ./ampctl -m 1 >/dev/null

./rigctl -m 1 3>/dev/null <&3 >/dev/null
./rotctl -m 1 3>/dev/null <&3 >/dev/null
./ampctl -m 1 3>/dev/null <&3 >/dev/null

./rigctl -m 1 w FA >/dev/null
./rigctl -m 1 -t -1 w 'x01 x02' >/dev/null

binary_reply='\0x00\0xFF 2'
test "$(./rigctl -m 1 -t -1 w 'x00 xff')" = "$binary_reply"
test "$(./rigctl -m 1 -t -1 w '\0x00\0xff')" = "$binary_reply"
test "$(./rigctl -m 1 -t -1 w 'x00ff')" = "$binary_reply"
test "$(./rigctl -m 1 -t -1 w 'x00xff')" = "$binary_reply"
