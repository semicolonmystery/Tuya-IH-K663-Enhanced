/********************************************************************************
 * zigbee_app.c — Zigbee application core for the IH-K663.
 *
 * Endpoint 1 (HA): server Basic / Identify / Power Config / Multistate Input;
 * client On/Off / Level / Color Control / Groups. Gestures drive bound lights
 * (F3, binding-table only) and are published to the coordinator via Multistate
 * Input (F8). Pairing/join via the reset gesture (F10). Battery via F7.
 * All tunables live in app_config.h.
 ********************************************************************************/
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "general/zcl_multistate_input.h"
#include "bdb.h"

#include "app_config.h"
#include "zigbee_app.h"
#include "buttons.h"
#include "gestures.h"
#include "led_effects.h"
#include "battery.h"
#include "debug.h"

#define APP_ENDPOINT    1

/* F8 action codes carried in Multistate Input presentValue (0x0055). */
enum {
    ACT_SINGLE_CLICK = 1, ACT_DOUBLE_CLICK, ACT_TRIPLE_CLICK,
    ACT_SINGLE_HOLD_START, ACT_SINGLE_HOLD_STOP,
    ACT_DOUBLE_HOLD_START, ACT_DOUBLE_HOLD_STOP,
    ACT_TRIPLE_HOLD_START, ACT_TRIPLE_HOLD_STOP,
};
/* Manufacturer-specific attr on Multistate Input carrying the last hold
 * duration (ms); reported immediately before a *_hold_stop (F8). */
#define ATTR_HOLD_DURATION      0xF001

/* ---------------------------------------------------------------------------
 * Identity — length-prefixed ZCL strings (must match the Z2M fingerprint).
 * ------------------------------------------------------------------------- */
static const u8 g_modelId[] = {15, 'T','S','0','0','4','1','-','E','n','h','a','n','c','e','d'};
static const u8 g_mfrName[] = {25, '_','T','Z','3','0','0','0','_','f','a','9','m','l','v','j','a','-','E','n','h','a','n','c','e','d'};

/* ---------------------------------------------------------------------------
 * Endpoint / cluster lists
 * ------------------------------------------------------------------------- */
static const u16 app_inClusterList[] = {
    ZCL_CLUSTER_GEN_BASIC,
    ZCL_CLUSTER_GEN_IDENTIFY,
    ZCL_CLUSTER_GEN_POWER_CFG,
    ZCL_CLUSTER_GEN_MULTISTATE_INPUT_BASIC,
};

static const u16 app_outClusterList[] = {
    ZCL_CLUSTER_GEN_GROUPS,
    ZCL_CLUSTER_GEN_ON_OFF,
    ZCL_CLUSTER_GEN_LEVEL_CONTROL,
    ZCL_CLUSTER_LIGHTING_COLOR_CONTROL,
};

#define APP_IN_CLUSTER_NUM   (sizeof(app_inClusterList)  / sizeof(app_inClusterList[0]))
#define APP_OUT_CLUSTER_NUM  (sizeof(app_outClusterList) / sizeof(app_outClusterList[0]))

const af_simple_descriptor_t app_simpleDesc = {
    HA_PROFILE_ID, HA_DEV_ONOFF_SWITCH, APP_ENDPOINT, 1, 0,
    APP_IN_CLUSTER_NUM, APP_OUT_CLUSTER_NUM,
    (u16 *)app_inClusterList, (u16 *)app_outClusterList,
};

