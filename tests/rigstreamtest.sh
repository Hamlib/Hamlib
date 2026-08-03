#!/bin/sh
# Smoke tests for rigstreamtest against the dummy backend (model 1).
# Each streaming mode must open, run and shut down cleanly with a zero issue
# tally; "Done (result=0)." confirms both the clean exit and the empty tally.

RIGSTREAMTEST=./rigstreamtest
RATE=48000

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
run "alternating" $RIGSTREAMTEST -m 1 -s $RATE -c 1 --rx-secs 1 --tx-secs 1 --cycles 1
# Full-duplex uses I/Q: the dummy backend's audio TX loopback consumer cannot
# keep pace with the mode's free-running writer, so audio full-duplex overruns
# against the dummy (real hardware drains un-keyed TX cleanly). The I/Q loopback
# keeps pace, so it exercises the concurrent RX+TX path with a zero issue tally.
run "full-duplex" $RIGSTREAMTEST -m 1 -s $RATE --full-duplex --iq --rx-secs 1 --tx-secs 1 -d 2

echo "All rigstreamtest smoke tests passed."
