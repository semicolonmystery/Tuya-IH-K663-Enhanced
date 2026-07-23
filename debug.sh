#!/usr/bin/env bash
# ==============================================================================
# debug.sh — monitor the IH-K663 TX-only debug UART (115200 8N1) on a Pi CM4.
#
# The firmware bit-bangs debug output on DEBUG_TX_PIN (default PB1, see
# app_config.h). Wire that pad to the Pi UART's RX.
#
# NOTE: With the 4-wire TlsrComProg825x flashing setup, the Pi RXD is already
# permanently connected to the Tuya TX pad (PB1). You do not need to move any
# wires between flashing and debugging! Just run flash.sh, then run debug.sh.
#
#   ./debug.sh            # stream lines from $PORT
#   PORT=/dev/ttyUSB0 ./debug.sh
#   ./debug.sh -t         # prefix each line with a host timestamp
# ==============================================================================
set -euo pipefail

PORT="${PORT:-/dev/serial0}"
BAUD="${BAUD:-115200}"
STAMP=""
[[ "${1:-}" == "-t" ]] && STAMP=1

[[ -e "$PORT" ]] || { echo "!! serial port $PORT not found" >&2; exit 1; }

# 115200 8N1, raw, receive-only.
if ! stty -F "$PORT" "$BAUD" cs8 -cstopb -parenb raw -echo 2>/dev/null; then
    echo "!! cannot configure $PORT (try: sudo ./debug.sh, or add your user to the dialout group)" >&2
    exit 1
fi

echo ">> monitoring $PORT @ $BAUD 8N1 (Ctrl-C to stop)"
if [[ -n "$STAMP" ]]; then
    # Line-buffered timestamps without extra deps.
    cat "$PORT" | while IFS= read -r line; do printf '[%s] %s\n' "$(date +%H:%M:%S)" "$line"; done
else
    cat "$PORT"
fi
