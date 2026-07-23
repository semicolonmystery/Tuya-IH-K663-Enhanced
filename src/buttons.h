/********************************************************************************
 * buttons.h — debounced PC2 button sampler (F1), sleep-aware.
 *
 * The 10 ms sampling tick runs ONLY while the button is active (pressed, or a
 * gesture is still resolving). When idle the tick stops so the device can enter
 * deep sleep; the button is a PM wake pin, so a press wakes the MCU and
 * buttons_poll() re-arms the tick — sampling the live pin so the waking press is
 * not lost (F1).
 ********************************************************************************/
#ifndef BUTTONS_H
#define BUTTONS_H

#include "tl_common.h"

/* Configure the PC2 GPIO (input, pull-up). Does not start the tick. */
void buttons_init(void);

/* Called from the idle task each main-loop pass: if the button is pressed and
 * the sampling tick isn't running, (re)start it. This captures a press that
 * woke the MCU from deep sleep. */
void buttons_poll(void);

/* True while the button is pressed or the sampling tick is running — used by the
 * sleep policy to decide whether it is safe to enter deep sleep. */
bool buttons_active(void);

/* Current debounced state (1 = pressed). */
bool button_is_pressed(void);

#endif /* BUTTONS_H */
