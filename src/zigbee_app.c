/********************************************************************************
 * zigbee_app.c — Zigbee application core for the IH-K663 (M1 skeleton).
 *
 * M1 goal: a sleepy-ED image that boots on the TLSR8258, initialises the stack,
 * registers endpoint 1 with our identity, and prints a banner on the debug UART.
 * Gestures, LED, battery, sleep, rejoin tuning and OTA arrive in later
 * milestones. Everything tunable lives in app_config.h.
 ********************************************************************************/
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "bdb.h"

#include "app_config.h"
#include "zigbee_app.h"

#define APP_ENDPOINT    1

/* ---------------------------------------------------------------------------
 * Identity — ZCL character strings are length-prefixed (first byte = length).
 * Kept in lock-step with the Z2M converter fingerprint (F12).
 * ------------------------------------------------------------------------- */
static const u8 g_modelId[]  = {10, 'T','S','0','0','4','1','-','C','U','S'};
static const u8 g_mfrName[]  = {20, '_','T','Z','3','0','0','0','_','f','a','9','m','l','v','j','a','-','C','U','S'};

/* ---------------------------------------------------------------------------
 * Endpoint / cluster lists
 * ------------------------------------------------------------------------- */
static const u16 app_inClusterList[] = {
    ZCL_CLUSTER_GEN_BASIC,
    ZCL_CLUSTER_GEN_IDENTIFY,
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
    HA_PROFILE_ID,
    HA_DEV_ONOFF_SWITCH,
    APP_ENDPOINT,
    1,                              /* device version */
    0,                              /* reserved */
    APP_IN_CLUSTER_NUM,
    APP_OUT_CLUSTER_NUM,
    (u16 *)app_inClusterList,
    (u16 *)app_outClusterList,
};

/* ---- Basic cluster ---- *
 * The SDK doesn't provide the attribute-storage structs — each app defines its
 * own. String attributes point at the length-prefixed arrays above. */
typedef struct {
    u8 zclVersion;
    u8 appVersion;
    u8 stackVersion;
    u8 hwVersion;
    u8 powerSource;
    u8 deviceEnable;
} app_basicAttr_t;

static app_basicAttr_t g_basicAttrs = {
    .zclVersion   = 0x03,
    .appVersion   = 0x00,
    .stackVersion = 0x02,
    .hwVersion    = 0x00,
    .powerSource  = POWER_SOURCE_BATTERY,
    .deviceEnable = TRUE,
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

/* ---- Identify cluster ---- */
typedef struct { u16 identifyTime; } app_identifyAttr_t;
static app_identifyAttr_t g_identifyAttrs = { .identifyTime = 0x0000 };

static const zclAttrInfo_t identify_attrTbl[] = {
    { ZCL_ATTRID_IDENTIFY_TIME,           ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8 *)&g_identifyAttrs.identifyTime },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ, (u8 *)&zcl_attr_global_clusterRevision },
};
#define IDENTIFY_ATTR_NUM   (sizeof(identify_attrTbl) / sizeof(zclAttrInfo_t))

static const zcl_specClusterInfo_t g_appClusterList[] = {
    { ZCL_CLUSTER_GEN_BASIC,    MANUFACTURER_CODE_NONE, BASIC_ATTR_NUM,    basic_attrTbl,    zcl_basic_register,    NULL },
    { ZCL_CLUSTER_GEN_IDENTIFY, MANUFACTURER_CODE_NONE, IDENTIFY_ATTR_NUM, identify_attrTbl, zcl_identify_register, NULL },
};
#define APP_CLUSTER_NUM   (sizeof(g_appClusterList) / sizeof(g_appClusterList[0]))

/* ---------------------------------------------------------------------------
 * ZDO / BDB callbacks
 * ------------------------------------------------------------------------- */
const zdo_appIndCb_t appCbLst = {
    bdb_zdoStartDevCnf,     /* start device cnf */
    NULL,                   /* reset cnf */
    NULL,                   /* device announce ind */
    NULL,                   /* leave ind */
    NULL,                   /* leave cnf */
    NULL,                   /* nwk update ind */
    NULL,                   /* permit join ind */
    NULL,                   /* nlme sync cnf */
    NULL,                   /* tc join ind */
    NULL,                   /* tc frame counter near limit */
    NULL,                   /* nwk status ind */
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

static void app_bdbInitCb(u8 status, u8 joinedNetwork)
{
    printf("net=bdb_init status=%d joined=%d\n", status, joinedNetwork);
    /* Steering is only started by the reset/pair gesture (F10) — never here. */
}

static void app_bdbCommissioningCb(u8 status, void *arg)
{
    printf("net=commission status=%d\n", status);
    /* Real rejoin handling (F9) lands in M7. */
}

bdb_appCb_t g_appBdbCb = {
    app_bdbInitCb,
    app_bdbCommissioningCb,
    NULL,
    NULL,
};

#if PM_ENABLE
static drv_pm_pinCfg_t g_pmWakeupCfg[] = {
    { BUTTON_PIN, PM_WAKEUP_LEVEL_LOW },   /* button is active-low */
};
#endif

/* ---------------------------------------------------------------------------
 * ZCL foundation hook — minimal for M1 (per-cluster handling arrives in M4).
 * ------------------------------------------------------------------------- */
static void app_zclProcessIncomingMsg(zclIncoming_t *pInMsg)
{
    /* Per-cluster command handling arrives in M4. */
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
        printf("\n== IH-K663 boot (M1) model=%s ==\n", APP_MODEL_ID);
    }

#if PM_ENABLE
    drv_pm_wakeupPinConfig(g_pmWakeupCfg, sizeof(g_pmWakeupCfg) / sizeof(drv_pm_pinCfg_t));
#endif

    if (!isRetention) {
        app_stack_init();
        app_zb_init();

        u8 repower = drv_pm_deepSleep_flag_get() ? 0 : 1;
        bdb_init((af_simple_descriptor_t *)&app_simpleDesc,
                 &g_bdbCommissionSetting, &g_appBdbCb, repower);
    } else {
        mac_phyReconfig();
    }
}
