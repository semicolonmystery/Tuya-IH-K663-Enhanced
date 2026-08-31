/********************************************************************************
 * poll_control.c — Poll Control cluster (0x0020) server logic (F4b).
 * See poll_control.h for the rationale.
 ********************************************************************************/
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"

#include "app_config.h"
#include "zigbee_app.h"
#include "poll_control.h"
#include "debug.h"

#if ZCL_POLL_CTRL_SUPPORT

#include "general/zcl_pollCtrl.h"

static ev_timer_event_t *s_checkInTimer;
static ev_timer_event_t *s_fastPollTimer;
static u8 s_fastPolling;

bool pollctrl_fastPolling(void)
{
    return s_fastPolling != 0;
}

/* Switch between the long (idle) and short (fast) poll intervals. */
static void set_fast_poll(u8 fast)
{
    u32 qs = fast ? g_pollCtrlAttrs.shortPollInterval
                  : g_pollCtrlAttrs.longPollInterval;

    s_fastPolling = fast;

    /* An OTA download owns the poll rate while it runs — dropping back to the
     * long poll here (e.g. because a fast-poll window expired mid-transfer)
     * would stall it. Remember the state; ota_session_end() calls
     * pollctrl_restore_rate() to hand the rate back when it finishes. */
    if (app_otaBusy()) {
        return;
    }

    zb_setPollRate(qs * POLL_RATE_QUARTERSECONDS);
    DBG("poll=%s rate=%dms\n", fast ? "fast" : "long",
        (int)(qs * POLL_RATE_QUARTERSECONDS));
}

void pollctrl_restore_rate(void)
{
    set_fast_poll(s_fastPolling);
}

static int fast_poll_timeout_cb(void *arg)
{
    s_fastPollTimer = NULL;
    set_fast_poll(0);
    return -1;
}

/* Open a bounded fast-poll window. `qs` is in quarter-seconds; 0 means "use the
 * configured default". Always clamped to fastPollTimeoutMax so the radio can
 * never be left running indefinitely. */
static void fast_poll_begin(u32 qs)
{
    if (qs == 0) {
        qs = g_pollCtrlAttrs.fastPollTimeout;
    }
    if (g_pollCtrlAttrs.fastPollTimeoutMax &&
        qs > g_pollCtrlAttrs.fastPollTimeoutMax) {
        qs = g_pollCtrlAttrs.fastPollTimeoutMax;
    }
    if (!qs) {
        return;
    }

    if (s_fastPollTimer) {
        TL_ZB_TIMER_CANCEL(&s_fastPollTimer);
    }
    set_fast_poll(1);
    s_fastPollTimer = TL_ZB_TIMER_SCHEDULE(fast_poll_timeout_cb, NULL,
                                           qs * POLL_RATE_QUARTERSECONDS);
}

static void fast_poll_end(void)
{
    if (s_fastPollTimer) {
        TL_ZB_TIMER_CANCEL(&s_fastPollTimer);
    }
    set_fast_poll(0);
}

/* ---------------------------------------------------------------------------
 * Check-in
 * ------------------------------------------------------------------------- */
static void checkin_send(void)
{
    epInfo_t dst;
    TL_SETSTRUCTCONTENT(dst, 0);
    dst.dstAddrMode = APS_DSTADDR_EP_NOTPRESETNT;   /* binding table */
    dst.dstEp       = APP_ENDPOINT;
    dst.profileId   = HA_PROFILE_ID;

    DBG("poll=checkin\n");
    zcl_pollCtrl_checkInCmd(APP_ENDPOINT, &dst, TRUE);
}

static int checkin_cb(void *arg)
{
    if (!g_pollCtrlAttrs.chkInInterval) {
        s_checkInTimer = NULL;
        return -1;
    }
    checkin_send();
    return 0;   /* repeat at the interval this timer was armed with */
}

void pollctrl_start(void)
{
    if (!g_pollCtrlAttrs.chkInInterval) {
        return;
    }
    /* Re-arm rather than stack timers, so a rejoin picks up a check-in interval
     * the coordinator may have rewritten since we last started. */
    if (s_checkInTimer) {
        TL_ZB_TIMER_CANCEL(&s_checkInTimer);
    }
    s_checkInTimer = TL_ZB_TIMER_SCHEDULE(checkin_cb, NULL,
                                          g_pollCtrlAttrs.chkInInterval *
                                          POLL_RATE_QUARTERSECONDS);
}

void pollctrl_stop(void)
{
    if (s_checkInTimer) {
        TL_ZB_TIMER_CANCEL(&s_checkInTimer);
    }
    fast_poll_end();
}

/* ---------------------------------------------------------------------------
 * Incoming command handlers
 * ------------------------------------------------------------------------- */
static status_t on_checkin_rsp(zcl_chkInRsp_t *pCmd)
{
    if (!pCmd->startFastPolling) {
        return ZCL_STA_SUCCESS;     /* nothing queued — stay on the long poll */
    }
    fast_poll_begin(pCmd->fastPollTimeout);
    return ZCL_STA_SUCCESS;
}

static status_t on_fast_poll_stop(void)
{
    if (!s_fastPolling) {
        return ZCL_STA_ACTION_DENIED;
    }
    fast_poll_end();
    return ZCL_STA_SUCCESS;
}

static status_t on_set_long_poll_interval(zcl_setLongPollInterval_t *pCmd)
{
    if (pCmd->newLongPollInterval < g_pollCtrlAttrs.longPollIntervalMin) {
        return ZCL_STA_INVALID_VALUE;
    }
    g_pollCtrlAttrs.longPollInterval = pCmd->newLongPollInterval;
    if (!s_fastPolling) {
        set_fast_poll(0);           /* apply the new long rate immediately */
    }
    return ZCL_STA_SUCCESS;
}

static status_t on_set_short_poll_interval(zcl_setShortPollInterval_t *pCmd)
{
    g_pollCtrlAttrs.shortPollInterval = pCmd->newShortPollInterval;
    if (s_fastPolling) {
        set_fast_poll(1);           /* apply the new fast rate immediately */
    }
    return ZCL_STA_SUCCESS;
}

status_t pollctrl_cb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload)
{
    if (pAddrInfo->dstEp != APP_ENDPOINT ||
        pAddrInfo->dirCluster != ZCL_FRAME_CLIENT_SERVER_DIR) {
        return ZCL_STA_SUCCESS;
    }

    switch (cmdId) {
    case ZCL_CMD_CHK_IN_RSP:
        return on_checkin_rsp((zcl_chkInRsp_t *)cmdPayload);
    case ZCL_CMD_FAST_POLL_STOP:
        return on_fast_poll_stop();
    case ZCL_CMD_SET_LONG_POLL_INTERVAL:
        return on_set_long_poll_interval((zcl_setLongPollInterval_t *)cmdPayload);
    case ZCL_CMD_SET_SHORT_POLL_INTERVAL:
        return on_set_short_poll_interval((zcl_setShortPollInterval_t *)cmdPayload);
    default:
        return ZCL_STA_UNSUP_CLUSTER_COMMAND;
    }
}

#endif /* ZCL_POLL_CTRL_SUPPORT */
