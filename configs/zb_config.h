/********************************************************************************
 * zb_config.h — Zigbee stack network behaviour (F9).
 *
 * This is where the previous attempt failed: slow reconnects, then battery
 * drain once reconnect was made aggressive. The fix is to let the stack drive
 * rejoin/backoff/polling and just tune these macros. All values come from
 * app_config.h (CFG_*) so the user tunes in ONE place. See app_config.h for the
 * reconnect-speed vs battery-life tradeoff notes.
 ********************************************************************************/
#ifndef ZB_CONFIG_H
#define ZB_CONFIG_H

#include "tl_common.h"
#include "app_config.h"

/* Stack profile: Zigbee PRO. */
#define ZB_STACK_PROFILE           2
#define ZB_PROTOCOL_VERSION        2
#define ZB_PROTOCOL_VERSION_GP     3

#define ZB_FRAME_TYPE_MASK         0x03
#define ZB_PROTOCOL_VER_MASK       0x3c
#define ZB_PROTOCOL_VER_POS        2

#define MAC_FRAME_VERSION          MAC_FRAME_IEEE_802_15_4_2003

#ifndef ZB_NWK_DISTRIBUTED_ADDRESS_ASSIGN
#define ZB_NWK_STOCHASTIC_ADDRESS_ASSIGN
#endif

/* ZDO defaults */
#define ZDO_PERMIT_JOIN_DURATION   0

/* Network discovery */
#define ZDO_NWK_SCAN_ATTEMPTS      3
#define ZDO_NWK_TIME_BTWN_SCANS    100

/* --- Polling (F9): long normal poll for a battery ZED; fast rates only during
 *     active exchanges. All in ms. -------------------------------------------- */
#define POLL_RATE_QUARTERSECONDS   250
#define ZDO_NWK_INDIRECT_POLL_RATE CFG_POLL_RATE_NORMAL_MS
#define POLL_NO_DATA_MAX_COUNT     3
#define POLL_RATE                  CFG_POLL_RATE_NORMAL_MS
#define RESPONSE_POLL_RATE         CFG_POLL_RATE_RESPONSE_MS
#define QUEUE_POLL_RATE            CFG_POLL_RATE_QUEUE_MS
#define REJOIN_POLL_RATE           CFG_POLL_RATE_REJOIN_MS

/* --- Parent link + rejoin backoff (F9). ------------------------------------ */
#define ZDO_MAX_PARENT_THRESHOLD_RETRY   CFG_ZDO_MAX_PARENT_THRESHOLD_RETRY
#define ZDO_REJOIN_TIMES                 CFG_ZDO_REJOIN_TIMES
#define ZDO_REJOIN_DURATION              CFG_ZDO_REJOIN_DURATION
#define ZDO_REJOIN_BACKOFF_TIME          CFG_ZDO_REJOIN_BACKOFF_TIME
#define ZDO_MAX_REJOIN_BACKOFF_TIME      CFG_ZDO_MAX_REJOIN_BACKOFF_TIME
#define ZDO_REJOIN_BACKOFF_ITERATION     CFG_ZDO_REJOIN_BACKOFF_ITERATION

/* Security */
#define CCM_KEY_SIZE               16
#define SECUR_N_SECUR_MATERIAL     2
#define ZB_CCM_M                   4
#define ZB_SECURITY                1
#define APS_FRAME_SECURITY
#define ZB_STACK_PROFILE_2007

/* RF: battery-safe TX power (do not raise — coin cell). */
#define ZB_DEFAULT_TX_POWER_IDX    TX_POWER_IDX

#endif /* ZB_CONFIG_H */
