/********************************************************************************
 * app_config.h — THE single tunables file for the IH-K663 firmware.
 *
 * Every tunable in this project lives here as a documented #define. No magic
 * numbers anywhere else. Values in (parentheses) in the comments are the spec
 * defaults. The Zigbee-stack network macros (F9) are surfaced here too and are
 * consumed by configs/zb_config.h — those are the knobs the user tunes on
 * hardware for the reconnect-speed vs battery-life tradeoff.
 *
 * Device: Tuya IH-K663 one-button remote, Telink TLSR8258 (TC32), Zigbee 3.0.
 ********************************************************************************/
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ============================================================================
 * Identity (Z2M fingerprints on exactly these — keep as defines)
 * Stock device was TS0041 / _TZ3000_fa9mlvja.
 * ========================================================================== */
#define APP_MODEL_ID              "TS0041-Enhanced"
#define APP_MANUFACTURER_NAME     "_TZ3000_fa9mlvja-Enhanced"

/* OTA / manufacturer identity. 0x1141 is Telink's Zigbee manufacturer code. */
#define APP_MANUFACTURER_CODE     0x1141
#define APP_IMAGE_TYPE            0x0641   /* device-specific OTA image type    */

/* ============================================================================
 * Fixed hardware map (this device only — never runtime-configurable)
 * ========================================================================== */
#define BUTTON_PIN                GPIO_PC2 /* active-low, internal pull-up      */
#define LED_PIN                   GPIO_PC4 /* active-high                       */

/* LED is driven by hardware PWM for brightness ramps (F6). PC4's GPIO mux
 * exposes PWM2 and PWM0_N (verified on the K663 silkscreen/mux), so PWM2. */
#define LED_PWM_ID                PWM2_ID
#define LED_PWM_FUNC              AS_PWM2

/* Debug UART: TX-only, user solders one wire here. PB1 is free on this part.
 * VERIFY on hardware before soldering; single #define to change.             */
#define DEBUG_TX_PIN              GPIO_PB1
#define DEBUG_BAUDRATE            115200   /* 8N1                               */
#ifndef DEBUG_UART_ENABLED
#define DEBUG_UART_ENABLED        1        /* (1) bring-up; set 0 for release   */
#endif

/* ---- Temporary bring-up diagnostics (remove before release) --------------- */
/* DIAG_VERBOSE: 1s heartbeat dumping the sleep-gate booleans + raw button, plus
 * button-press / sleep-transition traces. Lets the UART pinpoint whether the
 * device is looping, deep-sleeping, or hung, and whether presses are seen. */
#ifndef DIAG_VERBOSE
#define DIAG_VERBOSE              1
#endif
/* DIAG_DISABLE_SLEEP: 1 compiles out drv_pm_lowPowerEnter() so the idle task
 * never deep-sleeps — used to bisect whether the sleep policy is the culprit. */
#ifndef DIAG_DISABLE_SLEEP
#define DIAG_DISABLE_SLEEP        0
#endif
#define DIAG_HEARTBEAT_MS         1000

/* Battery sensing: use the TLSR8258 internal supply-rail ADC path (no divider).
 * Fallback to an external ADC channel only if the internal path is unusable.  */
#define BATTERY_USE_EXTERNAL_ADC  0        /* (0)                               */
/* Detection IO for the internal VBAT ADC path (must be a floating pad). PB5 is
 * romasku's choice for this exact device; verify on your PCB. */
#define BATTERY_ADC_PIN           GPIO_PB5

/* ============================================================================
 * F1 — Wake & debounce
 * ========================================================================== */
#define DEBOUNCE_MS               20       /* (20)                              */
#define BUTTON_SAMPLE_MS          10       /* (10) debounce/gesture tick period */

/* ============================================================================
 * F2 — Gesture engine timing
 * ========================================================================== */
#define MULTI_CLICK_WINDOW_MS     300      /* (300) inter-click window          */
#define HOLD_MS                   400      /* (400) press>this at count N = hold */

/* ============================================================================
 * F3 / F3a — Actions, move rates, direction flip
 * ========================================================================== */
#define LEVEL_START_DIR           1        /* (up) first single-hold direction  */
#define LEVEL_MOVE_RATE           50       /* (50) level units/s                */
#define CT_MOVE_RATE              20       /* (20) mireds/s                     */
#define CT_MIN_MIREDS             153      /* (153)                             */
#define CT_MAX_MIREDS             500      /* (500)                             */

/* ============================================================================
 * F4 — Sleep policy
 * ========================================================================== */
