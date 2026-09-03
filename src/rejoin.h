/********************************************************************************
 * rejoin.h — parent loss and reparenting (F9b).
 *
 * Why this exists: the stack's own zb_rejoinReqWithBackOff() schedules attempts
 * on a backoff that grows toward CFG_ZDO_MAX_REJOIN_BACKOFF_TIME, so once a
 * device has been away for a while it can be half an hour from its next try —
 * and a button press asking for a rejoin *joins that queue* instead of jumping
 * it. That is why carrying the remote into a room served by a different router
 * left it offline for hours. The SDK's own end-device samples have every
 * zb_rejoinReqWithBackOff() call commented out in favour of plain zb_rejoinReq()
 * on an app-owned timer (apps/sampleSwitch/zb_appCb.c:98-115); this module is
 * that pattern.
 *
 * The campaign is deliberately BOUNDED: REJOIN_CAMPAIGN_ATTEMPTS tries, then it
 * stops dead. A remote left somewhere with no network in range must not scan
 * itself flat. Waking it — a button press, or the stack noticing the parent is
 * gone again — starts a fresh campaign.
 ********************************************************************************/
#ifndef REJOIN_H
#define REJOIN_H

#include "tl_common.h"

/* Start (or restart) a rejoin campaign. `why` is a short literal for the log.
 * Idempotent in the sense that it always restarts from attempt 1 — a fresh user
 * action means "try again now", not "carry on with the old countdown". */
void rejoin_start(const char *why);

/* Cancel any campaign in progress. Call on a successful (re)join and before a
 * factory reset, so we never keep hunting for a network we already have or are
 * deliberately leaving. */
void rejoin_stop(void);

/* True while a campaign is running (attempts remaining). */
bool rejoin_active(void);

#endif /* REJOIN_H */
