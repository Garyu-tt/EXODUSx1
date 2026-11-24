ifeq ($(strip $(DEVKITPRO)),)
$(error Set DEVKITPRO to the devkitPro installation directory)
endif

include $(DEVKITPRO)/devkitA64/base_tools

TARGET := exodus-ssh
BUILD := build
LIBNX := $(DEVKITPRO)/libnx
PORTLIBS := $(DEVKITPRO)/portlibs/switch
ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
# Override for an alternative libssh2 crypto backend.
SSH_LIBS ?= -lssh2 -lmbedtls -lmbedx509 -lmbedcrypto -lz

all: $(TARGET).nro

$(BUILD):
	mkdir -p $@

$(BUILD)/$(TARGET).elf: main.c Makefile | $(BUILD)
	$(CC) -std=gnu11 -O2 -Wall -Wextra $(CFLAGS) $(ARCH) -D__SWITCH__ \
		-I$(LIBNX)/include -I$(PORTLIBS)/include -specs=$(LIBNX)/switch.specs \
		$(LDFLAGS) main.c -L$(PORTLIBS)/lib -L$(LIBNX)/lib $(SSH_LIBS) -lnx -o $@

$(BUILD)/$(TARGET).nacp: Makefile | $(BUILD)
	nacptool --create "EXODUS SSH" "EXODUSx1" "0.1.0" $@

$(TARGET).nro: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).nacp $(LIBNX)/default_icon.jpg
	elf2nro $< $@ --icon=$(LIBNX)/default_icon.jpg --nacp=$(BUILD)/$(TARGET).nacp

clean:
	rm -rf $(BUILD) $(TARGET).nro

.PHONY: all clean
