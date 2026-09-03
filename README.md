# Tuya-IH-K663-Enhanced

Custom Zigbee 3.0 firmware for the **Tuya IH-K663** one-button smart remote
(stock identity `TS0041` / `_TZ3000_fa9mlvja`), built on the Telink **TLSR8258**
(TC32) with the [telink_zigbee_sdk](https://github.com/telink-semi/telink_zigbee_sdk).

This firmware replaces the stock Tuya firmware entirely. It exposes a rich
single-button gesture set (click / hold up to triple, plus a pairing/reset
gesture), drives bound lights directly over On/Off / Level / Color-Temperature
clusters, publishes gesture `action`s to Zigbee2MQTT, reports coin-cell battery
level, supports Z2M-initiated OTA, and is a proper sleepy end device that hunts
for a new parent when you carry it out of range of its old one.

> Status: gestures, bindings, battery, deep sleep, rejoin, offline caching and
> OTA are implemented and hardware-tested. The 30 min idle poll and the bounded
> reparenting campaign (F9b) are new in v0.23 and **not yet hardware-verified**.
> Long-run coin-cell battery life has not been measured yet. See `PLAN.md` for
> milestones.

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
| 5 clicks | — (local) | — | one blink per rejoin attempt | 
| Hold 20 s | — (local) | Move stopped, gesture abandoned | off |

Every `*_hold_stop` also publishes **`action_duration`** (ms). It persists in Z2M
rather than resetting to null, so automations can read it after the fact.

**Pairing / factory reset** = 4 clicks then hold the 5th press for 5 s. The LED
blinks fast while the device is joining.

This is a **true factory reset**: the device broadcasts a Leave, clears the
binding table and erases NV (network keys, bindings, reporting configuration),
then reboots factory-new and pairs from scratch. Everything configured before is
forgotten — you will need to re-create your light bindings afterwards.

**Reconnect (5 clicks)** tells the remote to go looking for a new parent right
now — see [Losing and finding a parent](#losing-and-finding-a-parent). It does
*not* unpair anything. Note it is one click past the reset prefix, so if you aim
for a factory reset and let go of the last press too early you get a harmless
reconnect attempt instead of a reset.

A factory-new device (fresh flash, or just after a reset) pairs automatically on
boot for `PAIR_WINDOW_MS`, with the pairing LED running. If no network is found
it stops rather than steering indefinitely; re-run the gesture to try again. The 20 s hold is a stuck-button guard:
it abandons the gesture and lets the device sleep so a wedged button cannot
flatten the cell.

## Zigbee2MQTT

1. Copy `z2m/ts0041-enhanced.js` into your Z2M `external_converters` directory
   (e.g. `/opt/zigbee2mqtt/data/external_converters/`) and restart Z2M.
2. Pair the device (4 clicks + 5 s hold) with Z2M permitting joins.
3. Bind it to your lights/groups from the Z2M **Bind** tab. The converter
   deliberately does *not* create light bindings itself, so it never fights your
   own. It only binds `genPowerCfg` + `genMultistateInput` to the coordinator.

## Using the actions in Home Assistant

`action` is an **event**, not a state, so Zigbee2MQTT deliberately does not create
a `sensor.<device>_action` entity for it by default — unlike `battery`,
`voltage` or `action_duration`, which are states and do get entities. The
gestures are still published; you just pick how you want to consume them.

**1. Device triggers (built in, no config).** HA → Settings → Automations → new
automation → *Add trigger* → *Device* → pick the remote. The gestures appear as
triggers.

> Gotcha: Z2M only discovers a device trigger **after that action has fired at
> least once**. On a freshly paired remote the list is empty or partial. Press
> each gesture once (single/double/triple click, and each hold) and they appear.
> This catches almost everyone.

**2. An `event` entity (modern, recommended).** In Z2M `configuration.yaml`:

```yaml
homeassistant:
  experimental_event_entities: true
```

You get an `event` entity whose `event_type` is the action — clean to use in
automations and visible in the UI.

**3. A plain sensor entity (simplest).** If you specifically want
`sensor.<device>_action`:

```yaml
homeassistant:
  legacy_action_sensor: true
```

The sensor takes the action value then returns to empty, so trigger on the state
changing **to** a specific value, e.g. `single_click`.

Restart Z2M after changing any of these.

**4. Or skip HA entities entirely** and trigger on MQTT directly:

```yaml
triggers:
  - trigger: mqtt
    topic: zigbee2mqtt/<your device>
    value_template: "{{ value_json.action }}"
    payload: single_hold_stop
```

This also gives you `action_duration` in the same payload, which is handy for
"how long was it held" automations.

## OTA updates

Updates are **initiated from Zigbee2MQTT** — there is no on-device OTA gesture.
Point Z2M at this repo's OTA index (`configuration.yaml`):

```yaml
ota:
  zigbee_ota_override_index_location: https://github.com/semicolonmystery/Tuya-IH-K663-Enhanced/releases/latest/download/index.json
```

That URL always tracks the newest release. Then in Z2M use **OTA → Check for
new updates** on the device and start the update.

**Run one OTA at a time.** Two concurrent image downloads on the same network
contend for the coordinator and airtime, and blocks start failing to deliver
(`Delivery failed for ...`). A full image is ~12 minutes; queue them.

Because this is a sleepy battery device it only fetches while awake, so
**press the button** after starting the update to wake it — the transfer then
runs at a fast poll rate (the LED gives a brief flash once a second while it
downloads) and the device reboots into the new image when finished. A stalled transfer is abandoned
after `OTA_SESSION_MAX_S` (10 min) so it cannot drain the battery.

> The OTA Upgrade cluster is advertised in the device's simple descriptor. A
> device that was interviewed by Z2M on firmware **older than v0.13** must be
> re-interviewed (Z2M device page → *Reconfigure*) or re-paired before Z2M will
> offer it updates.

Each release also ships a `.bin` (for SWS flashing via `./flash.sh`) and a
`.ota` (for Z2M), and the OTA file version increases every release, so Z2M
reliably sees newer builds.

## Flash map

TLSR8258 with 512 KB flash, SDK **Normal Mode** (`BOOT_LOADER_MODE 0`):

| Range | Size | Contents |
|---|---|---|
| `0x00000`–`0x34000` | 208 KB | **Running app image** (`FLASH_ADDR_OF_APP_FW`) |
| `0x34000`–`0x40000` | 48 KB | NV storage (`NV_BASE_ADDRESS`) — network keys, bindings |
| `0x40000`–`0x74000` | 208 KB | **OTA download slot** (`FLASH_ADDR_OF_OTA_IMAGE`, fixed) |
| `0x7A000` | — | Secondary NV area (`NV_BASE_ADDRESS2`) |

The app budget is therefore **208 KB**. The current build uses ~132 KB (~63%),
so there is comfortable headroom. An OTA is downloaded into the `0x40000` slot
and only becomes the boot image once fully written and validated, so a failed or
interrupted update leaves the running firmware untouched — the device simply
keeps running the old image.

Because the network keys and bindings live in NV (`0x34000`), an OTA update
**keeps the device paired**; only a factory reset (4 clicks + 5 s hold) clears
them.

## Tuning (`app_config.h`)

Every tunable lives in `app_config.h`. The two that actually matter for the
reconnect-speed vs battery-life tradeoff:

| Define | Default | Effect |
|---|---|---|
| `POLL_CTRL_LONG_POLL_S` | `1800` | How often the device wakes to poll its parent. **The dominant battery knob** — `CFG_POLL_RATE_NORMAL_MS` is derived from it so the two can't disagree. 30 min sits far under the ~256 min parent child-aging timeout, so the device stays a child while barely using the radio. Raising it further also slows unattended parent-loss detection, which is measured in missed polls. |
| `REJOIN_CAMPAIGN_ATTEMPTS` | `5` | How many rejoin attempts one campaign makes before stopping completely. Higher = better odds of reconnecting unattended; lower = less radio time somewhere with no network in range. |
| `REJOIN_ATTEMPT_INTERVAL_MS` | `60000` | Gap between attempts within a campaign. |

Others worth knowing:

- `REJOIN_TRIGGER_CLICKS` (5) — the click count that forces a reconnect.
- `REJOIN_FAST_POLL_S` (8) — fast-poll window opened around each rejoin attempt
  so the Rejoin Response is actually collected.
- `POLL_CTRL_LONG_POLL_MIN_S` (5 s) — floor a coordinator may write via *Set Long
  Poll Interval*, so a misbehaving one cannot pin the radio on.
- `CFG_ZDO_MAX_REJOIN_BACKOFF_TIME` (1800 s), `CFG_ZDO_REJOIN_BACKOFF_TIME`
  (30 s), `CFG_ZDO_REJOIN_TIMES`, `CFG_ZDO_REJOIN_DURATION` — the **stack's** own
  backoff, now only a fallback behind the campaign above.
- `CFG_ZDO_MAX_PARENT_THRESHOLD_RETRY` (3) — missed polls before the stack calls
  the parent lost. This is in *polls*, so it scales with `POLL_CTRL_LONG_POLL_S`.
- `BATTERY_MEASURE_MIN_INTERVAL_S` (1 h) / `BATTERY_REPORT_INTERVAL_S` (6 h).
- `HOLD_MS` (400), `MULTI_CLICK_WINDOW_MS` (300), `DEBOUNCE_MS` (20) — gesture
  feel. Raising `MULTI_CLICK_WINDOW_MS` makes multi-clicks easier to land but
  adds that much latency before a single click is emitted.
- `STUCK_BUTTON_MS` (20 s) — when a held button is treated as wedged, abandoned
  and allowed to sleep.
- `TX_POWER_IDX` — **do not raise.** A coin cell cannot sustain high TX power.
- `DEBUG_UART_ENABLED` — set to `0` for a release build. Removes all logging
  code and its flash/power cost.

Note that a device wakes on **every** button press regardless of poll rate, so
a slow poll does not make the remote feel slower to use — it only delays
messages the coordinator wants to push *to* the device.

### Battery life and the Poll Control cluster

A remote is *transmit*-driven: it sends when you press a button and otherwise
only polls to keep the parent link alive and collect anything queued for it. So
the idle poll dominates the energy budget, and it is set slow: **30 min**, the
same value as the EFR32 sibling project. That is far under the ~256 min parent
child-aging timeout, so the device comfortably stays a child while spending
almost no time with the radio on.

Because a slow-polling device is hard for a coordinator to reach (a parent only
buffers a message for it for a few seconds), the firmware implements the
standard **Poll Control cluster (0x0020)**, the same mechanism commercial
battery remotes use:

| Attribute | Default | Meaning |
|---|---|---|
| `checkinInterval` | 1 h | How often the device announces itself to the coordinator |
| `longPollInterval` | 30 min | Idle poll rate |
| `shortPollInterval` | 250 ms | Poll rate during a fast-poll window |
| `fastPollTimeout` | 10 s | Default length of a fast-poll window |
| `longPollIntervalMin` | 5 s | Floor a coordinator may write, so it cannot pin the radio on |

On each check-in the coordinator may answer *"start fast polling"*, which opens a
short window where it can actually talk to the device (attribute reads, config,
starting an OTA). Z2M can also change these at runtime, and `genPollCtrl` is
bound during `configure` so check-ins reach the coordinator.

**Commissioning window.** On joining, the device holds the *fast* poll for
`POLL_CTRL_JOIN_FAST_POLL_S` (120 s) before dropping to the idle rate. Z2M's
interview and especially `configure()` (binds + reporting setup) are
coordinator→device requests, and a parent only buffers data for a sleepy child
for about 7.7 s — at the idle poll those requests expire long before the device
ever asks for them, and `configure` silently never completes. The window costs
one burst of polls per pairing.

Two safety properties are enforced in firmware, both deliberately:

- A fast-poll window is **always** time-bounded and clamped to
  `POLL_CTRL_FAST_POLL_TIMEOUT_MAX_S` (60 s), so a buggy or hostile coordinator
  cannot leave the radio running and flatten the cell.
- An in-progress OTA **owns** the poll rate; a fast-poll window expiring
  mid-download can't drop the device back to the slow poll and stall it.

A slow poll is safe against parent child-aging: the Zigbee end-device timeout
default is 256 minutes, comfortably longer than the 30 min poll.

> Practical consequence, and it matches how commercial battery remotes behave:
> **press the button** to wake the device when you want Z2M to talk to it —
> `Reconfigure` or starting an OTA.
>
> If a Z2M operation that pushes several requests at once (notably
> `Reconfigure`) does not complete, re-pair the device instead: 4 clicks + a 5 s
> hold. Re-joining opens the 120 s commissioning window above, which is the
> reliable way to give Z2M a long enough conversation.

### Losing and finding a parent

A sleepy end device talks to the mesh only through one **parent** router. Carry
the remote out of range of that parent and it goes quiet — it has to notice, then
attach to a different router. Two things shape how that goes.

**Noticing** is counted in *missed polls*, not in wall-clock time: the stack
gives up on the parent after `CFG_ZDO_MAX_PARENT_THRESHOLD_RETRY` (3) failed
polls, so at the 30 min idle poll that is roughly 90 minutes unattended. This is
the direct cost of a long poll, and it is why the button matters:

**Waking the remote is what starts the search.** Any press while it cannot reach
the network — and the explicit **5-click reconnect** gesture even when it thinks
it still can — starts a *campaign* immediately: `REJOIN_CAMPAIGN_ATTEMPTS` (5)
rejoin attempts, `REJOIN_ATTEMPT_INTERVAL_MS` (60 s) apart, one LED blink each.
So in practice you walk into the other room, press the button, and it reconnects
within about a minute rather than waiting to notice on its own.

A campaign then **stops completely**. A remote left somewhere with no network in
range must not scan itself flat, so it goes silent until something wakes it
again. Each attempt:

- alternates between a **secure** rejoin (keeps the network key) and a
  **trust-center** rejoin — trying only one mode forever is a common way for a
  device to stay offline permanently;
- stops preferring the previous parent after the first try, so a nearer router
  gets a real chance;
- opens a short fast-poll window, because the Rejoin Response comes back through
  the parent's indirect queue and only survives there ~7.7 s.

> Firmware before v0.23 handed this to the stack's own backoff, which grows
> toward `CFG_ZDO_MAX_REJOIN_BACKOFF_TIME`. Once the device had been away a
> while its next attempt could be half an hour out, and a button press *joined*
> that queue instead of jumping it — so a remote carried to another room could
> sit offline for hours. The SDK's own end-device samples avoid the same trap:
> every `zb_rejoinReqWithBackOff()` call in `apps/sampleSwitch` is commented out
> in favour of a plain `zb_rejoinReq()` on an app-owned timer.

## Acceptance checklist

Run against a flashed device joined to Z2M, watching `./debug.sh`.

1. **Boot** — banner prints model + firmware version; LED blinks 3×; `batt=` line
   shows a sane voltage and percentage.
2. **Pairing/reset (F10)** — 4 clicks then hold the 5th press 5 s. On a device
   that is currently joined expect `gesture=reset` → `net=factory_reset` → the
   device **reboots** (boot banner) → `net=steering` → `net=joined`. Seeing
   `net=joined` immediately with no reboot means the reset did not happen. In
   the Z2M log the interview must be followed by `Configuring` **and**
   `Successfully configured` — an interview that succeeds with no configure line
   means the commissioning fast-poll window is not holding.
3. **Gestures (F2/F8)** — each row of the gesture table produces the expected
   `gesture=` line and the matching Z2M `action`.
4. **Hold duration** — every `*_hold_stop` reports a plausible `dur=`, and Z2M's
   `action_duration` updates and then *persists* (does not fall back to null).
5. **Bound light (F3)** — bind to a light: 1 click toggles; hold dims and the
   direction alternates between holds; 2 clicks + hold shifts colour temperature.
   Verify `action_duration` still updates while bound (this needs the deferred
   duration report — see `HOLD_DURATION_REPORT_DELAY_MS`).
6. **Sleep (F4)** — when idle the UART emits one wake transient per poll interval
   (~30 min by default). Confirm the cadence tracks `POLL_CTRL_LONG_POLL_S`;
   anything above ~100 s also proves the SDK's long-sleep path is being taken.
6b. **Poll Control (F4b)** — `poll=checkin` appears at the check-in interval. If
   the coordinator opens a fast-poll window you should see `poll=fast` followed
   by `poll=long` when it expires (never `poll=fast` left standing).
7. **Stuck button (F4)** — hold for 20 s → `gesture=stuck`, LED off, device
   returns to sleeping instead of staying awake.
8. **Rejoin (F9)** — power the coordinator down and back up; the device rejoins
   without re-pairing. A press while offline logs `rejoin=start why=button`
   **immediately**, not after a backoff.
8b. **Reparenting (F9b)** — the one that matters in a real house. Pair in one
   room, carry the remote to a room served by a *different* router, and press the
   button once. Expect `rejoin=start why=gesture|button`, then up to five
   `rejoin=try n=… mode=sec|insec` lines a minute apart, with `mode` alternating,
   and a reconnect on a new parent (Z2M's network map, or its `changed parent`
   log line). If nothing is in range, expect `rejoin=giveup` and then **silence** —
   a campaign that keeps scanning is the failure to look for here.
9. **Offline cache (F5)** — with the coordinator down, perform several gestures;
   on reconnect `cache=flush` appears and the actions arrive as a few messages,
   not a verbatim replay.
10. **Battery (F7)** — Z2M shows battery % and voltage. Check readings taken
    *after* the device has slept (they arrive hourly), not just the one at boot:
    the ADC config is lost across deep-retention wake, so a regression here shows
    up as wildly wrong values (e.g. `batt=584 pct=0`) while the cell is fine.
11. **OTA (F11)** — with a newer release published, Z2M offers an update; press
    the button to wake the device; expect `ota=start` → `ota=image_done` →
    `ota=complete st=0`, then a reboot banner showing the **new** version. A full
    image takes roughly 12 minutes.

> Not yet measured: real coin-cell battery life. That needs a long unattended run
> on hardware.