/* ---- Basic ---- */
typedef struct {
    u8 zclVersion, appVersion, stackVersion, hwVersion, powerSource, deviceEnable;
} app_basicAttr_t;
static app_basicAttr_t g_basicAttrs = {
    .zclVersion = 0x03, .appVersion = 0x00, .stackVersion = 0x02, .hwVersion = 0x00,
    .powerSource = POWER_SOURCE_BATTERY, .deviceEnable = TRUE,
};
static const zclAttrInfo_t basic_attrTbl[] = {
    { ZCL_ATTRID_BASIC_ZCL_VER,           ZCL_DATA_TYPE_UINT8,    ACCESS_CONTROL_READ, (u8 *)&g_basicAttrs.zclVersion },
    { ZCL_ATTRID_BASIC_APP_VER,           ZCL_DATA_TYPE_UINT8,    ACCESS_CONTROL_READ, (u8 *)&g_basicAttrs.appVersion },
    { ZCL_ATTRID_BASIC_STACK_VER,         ZCL_DATA_TYPE_UINT8,    ACCESS_CONTROL_READ, (u8 *)&g_basicAttrs.stackVersion },
    { ZCL_ATTRID_BASIC_HW_VER,            ZCL_DATA_TYPE_UINT8,    ACCESS_CONTROL_READ, (u8 *)&g_basicAttrs.hwVersion },
    { ZCL_ATTRID_BASIC_MFR_NAME,          ZCL_DATA_TYPE_CHAR_STR, ACCESS_CONTROL_READ, (u8 *)g_mfrName },
    { ZCL_ATTRID_BASIC_MODEL_ID,          ZCL_DATA_TYPE_CHAR_STR, ACCESS_CONTROL_READ, (u8 *)g_modelId },
    { ZCL_ATTRID_BASIC_POWER_SOURCE,      ZCL_DATA_TYPE_ENUM8,    ACCESS_CONTROL_READ, (u8 *)&g_basicAttrs.powerSource },
    { ZCL_ATTRID_BASIC_DEV_ENABLED,       ZCL_DATA_TYPE_BOOLEAN,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8 *)&g_basicAttrs.deviceEnable },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16,   ACCESS_CONTROL_READ, (u8 *)&zcl_attr_global_clusterRevision },
};
#define BASIC_ATTR_NUM   (sizeof(basic_attrTbl) / sizeof(zclAttrInfo_t))

/* ---- Identify ---- */
typedef struct { u16 identifyTime; } app_identifyAttr_t;
static app_identifyAttr_t g_identifyAttrs = { .identifyTime = 0x0000 };
static const zclAttrInfo_t identify_attrTbl[] = {
    { ZCL_ATTRID_IDENTIFY_TIME,           ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8 *)&g_identifyAttrs.identifyTime },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&zcl_attr_global_clusterRevision },
};
#define IDENTIFY_ATTR_NUM   (sizeof(identify_attrTbl) / sizeof(zclAttrInfo_t))

/* ---- Power Config (battery values live in battery.c) ---- */
static const zclAttrInfo_t powerCfg_attrTbl[] = {
    { ZCL_ATTRID_BATTERY_VOLTAGE,              ZCL_DATA_TYPE_UINT8,  ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8 *)&g_batteryVoltage },
    { ZCL_ATTRID_BATTERY_PERCENTAGE_REMAINING, ZCL_DATA_TYPE_UINT8,  ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8 *)&g_batteryPercentage },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION,      ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&zcl_attr_global_clusterRevision },
};
#define POWERCFG_ATTR_NUM   (sizeof(powerCfg_attrTbl) / sizeof(zclAttrInfo_t))

/* ---- Multistate Input (gesture publishing, F8) ---- */
static u16 g_msPresentValue = 0;
static u16 g_msNumStates    = 10;
static u8  g_msOutOfService = 0;
static u8  g_msStatusFlags  = 0;
static u32 g_holdDuration   = 0;
static const zclAttrInfo_t multistate_attrTbl[] = {
    { ZCL_MULTISTATE_INPUT_ATTRID_PRESENT_VALUE,  ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8 *)&g_msPresentValue },
    { ZCL_MULTISTATE_INPUT_ATTRID_NUM_OF_STATES,  ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ, (u8 *)&g_msNumStates },
    { ZCL_MULTISTATE_INPUT_ATTRID_OUT_OF_SERVICE, ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ, (u8 *)&g_msOutOfService },
    { ZCL_MULTISTATE_INPUT_ATTRID_STATUS_FLAGS,   ZCL_DATA_TYPE_BITMAP8, ACCESS_CONTROL_READ, (u8 *)&g_msStatusFlags },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION,         ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ, (u8 *)&zcl_attr_global_clusterRevision },
};
#define MULTISTATE_ATTR_NUM   (sizeof(multistate_attrTbl) / sizeof(zclAttrInfo_t))

