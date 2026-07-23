/********************************************************************************
 * buttons.c — debounced PC2 sampler feeding the gesture engine (F1), sleep-aware.
 * See buttons.h. The tick self-stops when idle so the device can deep-sleep.
 ********************************************************************************/
#include "tl_common.h"
#include "app_config.h"
#include "buttons.h"
#include "gestures.h"

static ev_timer_event_t *s_tick;       /* NULL when not sampling           */
static u8  s_committed;                 /* debounced pressed state          */
static u8  s_cand;                      /* candidate raw state              */
static u32 s_cand_ms;                   /* how long the candidate has held  */

#if PM_ENABLE
static u8 s_stuck;                      /* wake-on-release armed for a wedged pin */
static drv_pm_pinCfg_t s_wakeCfg[] = { { BUTTON_PIN, PM_WAKEUP_LEVEL_LOW } };

/* Program the PM wake polarity for the button pin. Normally wake-on-press
 * (level LOW, active-low button); flipped to wake-on-release (HIGH) while a
 * stuck button is held, so the low level does not immediately re-wake us. */
static void set_wake_level(u8 high)
{
    s_wakeCfg[0].wakeupLevel = high ? PM_WAKEUP_LEVEL_HIGH : PM_WAKEUP_LEVEL_LOW;
    drv_pm_wakeupPinConfig(s_wakeCfg, sizeof(s_wakeCfg) / sizeof(drv_pm_pinCfg_t));
}
#endif

/* Active-low: pin low = pressed. */
static u8 read_pressed(void)
{
    return (drv_gpio_read(BUTTON_PIN) == 0) ? 1 : 0;
}

static int button_tick(void *arg)
{
    u8 raw = read_pressed();

    if (raw == s_committed) {
        s_cand = raw;
        s_cand_ms = 0;
    } else if (raw == s_cand) {
        s_cand_ms += BUTTON_SAMPLE_MS;
        if (s_cand_ms >= DEBOUNCE_MS) {
            s_committed = raw;
        }
    } else {
        s_cand = raw;
        s_cand_ms = 0;
    }

    gestures_update(s_committed, BUTTON_SAMPLE_MS);

#if PM_ENABLE
    /* A stuck button was detected (G_STUCK -> buttons_stuck): stop sampling and
     * let the device sleep. Wake is now armed on release; buttons_poll() cleans
     * up once the pin actually goes high. */
    if (s_stuck) {
        s_tick = NULL;
        return -1;
    }
#endif

    /* Stop sampling once the button is released and no gesture is resolving —
     * this lets the idle task enter deep sleep (nearest timer becomes the long
     * poll, not this 10 ms tick). A new press re-arms via buttons_poll(). */
    if (!s_committed && !gestures_busy()) {
        s_tick = NULL;
        return -1;
    }
    return 0;
}

static void start_tick(void)
{
    if (!s_tick) {
        s_tick = TL_ZB_TIMER_SCHEDULE(button_tick, NULL, BUTTON_SAMPLE_MS);
    }
}

void buttons_init(void)
{
    drv_gpio_func_set(BUTTON_PIN);
    drv_gpio_output_en(BUTTON_PIN, 0);
    drv_gpio_input_en(BUTTON_PIN, 1);
    drv_gpio_up_down_resistor(BUTTON_PIN, PM_PIN_PULLUP_10K);

    s_committed = 0;
    s_cand = 0;
    s_cand_ms = 0;

    /* If the button is already held at boot, start sampling immediately. */
    if (read_pressed()) {
        start_tick();
    }
}

void buttons_poll(void)
{
#if PM_ENABLE
    if (s_stuck) {
        /* Wedged button: wait (asleep) for the pin to finally go high. Once
         * released, restore wake-on-press and start clean. */
        if (!read_pressed()) {
            s_stuck = 0;
            set_wake_level(0);
            gestures_reset();
            s_committed = 0;
            s_cand = 0;
            s_cand_ms = 0;
        }
        return;
    }
#endif

    /* Re-arm sampling on a press (including a press that just woke us from deep
     * sleep — the live pin is sampled here so the waking press is processed). */
    if (!s_tick && read_pressed()) {
        s_committed = 0;
        s_cand = 1;
        s_cand_ms = 0;
        start_tick();
    }
}

bool buttons_active(void)
{
#if PM_ENABLE
    /* While stuck, report inactive so the sleep policy can force deep sleep —
     * the wedged pin no longer blocks it (wake is armed on release). */
    if (s_stuck) return false;
#endif
    return (s_tick != NULL) || read_pressed();
}

#if PM_ENABLE
void buttons_wakeup_init(void)
{
    set_wake_level(0);   /* wake on press (active-low) */
}

void buttons_stuck(void)
{
    /* Called from the G_STUCK handler. Arm wake-on-release so the held-low pin
     * stops re-waking us, and mark stuck so the sampler stops and sleep runs. */
    s_stuck = 1;
    set_wake_level(1);
}
#endif

bool button_is_pressed(void)
{
    return s_committed != 0;
}
