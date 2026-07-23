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
    return (s_tick != NULL) || read_pressed();
}

bool button_is_pressed(void)
{
    return s_committed != 0;
}
