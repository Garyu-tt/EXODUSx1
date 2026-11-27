DEVKITPRO ?= /opt/devkitpro
DEVKITA64 ?= $(DEVKITPRO)/devkitA64
CC := $(DEVKITA64)/bin/aarch64-none-elf-gcc
TOOLS := $(DEVKITPRO)/tools/bin
ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS := -O2 -Wall -Wextra $(ARCH) -D__SWITCH__ -I$(DEVKITPRO)/libnx/include
all: tumor-hos-probe.nro
tumor-hos-probe.elf: main.c
	$(CC) $(CFLAGS) -specs=$(DEVKITPRO)/libnx/switch.specs $< -L$(DEVKITPRO)/libnx/lib -lnx -o $@
tumor-hos-probe.nacp:
	$(TOOLS)/nacptool --create "Tumor HOS Probe" "InfraCare-IT" "3.0" $@
tumor-hos-probe.nro: tumor-hos-probe.elf tumor-hos-probe.nacp
	$(TOOLS)/elf2nro $< $@ --nacp=tumor-hos-probe.nacp
