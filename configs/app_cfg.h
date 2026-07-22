/********************************************************************************
 * app_cfg.h — SDK entry configuration (the SDK includes this by name).
 *
 * This wires our single tunables file (app_config.h) into the Telink SDK's
 * expected configuration macros, selects the sleepy-ED feature set, and pulls
 * in the version / stack / network config headers. We deliberately do NOT
 * include any SDK board_*.h: this is one device with a fixed pinout defined in
 * app_config.h, and the precompiled stack/driver libs don't need board macros.
 ********************************************************************************/
#ifndef APP_CFG_H
#define APP_CFG_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "app_config.h"      /* our master tunables + pinout + identity */

/* ---- Core platform ---- */
#define CLOCK_SYS_CLOCK_HZ         48000000   /* TLSR8258 @ 48 MHz */
#define CLOCK_32K_EXT_CRYSTAL      0          /* use internal 32K RC */

/* ---- Power management (sleepy end device) ---- */
#ifndef PM_ENABLE
#define PM_ENABLE                  1
#endif
#define PA_ENABLE                  0          /* no external PA */

/* ---- Modules ---- */
#define FLASH_PROTECT_ENABLE       1
#define MODULE_WATCHDOG_ENABLE     1
#define MODULE_UART_ENABLE         0          /* debug is TX-only bit-bang printf */
#define VOLTAGE_DETECT_ENABLE      0          /* battery read is app-driven (F7) */
/* The SDK still arms flash brownout protection via this pin when detection is
 * off; 0 selects the internal VBAT path (matches F7). */
#define VOLTAGE_DETECT_ADC_PIN     0

/* ---- Debug UART (TX-only bit-bang printf) ---- */
#if DEBUG_UART_ENABLED
#define UART_PRINTF_MODE           1
#else
#define UART_PRINTF_MODE           0
#endif
#define DEBUG_INFO_TX_PIN          DEBUG_TX_PIN
#define BAUDRATE                   DEBUG_BAUDRATE

/* ---- Host controller interface (unused) ---- */
#if (ZBHCI_USB_PRINT || ZBHCI_USB_CDC || ZBHCI_USB_HID || ZBHCI_UART)
#define ZBHCI_EN                   1
#endif

/* ---- Version (OTA identity / Basic cluster) ---- */
#include "version_cfg.h"

/* ---- Stack sizing + role ---- */
#include "stack_cfg.h"

/* ---- Network behaviour / rejoin / poll (F9) ---- */
#include "zb_config.h"

/* ---- BDB / commissioning features ---- */
#define TOUCHLINK_SUPPORT          0
#define FIND_AND_BIND_SUPPORT      0

/* ---- ZCL clusters this device implements ---- *
 * Server: Basic, Identify, Power Config, Multistate Input (gesture publish).
 * Client: On/Off, Level, Color Control, Identify, OTA.                        */
#define ZCL_ON_OFF_SUPPORT                 1
#define ZCL_LEVEL_CTRL_SUPPORT             1
#define ZCL_LIGHT_COLOR_CONTROL_SUPPORT    1
#define ZCL_GROUP_SUPPORT                  1
#define ZCL_OTA_SUPPORT                    1
#define ZCL_POWER_CFG_SUPPORT              1
#define ZCL_POLL_CTRL_SUPPORT              1
#define ZCL_MULTISTATE_INPUT_SUPPORT       1

/* ---- Event poll slots ---- */
typedef enum {
    EV_POLL_ED_DETECT,
    EV_POLL_IDLE,
    EV_POLL_MAX,
} ev_poll_e;

#if defined(__cplusplus)
}
#endif

#endif /* APP_CFG_H */
