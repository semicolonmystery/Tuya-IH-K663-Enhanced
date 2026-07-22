/********************************************************************************
 * buttons.c — debounced PC2 sampler feeding the gesture engine (F1).
 *
 * A repeating BUTTON_SAMPLE_MS software timer reads the active-low pin, applies
 * a simple stability debounce (DEBOUNCE_MS), and calls gestures_update() with
 * the debounced state each tick. Running continuously keeps the gesture engine
 * live while awake; deep-sleep wake integrity is added with the sleep policy.
 ********************************************************************************/
#include "tl_common.h"
#include "app_config.h"
#include "buttons.h"
#include "gestures.h"

static u8  s_committed;    /* debounced pressed state         */
static u8  s_cand;         /* candidate raw state             */
static u32 s_cand_ms;      /* how long the candidate has held */

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
    return 0;   /* keep ticking */
}

void buttons_init(void)
{
    drv_gpio_func_set(BUTTON_PIN);
    drv_gpio_output_en(BUTTON_PIN, 0);
    drv_gpio_input_en(BUTTON_PIN, 1);
    drv_gpio_up_down_resistor(BUTTON_PIN, PM_PIN_PULLUP_10K);

    s_committed = read_pressed();   /* sample live state at init (wake press)  */
    s_cand = s_committed;
    s_cand_ms = 0;

    TL_ZB_TIMER_SCHEDULE(button_tick, NULL, BUTTON_SAMPLE_MS);
}

bool button_is_pressed(void)
{
    return s_committed != 0;
}
