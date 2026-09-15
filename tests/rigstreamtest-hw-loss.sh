#!/bin/sh
# Packet-loss and source-failure test for a network radio, on real hardware.
#
# rigstreamtest-hw.sh checks that every stream mode works on a clean link.
# This script makes the link lossy on purpose and checks what the streaming
# subsystem does about it:
#
#   window   For each loss rate and each receive reorder window
#            (net_rx_latency), a receive run: how many losses are reported,
#            how many retransmits are requested, how many late packets are
#            dropped. Shows how much a window recovers, which is what to base
#            a net_rx_latency choice on.
#   silence  The radio goes silent mid-stream: the stream must end with
#            -RIG_EIO and fail_reason LINK_TIMEOUT, not idle forever.
#   chain    The same silence behind rigctld: a netrigctl client (model 2) must
#            get -RIG_EIO through rigctld's ERROR frame.
#   handshake
#            Opening the rig while the link is lossy (the first --loss rate),
#            several times: the session handshake must recover lost replies,
#            and a failed attempt must not lock the radio out of the next.
#
# It is not part of "make check": it needs a radio and root.
#
# HOW IT MAKES LOSS (macOS or Linux)
#
# A random fraction of the UDP packets arriving FROM the radio is dropped;
# nothing sent to it is touched. The method follows the OS (--loss-tool
# overrides it):
#
#   macOS, dummynet:
#     - a pf reference is taken with "pfctl -E" and released with "pfctl -X"
#       (the stock /etc/pf.conf is loaded first only if no ruleset is loaded);
#     - one rule is added in its own anchor, com.apple/hamlib-loss, sending UDP
#       from the radio's address through dummynet pipe 7171;
#     - the pipe's packet-loss rate is changed between runs.
#   Linux, nftables ("nft"; used when installed):
#     - a table of its own, inet hamlib_loss, with one input chain;
#     - its single rule drops UDP from the radio's address at random
#       ("numgen random") and is replaced between runs.
#   Linux, iptables (used when nft is not installed):
#     - a chain of its own, HAMLIB_LOSS, jumped to from the top of INPUT for
#       UDP from the radio's address;
#     - its single rule drops at random ("-m statistic --mode random") and is
#       replaced between runs.
#
# Everything is removed when the script exits, including on Ctrl-C; it prints
# "loss injection removed". Nothing else on the machine is affected. The
# stream tools themselves run as the invoking user, not as root.
#
# RUNNING IT
#
# Build first (plain "make" builds rigstreamtest and rigctld). Then, from the
# build tree, with the radio idle (no other client connected):
#
#   sudo ./tests/rigstreamtest-hw-loss.sh -m 3095 -r IP_ADDRESS \
#       -C net_username=USER,net_password=PASS
#
# -r must be the radio's IP address (the loss rule matches it). The -C string
# is visible to other local users while it runs, as with rigstreamtest-hw.sh;
# start the command with a space to keep it out of shell history where the
# shell honours that. The default run takes about 8 minutes. Only receive is
# used: nothing is transmitted.
#
# WHAT TO REPORT
#
# Loss is applied only once each stream is open, so the numbers describe the
# stream, not the session handshake; a run whose rig never opens (twice) is
# reported as SETUP FAIL rather than as a stream result.
#
# Everything needed is in <outdir>/summary.txt, printed at the end:
#   - one line per loss rate and window: gaps (unsized), lost packets,
#     concealed samples, retransmit requests, late drops, resyncs, underruns;
#   - PASS/FAIL for silence and chain, with the lines that decided it.
# Send summary.txt. The per-run logs next to it (trace level) are only needed
# if something looks wrong. Expected on a working build:
#   - at 0 ms: losses reported (gaps > 0), retx_requests = 0, late_drops small;
#   - above 0 ms: retx_requests > 0, and gaps falling as the window grows,
#     until the window covers the radio's retransmit latency;
#   - silence and chain: PASS;
#   - handshake: every attempt opened, in a few seconds each.
#
# Examples:
#
#   # IC-7610, the default sweep (1% and 5% loss; 0, 20, 50, 100, 200 ms)
#   sudo ./tests/rigstreamtest-hw-loss.sh -m 3095 -r IP_ADDRESS \
#       -C net_username=USER,net_password=PASS
#
#   # a finer window sweep at 2% loss only, 30 s per run
#   sudo ./tests/rigstreamtest-hw-loss.sh -m 3095 -r IP_ADDRESS \
#       -C net_username=USER,net_password=PASS \
#       --loss 0.02 --windows 0,10,20,30,40,60,80 -d 30 --tests window

