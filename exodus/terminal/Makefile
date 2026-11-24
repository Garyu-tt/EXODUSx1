ifeq ($(strip $(DEVKITPRO)),)
$(error Set DEVKITPRO)
endif
include $(DEVKITPRO)/devkitA64/base_tools
TARGET := exodus-terminal
BUILD := build
LIBNX := $(DEVKITPRO)/libnx
ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
all: $(TARGET).nro
$(BUILD):
	mkdir -p $@
$(BUILD)/$(TARGET).elf: main.c bsd_path.h Makefile | $(BUILD)
	$(CC) -std=gnu11 -O2 -Wall -Wextra $(CFLAGS) $(ARCH) -D__SWITCH__ -I$(LIBNX)/include -specs=$(LIBNX)/switch.specs $(LDFLAGS) main.c -L$(LIBNX)/lib -lnx -o $@
# Link-only pipe contract probe; not a device-executed test.
$(BUILD)/nxbsd-pipe-probe.elf: nxbsd_pipe.c nxbsd_pipe.h test_nxbsd_pipe.c Makefile | $(BUILD)
	$(CC) -std=gnu11 -O2 -Wall -Wextra $(ARCH) -D__SWITCH__ -I$(LIBNX)/include -specs=$(LIBNX)/switch.specs nxbsd_pipe.c test_nxbsd_pipe.c -L$(LIBNX)/lib -lnx -o $@
$(BUILD)/nxbsd-fd-probe.elf: nxbsd_pipe.c nxbsd_pipe.h nxbsd_fd.c nxbsd_fd.h test_nxbsd_fd.c Makefile | $(BUILD)
	$(CC) -std=gnu11 -O2 -Wall -Wextra $(ARCH) -D__SWITCH__ -I$(LIBNX)/include -specs=$(LIBNX)/switch.specs nxbsd_pipe.c nxbsd_fd.c test_nxbsd_fd.c -L$(LIBNX)/lib -lnx -o $@
$(BUILD)/$(TARGET).nacp: Makefile | $(BUILD)
	nacptool --create "EXODUS Terminal" "EXODUSx1" "0.1.0" $@
$(TARGET).nro: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).nacp $(LIBNX)/default_icon.jpg
	elf2nro $< $@ --icon=$(LIBNX)/default_icon.jpg --nacp=$(BUILD)/$(TARGET).nacp
clean:
	rm -rf $(BUILD) $(TARGET).nro
.PHONY: all clean
