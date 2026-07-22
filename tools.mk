# tools.mk — download the Telink Zigbee SDK + TC32 GCC toolchain.
# Both are fetched into telink_tools/ (gitignored). Run automatically by the
# Docker image and by `make tools`. Mirrors the approach used by
# romasku/tuya-zigbee-switch so the build is reproducible and headless.

TOOLS_DIR    := telink_tools
DOWNLOAD_DIR := $(TOOLS_DIR)/downloads

SDK_VERSION  := 3.7.2.0
SDK_URL      := https://github.com/telink-semi/telink_zigbee_sdk/archive/refs/tags/V$(SDK_VERSION).zip
SDK_ARCHIVE  := $(DOWNLOAD_DIR)/V$(SDK_VERSION).zip

TOOLCHAIN_URL      := https://shyboy.oss-cn-shenzhen.aliyuncs.com/readonly/tc32_gcc_v2.0.tar.bz2
TOOLCHAIN_CHECKSUM := 33b854be3e3db3dba4b4dacdda2cd4ea1c94dfd4d562864a095956de7991b430
TOOLCHAIN_ARCHIVE  := $(DOWNLOAD_DIR)/tc32_gcc_v2.0.tar.bz2

.PHONY: tools tools-sdk tools-toolchain tools-clean

tools: tools-sdk tools-toolchain
	@echo "Telink SDK + TC32 toolchain ready under $(TOOLS_DIR)/"

tools-sdk: $(TOOLS_DIR)/sdk
tools-toolchain: $(TOOLS_DIR)/toolchain/tc32/bin/tc32-elf-gcc

$(DOWNLOAD_DIR):
	@mkdir -p $(DOWNLOAD_DIR)

$(TOOLS_DIR)/sdk: | $(DOWNLOAD_DIR)
	@echo "Downloading Telink Zigbee SDK v$(SDK_VERSION)..."
	@[ -f "$(SDK_ARCHIVE)" ] || curl -L "$(SDK_URL)" -o "$(SDK_ARCHIVE)" --fail --show-error --connect-timeout 30
	@echo "Extracting SDK..."
	@rm -rf $(TOOLS_DIR)/sdk_temp && mkdir -p $(TOOLS_DIR)/sdk_temp
	@unzip -q "$(SDK_ARCHIVE)" -d $(TOOLS_DIR)/sdk_temp
	@SDK_DIR=$$(find $(TOOLS_DIR)/sdk_temp -name tl_zigbee_sdk -type d | head -1); \
		[ -n "$$SDK_DIR" ] && mv "$$SDK_DIR" $(TOOLS_DIR)/sdk || { echo "tl_zigbee_sdk not found in archive"; exit 1; }
	@rm -rf $(TOOLS_DIR)/sdk_temp
	@echo "SDK installed to $(TOOLS_DIR)/sdk"

$(TOOLS_DIR)/toolchain/tc32/bin/tc32-elf-gcc: | $(DOWNLOAD_DIR)
	@echo "Downloading TC32 GCC toolchain..."
	@[ -f "$(TOOLCHAIN_ARCHIVE)" ] || curl -L "$(TOOLCHAIN_URL)" -o "$(TOOLCHAIN_ARCHIVE)" --fail --show-error --connect-timeout 30
	@echo "$(TOOLCHAIN_CHECKSUM)  $(TOOLCHAIN_ARCHIVE)" | sha256sum -c - || { rm -f "$(TOOLCHAIN_ARCHIVE)"; exit 1; }
	@echo "Extracting toolchain..."
	@rm -rf $(TOOLS_DIR)/toolchain && mkdir -p $(TOOLS_DIR)/toolchain
	@tar -xjf "$(TOOLCHAIN_ARCHIVE)" -C $(TOOLS_DIR)/toolchain
	@find $(TOOLS_DIR)/toolchain -name 'tc32-elf-*' -type f -exec chmod +x {} \; 2>/dev/null || true
	@echo "Toolchain installed to $(TOOLS_DIR)/toolchain"

tools-clean:
	@rm -rf $(TOOLS_DIR)/sdk $(TOOLS_DIR)/toolchain