static const zcl_specClusterInfo_t g_appClusterList[] = {
    { ZCL_CLUSTER_GEN_BASIC,                 MANUFACTURER_CODE_NONE, BASIC_ATTR_NUM,      basic_attrTbl,      zcl_basic_register,           NULL },
    { ZCL_CLUSTER_GEN_IDENTIFY,              MANUFACTURER_CODE_NONE, IDENTIFY_ATTR_NUM,   identify_attrTbl,   zcl_identify_register,        NULL },
    { ZCL_CLUSTER_GEN_POWER_CFG,             MANUFACTURER_CODE_NONE, POWERCFG_ATTR_NUM,   powerCfg_attrTbl,   zcl_powerCfg_register,        NULL },
    { ZCL_CLUSTER_GEN_MULTISTATE_INPUT_BASIC,MANUFACTURER_CODE_NONE, MULTISTATE_ATTR_NUM, multistate_attrTbl, zcl_multistate_input_register,NULL },
};
#define APP_CLUSTER_NUM   (sizeof(g_appClusterList) / sizeof(g_appClusterList[0]))

/* ---------------------------------------------------------------------------
 * ZDO / BDB callbacks + pairing (F10)
 * ------------------------------------------------------------------------- */
const zdo_appIndCb_t appCbLst = {
    bdb_zdoStartDevCnf, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};

bdb_commissionSetting_t g_bdbCommissionSetting = {
    .linkKey.tcLinkKey.keyType         = SS_GLOBAL_LINK_KEY,
    .linkKey.tcLinkKey.key             = (u8 *)tcLinkKeyCentralDefault,
    .linkKey.distributeLinkKey.keyType = MASTER_KEY,
    .linkKey.distributeLinkKey.key     = (u8 *)linkKeyDistributedMaster,
    .linkKey.touchLinkKey.keyType      = MASTER_KEY,
    .linkKey.touchLinkKey.key          = (u8 *)touchLinkKeyMaster,
    .touchlinkEnable                   = 0,
    .touchlinkChannel                  = DEFAULT_CHANNEL,
    .touchlinkLqiThreshold             = 0xA0,
};

static u8 s_pairing;
static ev_timer_event_t *s_pairTimer;

static int pair_timeout_cb(void *arg)
{
    if (s_pairing) {
        s_pairing = 0;
        led_stop();
        DBG("net=pair_timeout\n");
    }
    s_pairTimer = NULL;
    return -1;
}

/* Reset gesture (F10): begin network steering with the pairing LED. On a fresh
 * flash this joins Z2M. (Leave-and-rejoin for re-pairing an already-joined
 * device is refined with the sleep/network work in Test 2.) */
static void start_pairing(void)
{
    DBG("net=pairing\n");
    s_pairing = 1;
    led_pair();
    bdb_networkSteerStart();
    if (s_pairTimer) TL_ZB_TIMER_CANCEL(&s_pairTimer);
    s_pairTimer = TL_ZB_TIMER_SCHEDULE(pair_timeout_cb, NULL, PAIR_WINDOW_MS);
}

/* Network join succeeded — settle poll rate and clear pairing UI. */
static void app_on_joined(void)
{
    if (s_pairing) {
        s_pairing = 0;
        if (s_pairTimer) TL_ZB_TIMER_CANCEL(&s_pairTimer);
        led_stop();
    }
#ifdef ZB_ED_ROLE
    /* Set the poll rate explicitly after (re)connect — the stack can otherwise
     * miss scheduling the poll task on a fast reconnect (per romasku). */
    zb_setPollRate(POLL_RATE);
#endif
    DBG("net=joined\n");
}

static void app_bdbInitCb(u8 status, u8 joinedNetwork)
{
    DBG("net=bdb_init status=%d joined=%d\n", status, joinedNetwork);
    if (status == BDB_INIT_STATUS_SUCCESS) {
        if (joinedNetwork) {
            app_on_joined();
        }
    } else if (joinedNetwork) {
        /* Was joined but init couldn't reach the network — let the stack rejoin. */
        zb_rejoinReqWithBackOff(zb_apsChannelMaskGet(), g_bdbAttrs.scanDuration);
    }
    /* Steering is only started by the reset gesture (F10) — never here. */
}

