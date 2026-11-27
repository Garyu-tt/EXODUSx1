DEVKITPRO ?= /opt/devkitpro
DEVKITA64 ?= $(DEVKITPRO)/devkitA64
CC := $(DEVKITA64)/bin/aarch64-none-elf-gcc
TOOLS := $(DEVKITPRO)/tools/bin
CFLAGS := -O2 -Wall -Wextra -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE -D__SWITCH__ -I$(DEVKITPRO)/libnx/include
all: exodus-diagnostic-shell.nro
exodus-diagnostic-shell.elf: main.c diag_vm.h workspace.h update_manifest.h
	$(CC) $(CFLAGS) -specs=$(DEVKITPRO)/libnx/switch.specs $< -L$(DEVKITPRO)/libnx/lib -lnx -o $@
exodus-diagnostic-shell.nacp:
	$(TOOLS)/nacptool --create "EXODUS Diagnostic Shell" "InfraCare-IT" "9.0" $@
exodus-diagnostic-shell.nro: exodus-diagnostic-shell.elf exodus-diagnostic-shell.nacp
	$(TOOLS)/elf2nro $< $@ --nacp=exodus-diagnostic-shell.nacp
