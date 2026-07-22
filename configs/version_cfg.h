/********************************************************************************
 * version_cfg.h — firmware version + OTA identity for the IH-K663 build.
 * Pulls the SDK flash-layout / image-address definitions from the SDK's
 * apps/common/comm_cfg.h so we stay compatible with the stock OTA scheme.
 ********************************************************************************/
#pragma once

#include "comm_cfg.h"        /* SDK: APP_IMAGE_ADDR, IMAGE_OFFSET base, chip defs */
#include "app_config.h"      /* our identity: APP_MANUFACTURER_CODE / APP_IMAGE_TYPE */

/* Chip selection for the TLSR8258 with 512 KB flash. */
#if defined(MCU_CORE_8258)
    #define CHIP_TYPE                   TLSR_8258_512K
#endif

/* App / stack version bytes. Bump APP_BUILD for each OTA-visible release. */
#define APP_RELEASE                     0x10  /* app 1.0 */
#ifndef APP_BUILD
#define APP_BUILD                       0x01  /* app build 01 (Makefile may override) */
#endif
#define STACK_RELEASE                   0x30  /* stack 3.0 */
#define STACK_BUILD                     0x01

/* OTA matching fields (manufacturer code + image type + file version). Kept in
 * lock-step with the .ota wrapper (make_ota.py) and the Z2M OTA index. */
#define MANUFACTURER_CODE_TELINK        APP_MANUFACTURER_CODE
#define IMAGE_TYPE                      APP_IMAGE_TYPE
#ifndef FILE_VERSION
#define FILE_VERSION                    ((APP_RELEASE << 24) | (APP_BUILD << 16) | (STACK_RELEASE << 8) | STACK_BUILD)
#endif

/* Pre-compiled link configuration (normal, non-bootloader image at 0x0). */
#define IS_BOOT_LOADER_IMAGE            0
#define RESV_FOR_APP_RAM_CODE_SIZE      0
#define IMAGE_OFFSET                    APP_IMAGE_ADDR