/* F9 — the whole application-level rejoin policy: hand parent-loss / scan-failure
 * back to the stack's own backoff-driven rejoin. The backoff SCHEDULE is config
 * in zb_config.h (battery-safe max backoff), not code. */
static void app_bdbCommissioningCb(u8 status, void *arg)
{
    DBG("net=commission status=%d\n", status);
    switch (status) {
    case BDB_COMMISSION_STA_SUCCESS:
        app_on_joined();
        break;

    case BDB_COMMISSION_STA_NO_SCAN_RESPONSE:
    case BDB_COMMISSION_STA_PARENT_LOST:
        DBG("rejoin=backoff\n");
        zb_rejoinReqWithBackOff(zb_apsChannelMaskGet(), g_bdbAttrs.scanDuration);
        break;

    case BDB_COMMISSION_STA_REJOIN_FAILURE:
        /* Guard so a never-joined (factory-new) device doesn't loop. */
        if (!zb_isDeviceFactoryNew()) {
            DBG("rejoin=backoff\n");
            zb_rejoinReqWithBackOff(zb_apsChannelMaskGet(), g_bdbAttrs.scanDuration);
        }
        break;

    default:
        break;
    }
}

bdb_appCb_t g_appBdbCb = { app_bdbInitCb, app_bdbCommissioningCb, NULL, NULL };

#if PM_ENABLE
static drv_pm_pinCfg_t g_pmWakeupCfg[] = { { BUTTON_PIN, PM_WAKEUP_LEVEL_LOW } };
#endif

static void app_zclProcessIncomingMsg(zclIncoming_t *pInMsg) { (void)pInMsg; }

/* ---------------------------------------------------------------------------
 * Cluster commands to bindings (F3) — binding-table delivery only.
 * ------------------------------------------------------------------------- */
static void bind_dst(epInfo_t *dst)
{
    TL_SETSTRUCTCONTENT(*dst, 0);
    dst->dstAddrMode = APS_DSTADDR_EP_NOTPRESETNT;   /* use the binding table */
    dst->dstEp       = APP_ENDPOINT;
    dst->profileId   = HA_PROFILE_ID;
}

static void act_toggle(void)
{
    epInfo_t dst; bind_dst(&dst);
    zcl_onOff_toggleCmd(APP_ENDPOINT, &dst, TRUE);
}

static void act_level_move(u8 up)
{
    epInfo_t dst; bind_dst(&dst);
    move_t m; TL_SETSTRUCTCONTENT(m, 0);
    m.moveMode = up ? LEVEL_MOVE_UP : LEVEL_MOVE_DOWN;
    m.rate     = LEVEL_MOVE_RATE;
    zcl_level_moveCmd(APP_ENDPOINT, &dst, TRUE, &m);
}

static void act_level_stop(void)
{
    epInfo_t dst; bind_dst(&dst);
    stop_t s; TL_SETSTRUCTCONTENT(s, 0);
    zcl_level_stopCmd(APP_ENDPOINT, &dst, TRUE, &s);
}

static void act_ct_move(u8 up)
{
    epInfo_t dst; bind_dst(&dst);
    zcl_colorCtrlMoveColorTemperatureCmd_t c; TL_SETSTRUCTCONTENT(c, 0);
    c.moveMode           = up ? COLOR_CTRL_MOVE_UP : COLOR_CTRL_MOVE_DOWN;
    c.rate               = CT_MOVE_RATE;
    c.colorTempMinMireds = CT_MIN_MIREDS;
    c.colorTempMaxMireds = CT_MAX_MIREDS;
    zcl_lightColorCtrl_moveColorTemperatureCmd(APP_ENDPOINT, &dst, TRUE, &c);
}

static void act_ct_stop(void)
{
    epInfo_t dst; bind_dst(&dst);
    zcl_colorCtrlMoveColorTemperatureCmd_t c; TL_SETSTRUCTCONTENT(c, 0);
    c.moveMode = COLOR_CTRL_MOVE_STOP;
    zcl_lightColorCtrl_moveColorTemperatureCmd(APP_ENDPOINT, &dst, TRUE, &c);
}