set -u

MODEL=""
RADIO=""
SET_CONF=""
RATE=48000
CHANNELS=1
DURATION=20
OUTDIR=""
LOSSES="0.01,0.05"
WINDOWS="0,20,50,100,200"
TESTS="window,silence,chain,handshake"
HANDSHAKE_TRIES=10
CHAIN_PORT=4633
LOSS_TOOL=""
PIPE=7171
ANCHOR=com.apple/hamlib-loss
NFT_TABLE=hamlib_loss
IPT_CHAIN=HAMLIB_LOSS

usage()
{
    cat <<'EOF'
Usage: sudo rigstreamtest-hw-loss.sh -m MODEL -r IP_ADDRESS -C K=V,.. [options]

  -m, --model N          rig model number (required)
  -r, --rig-file IP      the radio's IP address (required; the loss rule
                         matches it)
  -C, --set-conf K=V,..  backend config tokens (e.g. net_username=...)
  -s, --sample-rate HZ   stream sample rate (default 48000)
  -c, --channels N       stream channels (default 1)
  -d, --duration SEC     seconds per window run (default 20)
  -o, --outdir DIR       output directory (default streamloss-<model>-<stamp>)
      --loss LIST        comma-separated loss rates 0..1 (default 0.01,0.05)
      --windows LIST     comma-separated net_rx_latency values in ms
                         (default 0,20,50,100,200)
      --tests LIST       subset of window,silence,chain,handshake (default all)
      --handshake-tries N  rig opens under loss in the handshake test (default 10)
      --chain-port N     local rigctld port for the chain test (default 4633)
      --loss-tool T      dummynet (macOS), nft or iptables (Linux); default:
                         dummynet on macOS, nft on Linux if installed, else
                         iptables
  -h, --help             this help

Runs as root (to change the packet filter) on macOS or Linux; the stream tools
run as $SUDO_USER.
Report <outdir>/summary.txt.
EOF
}

while [ $# -gt 0 ]; do
    case $1 in
    -m|--model)       MODEL=$2; shift 2 ;;
    -r|--rig-file)    RADIO=$2; shift 2 ;;
    -C|--set-conf)    SET_CONF=$2; shift 2 ;;
    -s|--sample-rate) RATE=$2; shift 2 ;;
    -c|--channels)    CHANNELS=$2; shift 2 ;;
    -d|--duration)    DURATION=$2; shift 2 ;;
    -o|--outdir)      OUTDIR=$2; shift 2 ;;
    --loss)           LOSSES=$2; shift 2 ;;
    --windows)        WINDOWS=$2; shift 2 ;;
    --tests)          TESTS=$2; shift 2 ;;
    --chain-port)     CHAIN_PORT=$2; shift 2 ;;
    --handshake-tries) HANDSHAKE_TRIES=$2; shift 2 ;;
    --loss-tool)      LOSS_TOOL=$2; shift 2 ;;
    -h|--help)        usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if [ -z "$MODEL" ] || [ -z "$RADIO" ]; then
    echo "Error: -m and -r are required" >&2
    usage >&2
    exit 2
fi

case $RADIO in
*[!0-9.]*|"")
    echo "Error: -r must be the radio's IPv4 address, got '$RADIO'" >&2
    exit 2
    ;;
esac