#define SLEEP_IDLE_MS             2000     /* (2000) quiet time before sleep    */
#define TX_GRACE_MS               500      /* (500) max wait for in-flight TX   */
#define STUCK_BUTTON_MS           20000    /* (20000) abandon stuck gesture     */

/* ============================================================================
 * F5 — Offline action cache
 * ========================================================================== */
#define ACTION_CACHE_MAX          10       /* (10) queued published actions cap */

/* ============================================================================
 * F6 — LED effect timing (ms)
 * ========================================================================== */
#define LED_RAMP_LEVEL_MS         400      /* (400) single-hold ramp period     */
#define LED_RAMP_CT_MS            250      /* (250) double-hold ramp (faster)   */
#define LED_RAMP_TRIPLE_MS        700      /* (700) triple-hold ramp (slowest)  */
#define LED_BLINK_MS              100      /* (100) single click blink          */
#define LED_PAIR_BLINK_MS         200      /* (200) pairing fast blink          */
#define LED_OTA_PULSE_MS          1000     /* (1000) OTA slow pulse             */
#define LED_STEP_MS               20       /* (20) PWM update tick for ramps    */
#define LED_PWM_HZ                1000     /* (1000) PWM carrier frequency      */

/* ============================================================================
 * F7 — Battery
 * ========================================================================== */
#define BATTERY_V_100                   3000  /* (3.00 V) mV at 100%           */
#define BATTERY_V_0                     2300  /* (2.30 V) mV at 0% — keep >brownout */
#define BATTERY_MEASURE_MIN_INTERVAL_S  3600  /* (3600) min seconds between reads */
#define BATTERY_REPORT_INTERVAL_S       21600 /* (21600) periodic report        */

/* ============================================================================
 * F9 — Network: join / rejoin / reparent (highest-risk area).
 * These feed configs/zb_config.h. Start from romasku's proven values, then
 * raise the MAX backoff and normal poll for a battery ZED so an unreachable
 * network cannot drain the coin cell. Tradeoff: bigger backoff = slower
 * reconnect but far less battery use while the network is gone.
 * ========================================================================== */
#define CFG_ZDO_REJOIN_TIMES              5    /* rejoin attempts per burst      */
#define CFG_ZDO_REJOIN_DURATION           6    /* rejoin scan duration (s)       */
#define CFG_ZDO_REJOIN_BACKOFF_TIME       30   /* initial backoff (s)            */
#define CFG_ZDO_MAX_REJOIN_BACKOFF_TIME   3600 /* max backoff (s) — battery-safe */
#define CFG_ZDO_REJOIN_BACKOFF_ITERATION  8    /* backoff growth iterations      */
#define CFG_ZDO_MAX_PARENT_THRESHOLD_RETRY 5   /* parent-loss retry threshold    */

/* Poll rates (ms). Long normal poll for a battery ZED; the stack uses the
 * faster rates only during active exchanges. */
#define CFG_POLL_RATE_NORMAL_MS           7000 /* normal indirect poll           */
#define CFG_POLL_RATE_RESPONSE_MS         250  /* when coordinator has data       */
#define CFG_POLL_RATE_QUEUE_MS            250  /* when outbound queued            */
#define CFG_POLL_RATE_REJOIN_MS           500  /* during rejoin                   */

/* Binding table (F: command delivery is binding-table only). */
#define BINDING_TABLE_SIZE               16    /* (16)                            */

/* TX power: do NOT raise — a coin cell cannot sustain high TX power. Battery
 * builds use the SDK's low-power index. */
#define TX_POWER_IDX                     RF_POWER_INDEX_P3p01dBm

/* ============================================================================
 * F10 — Pairing / factory reset
 * ========================================================================== */
#define RESET_CLICK_COUNT                10    /* (10) rapid clicks to reset      */
#define PAIR_WINDOW_MS                   30000 /* (30000) steering window         */

/* ============================================================================
 * F11 — OTA
 * ========================================================================== */
#define OTA_TRIGGER_CLICKS               4     /* (4) clicks before the hold      */
#define OTA_TRIGGER_HOLD_MS              5000  /* (5000) hold on the 5th press    */
#define OTA_SESSION_MAX_S                600   /* (600) session cap               */
#define OTA_AUTO_QUERY_ENABLED           0     /* (0) manual trigger only         */
#define OTA_QUERY_MIN_INTERVAL_S         604800/* (604800) opportunistic query gap */

#endif /* APP_CONFIG_H */
