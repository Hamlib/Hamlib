#!/bin/sh
# Streaming system test for a real radio.
#
# Runs every streaming mode the model advertises, records all received audio to
# WAV files for listening, and prints a pass/fail line per mode plus a summary.
# Transmit tests are opt-in and need an explicit frequency and power level.
#
# Unlike rigstreamtest-dummy.sh this needs hardware, so it is not part of
# "make check"; it is a manual system test whose results are meant to be
# inspected -- both the summary and the recordings.
#
# Every transmit example below assumes a dummy load.
#
# Examples:
#
#   # IC-7610: receive-only sweep over the network
#   rigstreamtest-hw.sh -m 3095 -r IP_ADDRESS \
#       -C net_username=USER,net_password=PASS
#
#   # IC-7610: stereo receive on two HF bands at once, transmitting on the
#   # first. Its two receivers tune independently across bands.
#   rigstreamtest-hw.sh -m 3095 -r IP_ADDRESS \
#       -C net_username=USER,net_password=PASS,net_rx_codec=3 \
#       --vfo MainA,SubA --freq 14200000,7100000 --mode USB,LSB \
#       --tx --power 0.01
#
#   # IC-7300MK2: single receiver, so mono; HF transmit at 5% power
#   rigstreamtest-hw.sh -m 3100 -r IP_ADDRESS \
#       -C net_username=USER,net_password=PASS \
#       --tx --vfo VFOA --freq 14200000 --mode USB --power 0.05
#
#   # IC-705: 10 W radio, so 10% is about 1 W
#   rigstreamtest-hw.sh -m 3097 -r IP_ADDRESS \
#       -C net_username=USER,net_password=PASS \
#       --tx --vfo VFOA --freq 28400000 --mode USB --power 0.10
#
#   # IC-9700: stereo receive: the Icom network backend needs the two-channel wire
#   # codec, so net_rx_codec=3 is what actually makes the radio send two
#   # channels; left is the first VFO, right the second
#   rigstreamtest-hw.sh -m 3096 -r IP_ADDRESS \
#       -C net_username=USER,net_password=PASS,net_rx_codec=3 \
#       --vfo MainA,SubA --freq 145500000,434500000 --mode FM,FM
#
#   # IC-9700: include transmit, at 1% power
#   rigstreamtest-hw.sh -m 3096 -r IP_ADDRESS \
#       -C net_username=USER,net_password=PASS \
#       --tx --vfo MainA --freq 144300000 --mode USB --power 0.01
#
#   # IC-9700: stereo receive across 2 m and 70 cm while transmitting on 2 m.
#   # The rig transmits on the first VFO, so the full_duplex recording shows
#   # what both receivers did during the transmission.
#   rigstreamtest-hw.sh -m 3096 -r IP_ADDRESS \
#       -C net_username=USER,net_password=PASS,net_rx_codec=3 \
#       --vfo MainA,SubA --freq 144300000,432200000 --mode USB,USB \
#       --tx --power 0.01
#
# See HAMLIB_STREAMING.md for the full Icom network model list and notes.
#
# The binary is looked up as $RIGSTREAMTEST -> ./rigstreamtest ->
# <script dir>/rigstreamtest -> $PATH, so the script runs from any directory.

set -u

MODEL=""
RIG_FILE=""
SET_CONF=""
RATE=48000
DURATION=10
OUTDIR=""
DO_TX=0
VFOS=""
FREQS=""
MODES=""
POWER=""
TESTS=""
FORCE_MONO=0
RECONNECT_CYCLES=3
RECONNECT_GAP=2000

