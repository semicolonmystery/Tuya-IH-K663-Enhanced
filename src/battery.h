/********************************************************************************
 * battery.h — coin-cell battery measurement + Power Config attribute values (F7).
 *
 * Reads the supply rail via the TLSR8258 internal VBAT ADC path (no divider),
 * converts to a percentage through a non-linear coin-cell discharge lookup
 * table, and exposes the two Power Config attribute values for the endpoint.
 ********************************************************************************/
#ifndef BATTERY_H
#define BATTERY_H

#include "tl_common.h"

/* Power Config attribute storage, in ZCL units:
 *   voltage:    0x0020, 100 mV units
 *   percentage: 0x0021, 0.5 % units                                          */
extern u8 g_batteryVoltage;
extern u8 g_batteryPercentage;

void battery_init(void);

/* Measure now and refresh the two attribute values. Call when the radio is
 * idle so a TX burst does not skew the sample. */
void battery_update(void);

#endif /* BATTERY_H */