for plr in `echo "$LOSSES" | tr ',' ' '`; do
    if ! echo "$plr" | grep -Eq '^(0|1|0?\.[0-9]+|1\.0*)$'; then
        echo "Error: loss rates must be numbers from 0 to 1, got '$plr'" >&2
        exit 2
    fi
done

if [ "`id -u`" != 0 ] || [ -z "${SUDO_USER:-}" ]; then
    echo "Error: run with sudo from your own account (changing the packet" \
         "filter needs root; the tests run as you)" >&2
    exit 2
fi

if [ -z "$LOSS_TOOL" ]; then
    case `uname -s` in
    Darwin)
        LOSS_TOOL=dummynet
        ;;
    Linux)
        if command -v nft >/dev/null 2>&1; then
            LOSS_TOOL=nft
        else
            LOSS_TOOL=iptables
        fi
        ;;
    *)
        echo "Error: unsupported OS `uname -s`; this script needs macOS" \
             "(dummynet) or Linux (nftables or iptables)" >&2
        exit 2
        ;;
    esac
fi

case $LOSS_TOOL in
dummynet) needed="dnctl pfctl" ;;
nft)      needed="nft" ;;
iptables) needed="iptables" ;;
*)
    echo "Error: --loss-tool must be dummynet, nft or iptables" >&2
    exit 2
    ;;
esac

for tool in $needed; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Error: $tool not found; needed for --loss-tool $LOSS_TOOL" >&2
        exit 2
    fi
done

wanted()
{
    case ",$TESTS," in
    *",$1,"*) return 0 ;;
    *) return 1 ;;
    esac
}

# The tools, as absolute paths: sudo -u does not keep the caller's PATH.
find_tool()
{
    script_dir=`cd "\`dirname "$0"\`" && pwd`

    for candidate in "./$1" "$script_dir/$1"; do
        if [ -x "$candidate" ]; then
            echo "`cd "\`dirname "$candidate"\`" && pwd`/$1"
            return
        fi
    done

    command -v "$1"
}

RIGSTREAMTEST=${RIGSTREAMTEST:-`find_tool rigstreamtest`}
RIGCTLD=${RIGCTLD:-`find_tool rigctld`}
RIGCTL=${RIGCTL:-`find_tool rigctl`}

for tool in "$RIGSTREAMTEST" "$RIGCTLD" "$RIGCTL"; do
    if [ -z "$tool" ] || [ ! -x "$tool" ]; then
        echo "Error: rigstreamtest, rigctld and rigctl must be built" \
             "(or set RIGSTREAMTEST, RIGCTLD, RIGCTL)" >&2
        exit 2
    fi
done

STAMP=`date +%Y%m%d-%H%M%S`
OUTDIR=${OUTDIR:-streamloss-$MODEL-$STAMP}
mkdir -p "$OUTDIR" || exit 2
chown "$SUDO_USER" "$OUTDIR"
SUMMARY="$OUTDIR/summary.txt"
: > "$SUMMARY"
chown "$SUDO_USER" "$SUMMARY"

TOKEN=""

# Remove what loss_setup added (also any leftover of an earlier, killed run).
loss_remove()
{
    case $LOSS_TOOL in
    dummynet)
        pfctl -a "$ANCHOR" -F all >/dev/null 2>&1
        dnctl pipe "$PIPE" delete >/dev/null 2>&1

        if [ -n "$TOKEN" ]; then
            pfctl -X "$TOKEN" >/dev/null 2>&1
        fi
        ;;
    nft)
        nft delete table inet "$NFT_TABLE" >/dev/null 2>&1
        ;;
    iptables)
        # Every copy of the jump (a killed run may have left one), but bounded.
        n=0

        while [ $n -lt 10 ] && iptables -D INPUT -s "$RADIO" -p udp \
                -j "$IPT_CHAIN" >/dev/null 2>&1; do
            n=`expr $n + 1`
        done

        iptables -F "$IPT_CHAIN" >/dev/null 2>&1
        iptables -X "$IPT_CHAIN" >/dev/null 2>&1
        ;;
    esac
}

