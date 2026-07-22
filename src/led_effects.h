/********************************************************************************
 * led_effects.h — non-blocking LED effect engine for the single PC4 LED (F6).
 *
 * Hardware PWM on PC4 gives brightness; a software timer renders blinks, ramps
 * and pulses without ever blocking. Effects run only while active and stop
 * cleanly (never extend wakefulness past their natural end).
 ********************************************************************************/
#ifndef LED_EFFECTS_H
#define LED_EFFECTS_H

#include "tl_common.h"

void led_init(void);

/* One-shot: N quick blinks then off (click feedback). */
void led_blink(u8 count);

/* Continuous sawtooth brightness ramp with the given period; up=1 sweeps
 * 0->100%, up=0 sweeps 100->0%. Used for hold gestures (period distinguishes
 * single/double/triple). Runs until led_stop(). */
void led_ramp(u16 period_ms, u8 up);

/* Continuous triangle pulse (OTA feedback). Runs until led_stop(). */
void led_pulse(u16 period_ms);

/* Continuous fast blink (pairing/steering). Runs until led_stop(). */
void led_pair(void);

/* Stop any effect and turn the LED off. */
void led_stop(void);

/* True while an effect is running (used by the sleep policy later). */
bool led_busy(void);

#endif /* LED_EFFECTS_H */
