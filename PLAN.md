# PLAN — Tuya IH-K663 custom Zigbee firmware (TLSR8258)

Custom Zigbee 3.0 firmware for the Tuya IH-K663 one-button remote, replacing
stock Tuya firmware. Built on `telink_zigbee_sdk` V3.7.2.0, TLSR8258, headless
`make`. Full spec lives in the implementation brief; this file tracks how we
build it and the ordered milestones.

## Ground rules (from the brief)
- One milestone at a time, tree always compiles.
- Every tunable is a documented `#define` in **`app_config.h`** — no magic numbers.
- Never invent SDK APIs — verify signatures against `telink_tools/sdk` sources.
- Don't reimplement what the stack does (join/rejoin/poll/sleep/OTA). Configure it.
- Purpose-built for THIS device: one button (PC2), one LED (PC4), fixed pinout,
  no runtime config, no HAL abstraction for hardware that doesn't exist.
- After each milestone: diff summary + build instructions + numbered hardware
  test script (LED behavior + expected debug-UART lines). User flashes & reports.

## Environment & build strategy (decided)
- **We build; the user only flashes** the `.bin`/`.ota` from a Raspberry Pi CM4.
- **Toolchain + SDK are downloaded by the build** (romasku/tuya-zigbee-switch
  style): SDK `V3.7.2.0` zip + `tc32_gcc_v2.0` Linux toolchain. No IDE, no GUI.
- **Cross-platform / CI-first.** Local builds run in **Docker** (Linux); the same
  `make` runs in **GitHub Actions**. No local host compiler is used.
- **Artifacts are published as GitHub Release assets** (`.bin` + `.ota`), not
  committed to the tree. The Z2M OTA index points at the release asset URL.
- Repo: `github.com/semicolonmystery/Tuya-IH-K663-Enhanced` (main).
- Reference clones live in `references/` (gitignored, study-only):
  `tuya-zigbee-switch` (SDK usage + network layer), `ZigbeeTLc` (build refs).
  Downloaded SDK/toolchain live in `telink_tools/` (gitignored).

## Identity (defines; Z2M fingerprints on these)
- `modelIdentifier = "TS0041-CUS"`, `manufacturerName = "_TZ3000_fa9mlvja-CUS"`.
- Stock was `TS0041` / `_TZ3000_fa9mlvja`.

## Fixed hardware map
- Button: `GPIO_PC2`, active-low, internal pull-up, deep-sleep wake source.
- LED: `GPIO_PC4`, active-high (PWM if PC4 is a PWM channel — verify; else soft-PWM).
- Battery: internal ADC supply-rail path (no divider). External fallback behind
  `BATTERY_USE_EXTERNAL_ADC` (0).
- Debug UART: TX-only on a free GPIO (chosen in M2), 115200 8N1,
  `DEBUG_UART_ENABLED` (1 during bring-up).

---

## Milestones

- [x] **M0 — Investigation & scaffolding.** Environment settled (Docker build,
  CI→Release, downloaded toolchain), SDK + references fetched, git repo pushed,
  PLAN written.

- [x] **M1 — Headless build skeleton.** Minimal sleepy-ED image (endpoint 1,
  Basic+Identify server, On/Off/Level/Color client, our identity) that boots and
  prints a UART banner. Top-level `make` downloads the SDK+TC32 toolchain and
  produces `build/bin/ih-k663.bin` + `.ota`, printing flash/RAM usage
  (~112 KB flash / ~13.4 KB RAM). `Dockerfile` + `build.sh` for local Docker
  builds; `.github/workflows/build.yml` builds, and on a `v*` tag publishes a
  GitHub Release with the assets and refreshes the Z2M OTA index. Verified
  building in Docker locally.

- [ ] **M2 — Board/HAL: button, LED, debug UART.** Fixed pinout. `buttons.c/h`
  debounced sampler with **wake-press integrity (F1)**. `led_effects.c/h`
  non-blocking effect engine (F6, PWM vs soft-PWM documented). Debug UART TX.
  Battery module stub. Boots → button edges logged, LED blinks.

- [ ] **M3 — Gesture engine (F2/F3a/F4-stuck).** `gestures.c/h` click+hold state
  machine: single/double/triple click & hold, `MULTI_CLICK_WINDOW_MS`,
  `HOLD_MS`, hold duration, direction-flip flags in retained RAM, stuck-button
  cap. Drives the LED ramp variants (F6). Emits gesture events (no radio yet).

