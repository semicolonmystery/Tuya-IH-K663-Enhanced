# sdk.mk — compile the Telink SDK sources we link against.
# The closed Zigbee stack itself comes from libzb_ed.a + libdrivers_8258.a; this
# list is the open SDK glue (platform, os, drivers, ZCL clusters). Reused from
# the proven romasku/tuya-zigbee-switch set for SDK v3.7.2.0 on TLSR8258.
# Unreferenced objects are dropped at link time by --gc-sections.

PLATFORM_SOURCES := \
	$(SDK_PATH)/platform/boot/8258/cstartup_8258.S \
	$(SDK_PATH)/platform/boot/link_cfg.S \
	$(SDK_PATH)/platform/services/b85m/irq_handler.c \
	$(SDK_PATH)/platform/tc32/div_mod.S \
	$(SDK_PATH)/platform/chip_8258/flash.c \
	$(SDK_PATH)/platform/chip_8258/flash/flash_common.c \
	$(SDK_PATH)/platform/chip_8258/flash/flash_mid1060c8.c \
	$(SDK_PATH)/platform/chip_8258/flash/flash_mid1360c8.c \
	$(SDK_PATH)/platform/chip_8258/flash/flash_mid011460c8.c \
	$(SDK_PATH)/platform/chip_8258/flash/flash_mid134051.c \
	$(SDK_PATH)/platform/chip_8258/flash/flash_mid136085.c \
	$(SDK_PATH)/platform/chip_8258/flash/flash_mid1360eb.c \
	$(SDK_PATH)/platform/chip_8258/flash/flash_mid14325e.c \
	$(SDK_PATH)/platform/chip_8258/flash/flash_mid1460c8.c \
	$(SDK_PATH)/platform/chip_8258/flash/flash_mid13325e.c \
	$(SDK_PATH)/platform/chip_8258/adc.c

PROJ_SOURCES := \
	$(SDK_PATH)/proj/common/list.c \
	$(SDK_PATH)/proj/common/mempool.c \
	$(SDK_PATH)/proj/common/tlPrintf.c \
	$(SDK_PATH)/proj/common/string.c \
	$(SDK_PATH)/proj/common/utility.c \
	$(SDK_PATH)/proj/drivers/drv_gpio.c \
	$(SDK_PATH)/proj/drivers/drv_adc.c \
	$(SDK_PATH)/proj/drivers/drv_nv.c \
	$(SDK_PATH)/proj/drivers/drv_pm.c \
	$(SDK_PATH)/proj/drivers/drv_putchar.c \
	$(SDK_PATH)/proj/drivers/drv_timer.c \
	$(SDK_PATH)/proj/drivers/drv_uart.c \
	$(SDK_PATH)/proj/drivers/drv_calibration.c \
	$(SDK_PATH)/proj/drivers/drv_flash.c \
	$(SDK_PATH)/proj/drivers/drv_hw.c \
	$(SDK_PATH)/proj/drivers/drv_security.c \
	$(SDK_PATH)/proj/os/ev.c \
	$(SDK_PATH)/proj/os/ev_buffer.c \
	$(SDK_PATH)/proj/os/ev_poll.c \
	$(SDK_PATH)/proj/os/ev_queue.c \
	$(SDK_PATH)/proj/os/ev_timer.c \
	$(SDK_PATH)/proj/os/ev_rtc.c

