#!/usr/bin/env bash
# ==============================================================================
# debug.sh — monitor the IH-K663 TX-only debug UART (115200 8N1) on a Pi CM4.
#
# The firmware bit-bangs debug output on DEBUG_TX_PIN (default PB1, see
# app_config.h). Wire that pad to the Pi UART's RX.
#
# NOTE — UART contention: flashing (flash.sh) uses the Pi PL011 UART on the
# single SWS wire to a *different* device pad. If you only have one Pi UART you
# must move the wire between the SWS pad (to flash) and the PB1 debug pad (to
# monitor). Easiest workflow: flash with flash.sh, then move the jumper to PB1
# and run ./debug.sh. Alternatively use a second UART / USB-serial adapter for
# debug so you can flash and monitor without re-wiring.
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