cleanup()
{
    loss_remove

    pkill -f "rigctld -m $MODEL -r $RADIO .* -t $CHAIN_PORT" >/dev/null 2>&1
    echo "loss injection removed"
}

trap cleanup EXIT
trap 'exit 130' INT TERM

say()
{
    echo "$*"
    echo "$*" >> "$SUMMARY"
}

as_user()
{
    sudo -u "$SUDO_USER" "$@"
}

# Set the fraction (0..1) of the radio's UDP packets that is dropped.
loss()
{
    case $LOSS_TOOL in
    dummynet)
        dnctl pipe "$PIPE" config plr "$1" || exit 1
        ;;
    nft)
        nft flush chain inet "$NFT_TABLE" input || exit 1

        if awk "BEGIN { exit !($1 > 0) }"; then
            per10k=`awk "BEGIN { printf \"%d\", $1 * 10000 + 0.5 }"`
            nft "add rule inet $NFT_TABLE input ip saddr $RADIO" \
                "meta l4proto udp numgen random mod 10000 < $per10k drop" \
                || exit 1
        fi
        ;;
    iptables)
        iptables -F "$IPT_CHAIN" || exit 1

        if awk "BEGIN { exit !($1 >= 1) }"; then
            iptables -A "$IPT_CHAIN" -j DROP || exit 1
        elif awk "BEGIN { exit !($1 > 0) }"; then
            iptables -A "$IPT_CHAIN" -m statistic --mode random \
                --probability "$1" -j DROP || exit 1
        fi
        ;;
    esac
}

# Milliseconds since the epoch.
now_ms()
{
    perl -MTime::HiRes=time -e 'printf "%d", time*1000' 2>/dev/null \
        || date +%s%3N
}

# --- set up the loss rule ---------------------------------------------------

loss_remove

case $LOSS_TOOL in
dummynet)
    if ! pfctl -s rules 2>/dev/null | grep -q .; then
        pfctl -f /etc/pf.conf >/dev/null 2>&1
    fi

    TOKEN=`pfctl -E 2>&1 | sed -n 's/^Token : //p'`
    loss 0

    if ! echo "dummynet in quick proto udp from $RADIO to any pipe $PIPE" \
            | pfctl -a "$ANCHOR" -f - >/dev/null 2>&1; then
        echo "Error: could not load the dummynet rule into anchor $ANCHOR" >&2
        exit 1
    fi
    ;;
nft)
    if ! nft add table inet "$NFT_TABLE" \
            || ! nft "add chain inet $NFT_TABLE input" \
                     "{ type filter hook input priority 0; policy accept; }"; then
        echo "Error: could not create nftables table inet $NFT_TABLE" >&2
        exit 1
    fi
    ;;
iptables)
    if ! iptables -N "$IPT_CHAIN" \
            || ! iptables -I INPUT 1 -s "$RADIO" -p udp -j "$IPT_CHAIN"; then
        echo "Error: could not create iptables chain $IPT_CHAIN" >&2
        exit 1
    fi
    ;;
esac

# Try a partial loss once, so a missing random-drop feature (nft numgen,
# the iptables statistic match) shows up now, not in the middle of a run.
if ! (loss 0.5 && loss 0); then
    echo "Error: $LOSS_TOOL could not set a random packet loss" >&2
    exit 1
fi

say "rigstreamtest-hw-loss: model $MODEL at $RADIO, $RATE Hz, $CHANNELS ch, $STAMP ($LOSS_TOOL)"

