#!/usr/bin/env bash
# ==============================================================================
# flash.sh — flash / back up the IH-K663 (TLSR8258) over Telink SWS (single wire)
# from a Raspberry Pi CM4, using pvvx/TlsrComSwireWriter's TLSR825xComFlasher.py.
#
# The Pi's PL011 UART exposes no usable RTS to pyserial, so the chip cannot be
# put into the SWS boot window via RTS-reset. Instead we drive a Pi GPIO (via
# libgpiod) to reset (preferred) or power-cycle the module while the flasher
# runs its activation window.
#
# Subcommands:
#   backup            full stock flash read (0..0x80000) into backups/<timestamp>/
#   app [-r]          write build/bin/ih-k663.bin at 0x0 (-r = run after)
#   erase             erase all flash (recovery)
#   restore <file>    write a saved stock dump back (size sanity-checked)
#   check             minimal read to prove the SWS link is alive
#
# Wiring / tunables live in the CONFIG block below. TACT_MS and the GPIO line
# are the two things most likely to need adjusting on your board — verify them.
#
# >>> EXACT Pi wiring + one-time Pi UART setup: see FLASHING.md <<<
# ==============================================================================
set -euo pipefail

# ------------------------------ CONFIG ----------------------------------------
PORT="${PORT:-/dev/serial0}"          # Pi PL011 UART wired to the SWS pad
BAUD="${BAUD:-230400}"                # -b: SWS UART baud
TACT_MS="${TACT_MS:-1500}"             # -t: flasher activation window (ms). Tune!

# GPIO activation. Preferred: RST_GPIO drives the module reset pad.
# Fallback for boards with no reset pad: set PWR_GPIO instead (switches module
# power). Set exactly one. BCM line numbers on gpiochip0.
GPIOCHIP="${GPIOCHIP:-gpiochip0}"
RST_GPIO="${RST_GPIO:-17}"            # module reset pad (preferred)
PWR_GPIO="${PWR_GPIO:-}"             # module power switch (only if no reset pad)
RST_ACTIVE_LOW="${RST_ACTIVE_LOW:-1}" # 1: reset asserted by driving LOW
PWR_ACTIVE_LOW="${PWR_ACTIVE_LOW:-0}" # 1: power CUT by driving LOW (e.g. P-FET/hi-side)

# Tooling
HERE="$(cd "$(dirname "$0")" && pwd)"
FLASHER="${FLASHER:-$HERE/tools/TlsrComProg.py}"
FLASHER_URL="https://raw.githubusercontent.com/pvvx/TlsrComProg825x/master/TlsrComProg.py"
FLOADER="${FLOADER:-$HERE/tools/floader.bin}"
FLOADER_URL="https://raw.githubusercontent.com/pvvx/TlsrComProg825x/master/floader.bin"
BIN="${BIN:-$HERE/build/bin/ih-k663.bin}"
FLASH_SIZE=0x80000                    # 512 KB
# ------------------------------------------------------------------------------

log() { printf '>> %s\n' "$*"; }
die() { printf '!! %s\n' "$*" >&2; exit 1; }

# Re-exec under sudo for GPIO + serial access.
if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then
    exec sudo -E PORT="$PORT" BAUD="$BAUD" TACT_MS="$TACT_MS" \
        GPIOCHIP="$GPIOCHIP" RST_GPIO="$RST_GPIO" PWR_GPIO="$PWR_GPIO" \
        RST_ACTIVE_LOW="$RST_ACTIVE_LOW" PWR_ACTIVE_LOW="$PWR_ACTIVE_LOW" \
        FLASHER="$FLASHER" FLOADER="$FLOADER" BIN="$BIN" bash "$0" "$@"
fi

# ---- Preflight -------------------------------------------------------------
preflight() {
    command -v gpioset >/dev/null 2>&1 || die "libgpiod not found. Install: sudo apt install gpiod"
    [[ -e "$PORT" ]] || die "serial port $PORT not found (enable the PL011 UART / check wiring)"
    if [[ ! -f "$FLASHER" || ! -f "$FLOADER" ]]; then
        log "flasher tools not present, downloading TlsrComProg825x ..."
        mkdir -p "$(dirname "$FLASHER")"
        curl -fsSL "$FLASHER_URL" -o "$FLASHER" || die "could not download flasher; place it at $FLASHER"
        curl -fsSL "$FLOADER_URL" -o "$FLOADER" || die "could not download floader; place it at $FLOADER"
    fi
    [[ -n "$RST_GPIO" || -n "$PWR_GPIO" ]] || die "set RST_GPIO or PWR_GPIO"
}