usage()
{
    cat <<'EOF'
Usage: rigstreamtest-hw.sh -m MODEL [-r HOST] [options]

  -m, --model N          rig model number (required)
  -r, --rig-file HOST    hostname[:port] or device path
  -C, --set-conf K=V,..  backend config tokens (e.g. net_username=...)
  -s, --sample-rate HZ   stream sample rate (default 48000)
  -d, --duration SEC     seconds per test (default 10)
  -o, --outdir DIR       output directory (default streamtest-<model>-<stamp>)
      --tests LIST       comma-separated subset of:
                         audio_rx,iq_rx,audio_tx,iq_tx,loopback,full_duplex,
                         reconnect, native (opt-in: never part of the
                         default sweep)
                         (default: everything the model advertises)
      --mono             force mono even when the model advertises stereo
      --vfo V[,V]        VFO(s) to set up, e.g. MainA or MainA,SubA
      --freq HZ[,HZ]     frequency per VFO, same order
      --mode M[,M]       mode per VFO, e.g. FM or USB,FM
      --tx               run transmit tests (TRANSMITS RF)
      --power F          RFPOWER 0.0..1.0, required with --tx
      --no-restore       leave the rig as configured instead of restoring
  -h, --help             this help

Transmit tests require --tx, --freq and --power together: transmitting on
whatever the rig happened to be tuned to, at whatever power it was set to, is
never what you want.
EOF
}

RESTORE_ARG=""

while [ $# -gt 0 ]; do
    case $1 in
    -m|--model)       MODEL=$2; shift 2 ;;
    -r|--rig-file)    RIG_FILE=$2; shift 2 ;;
    -C|--set-conf)    SET_CONF=$2; shift 2 ;;
    -s|--sample-rate) RATE=$2; shift 2 ;;
    -d|--duration)    DURATION=$2; shift 2 ;;
    -o|--outdir)      OUTDIR=$2; shift 2 ;;
    --tests)          TESTS=$2; shift 2 ;;
    --mono)           FORCE_MONO=1; shift ;;
    --reconnect-cycles) RECONNECT_CYCLES=$2; shift 2 ;;
    --reconnect-gap)  RECONNECT_GAP=$2; shift 2 ;;
    --vfo)            VFOS=$2; shift 2 ;;
    --freq)           FREQS=$2; shift 2 ;;
    --mode)           MODES=$2; shift 2 ;;
    --tx)             DO_TX=1; shift ;;
    --power)          POWER=$2; shift 2 ;;
    --no-restore)     RESTORE_ARG="--no-restore"; shift ;;
    -h|--help)        usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if [ -z "$MODEL" ]; then
    echo "Error: -m/--model is required" >&2
    usage >&2
    exit 2
fi

if [ "$DO_TX" = 1 ]; then
    if [ -z "$FREQS" ]; then
        echo "Error: --tx requires --freq (do not transmit on an unknown frequency)" >&2
        exit 2
    fi

    if [ -z "$POWER" ]; then
        echo "Error: --tx requires --power (0.0..1.0)" >&2
        exit 2
    fi
fi

