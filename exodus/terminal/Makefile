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
$(BUILD)/$(TARGET).elf: main.c bsd_path.h tool_return.h Makefile | $(BUILD)
	$(CC) -std=gnu11 -O2 -Wall -Wextra $(CFLAGS) $(ARCH) -D__SWITCH__ -I$(LIBNX)/include -specs=$(LIBNX)/switch.specs $(LDFLAGS) main.c -L$(LIBNX)/lib -lnx -o $@
# Link-only pipe contract probe; not a device-executed test.
$(BUILD)/nxbsd-pipe-probe.elf: nxbsd_pipe.c nxbsd_pipe.h test_nxbsd_pipe.c Makefile | $(BUILD)
	$(CC) -std=gnu11 -O2 -Wall -Wextra $(ARCH) -D__SWITCH__ -I$(LIBNX)/include -specs=$(LIBNX)/switch.specs nxbsd_pipe.c test_nxbsd_pipe.c -L$(LIBNX)/lib -lnx -o $@
$(BUILD)/nxbsd-fd-probe.elf: nxbsd_pipe.c nxbsd_pipe.h nxbsd_fd.c nxbsd_fd.h test_nxbsd_fd.c Makefile | $(BUILD)
	$(CC) -std=gnu11 -O2 -Wall -Wextra $(ARCH) -D__SWITCH__ -I$(LIBNX)/include -specs=$(LIBNX)/switch.specs nxbsd_pipe.c nxbsd_fd.c test_nxbsd_fd.c -L$(LIBNX)/lib -lnx -o $@
$(BUILD)/nxbsd-context-probe.elf: nxbsd_pipe.c nxbsd_pipe.h nxbsd_fd.c nxbsd_fd.h nxbsd_context.c nxbsd_context.h test_nxbsd_context.c Makefile | $(BUILD)
	$(CC) -std=gnu11 -O2 -Wall -Wextra $(ARCH) -D__SWITCH__ -I$(LIBNX)/include -specs=$(LIBNX)/switch.specs nxbsd_pipe.c nxbsd_fd.c nxbsd_context.c test_nxbsd_context.c -L$(LIBNX)/lib -lnx -o $@
$(BUILD)/nxbsd-task-probe.elf: nxbsd_pipe.c nxbsd_pipe.h nxbsd_fd.c nxbsd_fd.h nxbsd_context.c nxbsd_context.h nxbsd_task.c nxbsd_task.h test_nxbsd_task.c Makefile | $(BUILD)
	$(CC) -std=gnu11 -O2 -Wall -Wextra $(ARCH) -D__SWITCH__ -I$(LIBNX)/include -specs=$(LIBNX)/switch.specs nxbsd_pipe.c nxbsd_fd.c nxbsd_context.c nxbsd_task.c test_nxbsd_task.c -L$(LIBNX)/lib -lnx -o $@
$(BUILD)/nxbsd-pipeline-probe.elf: nxbsd_pipe.c nxbsd_pipe.h nxbsd_fd.c nxbsd_fd.h nxbsd_context.c nxbsd_context.h nxbsd_task.c nxbsd_task.h nxbsd_pipeline.c nxbsd_pipeline.h test_nxbsd_pipeline.c Makefile | $(BUILD)
	$(CC) -std=gnu11 -O2 -Wall -Wextra $(ARCH) -D__SWITCH__ -I$(LIBNX)/include -specs=$(LIBNX)/switch.specs nxbsd_pipe.c nxbsd_fd.c nxbsd_context.c nxbsd_task.c nxbsd_pipeline.c test_nxbsd_pipeline.c -L$(LIBNX)/lib -lnx -o $@
$(BUILD)/nxbsd-commands-probe.elf: nxbsd_pipe.c nxbsd_pipe.h nxbsd_fd.c nxbsd_fd.h nxbsd_context.c nxbsd_context.h nxbsd_task.c nxbsd_task.h nxbsd_pipeline.c nxbsd_pipeline.h nxbsd_commands.c nxbsd_commands.h test_nxbsd_commands.c Makefile | $(BUILD)
	$(CC) -std=gnu11 -O2 -Wall -Wextra $(ARCH) -D__SWITCH__ -I$(LIBNX)/include -specs=$(LIBNX)/switch.specs nxbsd_pipe.c nxbsd_fd.c nxbsd_context.c nxbsd_task.c nxbsd_pipeline.c nxbsd_commands.c test_nxbsd_commands.c -L$(LIBNX)/lib -lnx -o $@
$(BUILD)/$(TARGET).nacp: Makefile | $(BUILD)
	nacptool --create "EXODUS Terminal" "EXODUSx1" "0.1.0" $@
$(TARGET).nro: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).nacp $(LIBNX)/default_icon.jpg
	elf2nro $< $@ --icon=$(LIBNX)/default_icon.jpg --nacp=$(BUILD)/$(TARGET).nacp
clean:
	rm -rf $(BUILD) $(TARGET).nro
.PHONY: all clean
