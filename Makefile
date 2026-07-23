# ==============================================================================
# Tuya IH-K663 custom Zigbee firmware — headless build (TLSR8258 / TC32).
#
#   make tools     download the SDK + TC32 toolchain into telink_tools/
#   make           build the flashable .bin (+ .ota)
#   make ota       build only the .ota image
#   make size      print flash/RAM usage
#   make clean     remove build outputs
#
# Intended to run on Linux (Docker locally, GitHub Actions in CI). Do not run
# the host compiler — the TC32 cross-toolchain is downloaded by `make tools`.
# ==============================================================================

# ---- Paths ----
TOOLS_DIR   := telink_tools
SDK_PATH    := $(TOOLS_DIR)/sdk
TC32        := $(TOOLS_DIR)/toolchain/tc32/bin
BUILD_DIR   := build
BIN_DIR     := $(BUILD_DIR)/bin
SRC_DIR     := src
FW_NAME     := ih-k663

CC      := $(TC32)/tc32-elf-gcc
LD      := $(TC32)/tc32-elf-ld
OBJCOPY := $(TC32)/tc32-elf-objcopy
OBJDUMP := $(TC32)/tc32-elf-objdump
SIZE    := $(TC32)/tc32-elf-size

# ---- Version / OTA identity (overridable by CI) ----
# FILE_VERSION and IMAGE_TYPE are shared by the firmware (via -D) and the .ota
# wrapper so OTA matching stays consistent. Layout: 0x10<APP_BUILD>3001
# = (APP_RELEASE 0x10)(APP_BUILD)(STACK_RELEASE 0x30)(STACK_BUILD 0x01).
APP_BUILD    ?= 1          # bump per OTA-visible release
VERSION_STR  ?= 0.0.0
IMAGE_TYPE   ?= 0x0641     # must match APP_IMAGE_TYPE in app_config.h
MANUF_ID     ?= 0x1141     # Telink manufacturer code
FILE_VERSION ?= $(shell printf '0x10%02x3001' $(APP_BUILD))

# ---- Chip + role ----
DEVICE_DEFS := \
	-DMCU_CORE_8258=1 \
	-DEND_DEVICE=1 \
	-DMCU_STARTUP_8258=1 \
	-DPM_ENABLE \
	-DAPP_BUILD=$(APP_BUILD) \
	-DFILE_VERSION=$(FILE_VERSION) \
	-DVERSION_STR=\"$(VERSION_STR)\"

LIBS          := -ldrivers_8258 -lzb_ed
LINKER_SCRIPT := $(SDK_PATH)/platform/boot/8258/boot_8258.link
LIBRARY_PATHS := -L$(SDK_PATH)/zigbee/lib/tc32 -L$(SDK_PATH)/platform/lib

# ---- Compiler flags (matched to the SDK / reference build) ----
# -fpack-struct is REQUIRED and must match the SDK: the prebuilt stack libs and
# the SDK sources are all built packed, so app code passing structs to the stack
# (simple descriptor, cluster/attr tables, epInfo, ...) must be packed too — an
# unpacked app reads/writes struct fields at the wrong offsets and the stack ends
# up calling a garbage function pointer during endpoint/cluster registration.
GCC_FLAGS := \
	-O2 -ffunction-sections -fdata-sections -Wall -fshort-enums \
	-finline-small-functions -std=gnu99 -funsigned-char -fshort-wchar \
	-fms-extensions -fpack-struct -nostartfiles -nostdlib -MMD -MP

ASM_FLAGS := \
	-fomit-frame-pointer -fshort-enums -Wall -Wpacked -Wcast-align \
	-fdata-sections -ffunction-sections -fno-use-cxa-atexit

LNK_FLAGS := --gc-sections -nostartfiles

INCLUDE_PATHS := \
	-I. \
	-I$(SRC_DIR) \
	-Iconfigs \
	-I$(SDK_PATH)/apps/common \
	-I$(SDK_PATH)/platform \
	-I$(SDK_PATH)/platform/chip_8258 \
	-I$(SDK_PATH)/proj \
	-I$(SDK_PATH)/proj/common \
	-I$(SDK_PATH)/zigbee \
	-I$(SDK_PATH)/zigbee/af \
	-I$(SDK_PATH)/zigbee/include \
	-I$(SDK_PATH)/zigbee/bdb/includes \
	-I$(SDK_PATH)/zigbee/common/includes \
	-I$(SDK_PATH)/zigbee/ota \
	-I$(SDK_PATH)/zigbee/zbapi \
	-I$(SDK_PATH)/zigbee/zcl \
	-I$(SDK_PATH)/zigbee/zdo

# ---- Application sources ----
APP_SOURCES := \
	$(SRC_DIR)/zigbee_app.c \
	$(SRC_DIR)/buttons.c \
	$(SRC_DIR)/gestures.c \
	$(SRC_DIR)/led_effects.c \
	$(SRC_DIR)/battery.c
MAIN_SRC := $(SDK_PATH)/apps/common/main.c

APP_OBJS := $(APP_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/app/%.o)
APP_OBJS += $(BUILD_DIR)/app/main.o
APP_DEPS := $(APP_OBJS:.o=.d)

ELF_FILE := $(BUILD_DIR)/$(FW_NAME).elf
LST_FILE := $(BUILD_DIR)/$(FW_NAME).lst
BIN_FILE := $(BIN_DIR)/$(FW_NAME).bin
OTA_FILE := $(BIN_DIR)/$(FW_NAME).ota

include tools.mk
include sdk.mk

.PHONY: all ota size clean
all: $(BIN_FILE) $(OTA_FILE) size

# ---- App compile rules ----
$(BUILD_DIR)/app/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "CC  (app) $<"
	@$(CC) $(GCC_FLAGS) $(DEVICE_DEFS) $(INCLUDE_PATHS) -c -o $@ $<

$(BUILD_DIR)/app/main.o: $(MAIN_SRC)
	@mkdir -p $(dir $@)
	@echo "CC  (app) $<"
	@$(CC) $(GCC_FLAGS) $(DEVICE_DEFS) $(INCLUDE_PATHS) -c -o $@ $<

# ---- Link ----
$(ELF_FILE): $(APP_OBJS) $(SDK_OBJS)
	@mkdir -p $(dir $@)
	@echo "LD  $@"
	@$(LD) $(LNK_FLAGS) -T$(LINKER_SCRIPT) $(LIBRARY_PATHS) -o $@ $(APP_OBJS) $(SDK_OBJS) $(LIBS)

$(BIN_FILE): $(ELF_FILE)
	@mkdir -p $(dir $@)
	@echo "BIN $@"
	@$(OBJCOPY) -O binary $< $@

# ---- OTA image (Telink magic+CRC + Zigbee OTA header) ----
$(OTA_FILE): $(BIN_FILE)
	@mkdir -p $(dir $@)
	@echo "OTA $@"
	@python3 tools/make_ota.py $(BIN_FILE) $(OTA_FILE) \
		--manufacturer-id $(MANUF_ID) --image-type $(IMAGE_TYPE) \
		--file-version $(FILE_VERSION) --header-string "IH-K663"

ota: $(OTA_FILE)

size: $(ELF_FILE)
	@echo "---- flash / RAM usage ----"
	@$(SIZE) $(ELF_FILE)

clean:
	@rm -rf $(BUILD_DIR)

-include $(APP_DEPS)