# Run one receive stream as the user (command in "$@", output to $1) and make
# the link lossy only once the stream is open: the loss is aimed at the stream,
# not at the session handshake, which does not tolerate much of it. $2 is the
# loss rate, $3 the seconds to wait after the open before applying it. A radio
# that refuses the open -- typically because an earlier session still holds its
# slot -- gets one more attempt after a pause. Returns 1 if it never opened.
lossy_run()
{
    run_log=$1
    run_plr=$2
    run_delay=$3
    shift 3
    attempt=1

    while :; do
        loss 0
        as_user "$@" > "$run_log" 2>&1 &
        run_pid=$!
        opened=0
        waited=0

        while [ $waited -lt 60 ] && kill -0 $run_pid 2>/dev/null; do
            if grep -q '^Stream AUDIO_RX:' "$run_log"; then
                opened=1
                break
            fi

            sleep 1
            waited=`expr $waited + 1`
        done

        if [ $opened = 1 ]; then
            sleep "$run_delay"
            loss "$run_plr"
            wait $run_pid
            loss 0
            return 0
        fi

        wait $run_pid

        if [ $attempt -ge 2 ]; then
            return 1
        fi

        attempt=2
        sleep 45
    done
}

# The value of KEY=N in a stats line.
stat_of()
{
    echo "$1" | sed -n "s/.*[ (]$2=\\([0-9]*\\).*/\\1/p"
}

# --- window: loss rate x reorder window ------------------------------------

if wanted window; then
    say ""
    say "window runs (${DURATION}s each):"

    for plr in `echo "$LOSSES" | tr ',' ' '`; do
        for win in `echo "$WINDOWS" | tr ',' ' '`; do
            log="$OUTDIR/window-plr$plr-${win}ms.log"

            if ! lossy_run "$log" "$plr" 0 "$RIGSTREAMTEST" -m "$MODEL" \
                    -r "$RADIO" -C "${SET_CONF:+$SET_CONF,}net_rx_latency=$win" \
                    -t audio_rx -s "$RATE" -c "$CHANNELS" -d "$DURATION" -vvvvv; then
                say "  plr=$plr window=${win}ms  SETUP FAIL: the rig did not open" \
                    "(twice; see `basename "$log"`)"
                sleep 10
                continue
            fi

            last=`grep -E '^\[ *[0-9]+s\] bytes=' "$log" | tail -1`
            gaps=`echo "$last" | sed -n 's/.*gaps=\([0-9]*\)(\([0-9]*\) unsized).*/\1 \2/p'`
            concealed=`echo "$last" | sed -n 's/.*concealed(gap\/ovr)=\([0-9]*\)\/\([0-9]*\).*/\1/p'`
            underruns=`stat_of "$last" underruns`
            lost=`sed -n 's/.*: lost \([0-9]*\) packet(s).*/\1/p' "$log" \
                  | awk '{ s += $1 } END { print s + 0 }'`
            requests=`grep -c 'requesting .* audio packet' "$log"`
            late=`grep -c 'late audio packet' "$log"`
            resyncs=`grep -c 'audio sequence resync' "$log"`

            if [ -z "$last" ]; then
                say "  plr=$plr window=${win}ms  NO DATA (see `basename "$log"`)"
            else
                say "  plr=$plr window=${win}ms  gaps=${gaps% *} unsized=${gaps#* }" \
                    "lost_packets=$lost concealed_samples=${concealed:-?}" \
                    "retx_requests=$requests late_drops=$late resyncs=$resyncs" \
                    "underruns=${underruns:-?}"
            fi

            sleep 10
        done
    done
fi

# --- silence: total loss mid-stream -----------------------------------------

if wanted silence; then
    say ""
    log="$OUTDIR/silence.log"
    sleep 10

    if ! lossy_run "$log" 1 5 "$RIGSTREAMTEST" -m "$MODEL" -r "$RADIO" \
            -C "$SET_CONF" -t audio_rx -s "$RATE" -c "$CHANNELS" -d 60; then
        say "silence: SETUP FAIL  (the rig did not open, twice; see silence.log)"
        err=""
        fail="setup"
    else
        err=`grep 'rig_stream_read error' "$log" | head -1`
        fail=`grep -o 'failed=[A-Z_]*' "$log" | tail -1`
    fi

    if [ "$fail" = "setup" ]; then
        :
    elif [ -n "$err" ] && [ "$fail" = "failed=LINK_TIMEOUT" ]; then
        say "silence: PASS  ($fail; $err)"
    else
        say "silence: FAIL  (read error: '${err:-none}'; ${fail:-no failed=};" \
            "see silence.log)"
    fi
