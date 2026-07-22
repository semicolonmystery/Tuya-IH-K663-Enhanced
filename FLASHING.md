# Flashing the IH-K663 from a Raspberry Pi (4B or CM4)

This flashes the TLSR8258 over its **single-wire SWS** interface using the Pi's
built-in UART and `flash.sh` (which wraps pvvx's `TLSR825xComFlasher.py`). The
wiring and Pi setup below are the same on a Pi 4B and a CM4 — both use the
standard 40-pin header.

> **Read this once fully before soldering.** Two things bite everyone: the Pi
> serial port must be the real **PL011** UART (not the mini-UART), and the chip
> must be **reset/power-cycled at the right instant** — which is what the GPIO
> wire is for, because the Pi has no RTS line the flasher could use.

---

## 1. How SWS flashing works here

The TLSR8258 talks a one-wire protocol (SWS / "single wire"). pvvx's tool
emulates it over a normal UART by tying **TX and RX together onto that one
wire**, TX through a resistor:

```
  Pi TXD ──[ 1 kΩ ]──┐
                     ├──────── SWS pad on the K663
  Pi RXD ────────────┘
  Pi GND ─────────────────────  GND on the K663
```

The chip only listens for SWS right after it powers up / resets. On a PC the
flasher pulses **RTS** to do that reset. **The Pi's UART has no usable RTS**, so
`flash.sh` instead drives one Pi **GPIO** to reset (or power-cycle) the module at
the moment the flasher opens its activation window.

There are two ways to do that reset, and which one you use depends on whether
your K663 board exposes a **reset pad**:

| Your board has… | Use mode | GPIO connects to | flash.sh setting |
|---|---|---|---|
| a RESET pad | **reset** (preferred) | K663 RESET pad, module powered from Pi 3V3 | `RST_GPIO=17` (default), leave `PWR_GPIO` empty |
| **no** reset pad (typical for these) | **power-cycle** | K663 **+3V3** (GPIO powers the module) | `PWR_GPIO=17`, set `RST_GPIO=""` |

Most of these little Tuya buttons **don't** break out a reset pad, so you'll most
likely use **power-cycle mode**. In that mode the module is powered *from the Pi
GPIO*, so **remove the coin cell while flashing.**

---

## 2. Wiring

### Pi pins used (physical pin numbers on the 40-pin header)

| Pi signal | BCM | Physical pin | Goes to K663 |
|---|---|---|---|
| UART **TXD** | GPIO14 | **pin 8**  | → 1 kΩ → SWS pad |
| UART **RXD** | GPIO15 | **pin 10** | → SWS pad (direct) |
| **GND** | – | **pin 6** | GND |
| reset/power ctrl | GPIO17 | **pin 11** | RESET pad *(reset mode)* **or** +3V3 *(power-cycle mode)* |
| 3V3 (reset mode only) | – | **pin 1** | +3V3 |

### Power-cycle mode (no reset pad — the common case)

```
  Pi pin 8  (GPIO14 TXD) ──[1 kΩ]──┬── SWS pad
  Pi pin 10 (GPIO15 RXD) ──────────┘
  Pi pin 6  (GND) ───────────────────  GND
  Pi pin 11 (GPIO17) ────────────────  +3V3 of the module   (powers + resets it)
  (coin cell REMOVED)
```
`flash.sh` config: `PWR_GPIO=17 RST_GPIO="" ./flash.sh ...`

### Reset mode (only if your board has a reset pad)

```
  Pi pin 8  (GPIO14 TXD) ──[1 kΩ]──┬── SWS pad
  Pi pin 10 (GPIO15 RXD) ──────────┘
  Pi pin 6  (GND) ───────────────────  GND
  Pi pin 1  (3V3) ───────────────────  +3V3 of the module
  Pi pin 11 (GPIO17) ────────────────  RESET pad
```
`flash.sh` config: defaults (`RST_GPIO=17`).

### Finding the pads on the K663
You must locate, on your specific PCB: **SWS**, **GND**, **+3V3 (VCC)**, and a
**RESET** pad if present. On TLSR8258 Tuya modules the SWS test point is usually a
small labeled/again exposed pad near the MCU. The authoritative rig drawing (incl.
the exact resistor) is pvvx's schematic:
<https://github.com/pvvx/TlsrComSwireWriter/blob/master/schematicc.gif>
(1 kΩ works well; 1–4.7 kΩ is the usable range.)

---

## 3. One-time Raspberry Pi setup

The flasher runs at **921600 baud**, where the Pi's *mini-UART* (`ttyS0`) is
unreliable. You must map `/dev/serial0` to the real **PL011** (`ttyAMA0`) and free
it from the serial login console.

Edit `/boot/firmware/config.txt` (older OS: `/boot/config.txt`) and add:

```ini
enable_uart=1
dtoverlay=disable-bt
```

Then disable the serial console and the BT UART service:

```bash
sudo raspi-config   # Interface Options → Serial Port:
                    #   "login shell over serial?"  -> NO
                    #   "serial port hardware enabled?" -> YES
sudo systemctl disable --now hciuart
sudo reboot
```

After reboot, confirm `/dev/serial0` points at `ttyAMA0` (PL011), not `ttyS0`:

```bash
ls -l /dev/serial0        # -> ../ttyAMA0
sudo apt install -y gpiod python3-serial   # libgpiod (gpioset) + pyserial
```

> Do **not** use a USB-serial adapter with an FTDI chip or an RX-LED — pvvx's
> tool documents that those don't work for SWS. The Pi's native UART is fine.

---

## 4. Flashing steps

```bash
git clone https://github.com/semicolonmystery/Tuya-IH-K663-Enhanced.git
cd Tuya-IH-K663-Enhanced
./build.sh                       # produces build/bin/ih-k663.bin

# If your board has NO reset pad (power-cycle mode), export these first:
export PWR_GPIO=17 RST_GPIO=""    # (skip this line if using a reset pad)

# 1) BACK UP THE STOCK FIRMWARE — do this before anything else, once:
./flash.sh backup                # -> backups/<timestamp>/stock-full-0x80000.bin

# 2) Prove the SWS link is alive:
./flash.sh check                 # expect "SWS link OK"

# 3) Flash our firmware and run it:
./flash.sh app -r
```

`flash.sh` re-runs itself under `sudo` automatically (needed for GPIO + serial).

**Expected after `app -r`:** the LED blinks **3×** and, if the debug wire is
connected (next section), the UART prints
`== IH-K663 boot (M1) model=TS0041-CUS ==`.

Recovery if something goes wrong:
```bash
./flash.sh erase                 # erase all flash
./flash.sh restore backups/<timestamp>/stock-full-0x80000.bin   # back to stock
```

---

## 5. Watching the debug UART

The firmware prints debug on **PB1** (`DEBUG_TX_PIN` in `app_config.h`), TX-only,
115200 8N1.

```
  K663 PB1 ─────────────  Pi RXD (pin 10, GPIO15)
  common GND
```

```bash
./debug.sh          # 115200 8N1 stream (Ctrl-C to stop)
./debug.sh -t       # with host timestamps
```

**UART sharing:** flashing uses the SWS pad on pins 8+10; debug uses PB1 on
pin 10. With a single Pi UART you move the **pin-10 (RXD)** wire between the SWS
pad (to flash) and PB1 (to monitor). Easiest flow: flash, then move that one
jumper to PB1 and run `./debug.sh`. If you'd rather flash and monitor without
re-wiring, use a **second** UART / USB-serial adapter for PB1 and point
`debug.sh` at it: `PORT=/dev/ttyUSB0 ./debug.sh`.

---

## 6. Troubleshooting

| Symptom | Fix |
|---|---|
| `check` fails / "no SWS response" | Increase the activation window: `TACT_MS=1000 ./flash.sh check`. Confirm the GPIO wire and mode (reset vs power-cycle) match your board. |
| Works sometimes, garbled | You're on the mini-UART. Confirm `/dev/serial0 -> ttyAMA0` and that `dtoverlay=disable-bt` is set. |
| `libgpiod not found` | `sudo apt install gpiod`. This assumes libgpiod **v2** (Raspberry Pi OS Bookworm); on Bullseye `gpioset` needs `--mode=wait` (upgrade recommended). |
| Nothing on debug UART | Check PB1 is actually the pad you soldered and is broken out on your PCB; change `DEBUG_TX_PIN` in `app_config.h` and rebuild if not. Confirm 115200 8N1 and common GND. |
| Chip won't stay off in power-cycle mode | TX/RX at 3V3 can back-power the chip; make sure the coin cell is removed and only the GPIO powers VCC. |

All tunables (`TACT_MS`, `RST_GPIO`, `PWR_GPIO`, `RST_ACTIVE_LOW`, `PORT`, `BAUD`)
are environment variables at the top of `flash.sh` and can be overridden inline,
e.g. `TACT_MS=800 RST_GPIO=27 ./flash.sh app -r`.
