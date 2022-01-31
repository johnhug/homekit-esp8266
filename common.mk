FLASH_SIZE ?= 32
HOMEKIT_SPI_FLASH_BASE_ADDR=0x7A000
HOMEKIT_MAX_CLIENTS=8

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS

HKESP_COMMON_ROOT = $(PROGRAM_ROOT)../
HKESP_COMMON_INCLUDE = $(HKESP_COMMON_ROOT)include/

EXTRA_COMPONENTS += \
	extras/http-parser \
	extras/dhcpserver \
	$(HKESP_COMMON_INCLUDE)esp-cjson \
	$(HKESP_COMMON_INCLUDE)esp-wifi-config \
	$(HKESP_COMMON_INCLUDE)esp-wolfssl \
	$(HKESP_COMMON_INCLUDE)esp-homekit

ifeq ($(HOMEKIT_ESP8266_DEBUG),1)
	EXTRA_CFLAGS += -DHOMEKIT_ESP8266_DEBUG
endif

ifeq ($(HOMEKIT_DEVICE),test)
	HOMEKIT_DEVICE_DIR=$(HKESP_COMMON_ROOT)/testdevice
else
	HOMEKIT_DEVICE_DIR=$(PROGRAM_ROOT)/devices/$(HOMEKIT_DEVICE)
endif

include $(SDK_PATH)/common.mk

INC_DIRS += $(HOMEKIT_DEVICE_DIR) \
	$(HKESP_COMMON_INCLUDE)

LIBS += m

DEVICE_MARKER := $(BUILD_DIR)$(HOMEKIT_DEVICE).device
$(DEVICE_MARKER): $(BUILD_DIR)
	@rm -rf $(BUILD_DIR)program
	@rm -f $(BUILD_DIR)program.a
	@rm -f $(BUILD_DIR)$(PROGRAM).*
	@rm -f $(BUILD_DIR)*.device
	@touch $(DEVICE_MARKER)

.PHONY: verify_device
verify_device: $(DEVICE_MARKER)

QRCODE_FILE := $(FIRMWARE_DIR)$(HOMEKIT_DEVICE)-qrcode.png
$(QRCODE_FILE): $(FIRMWARE_DIR)
	@rm -f $(FIRMWARE_DIR)*-qrcode.png
	$(HKESP_COMMON_INCLUDE)/esp-homekit/tools/gen_qrcode 5 \
		`grep HOMEKIT_PASSWORD $(HOMEKIT_DEVICE_DIR)/homekit_device.h | cut -d \" -f2` \
		`grep HOMEKIT_SETUPID $(HOMEKIT_DEVICE_DIR)/homekit_device.h | cut -d \" -f2` \
		$(QRCODE_FILE)
.PHONY: qrcode open_qrcode
qrcode: $(QRCODE_FILE)
open_qrcode: $(QRCODE_FILE)
	@open $(QRCODE_FILE)

.PHONY: monitor
monitor:
	$(IDF_PATH)/tools/idf_monitor.py --port $(ESPPORT) --baud 115200 $(PROGRAM_OUT)