ZIGBEE_SOURCES := \
	$(SDK_PATH)/zigbee/bdb/bdb.c \
	$(SDK_PATH)/zigbee/aps/aps_group.c \
	$(SDK_PATH)/zigbee/mac/mac_phy.c \
	$(SDK_PATH)/zigbee/mac/mac_pib.c \
	$(SDK_PATH)/zigbee/zdo/zdp.c \
	$(SDK_PATH)/zigbee/zcl/zcl.c \
	$(SDK_PATH)/zigbee/zcl/zcl_nv.c \
	$(SDK_PATH)/zigbee/zcl/zcl_reporting.c \
	$(SDK_PATH)/zigbee/zcl/zll_commissioning/zcl_toucklink_security.c \
	$(SDK_PATH)/zigbee/zcl/zll_commissioning/zcl_zllTouchLinkDiscovery.c \
	$(SDK_PATH)/zigbee/zcl/zll_commissioning/zcl_zllTouchLinkJoinOrStart.c \
	$(SDK_PATH)/zigbee/zcl/zll_commissioning/zcl_zll_commissioning.c \
	$(SDK_PATH)/zigbee/zcl/commissioning/zcl_commissioning.c \
	$(SDK_PATH)/zigbee/zcl/commissioning/zcl_commissioning_attr.c \
	$(SDK_PATH)/zigbee/zcl/light_color_control/zcl_light_colorCtrl.c \
	$(SDK_PATH)/zigbee/zcl/light_color_control/zcl_light_colorCtrl_attr.c \
	$(SDK_PATH)/zigbee/zcl/general/zcl_basic.c \
	$(SDK_PATH)/zigbee/zcl/general/zcl_basic_attr.c \
	$(SDK_PATH)/zigbee/zcl/general/zcl_group.c \
	$(SDK_PATH)/zigbee/zcl/general/zcl_group_attr.c \
	$(SDK_PATH)/zigbee/zcl/general/zcl_identify.c \
	$(SDK_PATH)/zigbee/zcl/general/zcl_identify_attr.c \
	$(SDK_PATH)/zigbee/zcl/general/zcl_level.c \
	$(SDK_PATH)/zigbee/zcl/general/zcl_level_attr.c \
	$(SDK_PATH)/zigbee/zcl/general/zcl_multistate_input.c \
	$(SDK_PATH)/zigbee/zcl/general/zcl_multistate_input_attr.c \
	$(SDK_PATH)/zigbee/zcl/general/zcl_onoff.c \
	$(SDK_PATH)/zigbee/zcl/general/zcl_onoff_attr.c \
	$(SDK_PATH)/zigbee/zcl/general/zcl_pollCtrl.c \
	$(SDK_PATH)/zigbee/zcl/general/zcl_pollCtrl_attr.c \
	$(SDK_PATH)/zigbee/zcl/general/zcl_powerCfg.c \
	$(SDK_PATH)/zigbee/zcl/general/zcl_powerCfg_attr.c \
	$(SDK_PATH)/zigbee/zcl/general/zcl_scene.c \
	$(SDK_PATH)/zigbee/zcl/general/zcl_scene_attr.c \
	$(SDK_PATH)/zigbee/zcl/general/zcl_time.c \
	$(SDK_PATH)/zigbee/zcl/general/zcl_time_attr.c \
	$(SDK_PATH)/zigbee/zcl/ota_upgrading/zcl_ota.c \
	$(SDK_PATH)/zigbee/zcl/ota_upgrading/zcl_ota_attr.c \
	$(SDK_PATH)/zigbee/common/zb_config.c \
	$(SDK_PATH)/zigbee/af/zb_af.c \
	$(SDK_PATH)/zigbee/gp/gp.c \
	$(SDK_PATH)/zigbee/gp/gpEpCfg.c \
	$(SDK_PATH)/zigbee/gp/gp_proxy.c \
	$(SDK_PATH)/zigbee/gp/gp_proxyTab.c \
	$(SDK_PATH)/zigbee/ss/ss_nv.c \
	$(SDK_PATH)/zigbee/ota/ota.c \
	$(SDK_PATH)/zigbee/ota/otaEpCfg.c

ALL_SDK_SOURCES := $(PLATFORM_SOURCES) $(PROJ_SOURCES) $(ZIGBEE_SOURCES)

SDK_OBJS := $(ALL_SDK_SOURCES:$(SDK_PATH)/%.c=$(BUILD_DIR)/sdk/%.o)
SDK_OBJS := $(SDK_OBJS:$(SDK_PATH)/%.S=$(BUILD_DIR)/sdk/%.o)

SDK_C_SOURCES := $(filter %.c,$(ALL_SDK_SOURCES))
SDK_DEPS := $(SDK_C_SOURCES:$(SDK_PATH)/%.c=$(BUILD_DIR)/sdk/%.d)
-include $(SDK_DEPS)

# The stack is built with packed structs.
$(SDK_OBJS): GCC_FLAGS += -fpack-struct

$(BUILD_DIR)/sdk/%.o: $(SDK_PATH)/%.c
	@mkdir -p $(dir $@)
	@echo "CC  (sdk) $<"
	@$(CC) $(GCC_FLAGS) $(DEVICE_DEFS) $(INCLUDE_PATHS) -c -o $@ $<

$(BUILD_DIR)/sdk/%.o: $(SDK_PATH)/%.S
	@mkdir -p $(dir $@)
	@echo "AS  (sdk) $<"
	@$(CC) $(ASM_FLAGS) $(DEVICE_DEFS) $(INCLUDE_PATHS) -c -o $@ $<
