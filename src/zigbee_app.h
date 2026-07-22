/********************************************************************************
 * zigbee_app.h — Zigbee application core for the IH-K663.
 ********************************************************************************/
#ifndef ZIGBEE_APP_H
#define ZIGBEE_APP_H

#include "tl_common.h"

/* Called by the SDK's apps/common/main.c after platform + os init. */
void user_init(bool isRetention);

#endif /* ZIGBEE_APP_H */
