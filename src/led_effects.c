/********************************************************************************
 * led_effects.c — hardware-PWM LED effect engine (F6). See led_effects.h.
 *
 * A single repeating software timer (period LED_STEP_MS) advances an elapsed-ms
 * accumulator and recomputes the PWM duty for the active effect. Blinks are
 * rendered as full-on/full-off; ramps/pulses sweep the duty for real brightness
 * gradients. The timer is one-shot-cancelled (returns -1) when a finite effect
 * finishes, so the LED never keeps the device awake.
 ********************************************************************************/
#include "tl_common.h"
#include "app_config.h"
#include "led_effects.h"

#define PWM_CYCLE   (CLOCK_SYS_CLOCK_HZ / LED_PWM_HZ)   /* ticks per PWM period */

typedef enum {
    MODE_OFF = 0,
    MODE_BLINK,     /* count blinks then off        */
    MODE_RAMP,      /* sawtooth, up or down         */
    MODE_PULSE,     /* triangle up/down             */
    MODE_PAIR,      /* fast blink, continuous       */
} led_mode_t;

static ev_timer_event_t *s_timer;
static led_mode_t s_mode;
static u32 s_elapsed;      /* ms since effect start           */
static u16 s_period;       /* effect period (ms)              */
static u8  s_up;           /* ramp direction                  */
static u8  s_blinks;       /* remaining blinks (MODE_BLINK)   */

/* Set brightness 0..100 % via PWM duty. */
static void led_set_pct(u8 pct)
{
    if (pct > 100) pct = 100;
    pwm_set_cmp(LED_PWM_ID, (u16)((u32)PWM_CYCLE * pct / 100));
}

static int led_timer_cb(void *arg);

static void start(led_mode_t mode, u16 period_ms, u8 up)
{
    s_mode = mode;
    s_period = period_ms ? period_ms : 1;
    s_up = up;
    s_elapsed = 0;
    if (s_timer) {
        TL_ZB_TIMER_CANCEL(&s_timer);
    }
    /* Render frame 0 immediately, then tick. */
    led_timer_cb(NULL);
    if (s_mode != MODE_OFF) {
        s_timer = TL_ZB_TIMER_SCHEDULE(led_timer_cb, NULL, LED_STEP_MS);
    }
}

/* Returns 0 to keep ticking, -1 to stop (SDK ev_timer convention). */
static int led_timer_cb(void *arg)
{
    switch (s_mode) {
    case MODE_BLINK: {
        /* Each blink = LED_BLINK_MS on + LED_BLINK_MS off. */
        u32 half = LED_BLINK_MS;
        u32 done = (u32)s_blinks * 2 * half;
        if (s_elapsed >= done) {
            led_set_pct(0);
            s_mode = MODE_OFF;
            s_timer = NULL;
            return -1;
        }
        u8 phase_on = ((s_elapsed / half) & 1) == 0;
        led_set_pct(phase_on ? 100 : 0);
        break;
    }
    case MODE_RAMP: {
        u32 pos = s_elapsed % s_period;
        u8 pct = (u8)(pos * 100 / s_period);
        led_set_pct(s_up ? pct : (100 - pct));
        break;
    }
    case MODE_PULSE: {
        u32 pos = s_elapsed % s_period;
        u32 half = s_period / 2;
        u8 pct = (pos < half) ? (u8)(pos * 100 / half)
                              : (u8)((s_period - pos) * 100 / half);
        led_set_pct(pct);
        break;
    }
    case MODE_PAIR: {
        u8 phase_on = ((s_elapsed / LED_PAIR_BLINK_MS) & 1) == 0;
        led_set_pct(phase_on ? 100 : 0);
        break;
    }
    default:
        led_set_pct(0);
        s_timer = NULL;
        return -1;
    }
    s_elapsed += LED_STEP_MS;
    return 0;
}

void led_init(void)
{
    /* Configure PWM: carrier at LED_PWM_HZ, start at 0 % duty, route to PC4. */
    pwm_set_clk(CLOCK_SYS_CLOCK_HZ, CLOCK_SYS_CLOCK_HZ);
    pwm_set_mode(LED_PWM_ID, PWM_NORMAL_MODE);
    pwm_set_phase(LED_PWM_ID, 0);
    pwm_set_cycle_and_duty(LED_PWM_ID, (u16)PWM_CYCLE, 0);
    gpio_set_func(LED_PIN, LED_PWM_FUNC);
    pwm_start(LED_PWM_ID);
    s_mode = MODE_OFF;
    s_timer = NULL;
}

void led_blink(u8 count)
{
    if (count == 0) return;
    s_blinks = count;
    start(MODE_BLINK, LED_BLINK_MS, 1);
}

void led_ramp(u16 period_ms, u8 up)
{
    start(MODE_RAMP, period_ms, up);
}

void led_pulse(u16 period_ms)
{
    start(MODE_PULSE, period_ms, 1);
}

void led_pair(void)
{
    start(MODE_PAIR, LED_PAIR_BLINK_MS, 1);
}

void led_stop(void)
{
    if (s_timer) {
        TL_ZB_TIMER_CANCEL(&s_timer);
    }
    s_mode = MODE_OFF;
    led_set_pct(0);
}

bool led_busy(void)
{
    return s_mode != MODE_OFF;
}
