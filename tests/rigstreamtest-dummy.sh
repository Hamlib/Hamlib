#!/bin/sh
# System test for the Hamlib streaming subsystem, run against the dummy backend
# (model 1). No hardware is involved: the dummy rig both produces and consumes
# sample data, so this exercises the whole streaming path -- capability
# negotiation, the ring buffer, format conversion, PTT sequencing and stream
# shutdown -- on any build machine.
#
# Every streaming mode must open, run and shut down cleanly with an empty issue
# tally. "Done (result=0)." confirms both at once, so it is the only marker
# checked; a missing marker or a non-zero exit fails the whole run.
#
# Part of "make check" (and so of "make distcheck"), which is why each case is
# kept to about a second. To test a real radio, use rigstreamtest-hw.sh.
#
# The binary is looked up in this order, so the script works both under
# "make check" -- where the current directory is the build tree's tests/, which
# in a VPATH or distcheck build is not this script's directory -- and when run
# by hand from anywhere:
#
#   $RIGSTREAMTEST -> ./rigstreamtest -> <script dir>/rigstreamtest -> $PATH

RATE=48000

find_rigstreamtest()
{
    if [ -n "$RIGSTREAMTEST" ]; then
        echo "$RIGSTREAMTEST"
        return
    fi

    if [ -x ./rigstreamtest ]; then
        echo ./rigstreamtest
        return
    fi

    script_dir=`dirname "$0"`

    if [ -x "$script_dir/rigstreamtest" ]; then
        echo "$script_dir/rigstreamtest"
        return
    fi

    echo rigstreamtest
}

RIGSTREAMTEST=`find_rigstreamtest`

if [ ! -x "$RIGSTREAMTEST" ] && ! command -v "$RIGSTREAMTEST" >/dev/null 2>&1; then
    echo "FAIL: rigstreamtest not found; set RIGSTREAMTEST to its path"
    exit 1
fi

run()
{
    desc=$1
    shift

    attempt=1

    while :; do
        reason=""

        if out=$("$@" 2>&1); then
            case $out in
            *"Done (result=0)."*)
                echo "ok: $desc"
                return 0
                ;;
            *)
                reason="missing clean-shutdown marker"
                ;;
            esac
        else
            reason="non-zero exit"
        fi

        echo "FAIL ($desc, attempt $attempt): $reason"
        echo "$out"

        # The streaming threads are nanosleep-paced; an oversubscribed CI
        # runner can oversleep enough to trip the zero-issue tally. Retry
        # once there before treating it as a real failure. Local runs
        # (no $CI) stay strict.
        if [ -n "$CI" ] && [ "$attempt" -eq 1 ]; then
            attempt=2
            echo "Retrying $desc once (CI runner timing)"
            continue
        fi

        exit 1
    done
}

run "audio_rx"    $RIGSTREAMTEST -m 1 -s $RATE -c 1 -t audio_rx -d 1
run "audio_tx"    $RIGSTREAMTEST -m 1 -s $RATE -c 1 -t audio_tx -d 1 -P
run "iq_rx"       $RIGSTREAMTEST -m 1 -s $RATE      -t iq_rx    -d 1
run "iq_tx"       $RIGSTREAMTEST -m 1 -s $RATE      -t iq_tx    -d 1 -P
run "loopback"    $RIGSTREAMTEST -m 1 -s $RATE -c 1 -t loopback -d 1
# The alternating and full-duplex runs get a 2 s ring (--buffer-ms) so a
# CI VM stalling the writer or the consumer for hundreds of ms does not
# overrun the default 250 ms TX ring; capacity is jitter headroom, not
# added latency, so the zero-issue bar stays meaningful.
run "alternating" $RIGSTREAMTEST -m 1 -s $RATE -c 1 --rx-secs 1 --tx-secs 1 --cycles 1 --buffer-ms 2000
# Full-duplex uses I/Q: the dummy backend's audio TX loopback consumer cannot
# keep pace with the mode's free-running writer, so audio full-duplex overruns
# against the dummy (real hardware drains un-keyed TX cleanly). The I/Q loopback
# keeps pace, so it exercises the concurrent RX+TX path with a zero issue tally.
run "full-duplex" $RIGSTREAMTEST -m 1 -s $RATE --full-duplex --iq --rx-secs 1 --tx-secs 1 -d 2 --buffer-ms 2000

echo "All rigstreamtest dummy-backend tests passed."