/* ---------------------------------------------------------------------------
 * Publish the gesture to the coordinator via Multistate Input (F8).
 * ------------------------------------------------------------------------- */
static void publish_action(u16 code, u8 is_hold_stop, u32 dur)
{
    epInfo_t dst; bind_dst(&dst);
    if (is_hold_stop) {
        g_holdDuration = dur;
        zcl_report(APP_ENDPOINT, &dst, TRUE, ZCL_FRAME_SERVER_CLIENT_DIR, ZCL_SEQ_NUM,
                   APP_MANUFACTURER_CODE, ZCL_CLUSTER_GEN_MULTISTATE_INPUT_BASIC,
                   ATTR_HOLD_DURATION, ZCL_DATA_TYPE_UINT32, (u8 *)&g_holdDuration);
    }
    g_msPresentValue = code;
    zcl_sendReportCmd(APP_ENDPOINT, &dst, TRUE, ZCL_FRAME_SERVER_CLIENT_DIR,
                      ZCL_CLUSTER_GEN_MULTISTATE_INPUT_BASIC,
                      ZCL_MULTISTATE_INPUT_ATTRID_PRESENT_VALUE,
                      ZCL_DATA_TYPE_UINT16, (u8 *)&g_msPresentValue);
}

/* ---------------------------------------------------------------------------
 * Direction flip flags (F3a) — retained RAM (survive sleep, reset on power loss).
 * ------------------------------------------------------------------------- */
static u8 s_level_dir = LEVEL_START_DIR;   /* single-hold level up/down       */
static u8 s_ct_dir    = 1;                 /* double-hold colour temp up/down */

/* ---------------------------------------------------------------------------
 * Gesture handler — F3 command + F8 publish + F6 LED effect + logging.
 * ------------------------------------------------------------------------- */
static void on_gesture(const gesture_event_t *e)
{
    if (e->id != G_CLICK_TICK) {
        DBG("gesture=%s dur=%d\n", gesture_name(e->id), (int)e->duration_ms);
    }

    /* F9: a real press while offline kicks an immediate rejoin instead of waiting
     * out the current backoff. */
    if (e->id != G_CLICK_TICK && !s_pairing && !zb_isDeviceJoinedNwk()) {
        DBG("rejoin=button\n");
        zb_rejoinReqWithBackOff(zb_apsChannelMaskGet(), g_bdbAttrs.scanDuration);
    }

    switch (e->id) {
    case G_CLICK_TICK:
        led_blink(1);
        break;

    case G_SINGLE_CLICK:
        act_toggle();
        publish_action(ACT_SINGLE_CLICK, 0, 0);
        break;
    case G_DOUBLE_CLICK:
        publish_action(ACT_DOUBLE_CLICK, 0, 0);
        break;
    case G_TRIPLE_CLICK:
        publish_action(ACT_TRIPLE_CLICK, 0, 0);
        break;

    case G_SINGLE_HOLD_START: {
        u8 d = s_level_dir; s_level_dir ^= 1;
        act_level_move(d);
        led_ramp(LED_RAMP_LEVEL_MS, d);
        publish_action(ACT_SINGLE_HOLD_START, 0, 0);
        break;
    }
    case G_SINGLE_HOLD_STOP:
        act_level_stop();
        led_stop();
        publish_action(ACT_SINGLE_HOLD_STOP, 1, e->duration_ms);
        break;

    case G_DOUBLE_HOLD_START: {
        u8 d = s_ct_dir; s_ct_dir ^= 1;
        act_ct_move(d);
        led_ramp(LED_RAMP_CT_MS, d);
        publish_action(ACT_DOUBLE_HOLD_START, 0, 0);
        break;
    }
    case G_DOUBLE_HOLD_STOP:
        act_ct_stop();
        led_stop();
        publish_action(ACT_DOUBLE_HOLD_STOP, 1, e->duration_ms);
        break;

    case G_TRIPLE_HOLD_START:
        led_ramp(LED_RAMP_TRIPLE_MS, 1);
        publish_action(ACT_TRIPLE_HOLD_START, 0, 0);
        break;
    case G_TRIPLE_HOLD_STOP:
        led_stop();
        publish_action(ACT_TRIPLE_HOLD_STOP, 1, e->duration_ms);
        break;

    case G_RESET:
        start_pairing();
        break;

    case G_OTA_TRIGGER:
        led_pulse(LED_OTA_PULSE_MS);   /* real OTA session lands in Test 3 */
        break;

    case G_STUCK:
        /* Abandon: stop whatever Move might be active, LED off. */
        act_level_stop();
        act_ct_stop();
        led_stop();
        break;

    default:
        break;
    }
}

