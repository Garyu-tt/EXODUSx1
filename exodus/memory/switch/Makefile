ifeq ($(strip $(DEVKITPRO)),)
$(error Set DEVKITPRO to the devkitPro installation directory)
endif

include $(DEVKITPRO)/devkitA64/base_tools

TARGET := exodus-page-probe
BUILD := build
LIBNX := $(DEVKITPRO)/libnx
ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
SOURCES := main.c ../bench_ex005.c ../bdi_slab.c ../page_codec.c ../page_probe_stats.c ../page_store.c ../page_compact_index.c
HEADERS := ../bdi_slab.h ../page_codec.h ../page_probe_stats.h ../page_store.h ../page_compact_index.h

all: $(TARGET).nro

$(BUILD):
	mkdir -p $@

$(BUILD)/$(TARGET).elf: $(SOURCES) $(HEADERS) Makefile | $(BUILD)
	$(CC) -std=gnu11 -O2 -Wall -Wextra $(CFLAGS) $(ARCH) -ffunction-sections -fdata-sections \
		-D__SWITCH__ -I.. -I$(LIBNX)/include -specs=$(LIBNX)/switch.specs \
		$(LDFLAGS) $(SOURCES) -L$(LIBNX)/lib -lnx -o $@

$(BUILD)/$(TARGET).nacp: Makefile | $(BUILD)
	nacptool --create "EXODUS Page Probe" "EXODUSx1" "0.1.0" $@

$(TARGET).nro: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).nacp $(LIBNX)/default_icon.jpg
	elf2nro $< $@ --icon=$(LIBNX)/default_icon.jpg --nacp=$(BUILD)/$(TARGET).nacp

.PHONY: all