- [ ] **M4 — Zigbee app core + endpoints.** `zigbee_app.c/h`: endpoint 1, HA
  profile. Server: Basic, Identify, Power Config, Multistate Input (0x0012) +
  Analog Input (0x000C) for gesture publishing (F8). Client: On/Off, Level,
  Color, Identify, OTA. Identity defines. Network-state logging.

- [ ] **M5 — Actions & cluster commands (F3/F3a).** Gesture→command mapping over
  the **binding table only** (Toggle / Level Move+Stop / Color-temp Move+Stop),
  publish `action`/`action_duration` via Multistate/Analog Input. `BINDING_TABLE_SIZE` (16).

- [ ] **M6 — Sleep / power management (F4).** Deep sleep + SRAM retention, wake on
  button + poll timer, `SLEEP_IDLE_MS`, `TX_GRACE_MS`, force-sleep guarantees,
  stuck-button force-sleep. Nothing keeps the radio awake indefinitely.

- [ ] **M7 — Network join / rejoin / reparent (F9) — highest risk.** BDB
  commissioning callbacks → `zb_rejoinReqWithBackOff()`; tune `zb_config.h`
  rejoin/backoff/poll macros for a battery ZED (long max backoff, long normal
  poll); button-press-while-offline → immediate rejoin (prefer SDK support).
  Verify sleeping between attempts via debug UART.

- [ ] **M8 — Offline action cache (F5).** Semantic cache (On/Off latest slot,
  signed Level accumulator→Step, signed CT accumulator→Step), `ACTION_CACHE_MAX`
  (10), flush oldest-first as a handful of messages.

- [ ] **M9 — Battery reporting (F7).** Internal ADC vbat, coin-cell discharge LUT
  + interpolation, PowerConfig BatteryVoltage/PercentageRemaining, measure/report
  intervals, sample when radio idle.

- [ ] **M10 — Pairing / factory reset (F10).** `RESET_CLICK_COUNT` (10) rapid
  clicks → leave + clear + steering for `PAIR_WINDOW_MS` with pairing LED.
  Steering only via this gesture.

- [ ] **M11 — OTA (F11).** SDK OTA client, trigger = 4 clicks + 5 s hold, flash
  map documented, OTA image via SDK tooling (`tl_check_fw.sh`), Z2M OTA index JSON.

- [ ] **M12 — Z2M converter (F12).** `ts0041-cus.js` with exact `action` values,
  `action_duration`, battery, `ota: true`, `configure` bindings/reporting.

- [ ] **M13 — Flashing/debug tooling.** `flash.sh` (Pi CM4, GPIO-driven SWS
  activation via TlsrComSwireWriter: backup/app/erase/restore/check) + `debug.sh`.

- [ ] **M14 — Docs & acceptance.** README: toolchain, build, flashing/wiring,
  pairing, OTA, binding, flash map, F8 gesture code table, `app_config.h` tuning
  guide (reconnect-speed vs battery). Acceptance test scripts (section 8).

## Findings that shape later milestones
- **F8 wire format:** the SDK (v3.7.2.0) ships **Multistate Input** but **no
  Analog Input** cluster. So gesture publishing (F8) will use Multistate Input
  `presentValue` for the gesture code, and carry the hold duration via a second
  mechanism (a second Multistate/manufacturer attr, TBD in M4) — documented
  precisely there since the Z2M converter depends on it.
- The per-app ZCL attribute-storage structs (`zcl_basicAttr_t`, …) are **defined
  by each app**, not by the SDK — we define our own.
- The build links the closed stack (`libzb_ed.a` + `libdrivers_8258.a`) plus the
  open SDK glue compiled from source; the raw objcopy `.bin` is flashable and
  `make_ota.py` stamps the Telink magic/CRC + Zigbee OTA header for the `.ota`.

## Assumptions log
- "GitHub packages" interpreted as **GitHub Releases** (release assets) — the
  correct home for firmware binaries and OTA images. Revisit if the user meant
  the container/npm Packages registry.
- Single CR2032 3 V coin cell (F7) until the user says otherwise.
- Local build/verify happens in Docker on the dev machine; the canonical build
  is the same `make` invoked by CI on `ubuntu-latest`.
