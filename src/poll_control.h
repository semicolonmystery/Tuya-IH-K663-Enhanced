/********************************************************************************
 * poll_control.h — Poll Control cluster (0x0020) server logic (F4b).
 *
 * The SDK ships the ZCL plumbing (zcl_pollCtrl.c) but the *application* has to
 * drive it: send the periodic Check-In, react to the coordinator's Check-In
 * Response, and switch the poll rate between the long (idle) and short (fast)
 * intervals. That is what this module does.
 *
 * Why it exists: the idle poll rate is the dominant battery cost on a sleepy
 * remote, so it is slow (POLL_CTRL_LONG_POLL_S). Poll Control is the standard
 * way to keep a slow-polling device reachable — it checks in periodically and
 * the coordinator can answer "start fast polling" to open a window in which it
 * can actually talk to the device.
 *
 * Safety rule: fast-poll mode is ALWAYS bounded by a timeout, and any window the
 * coordinator asks for is clamped to POLL_CTRL_FAST_POLL_TIMEOUT_MAX_S, so a
 * buggy or hostile coordinator can never leave the radio running and flatten
 * the coin cell.
 *
 * The attribute table itself lives in zigbee_app.c next to the other cluster
 * tables (so its size is a compile-time constant); only the behaviour is here.
 ********************************************************************************/
#ifndef POLL_CONTROL_H
#define POLL_CONTROL_H

#include "tl_common.h"

#if ZCL_POLL_CTRL_SUPPORT

#include "zcl_include.h"

/* Poll Control attribute set. All intervals are in QUARTER-SECONDS, per the ZCL
 * spec — app_config.h holds the human-readable seconds and converts. */
typedef struct {
    u32 chkInInterval;
    u32 longPollInterval;
    u32 chkInIntervalMin;
    u32 longPollIntervalMin;
    u16 shortPollInterval;
    u16 fastPollTimeout;
    u16 fastPollTimeoutMax;
} app_pollCtrlAttr_t;

extern app_pollCtrlAttr_t g_pollCtrlAttrs;

/* ZCL cluster callback — dispatches the client->server Poll Control commands
 * (Check-In Response, Fast Poll Stop, Set Long/Short Poll Interval). */
status_t pollctrl_cb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload);

/* Begin periodic check-ins. Call once the device is on a network; safe to call
 * again (a rejoin re-arms it rather than stacking timers). */
void pollctrl_start(void);

/* Drop back to the long poll and cancel any fast-poll window. Called when the
 * device leaves the network so it cannot keep fast-polling with no parent. */
void pollctrl_stop(void);

/* Re-apply the poll rate this module thinks is correct (long, or short while a
 * fast-poll window is open). Called when OTA releases the rate. */
void pollctrl_restore_rate(void);

/* True while a fast-poll window is open. */
bool pollctrl_fastPolling(void);

#endif /* ZCL_POLL_CTRL_SUPPORT */
#endif /* POLL_CONTROL_H */
