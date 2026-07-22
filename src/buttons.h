/********************************************************************************
 * buttons.h — debounced PC2 button sampler (F1).
 *
 * Owns a periodic tick (BUTTON_SAMPLE_MS) that debounces the active-low button
 * and drives the gesture engine. The waking press is not lost because the tick
 * samples the live pin state.
 ********************************************************************************/
#ifndef BUTTONS_H
#define BUTTONS_H

#include "tl_common.h"

void buttons_init(void);

/* Current debounced state (1 = pressed). */
bool button_is_pressed(void);

#endif /* BUTTONS_H */
