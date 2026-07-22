/********************************************************************************
 * gestures.h — single-button gesture recognizer (F2).
 *
 * Fed a debounced pressed/released state each tick; emits recognized gestures
 * via a callback. Pure logic (no SDK/hardware) so it is easy to reason about.
 * Timing constants all come from app_config.h.
 ********************************************************************************/
#ifndef GESTURES_H
#define GESTURES_H

#include "tl_common.h"

typedef enum {
    G_SINGLE_CLICK = 0,
    G_DOUBLE_CLICK,
    G_TRIPLE_CLICK,
    G_SINGLE_HOLD_START,
    G_SINGLE_HOLD_STOP,
    G_DOUBLE_HOLD_START,
    G_DOUBLE_HOLD_STOP,
    G_TRIPLE_HOLD_START,
    G_TRIPLE_HOLD_STOP,
    G_RESET,          /* RESET_CLICK_COUNT rapid clicks (F10)                 */
    G_OTA_TRIGGER,    /* OTA_TRIGGER_CLICKS clicks + long hold (F11)          */
    G_STUCK,          /* button held past STUCK_BUTTON_MS — abandon (F4)      */
    G_CLICK_TICK,     /* one short click just registered — LED feedback only,
                       * emitted per press so the LED blinks "as they register".
                       * Not a published action. */
    G_NONE,
} gesture_id_t;

typedef struct {
    gesture_id_t id;
    u32          duration_ms;   /* hold duration for *_HOLD_STOP; else 0      */
} gesture_event_t;

typedef void (*gesture_report_fn)(const gesture_event_t *e);

void gestures_init(gesture_report_fn cb);

/* Advance the state machine. `pressed` is the debounced state (1 = pressed),
 * `dt_ms` the time since the previous call. */
void gestures_update(u8 pressed, u32 dt_ms);

/* True while a gesture is in progress (used by the sleep policy later). */
bool gestures_busy(void);

/* Human-readable name for logging / the Z2M action string. */
const char *gesture_name(gesture_id_t id);

#endif /* GESTURES_H */