/* ---------------------------------------------------------------------------
 * Battery (F7) — measure periodically; the stack reports per coordinator config.
 * ------------------------------------------------------------------------- */
static int battery_timer_cb(void *arg)
{
    battery_update();
    return 0;   /* repeat */
}

/* ---------------------------------------------------------------------------
 * Idle task (F4) — runs every main-loop pass. Re-arms the button sampler on a
 * press (incl. one that woke us), then enters deep sleep with retention when
 * nothing needs the CPU. The stack schedules its own wakeups (poll/rejoin), so
 * we never busy-wait and nothing keeps the radio awake indefinitely.
 * ------------------------------------------------------------------------- */
static void app_task(void)
{
    buttons_poll();

#if PM_ENABLE
    if (bdb_isIdle() && zb_isTaskDone() && !tl_stackBusy()
        && !buttons_active() && !gestures_busy() && !led_busy()) {
        drv_pm_lowPowerEnter();
        /* Deep-retention can drop GPIO/analog config — re-apply on wake. This
         * also samples the live button so a waking press is not lost (F1). */
        led_init();
        buttons_init();
    }
#endif
}

/* ---------------------------------------------------------------------------
 * Boot heartbeat — visible sign of life before the PWM engine takes over PC4.
 * ------------------------------------------------------------------------- */
static void led_boot_blink(void)
{
    drv_gpio_func_set(LED_PIN);
    drv_gpio_output_en(LED_PIN, 1);
    for (u8 i = 0; i < 3; i++) {
        drv_gpio_write(LED_PIN, 1);
        WaitMs(120);
        drv_gpio_write(LED_PIN, 0);
        WaitMs(120);
    }
}

/* ---------------------------------------------------------------------------
 * Init
 * ------------------------------------------------------------------------- */
static void app_stack_init(void)
{
    zb_init();
    zb_zdoCbRegister((zdo_appIndCb_t *)&appCbLst);
}

static void app_zb_init(void)
{
    af_powerDescPowerModeUpdate(POWER_MODE_RECEIVER_COMES_WHEN_STIMULATED);
    zcl_init(app_zclProcessIncomingMsg);
    af_endpointRegister(APP_ENDPOINT, (af_simple_descriptor_t *)&app_simpleDesc,
                        zcl_rx_handler, NULL);
    zcl_register(APP_ENDPOINT, APP_CLUSTER_NUM, (zcl_specClusterInfo_t *)g_appClusterList);
}

void user_init(bool isRetention)
{
    if (!isRetention) {
        led_boot_blink();
        DBG("\n== IH-K663 boot model=%s ==\n", APP_MODEL_ID);
    }

#if PM_ENABLE
    drv_pm_wakeupPinConfig(g_pmWakeupCfg, sizeof(g_pmWakeupCfg) / sizeof(drv_pm_pinCfg_t));
#endif

    if (!isRetention) {
        /* Hardware + non-timer init. */
        led_init();
        gestures_init(on_gesture);
        battery_init();

        app_stack_init();
        app_zb_init();

        u8 repower = drv_pm_deepSleep_flag_get() ? 0 : 1;
        bdb_init((af_simple_descriptor_t *)&app_simpleDesc,
                 &g_bdbCommissionSetting, &g_appBdbCb, repower);

        buttons_init();
        TL_ZB_TIMER_SCHEDULE(battery_timer_cb, NULL, BATTERY_MEASURE_MIN_INTERVAL_S * 1000);

        /* Idle task drives the sleep policy (F4). */
        ev_on_poll(EV_POLL_IDLE, app_task);
    } else {
        mac_phyReconfig();
    }
}
