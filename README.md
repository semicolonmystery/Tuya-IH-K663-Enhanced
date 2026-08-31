# Tuya-IH-K663-Enhanced

Custom Zigbee 3.0 firmware for the **Tuya IH-K663** one-button smart remote
(stock identity `TS0041` / `_TZ3000_fa9mlvja`), built on the Telink **TLSR8258**
(TC32) with the [telink_zigbee_sdk](https://github.com/telink-semi/telink_zigbee_sdk).

This firmware replaces the stock Tuya firmware entirely. It exposes a rich
single-button gesture set (click / hold up to triple, plus a pairing/reset
gesture), drives bound lights directly over On/Off / Level / Color-Temperature
clusters, publishes gesture `action`s to Zigbee2MQTT, reports coin-cell battery
level, supports Z2M-initiated OTA, and is a proper sleepy end device with
correct stack-driven rejoin.

> Status: gestures, bindings, battery, deep sleep, rejoin, offline caching and
> OTA are implemented and hardware-tested. Long-run coin-cell battery life has
> not been measured yet. See `PLAN.md` for milestones.

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

## Gestures

One button, all gestures. A "hold" is a press longer than `HOLD_MS` (400 ms);
clicks chain within `MULTI_CLICK_WINDOW_MS` (300 ms).

| Gesture | Z2M `action` | Bound-light command | LED |
|---|---|---|---|
| 1 click | `single_click` | On/Off **Toggle** | short blink |
| 2 clicks | `double_click` | — | blink per click |
| 3 clicks | `triple_click` | — | blink per click |
| Hold | `single_hold_start` / `single_hold_stop` | Level **Move** (dim), direction alternates each hold | ramp |
| 2 clicks + hold | `double_hold_start` / `double_hold_stop` | Colour-temperature **Move**, direction alternates | faster ramp |
| 3 clicks + hold | `triple_hold_start` / `triple_hold_stop` | — (publish only) | slow ramp |
| 4 clicks + hold 5 s | — (local) | — | fast blink = pairing | 
| Hold 20 s | — (local) | Move stopped, gesture abandoned | off |

Every `*_hold_stop` also publishes **`action_duration`** (ms). It persists in Z2M
rather than resetting to null, so automations can read it after the fact.

**Pairing / factory reset** = 4 clicks then hold the 5th press for 5 s. The LED
blinks fast while the device is joining. The 20 s hold is a stuck-button guard:
it abandons the gesture and lets the device sleep so a wedged button cannot
flatten the cell.

## Zigbee2MQTT

1. Copy `z2m/ts0041-enhanced.js` into your Z2M `external_converters` directory
   (e.g. `/opt/zigbee2mqtt/data/external_converters/`) and restart Z2M.
2. Pair the device (4 clicks + 5 s hold) with Z2M permitting joins.
3. Bind it to your lights/groups from the Z2M **Bind** tab. The converter
   deliberately does *not* create light bindings itself, so it never fights your
   own. It only binds `genPowerCfg` + `genMultistateInput` to the coordinator.

## OTA updates

Updates are **initiated from Zigbee2MQTT** — there is no on-device OTA gesture.
Point Z2M at this repo's OTA index (`configuration.yaml`):

```yaml
ota:
  zigbee_ota_override_index_location: https://github.com/semicolonmystery/Tuya-IH-K663-Enhanced/releases/latest/download/index.json
```

That URL always tracks the newest release. Then in Z2M use **OTA → Check for
new updates** on the device and start the update.

Because this is a sleepy battery device it only fetches while awake, so
**press the button** after starting the update to wake it — the transfer then
runs at a fast poll rate (the LED pulses slowly while it downloads) and the
device reboots into the new image when finished. A stalled transfer is abandoned
after `OTA_SESSION_MAX_S` (10 min) so it cannot drain the battery.

> The OTA Upgrade cluster is advertised in the device's simple descriptor. A
> device that was interviewed by Z2M on firmware **older than v0.13** must be
> re-interviewed (Z2M device page → *Reconfigure*) or re-paired before Z2M will
> offer it updates.

Each release also ships a `.bin` (for SWS flashing via `./flash.sh`) and a
`.ota` (for Z2M), and the OTA file version increases every release, so Z2M
reliably sees newer builds.
