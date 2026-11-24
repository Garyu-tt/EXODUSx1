TARGET := exodus-steam-file-check
BUILD := build
HOME_CATALOG := ../home/ulaunch
CORE := verify.cpp $(HOME_CATALOG)/catalog_steam_install.cpp
HEADERS := verify.hpp $(HOME_CATALOG)/catalog_steam_install.hpp
WARNINGS := -Wall -Wextra -Wpedantic -fno-exceptions -fno-rtti
HOST_CXX ?= c++
HOST_FLAGS ?= -Werror
HOST_LIBS ?= -lcrypto

.DEFAULT_GOAL := all

ifneq ($(MAKECMDGOALS),check)
ifeq ($(strip $(DEVKITPRO)),)
$(error Set DEVKITPRO to the devkitPro installation directory)
endif
include $(DEVKITPRO)/devkitA64/base_tools

LIBNX ?= $(DEVKITPRO)/libnx
ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
ICON := $(DEVKITPRO)/libnx/default_icon.jpg

all: $(TARGET).nro

$(BUILD)/$(TARGET).elf: main.cpp $(CORE) $(HEADERS) Makefile | $(BUILD)
	$(CXX) -std=gnu++17 -O2 $(WARNINGS) $(CXXFLAGS) $(ARCH) -ffunction-sections -fdata-sections \
		-D__SWITCH__ -isystem "$(LIBNX)/include" -specs="$(DEVKITPRO)/libnx/switch.specs" \
		$(LDFLAGS) main.cpp $(CORE) -L"$(LIBNX)/lib" -lnx -o $@

$(BUILD)/$(TARGET).nacp: Makefile | $(BUILD)
	nacptool --create "EXODUS Steam File Check" "EXODUSx1" "0.1.0" $@

$(TARGET).nro: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).nacp $(ICON)
	elf2nro $< $@ --icon="$(ICON)" --nacp=$(BUILD)/$(TARGET).nacp
endif

$(BUILD):
	mkdir -p $@

check: $(BUILD)/test-verify
	$<

$(BUILD)/test-verify: test_verify.cpp $(CORE) $(HEADERS) Makefile | $(BUILD)
	$(HOST_CXX) -std=gnu++17 -O2 $(WARNINGS) $(HOST_FLAGS) test_verify.cpp $(CORE) $(HOST_LIBS) -o $@

.PHONY: all check
