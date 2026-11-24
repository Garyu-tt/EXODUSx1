TARGET := exodus-steam-sync
BUILD := build
HOME_CATALOG := ../home/ulaunch
CORE := sync.cpp $(HOME_CATALOG)/catalog.cpp
CORE_HEADERS := sync.hpp $(HOME_CATALOG)/catalog.hpp
JSON_INCLUDE ?= $(DEVKITPRO)/portlibs/switch/include/nlohmann
WARNINGS := -Wall -Wextra -Wpedantic
CPP_FLAGS := -O2 $(WARNINGS) -fno-exceptions -fno-rtti -I"$(JSON_INCLUDE)"
HOST_CXX ?= c++
HOST_FLAGS ?= -Werror

.DEFAULT_GOAL := all

ifneq ($(filter-out check,$(MAKECMDGOALS)),)
NEED_DEVKITPRO := yes
else ifeq ($(MAKECMDGOALS),)
NEED_DEVKITPRO := yes
endif

ifeq ($(NEED_DEVKITPRO),yes)
ifeq ($(strip $(DEVKITPRO)),)
$(error Set DEVKITPRO to the devkitPro installation directory)
endif
include $(DEVKITPRO)/devkitA64/base_tools

LIBNX := $(DEVKITPRO)/libnx
ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
SWITCH_SOURCES := main.cpp https_switch.cpp $(CORE)

all: $(TARGET).nro

$(BUILD)/$(TARGET).elf: $(SWITCH_SOURCES) $(CORE_HEADERS) Makefile | $(BUILD)
	$(CXX) -std=gnu++20 $(CPP_FLAGS) $(CXXFLAGS) $(ARCH) -ffunction-sections -fdata-sections \
		-D__SWITCH__ -isystem $(LIBNX)/include -specs=$(LIBNX)/switch.specs \
		$(LDFLAGS) $(SWITCH_SOURCES) -L$(LIBNX)/lib -lnx -o $@

$(BUILD)/$(TARGET).nacp: Makefile | $(BUILD)
	nacptool --create "EXODUS Steam Sync" "EXODUSx1" "0.1.0" $@

$(TARGET).nro: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).nacp $(LIBNX)/default_icon.jpg
	elf2nro $< $@ --icon=$(LIBNX)/default_icon.jpg --nacp=$(BUILD)/$(TARGET).nacp
endif

$(BUILD):
	mkdir -p $@

check: $(BUILD)/test-sync
	$<

$(BUILD)/test-sync: test_sync.cpp $(CORE) $(CORE_HEADERS) Makefile | $(BUILD)
	$(HOST_CXX) -std=gnu++17 $(CPP_FLAGS) $(HOST_FLAGS) test_sync.cpp $(CORE) -o $@

.PHONY: all check