# ---- GPIO (libgpiod v2 / Raspberry Pi OS Bookworm). A backgrounded gpioset
#      holds the line until killed. On older libgpiod v1 (Bullseye) gpioset
#      needs `--mode=wait`; upgrade to Bookworm or adjust gpio_hold accordingly.
GPIO_HOLD_PID=""
gpio_hold() {   # <line> <value>  — drive and hold until gpio_release
    gpioset -c "$GPIOCHIP" "$1=$2" &
    GPIO_HOLD_PID=$!
}
gpio_release() {
    [[ -n "$GPIO_HOLD_PID" ]] && kill "$GPIO_HOLD_PID" 2>/dev/null || true
    GPIO_HOLD_PID=""
}

# Assert = put chip into reset / power off. Release = let it run / power on.
assert_level()  { if [[ -n "$PWR_GPIO" ]]; then [[ "$PWR_ACTIVE_LOW" == 1 ]] && echo 0 || echo 1; else [[ "$RST_ACTIVE_LOW" == 1 ]] && echo 0 || echo 1; fi; }
release_level() { if [[ -n "$PWR_GPIO" ]]; then [[ "$PWR_ACTIVE_LOW" == 1 ]] && echo 1 || echo 0; else [[ "$RST_ACTIVE_LOW" == 1 ]] && echo 1 || echo 0; fi; }
active_line()   { [[ -n "$PWR_GPIO" ]] && echo "$PWR_GPIO" || echo "$RST_GPIO"; }

# Run the flasher while pulsing the chip so it enters the SWS boot window.
# Order: assert (reset/off) -> start flasher (begins activation) -> release
# (chip boots fresh) -> flasher syncs within its -t window.
run_flasher() {
    local line rc=0; line="$(active_line)"
    log "activating via GPIO $line (${PWR_GPIO:+power}${PWR_GPIO:-reset}) on $GPIOCHIP"
    gpio_hold "$line" "$(assert_level)"     # 1) hold chip in reset / power off
    sleep 0.05
    python3 "$FLASHER" -p "$PORT" -b "$BAUD" -t "$TACT_MS" --fldr "$FLOADER" "$@" &
    local fpid=$!
    sleep 0.60
    gpio_release                            # 2) stop driving assert level (line free)
    gpio_hold "$line" "$(release_level)"    # 3) drive run/power-on; chip boots into window
    wait "$fpid" || rc=$?                    # 4) flasher syncs and does its work
    gpio_release
    return "$rc"
}

# ---- Subcommands -----------------------------------------------------------
cmd_check() {
    preflight
    log "checking SWS link (read 16 bytes @ 0x0)"
    local tmp; tmp="$(mktemp)"
    run_flasher rf 0x0 0x10 "$tmp" && log "SWS link OK" || die "no SWS response (tune TACT_MS / check wiring & GPIO)"
    rm -f "$tmp"
}

cmd_backup() {
    preflight
    local dir="$HERE/backups/$(date +%Y%m%d-%H%M%S)"
    mkdir -p "$dir"
    local out="$dir/stock-full-${FLASH_SIZE}.bin"
    log "reading full flash 0..$FLASH_SIZE -> $out (this is your only stock copy — keep it safe)"
    run_flasher rf 0x0 "$FLASH_SIZE" "$out" || die "backup read failed"
    log "backup saved: $out ($(stat -c%s "$out") bytes)"
}

cmd_app() {
    local run=""
    [[ "${1:-}" == "-r" ]] && run="-r"
    preflight
    [[ -f "$BIN" ]] || die "firmware not found: $BIN (run ./build.sh first)"
    log "writing $BIN -> 0x0 ${run:+(+run)}"
    run_flasher we 0x0 "$BIN" || die "flash write failed"
    if [[ "$run" == "-r" ]]; then
        log "resetting module to run application..."
        local line; line="$(active_line)"
        gpio_hold "$line" "$(assert_level)"
        sleep 0.1
        gpio_hold "$line" "$(release_level)"
        sleep 0.1
        gpio_release
    fi
    log "done."
}

cmd_erase() {
    preflight
    log "erasing ALL flash (recovery)"
    run_flasher ea || die "erase failed"
    log "erased."
}

cmd_restore() {
    local file="${1:-}"
    [[ -n "$file" ]] || die "usage: flash.sh restore <stock-dump.bin>"
    [[ -f "$file" ]] || die "no such file: $file"
    preflight
    local sz; sz="$(stat -c%s "$file")"
    if (( sz != FLASH_SIZE && sz != 0x40000 )); then
        die "refusing to restore: $file is $sz bytes, expected $FLASH_SIZE (or 0x40000). Override by editing the check."
    fi
    log "restoring $file -> 0x0 ($sz bytes)"
    run_flasher we 0x0 "$file" || die "restore failed"
    log "restored."
}

usage() {
    sed -n '2,32p' "$0" | sed 's/^# \{0,1\}//'
    exit 1
}

case "${1:-}" in
    check)   cmd_check ;;
    backup)  cmd_backup ;;
    app)     shift; cmd_app "$@" ;;
    erase)   cmd_erase ;;
    restore) shift; cmd_restore "$@" ;;
    *)       usage ;;
esac