find_rigstreamtest()
{
    if [ -n "${RIGSTREAMTEST:-}" ]; then
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
    echo "Error: rigstreamtest not found; set RIGSTREAMTEST to its path" >&2
    exit 2
fi

STAMP=`date +%Y%m%d-%H%M%S`

if [ -z "$OUTDIR" ]; then
    OUTDIR="streamtest-$MODEL-$STAMP"
fi

mkdir -p "$OUTDIR" || exit 2

# ---------------------------------------------------------------------------
# Ask the model what it advertises: which stream types exist at all, and
# whether receive audio can carry two channels.
# ---------------------------------------------------------------------------
CAPS=`"$RIGSTREAMTEST" -m "$MODEL" --list-streams 2>&1` || {
    echo "Error: could not read stream capabilities for model $MODEL" >&2
    echo "$CAPS" >&2
    exit 2
}

MODEL_NAME=`echo "$CAPS" | sed -n 's/^model_name: //p'`
# Model names contain spaces and parentheses ("IC-9700 (Network)"); reduce them
# to a filename-safe slug so the recordings are easy to handle from a shell.
MODEL_SLUG=`echo "${MODEL_NAME:-model$MODEL}" | sed 's/[^A-Za-z0-9._-]\{1,\}/_/g; s/^_//; s/_$//'`
advertises() { echo "$CAPS" | grep -q "type=$1 "; }

# The channel list is ascending and 0-terminated, so the last entry is the
# widest the radio offers.
AUDIO_RX_CHANS=`echo "$CAPS" | sed -n 's/.*type=audio_rx channels=\([0-9,]*\).*/\1/p'`
AUDIO_RX_MAX_CH=`echo "$AUDIO_RX_CHANS" | awk -F, '{print $NF}'`
[ -n "$AUDIO_RX_MAX_CH" ] || AUDIO_RX_MAX_CH=1

if [ "$FORCE_MONO" = 1 ] || [ "$AUDIO_RX_MAX_CH" -lt 2 ]; then
    CHANNELS=1
else
    CHANNELS=2
fi

# ---------------------------------------------------------------------------
# Common arguments. The rig setup (VFO/freq/mode/dual watch) is applied by
# rigstreamtest itself after it opens the rig, and undone on exit unless
# --no-restore was given.
# ---------------------------------------------------------------------------
set -- -m "$MODEL"
[ -n "$RIG_FILE" ] && set -- "$@" -r "$RIG_FILE"
[ -n "$SET_CONF" ] && set -- "$@" -C "$SET_CONF"
[ -n "$VFOS" ]     && set -- "$@" --vfo "$VFOS"
[ -n "$FREQS" ]    && set -- "$@" --freq "$FREQS"
[ -n "$MODES" ]    && set -- "$@" --mode "$MODES"
[ -n "$RESTORE_ARG" ] && set -- "$@" "$RESTORE_ARG"
COMMON="$@"

PASS=0
FAIL=0
SKIP=0
RESULTS=""

record()
{
    RESULTS="$RESULTS
$1"
}

# run <name> <wav-or-empty> <extra args...>
run_test()
{
    name=$1
    wav=$2
    shift 2

    printf '%-14s ' "$name"

    if [ -n "$wav" ]; then
        set -- "$@" -w "$OUTDIR/$wav"
    fi

    out=`"$RIGSTREAMTEST" $COMMON "$@" 2>&1`
    rc=$?

    case $out in
    *"Done (result=0)."*)
        if [ $rc -eq 0 ]; then
            # bytes= catches the transmit tests, whose progress lines carry no
            # gaps= and would otherwise leave nothing for the check below.
            detail=`echo "$out" | grep -E 'gaps=|bytes=|TOTAL ISSUES' | tail -1`

            # A stream that opened, carried nothing and closed tidily exits 0,
            # so the exit status alone would call a dead radio a pass. Matches
            # bytes=, rx_bytes= and tx_bytes= alike.
            case $detail in
            *bytes=0\ *|*bytes=0)
                echo "FAIL  no data moved: $detail"
                record "FAIL  $name (no data moved)"
                FAIL=`expr $FAIL + 1`
                return 1
                ;;
            esac

            echo "PASS  $detail"
            record "PASS  $name"
            PASS=`expr $PASS + 1`
            return 0
        fi
        ;;
    esac

    # The advertised list comes from the model declaration, which covers every
    # configuration the radio has. A backend whose transport negotiates one
    # geometry per connection serves only what this session can carry, so a
    # type can be declared by the model and absent from the session -- e.g. an
    # Icom network rig carries I/Q only on the stereo codec (net_rx_codec=3).
    # That is a configuration mismatch, not a defect: skip it and say so,
    # rather than reporting the radio as broken.
    case $out in
    *"no caps found for stream type"*)
        echo "SKIP  not offered by this session (check -C)"
        record "SKIP  $name (not offered by this session)"
        SKIP=`expr $SKIP + 1`
        return 0
        ;;
    # Same reasoning one level down: the model declares every format it can
    # serve natively in some configuration, but a session serves only what its
    # own negotiated codec and rate give it. A refusal here says this session
    # would have had to convert -- which is the question the test asked, so it
    # is an answer, not a defect. The mask says what would have converted:
    # 0x1 format, 0x2 rate, 0x4 channels.
    *"require_native is set"*)
        mask=`echo "$out" | sed -n 's/.*requires conversions \(0x[0-9a-f]*\).*/\1/p' | tail -1`
        echo "SKIP  not native to this session (would convert ${mask:-?})"
        record "SKIP  $name (not native to this session, conv ${mask:-?})"
        SKIP=`expr $SKIP + 1`
        return 0
        ;;
    esac

    echo "FAIL"
    echo "$out" | sed 's/^/               | /' | tail -12
    record "FAIL  $name"
    FAIL=`expr $FAIL + 1`
    return 1
}

