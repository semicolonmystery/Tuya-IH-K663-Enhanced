/********************************************************************************
 * debug.h — one gate for all debug UART output.
 *
 * DBG(...) compiles to a printf when DEBUG_UART_ENABLED (app_config.h) is 1, and
 * to nothing when it is 0 — so a release build (DEBUG_UART_ENABLED 0) carries no
 * formatting code, no flash cost and no power cost for logging. app_cfg.h also
 * turns the SDK's own UART_PRINTF_MODE off in that case, so the debug pin is
 * fully idle.
 ********************************************************************************/
#ifndef APP_DEBUG_H
#define APP_DEBUG_H

#include "app_config.h"

#if DEBUG_UART_ENABLED
#include "tl_common.h"
#define DBG(...)   printf(__VA_ARGS__)
#else
#define DBG(...)   ((void)0)
#endif

#endif /* APP_DEBUG_H */
