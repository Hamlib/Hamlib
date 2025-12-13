#!/bin/bash
#
# FTX-1 Hamlib Test Harness (Modular Version)
# Main harness that sources and runs 13 test modules organized by command group.
#
# Usage: ./ftx1_test_main.sh <port> [baudrate] [options]
#
# Options:
#   -t, --tx-tests     Enable TX-related tests (PTT, tuner, keyer, etc.)
#   -o, --optima       Enable Optima/SPA-1 specific tests
#   -g, --group GROUP  Run only specific test group(s), comma-separated
#                      Groups: vfo, freq, mode, filter, audio, function,
#                              noise, preamp, cw, tx, memory, info, misc, power
#   -v, --verbose      Verbose output from rigctl
#   -h, --help         Show this help message
#
# Examples:
#   ./ftx1_test_main.sh /dev/cu.SLAB_USBtoUART
#   ./ftx1_test_main.sh /dev/cu.SLAB_USBtoUART --tx-tests --optima
#   ./ftx1_test_main.sh /dev/cu.SLAB_USBtoUART -g vfo,freq,mode

set -o pipefail

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Default values
VERBOSE=""
TX_TESTS=0
OPTIMA_ENABLED=0
GROUPS_FILTER=""
BAUD=38400
PORT=""

# Show help
show_help() {
    cat << 'EOF'
FTX-1 Hamlib Test Harness (Modular)

Usage: ./ftx1_test_main.sh <port> [baudrate] [options]

Arguments:
  port        Serial port (required)
  baudrate    Baud rate (default: 38400)

Options:
  -t, --tx-tests     Enable TX-related tests (PTT, tuner, keyer, etc.)
  -o, --optima       Enable Optima/SPA-1 specific tests
  -g, --group GROUP  Run only specific test group(s), comma-separated
  -v, --verbose      Verbose output from rigctl
  -h, --help         Show this help message

Test Groups:
  vfo      - VFO select, split, TX VFO, A/B copy
  freq     - Frequency control, band up/down, RIT/XIT
  mode     - Operating mode, filter number
  filter   - Beat cancel, contour, notch, width
  audio    - AF/RF gain, mic gain, AGC, squelch, meters
  function - Compressor, monitor, lock, tone, TSQL
  noise    - Noise blanker, noise reduction, attenuator
  preamp   - Preamp, RF attenuator
  cw       - Key speed, break-in, keyer memory
  tx       - PTT, VOX, tuner, MOX (requires --tx-tests)
  memory   - Memory read/write, memory zones
  info     - Radio ID, IF info, meters (read-only)
  misc     - CTCSS, scan, date/time, misc commands
  power    - RF power, SPA-1 power settings (requires --optima)

Examples:
  ./ftx1_test_main.sh /dev/cu.SLAB_USBtoUART
  ./ftx1_test_main.sh /dev/cu.SLAB_USBtoUART 38400
  ./ftx1_test_main.sh /dev/cu.SLAB_USBtoUART --tx-tests --optima
  ./ftx1_test_main.sh /dev/cu.SLAB_USBtoUART -g vfo,freq,mode

EOF
    exit 0
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            ;;
        -t|--tx-tests)
            TX_TESTS=1
            shift
            ;;
        -o|--optima)
            OPTIMA_ENABLED=1
            shift
            ;;
        -g|--group)
            GROUPS_FILTER="$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE="-v"
            shift
            ;;
        -*)
            echo "Unknown option: $1" 1>&2
            echo "Use -h or --help for usage information." 1>&2
            exit 1
            ;;
        *)
            if [ -z "$PORT" ]; then
                PORT="$1"
            elif [ -z "$BAUD_SET" ]; then
                BAUD="$1"
                BAUD_SET=1
            else
                echo "Unexpected argument: $1" 1>&2
                exit 1
            fi
            shift
            ;;
    esac
done

if [ -z "$PORT" ]; then
    echo "Error: Port is required." 1>&2
    echo "Usage: $0 <port> [baudrate] [options]" 1>&2
    echo "Use -h or --help for more information." 1>&2
    exit 1
fi

MODEL=1051
RIGCTL="rigctl -m $MODEL -r $PORT -s $BAUD $VERBOSE"

# Output files (overwritten each run)
SUCCESS_FILE="ftx1_test_success.txt"
FAILURE_FILE="ftx1_test_failure.txt"

# Counters
PASS_COUNT=0
FAIL_COUNT=0
SKIP_COUNT=0

# Initialize output files
init_files() {
    TIMESTAMP=$(date "+%Y-%m-%d %H:%M:%S")
    cat > "$SUCCESS_FILE" << EOF
FTX-1 Hamlib Test Results - Successes
Timestamp: $TIMESTAMP
Total Passed: (updating...)
==================================================

EOF
    cat > "$FAILURE_FILE" << EOF
FTX-1 Hamlib Test Results - Failures
Timestamp: $TIMESTAMP
Total Failed: (updating...)
==================================================

EOF
}

