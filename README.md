# Tuya-IH-K663-Enhanced

Custom Zigbee 3.0 firmware for the **Tuya IH-K663** one-button smart remote
(stock identity `TS0041` / `_TZ3000_fa9mlvja`), built on the Telink **TLSR8258**
(TC32) with the [telink_zigbee_sdk](https://github.com/telink-semi/telink_zigbee_sdk).

This firmware replaces the stock Tuya firmware entirely. It exposes a rich
single-button gesture set (click / hold up to triple, plus OTA and pairing
gestures), drives bound lights directly over On/Off / Level / Color-Temperature
clusters, publishes gesture `action`s to Zigbee2MQTT, reports coin-cell battery
level, and is a proper sleepy end device with correct stack-driven rejoin.

> Status: **early bring-up.** See `PLAN.md` for milestones.

## Requirements per action

Install what you need for the thing you're doing — you do **not** need the build
toolchain just to flash.

| Action | Command | Requirements |
|---|---|---|
| **Get firmware** (recommended on a Pi) | `./fetch.sh` | `curl` |
| **Flash** to the device | `./flash.sh …` | `gpiod` (provides `gpioset`, libgpiod **v2**), `python3`, `python3-serial` (pyserial), `curl` (auto-downloads the flasher once). Pi UART routed to PL011 — see [FLASHING.md](FLASHING.md) §3. Auto-elevates with `sudo`. |
| **Watch debug UART** | `./debug.sh` | just `coreutils` (`stty`/`cat`); serial access (`sudo` or `dialout` group) |
| **Build locally** (optional) | `./build.sh` | **Docker** (user in the `docker` group, or `sudo`). SDK + TC32 toolchain are downloaded automatically. The toolchain is **x86_64**, so on ARM (a Pi) it builds under slow QEMU emulation — prefer `./fetch.sh` there. |
| **Release / CI** | push a `v*` tag | nothing locally — GitHub Actions builds on x86_64 and publishes the Release |

**Raspberry Pi one-liner** (everything needed to fetch + flash + debug):

```bash
sudo apt install -y gpiod python3 python3-serial curl
```

> On a Raspberry Pi, the normal workflow is **`./fetch.sh` → `./flash.sh`** — the
> Pi does not build the firmware. Building happens in CI (or on an x86_64 machine
> via `./build.sh`).

## Build

Headless `make`, cross-platform, matching the
[romasku/tuya-zigbee-switch](https://github.com/romasku/tuya-zigbee-switch)
flow: the toolchain (TC32 GCC) and the Telink SDK are downloaded by the build,
so no IDE and no GUI step is required. Builds run in Docker locally and in
GitHub Actions; release binaries (`.bin` + `.ota`) are published as GitHub
Release assets.

## Flashing

See **[FLASHING.md](FLASHING.md)** for exact Raspberry Pi (4B / CM4) wiring —
which pad goes to which GPIO, the single-wire SWS resistor rig, the required Pi
UART setup, and the `flash.sh` / `debug.sh` steps.

Remaining docs (pairing, OTA, binding, flash map, gesture table, tuning guide)
are filled in as the milestones land.
