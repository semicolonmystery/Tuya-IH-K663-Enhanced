/********************************************************************************
 * gestures.c — single-button click/hold state machine (F2/F4/F10/F11).
 *
 * Rules (all thresholds from app_config.h):
 *   - A press released before HOLD_MS is a click; clicks within
 *     MULTI_CLICK_WINDOW_MS accumulate. When the window lapses the click count
 *     is emitted (single/double/triple).
 *   - A hold at count N = (N-1) completed clicks then a press crossing HOLD_MS.
 *     Emits *_HOLD_START, then *_HOLD_STOP with the total held duration.
 *   - RESET_CLICK_COUNT rapid clicks -> G_RESET (pairing/factory reset).
 *   - OTA_TRIGGER_CLICKS clicks then a press held >= OTA_TRIGGER_HOLD_MS
 *     -> G_OTA_TRIGGER.
 *   - A press held past STUCK_BUTTON_MS -> G_STUCK; the eventual release is
 *     silently swallowed.
 ********************************************************************************/
#include "tl_common.h"
#include "app_config.h"
#include "gestures.h"

typedef enum {
    ST_IDLE = 0,    /* nothing pressed, no pending clicks         */
    ST_PRESS,       /* button down, not yet a hold                */
    ST_HOLD,        /* hold recognized (count 1..3), Move active  */
    ST_OTA_HOLD,    /* 4 clicks + 5th press held, waiting for OTA */
    ST_GAP,         /* button up, waiting for the next click      */
    ST_SWALLOW,     /* swallow the release after stuck/reset/ota  */
} state_t;

static gesture_report_fn s_cb;
static state_t s_state;
static u8  s_prev;         /* previous debounced pressed state    */
static u8  s_clicks;       /* completed clicks in this sequence   */
static u32 s_press_ms;     /* current press duration              */
static u32 s_gap_ms;       /* time since release (ST_GAP)         */

void gestures_init(gesture_report_fn cb)
{
    s_cb = cb;
    s_state = ST_IDLE;
    s_prev = 0;
    s_clicks = 0;
    s_press_ms = 0;
    s_gap_ms = 0;
}

void gestures_reset(void)
{
    s_state = ST_IDLE;
    s_prev = 0;
    s_clicks = 0;
    s_press_ms = 0;
    s_gap_ms = 0;
}

static void emit(gesture_id_t id, u32 dur)
{
    if (s_cb) {
        gesture_event_t e = { id, dur };
        s_cb(&e);
    }
}

static void reset_seq(void)
{
    s_state = ST_IDLE;
    s_clicks = 0;
    s_press_ms = 0;
    s_gap_ms = 0;
}

/* Emit the click gesture for a completed sequence of `n` clicks. */
static void emit_clicks(u8 n)
{
    if (n == 1) emit(G_SINGLE_CLICK, 0);
    else if (n == 2) emit(G_DOUBLE_CLICK, 0);
    else if (n == 3) emit(G_TRIPLE_CLICK, 0);
    /* 4..9 clicks: no defined gesture (silently ignored). */
}

/* Emit HOLD_START for a hold at count 1..3. */
static void emit_hold_start(u8 count)
{
    if (count == 1) emit(G_SINGLE_HOLD_START, 0);
    else if (count == 2) emit(G_DOUBLE_HOLD_START, 0);
    else if (count == 3) emit(G_TRIPLE_HOLD_START, 0);
}

static void emit_hold_stop(u8 count, u32 dur)
{
    if (count == 1) emit(G_SINGLE_HOLD_STOP, dur);
    else if (count == 2) emit(G_DOUBLE_HOLD_STOP, dur);
    else if (count == 3) emit(G_TRIPLE_HOLD_STOP, dur);
}

void gestures_update(u8 pressed, u32 dt)
{
    u8 press_edge   = pressed && !s_prev;
    u8 release_edge = !pressed && s_prev;
    s_prev = pressed;

    switch (s_state) {
    case ST_IDLE:
        if (press_edge) {
            s_state = ST_PRESS;
            s_press_ms = 0;
        }
        break;

    case ST_PRESS:
        s_press_ms += dt;
        if (release_edge) {
            /* Short press -> a click. Blink immediately as it registers. */
            s_clicks++;
            emit(G_CLICK_TICK, 0);
            if (s_clicks >= RESET_CLICK_COUNT) {
                emit(G_RESET, 0);
                reset_seq();
            } else {
                s_state = ST_GAP;
                s_gap_ms = 0;
            }
        } else if (s_press_ms >= HOLD_MS) {
            u8 count = s_clicks + 1;
            if (count <= 3) {
                emit_hold_start(count);
                s_state = ST_HOLD;
            } else if (s_clicks == OTA_TRIGGER_CLICKS) {
                s_state = ST_OTA_HOLD;          /* wait for the long hold */
            } else {
                s_state = ST_SWALLOW;           /* undefined -> ignore    */
            }
        }
        break;

    case ST_HOLD:
        s_press_ms += dt;
        if (release_edge) {
            emit_hold_stop(s_clicks + 1, s_press_ms);
            reset_seq();
        } else if (s_press_ms >= STUCK_BUTTON_MS) {
            emit(G_STUCK, s_press_ms);
            /* Return to idle so the sleep policy can run; the button layer arms
             * wake-on-release for the wedged pin (see buttons_stuck). The pin
             * stays low, but ST_IDLE ignores the eventual release edge. */
            reset_seq();
        }
        break;

    case ST_OTA_HOLD:
        s_press_ms += dt;
        if (release_edge) {
            /* Released before the OTA threshold: treat the 5th press as a
             * click; the resulting 5-click sequence has no gesture. */
            reset_seq();
        } else if (s_press_ms >= OTA_TRIGGER_HOLD_MS) {
            emit(G_OTA_TRIGGER, s_press_ms);
            s_state = ST_SWALLOW;
        }
        break;

    case ST_GAP:
        s_gap_ms += dt;
        if (press_edge) {
            s_state = ST_PRESS;
            s_press_ms = 0;
        } else if (s_gap_ms >= MULTI_CLICK_WINDOW_MS) {
            emit_clicks(s_clicks);
            reset_seq();
        }
        break;

    case ST_SWALLOW:
        if (release_edge) {
            reset_seq();
        }
        break;
    }
}

bool gestures_busy(void)
{
    return s_state != ST_IDLE;
}

const char *gesture_name(gesture_id_t id)
{
    switch (id) {
    case G_SINGLE_CLICK:      return "single_click";
    case G_DOUBLE_CLICK:      return "double_click";
    case G_TRIPLE_CLICK:      return "triple_click";
    case G_SINGLE_HOLD_START: return "single_hold_start";
    case G_SINGLE_HOLD_STOP:  return "single_hold_stop";
    case G_DOUBLE_HOLD_START: return "double_hold_start";
    case G_DOUBLE_HOLD_STOP:  return "double_hold_stop";
    case G_TRIPLE_HOLD_START: return "triple_hold_start";
    case G_TRIPLE_HOLD_STOP:  return "triple_hold_stop";
    case G_RESET:             return "reset";
    case G_OTA_TRIGGER:       return "ota_trigger";
    case G_STUCK:             return "stuck";
    case G_CLICK_TICK:        return "click_tick";
    default:                  return "none";
    }
}