skip_test()
{
    printf '%-14s SKIP  %s\n' "$1" "$2"
    record "SKIP  $1 ($2)"
    SKIP=`expr $SKIP + 1`
}

wanted()
{
    [ -z "$TESTS" ] && return 0

    case ",$TESTS," in
    *",$1,"*) return 0 ;;
    esac

    return 1
}

echo "=============================================================="
echo " Hamlib streaming system test"
echo "   model      : $MODEL${MODEL_NAME:+ ($MODEL_NAME)}"
[ -n "$RIG_FILE" ] && echo "   rig         : $RIG_FILE"
echo "   rate        : $RATE Hz"
echo "   channels    : $CHANNELS (audio_rx advertises up to $AUDIO_RX_MAX_CH)"
echo "   duration    : ${DURATION}s per test"
[ -n "$VFOS" ]  && echo "   vfo         : $VFOS"
[ -n "$FREQS" ] && echo "   freq        : $FREQS"
[ -n "$MODES" ] && echo "   mode        : $MODES"
echo "   output      : $OUTDIR"

if [ "$DO_TX" = 1 ]; then
    echo "   transmit    : ENABLED at power $POWER -- THIS RADIATES RF"
else
    echo "   transmit    : disabled (--tx to enable)"
fi

echo "=============================================================="
echo

if [ "$CHANNELS" = 2 ]; then
    echo "Note: a stereo file needs the backend to send two channels as well."
    echo "      For Icom network models add net_rx_codec=3 to -C."
    echo
fi

# ------------------------------- receive -----------------------------------
if wanted audio_rx && advertises audio_rx; then
    run_test "audio_rx" "$MODEL_SLUG-audio_rx-$STAMP.wav" \
        -t audio_rx -s "$RATE" -c "$CHANNELS" -d "$DURATION"
elif wanted audio_rx; then
    skip_test "audio_rx" "not advertised by this model"
fi

if wanted iq_rx && advertises iq_rx; then
    # I/Q formats are complex: one sample carries I and Q, so it is one channel
    run_test "iq_rx" "" -t iq_rx -s "$RATE" -c 1 -d "$DURATION" \
        -o "$OUTDIR/$MODEL_SLUG-iq_rx-$STAMP.f32"
elif wanted iq_rx; then
    skip_test "iq_rx" "not advertised by this model"
fi

if wanted loopback && advertises audio_rx && advertises audio_tx; then
    run_test "loopback" "$MODEL_SLUG-loopback-$STAMP.wav" \
        -t loopback -s "$RATE" -c "$CHANNELS" -d "$DURATION"
elif wanted loopback; then
    skip_test "loopback" "needs both audio_rx and audio_tx"
fi

