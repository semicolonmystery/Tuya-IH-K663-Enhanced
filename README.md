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
