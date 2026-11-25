ifeq ($(strip $(DEVKITPRO)),)
$(error Set DEVKITPRO to the devkitPro installation directory)
endif

include $(DEVKITPRO)/devkitA64/base_tools

TARGET := exodus-switch-inventory
BUILD := build
LIBNX := $(DEVKITPRO)/libnx
ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

all: $(TARGET).nro

$(BUILD):
	mkdir -p $@

$(BUILD)/$(TARGET).elf: main.c Makefile | $(BUILD)
	$(CC) -std=gnu11 -O2 -Wall -Wextra $(CFLAGS) $(ARCH) -ffunction-sections -fdata-sections \
		-D__SWITCH__ -I$(LIBNX)/include -specs=$(LIBNX)/switch.specs \
		$(LDFLAGS) $< -L$(LIBNX)/lib -lnx -o $@

$(BUILD)/$(TARGET).nacp: Makefile | $(BUILD)
	nacptool --create "EXODUS Switch Inventory" "EXODUSx1" "0.1.0" $@

$(TARGET).nro: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).nacp $(LIBNX)/default_icon.jpg
	elf2nro $< $@ --icon=$(LIBNX)/default_icon.jpg --nacp=$(BUILD)/$(TARGET).nacp

.PHONY: all
