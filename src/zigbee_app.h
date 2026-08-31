/********************************************************************************
 * zigbee_app.h — Zigbee application core for the IH-K663.
 ********************************************************************************/
#ifndef ZIGBEE_APP_H
#define ZIGBEE_APP_H

#include "tl_common.h"

/* The device's single HA endpoint. Shared so cluster modules (poll_control.c)
 * can address it without duplicating the number. */
#define APP_ENDPOINT    1

/* True while an OTA download is in progress. Poll Control must not change the
 * poll rate while OTA owns it, or a fast-poll timeout would stall the
 * transfer. Always false when OTA support is compiled out. */
bool app_otaBusy(void);

/* Called by the SDK's apps/common/main.c after platform + os init. */
void user_init(bool isRetention);

#endif /* ZIGBEE_APP_H */
