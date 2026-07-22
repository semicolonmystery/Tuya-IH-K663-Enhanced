/********************************************************************************
 * stack_cfg.h — Zigbee stack sizing + role for the IH-K663 (sleepy end device).
 ********************************************************************************/
#ifndef STACK_CFG_H
#define STACK_CFG_H

#include "app_config.h"

/* Default working channel (11-26). The stack scans all channels in the primary
 * set during steering; this is only the touchlink/target default. */
#define DEFAULT_CHANNEL            11

#define NV_ENABLE                  1
#define SECURITY_ENABLE            1

/* ZCL sizing */
#define ZCL_CLUSTER_NUM_MAX        24    /* in + out clusters on our one endpoint */
#define ZCL_REPORTING_TABLE_NUM    8     /* battery + gesture reporting entries    */
#define ZCL_SCENE_TABLE_NUM        1

/* APS sizing — binding-table only command delivery (F). */
#define APS_GROUP_TABLE_NUM        4
#define APS_BINDING_TABLE_NUM      BINDING_TABLE_SIZE

/* Sleepy end device role. */
#define ZB_ED_ROLE                 1

/* Power management requires RX-off-when-idle. */
#if PM_ENABLE
#define ZB_MAC_RX_ON_WHEN_IDLE     0
#endif
#ifndef ZB_MAC_RX_ON_WHEN_IDLE
#define ZB_MAC_RX_ON_WHEN_IDLE     0
#endif

#endif /* STACK_CFG_H */
