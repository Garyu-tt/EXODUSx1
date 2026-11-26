ifeq ($(strip $(DEVKITPRO)),)
$(error Set DEVKITPRO)
endif
include $(DEVKITPRO)/devkitA64/base_tools
TARGET := exodus-store-trace
LIBNX := $(DEVKITPRO)/libnx
ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
SOURCES := main.c ../../bench_store_trace.c ../../page_codec.c ../../page_store.c ../../page_dedup_store.c ../../page_read.c ../../bdi_slab.c
all: $(TARGET).nro
build:
	mkdir -p $@
build/$(TARGET).elf: $(SOURCES) Makefile | build
	$(CC) -std=gnu11 -O2 -Wall -Wextra $(CFLAGS) $(ARCH) -ffunction-sections -fdata-sections -D__SWITCH__ -I../.. -I$(LIBNX)/include -specs=$(LIBNX)/switch.specs $(SOURCES) -L$(LIBNX)/lib -lnx -o $@
build/$(TARGET).nacp: Makefile | build
	nacptool --create "EXODUS Store Trace" "EXODUSx1" "0.1.0" $@
$(TARGET).nro: build/$(TARGET).elf build/$(TARGET).nacp $(LIBNX)/default_icon.jpg
	elf2nro $< $@ --icon=$(LIBNX)/default_icon.jpg --nacp=build/$(TARGET).nacp
.PHONY: all
