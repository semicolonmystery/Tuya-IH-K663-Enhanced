/********************************************************************************
 * rejoin.c — parent loss and reparenting (F9b). See rejoin.h for the rationale.
 ********************************************************************************/
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "bdb.h"

#include "app_config.h"
#include "rejoin.h"
#include "led_effects.h"
#include "poll_control.h"
#include "debug.h"

static ev_timer_event_t *s_timer;
static u8 s_attempt;        /* attempts already made in this campaign */
static u8 s_secure = 1;     /* alternates per attempt                 */

bool rejoin_active(void)
{
    return s_timer != NULL;
}

void rejoin_stop(void)
{
    if (s_timer) {
        TL_ZB_TIMER_CANCEL(&s_timer);
    }
    s_attempt = 0;
    /* Restore the stack default so the next campaign starts by preferring the
     * parent we last had — the common case is that it is still there. */
    PRE_PARENT_FIRST_WHEN_REJOIN = TRUE;
}

/* One attempt. Kept separate from the timer callback so the first attempt can be
 * made inline from rejoin_start() without the callback's timer bookkeeping —
 * issuing a rejoin straight out of the BDB commissioning callback is exactly what
 * the SDK samples do. */
static void do_attempt(void)
{
    s_attempt++;

    /* First try favours the previous parent (it is usually still there and the
     * reconnect is instant). After that, stop insisting on a parent we evidently
     * cannot hear, so a nearer router gets a chance — this is the reparenting
     * case that walking between rooms actually needs. */
    PRE_PARENT_FIRST_WHEN_REJOIN = (s_attempt == 1) ? TRUE : FALSE;

    /* A parent only buffers indirect data for ~7.7 s, far under the idle poll,
     * so without a fast-poll window the Rejoin Response expires unread. */
#if ZCL_POLL_CTRL_SUPPORT
    pollctrl_fast_window(REJOIN_FAST_POLL_S);
#endif

    DBG("rejoin=try n=%d mode=%s joined=%d\n", (int)s_attempt,
        s_secure ? "sec" : "insec", (int)zb_isDeviceJoinedNwk());

    led_blink_ms(1, LED_BLINK_MS);

    zb_rejoinSecModeSet(s_secure ? REJOIN_SECURITY : REJOIN_INSECURITY);
    zb_rejoinReq(zb_apsChannelMaskGet(), g_bdbAttrs.scanDuration);

    /* Alternate: a secure rejoin keeps the network key, a trust-center rejoin is
     * what succeeds when the secure one will not be accepted. Trying only one of
     * them forever is how a device stays offline indefinitely. */
    s_secure = !s_secure;
}

static int rejoin_attempt_cb(void *arg)
{
    (void)arg;

    /* A factory-new device has no stored network to rejoin; retrying would only
     * burn battery. Mirrors the guard in sampleSwitch_rejoinBackoff(). */
    if (zb_isDeviceFactoryNew() || s_attempt >= REJOIN_CAMPAIGN_ATTEMPTS) {
        /* Stop completely rather than scanning forever: nothing here can find a
         * network that is out of range, and the radio is the whole battery
         * budget. The next wake starts a fresh campaign. */
        DBG("rejoin=giveup after=%d\n", (int)s_attempt);
        s_timer = NULL;
        rejoin_stop();
        return -1;
    }

    do_attempt();
    return 0;   /* repeat at REJOIN_ATTEMPT_INTERVAL_MS */
}

void rejoin_start(const char *why)
{
    if (zb_isDeviceFactoryNew()) {
        /* Never joined anything — pairing (F10), not rejoining, is the answer. */
        return;
    }

    DBG("rejoin=start why=%s\n", why);

    rejoin_stop();
    s_secure = 1;

    /* Arm the repeat first, then make attempt 1 inline: the whole point is that a
     * press must not have to wait out a backoff before anything happens. */
    s_timer = TL_ZB_TIMER_SCHEDULE(rejoin_attempt_cb, NULL,
                                   REJOIN_ATTEMPT_INTERVAL_MS);
    do_attempt();
}
