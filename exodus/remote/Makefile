ifeq ($(strip $(DEVKITPRO)),)
$(error Set DEVKITPRO to the devkitPro installation directory)
endif
include $(DEVKITPRO)/devkitA64/base_tools
TARGET := exodus-remote-smoke
BUILD := build-native
LIBNX := $(DEVKITPRO)/libnx
PORTLIBS := $(DEVKITPRO)/portlibs/switch
ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
TLS_LIBS ?= -lssl -lcrypto

all: $(TARGET).nro

$(BUILD):
	mkdir -p $@

$(BUILD)/$(TARGET).elf: native_smoke.c horizon_provider.h remote_gate.h remote_wire.h remote_tls.h remote_session.h Makefile | $(BUILD)
	$(CC) -std=gnu11 -O2 -Wall -Wextra -Werror $(ARCH) -D__SWITCH__ $(CFLAGS) \
		-I$(LIBNX)/include -I$(PORTLIBS)/include -specs=$(LIBNX)/switch.specs \
		$(LDFLAGS) native_smoke.c -L$(PORTLIBS)/lib -L$(LIBNX)/lib $(TLS_LIBS) -lnx -o $@

$(BUILD)/$(TARGET).nacp: Makefile | $(BUILD)
	nacptool --create "EXODUS Remote Smoke" "EXODUSx1" "0.3.5" $@

$(TARGET).nro: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).nacp $(LIBNX)/default_icon.jpg
	elf2nro $< $@ --icon=$(LIBNX)/default_icon.jpg --nacp=$(BUILD)/$(TARGET).nacp

clean:
	rm -rf $(BUILD) $(TARGET).nro

.PHONY: all clean