log_pass() {
    echo "[PASS] $1"
    echo "[PASS] $1" >> "$SUCCESS_FILE"
    ((PASS_COUNT++))
}

log_fail() {
    echo "[FAIL] $1"
    echo "[FAIL] $1" >> "$FAILURE_FILE"
    ((FAIL_COUNT++))
}

log_skip() {
    echo "[SKIP] $1"
    ((SKIP_COUNT++))
}

# Run rigctl command and return output
run_cmd() {
    $RIGCTL "$@" 2>/dev/null
}

# Send raw CAT command and get response
raw_cmd() {
    local cmd="$1"
    $RIGCTL w "$cmd;" 2>/dev/null | tr -d ';\n\r'
}

# Check if a group should be tested
should_test_group() {
    local group="$1"
    if [ -z "$GROUPS_FILTER" ]; then
        return 0  # No filter, test all
    fi
    local filter_lower=$(echo "$GROUPS_FILTER" | tr '[:upper:]' '[:lower:]')
    local group_lower=$(echo "$group" | tr '[:upper:]' '[:lower:]')
    if [[ ",$filter_lower," == *",$group_lower,"* ]]; then
        return 0
    fi
    return 1
}

# Source all test modules
source "$SCRIPT_DIR/ftx1_test_vfo.sh"
source "$SCRIPT_DIR/ftx1_test_freq.sh"
source "$SCRIPT_DIR/ftx1_test_mode.sh"
source "$SCRIPT_DIR/ftx1_test_filter.sh"
source "$SCRIPT_DIR/ftx1_test_audio.sh"
source "$SCRIPT_DIR/ftx1_test_function.sh"
source "$SCRIPT_DIR/ftx1_test_noise.sh"
source "$SCRIPT_DIR/ftx1_test_preamp.sh"
source "$SCRIPT_DIR/ftx1_test_cw.sh"
source "$SCRIPT_DIR/ftx1_test_tx.sh"
source "$SCRIPT_DIR/ftx1_test_memory.sh"
source "$SCRIPT_DIR/ftx1_test_info.sh"
source "$SCRIPT_DIR/ftx1_test_power.sh"
source "$SCRIPT_DIR/ftx1_test_misc.sh"

# ============================================================
# Main
# ============================================================

echo "============================================================"
echo "FTX-1 Hamlib Test Harness (Modular)"
echo "============================================================"
echo "Port: $PORT"
echo "Baudrate: $BAUD"
echo "TX Tests: $([ "$TX_TESTS" -eq 1 ] && echo 'ENABLED' || echo 'DISABLED')"
echo "Optima (SPA-1): $([ "$OPTIMA_ENABLED" -eq 1 ] && echo 'ENABLED' || echo 'DISABLED')"
if [ -n "$GROUPS_FILTER" ]; then
    echo "Groups: $GROUPS_FILTER"
else
    echo "Groups: ALL"
fi
echo "============================================================"
echo ""

# TX test confirmation
if [ "$TX_TESTS" -eq 1 ]; then
    echo "WARNING: TX tests can cause transmission!"
    echo "Ensure a dummy load or antenna is connected."
    read -p "Type 'YES' to confirm: " response
    if [ "$response" != "YES" ]; then
        echo "TX tests cancelled."
        TX_TESTS=0
    fi
    echo ""
fi

init_files

# Run selected test groups
if should_test_group "vfo"; then
    run_vfo_tests
fi

if should_test_group "freq"; then
    run_freq_tests
fi

if should_test_group "mode"; then
    run_mode_tests
fi

if should_test_group "filter"; then
    run_filter_tests
fi

if should_test_group "audio"; then
    run_audio_tests
fi

if should_test_group "function"; then
    run_function_tests
fi

if should_test_group "noise"; then
    run_noise_tests
fi

if should_test_group "preamp"; then
    run_preamp_tests
fi

if should_test_group "cw"; then
    run_cw_tests
fi

if should_test_group "tx"; then
    run_tx_tests
fi

if should_test_group "memory"; then
    run_memory_tests
fi

if should_test_group "info"; then
    run_info_tests
fi

if should_test_group "power"; then
    run_power_tests
fi

if should_test_group "misc"; then
    run_misc_tests
fi

# Update totals
if [[ "$OSTYPE" == "darwin"* ]]; then
    sed -i '' "s/Total Passed: (updating...)/Total Passed: $PASS_COUNT/" "$SUCCESS_FILE"
    sed -i '' "s/Total Failed: (updating...)/Total Failed: $FAIL_COUNT/" "$FAILURE_FILE"
else
    sed -i "s/Total Passed: (updating...)/Total Passed: $PASS_COUNT/" "$SUCCESS_FILE"
    sed -i "s/Total Failed: (updating...)/Total Failed: $FAIL_COUNT/" "$FAILURE_FILE"
fi

echo "============================================================"
echo "Summary: $PASS_COUNT passed, $FAIL_COUNT failed, $SKIP_COUNT skipped"
echo "Results written to $SUCCESS_FILE and $FAILURE_FILE"
echo "============================================================"

exit $FAIL_COUNT