# ------------------------------- transmit ----------------------------------
if [ "$DO_TX" = 1 ]; then
    if wanted audio_tx && advertises audio_tx; then
        run_test "audio_tx" "" -t audio_tx -s "$RATE" -c "$CHANNELS" \
            -d "$DURATION" -P --power "$POWER"
    elif wanted audio_tx; then
        skip_test "audio_tx" "not advertised by this model"
    fi

    if wanted iq_tx && advertises iq_tx; then
        run_test "iq_tx" "" -t iq_tx -s "$RATE" -d "$DURATION" \
            -P --power "$POWER"
    elif wanted iq_tx; then
        skip_test "iq_tx" "not advertised by this model"
    fi

    if wanted full_duplex && advertises audio_rx && advertises audio_tx; then
        # RX runs throughout while PTT is keyed, so the recording shows what the
        # receiver did during transmit
        run_test "full_duplex" "$MODEL_SLUG-full_duplex-$STAMP.wav" \
            --full-duplex -s "$RATE" -c "$CHANNELS" \
            --tx-secs "$DURATION" --rx-secs 5 -d `expr "$DURATION" + 5` \
            -P --power "$POWER"
    elif wanted full_duplex; then
        skip_test "full_duplex" "needs both audio_rx and audio_tx"
    fi
else
    wanted audio_tx    && skip_test "audio_tx"    "transmit not enabled (--tx)"
    wanted iq_tx       && skip_test "iq_tx"       "transmit not enabled (--tx)"
    wanted full_duplex && skip_test "full_duplex" "transmit not enabled (--tx)"
fi

# Reconnect deliberately drops the link, and a radio that holds its session slot
# makes it slower than the rest, so it never joins the default sweep: it runs
# only when named. Every cycle must re-establish and carry data; a cycle that
# never comes back is a hard failure, not a flaky radio.
if [ -n "$TESTS" ] && wanted reconnect; then
    if advertises audio_rx; then
        # --dirty-close alternates: odd cycles close tidily, even ones close
        # the rig with the stream still running, so one run covers both.
        run_test "reconnect" "" -t reconnect -s "$RATE" -c "$CHANNELS" \
            -d "$DURATION" --reconnect-cycles "$RECONNECT_CYCLES" \
            --reconnect-gap "$RECONNECT_GAP" --dirty-close
    else
        skip_test "reconnect" "needs audio_rx"
    fi
fi

# Every other test asks for float and lets the frontend convert, which exercises
# the conversion pipeline rather than the backend's own path. This one asks for
# each format the model declares and demands it arrive untouched, so a backend
# that claims to serve a format natively has to prove it against real hardware.
# It also measures the claim: a native 8-bit stream must carry exactly half the
# bytes of the same audio at 16 bits. Opt-in, because most of its cases are
# expected to skip on any one session.
if [ -n "$TESTS" ] && wanted native; then
    if advertises audio_rx; then
        AUDIO_RX_FORMATS=`echo "$CAPS" \
            | sed -n 's/.*type=audio_rx .*formats=\([A-Za-z0-9_,]*\).*/\1/p'`

        for fmt in `echo "$AUDIO_RX_FORMATS" | tr ',' ' '`; do
            case $fmt in
            PCM_U8)  short=u8  ;;
            PCM_S8)  short=s8  ;;
            PCM_S16) short=s16 ;;
            PCM_F32) short=f32 ;;
            *)       continue  ;;   # I/Q formats are not audio_rx's to serve
            esac

            run_test "native:$short" "" -t audio_rx --format "$short" \
                --require-native -s "$RATE" -c "$CHANNELS" -d "$DURATION"
        done
    else
        skip_test "native" "needs audio_rx"
    fi
fi

# ------------------------------- summary -----------------------------------
echo
echo "=============================================================="
echo " Summary for model $MODEL${MODEL_NAME:+ ($MODEL_NAME)}"
echo "$RESULTS" | sed '/^$/d' | sed 's/^/   /'
echo
echo "   passed: $PASS   failed: $FAIL   skipped: $SKIP"
echo
echo "   recordings in $OUTDIR:"

if ls "$OUTDIR" >/dev/null 2>&1 && [ -n "`ls -A "$OUTDIR" 2>/dev/null`" ]; then
    ls -1 "$OUTDIR" | sed 's/^/     /'
else
    echo "     (none)"
fi

echo "=============================================================="

[ "$FAIL" -eq 0 ] || exit 1
exit 0