fi

# --- chain: the same behind rigctld -----------------------------------------

if wanted chain; then
    say ""
    dlog="$OUTDIR/chain-rigctld.log"
    clog="$OUTDIR/chain-client.log"

    # A session lost to silence keeps its slot on the radio for a while, so
    # wait until rigctld has really got the rig before starting the client.
    sleep 20
    as_user "$RIGCTLD" -m "$MODEL" -r "$RADIO" -C "$SET_CONF" -t "$CHAIN_PORT" \
        > "$dlog" 2>&1 &
    dpid=$!
    ready=0
    tries=0

    while [ $tries -lt 45 ]; do
        if as_user "$RIGCTL" -m 2 -r "127.0.0.1:$CHAIN_PORT" f >/dev/null 2>&1; then
            ready=1
            break
        fi

        sleep 2
        tries=`expr $tries + 1`
    done

    if [ $ready = 0 ]; then
        say "chain: FAIL  (rigctld never answered on port $CHAIN_PORT; see chain-rigctld.log)"
    else
        lossy_run "$clog" 1 5 "$RIGSTREAMTEST" -m 2 -r "127.0.0.1:$CHAIN_PORT" \
            -t audio_rx -s "$RATE" -c "$CHANNELS" -d 60

        reported=`grep 'server reports stream' "$clog" | head -1`
        err=`grep 'rig_stream_read error' "$clog" | head -1`
        fail=`grep -o 'failed=[A-Z_]*' "$clog" | tail -1`

        if [ -n "$reported" ] && [ -n "$err" ] \
                && [ "$fail" = "failed=LINK_TIMEOUT" ]; then
            say "chain: PASS  ($fail; ERROR frame received)"
        elif grep -q 'rig_open failed' "$clog"; then
            say "chain: FAIL  (netrigctl could not open the rig through rigctld;" \
                "see chain-client.log)"
        else
            say "chain: FAIL  (ERROR frame: ${reported:+yes}${reported:-no};" \
                "read error: '${err:-none}'; ${fail:-no failed=}; see chain-client.log)"
        fi
    fi

    kill $dpid 2>/dev/null
    wait $dpid 2>/dev/null
fi

# --- handshake: open the rig on a lossy link --------------------------------

if wanted handshake; then
    say ""
    plr=`echo "$LOSSES" | cut -d, -f1`
    log="$OUTDIR/handshake.log"
    : > "$log"
    chown "$SUDO_USER" "$log"
    opened=0
    total_ms=0
    try=1
    sleep 20
    loss "$plr"

    while [ $try -le "$HANDSHAKE_TRIES" ]; do
        start=`now_ms`

        echo "--- attempt $try" >> "$log"

        if as_user "$RIGCTL" -m "$MODEL" -r "$RADIO" -C "$SET_CONF" f \
                >> "$log" 2>&1; then
            end=`now_ms`
            opened=`expr $opened + 1`
            total_ms=`expr $total_ms + $end - $start`
            echo "--- attempt $try opened in `expr $end - $start` ms" >> "$log"
        else
            echo "--- attempt $try FAILED" >> "$log"
        fi

        try=`expr $try + 1`
        sleep 5
    done

    loss 0

    if [ $opened -gt 0 ]; then
        avg=`expr $total_ms / $opened`
    else
        avg="-"
    fi

    if [ $opened -eq "$HANDSHAKE_TRIES" ]; then
        verdict=PASS
    else
        verdict=FAIL
    fi

    say "handshake: $verdict  ($opened/$HANDSHAKE_TRIES opened at plr=$plr," \
        "average $avg ms; see handshake.log)"
fi

say ""
say "logs and summary in $OUTDIR"
