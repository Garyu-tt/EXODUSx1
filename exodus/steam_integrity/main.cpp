// SPDX-License-Identifier: GPL-2.0-only
#include "verify.hpp"

#include <charconv>
#include <cinttypes>
#include <cstdio>
#include <switch.h>

namespace {

    using exodus::steam_integrity::Status;

    bool ReadInput(const char *title, const char *guide, char *output, std::size_t capacity,
        bool digits_only, u32 minimum_length) {
        SwkbdConfig keyboard;
        if (R_FAILED(swkbdCreate(&keyboard, 0)))
            return false;
        swkbdConfigMakePresetDefault(&keyboard);
        if (digits_only)
            swkbdConfigSetType(&keyboard, SwkbdType_NumPad);
        swkbdConfigSetHeaderText(&keyboard, title);
        swkbdConfigSetGuideText(&keyboard, guide);
        swkbdConfigSetStringLenMin(&keyboard, minimum_length);
        swkbdConfigSetStringLenMax(&keyboard, static_cast<u32>(capacity - 1));
        const auto result = swkbdShow(&keyboard, output, capacity);
        swkbdClose(&keyboard);
        return R_SUCCEEDED(result);
    }

    const char *ResultText(Status status) {
        switch (status) {
            case Status::Match: return "Every listed file matches the reference.";
            case Status::ReferenceUnavailable: return "Could not read the reference from SD.";
            case Status::ReferenceDigestMismatch: return "The SD reference differs from the SHA-256 you entered.";
            case Status::InvalidReference: return "The reference format or AppID is invalid.";
            case Status::InstallationUnavailable: return "The matching Steam installation is unavailable on SD.";
            case Status::MissingOrUnsafeFile: return "A listed path is missing or has an unsupported file type.";
            case Status::SizeMismatch: return "A listed file differs in size.";
            case Status::HashMismatch: return "A listed file differs from its SHA-256.";
            case Status::ReadFailure: return "A file became unreadable during the check.";
            case Status::Cancelled: return "File check cancelled.";
        }
        return "File check failed.";
    }

    struct ProgressDisplay {
        PadState &pad;
        u64 last_draw = 0;
    };

    bool UpdateProgress(std::size_t checked, std::size_t total, std::uint64_t bytes, void *context) {
        auto &display = *static_cast<ProgressDisplay *>(context);
        if (!appletMainLoop())
            return false;
        padUpdate(&display.pad);
        if (padGetButtonsDown(&display.pad) & HidNpadButton_B)
            return false;
        const auto now = armGetSystemTick();
        if (display.last_draw == 0 || now - display.last_draw >= armGetSystemTickFreq() / 4) {
            display.last_draw = now;
            consoleClear();
            std::puts("EXODUS Steam File Check\n");
            if (total == 0) {
                std::puts("Reading and checking the SD reference.");
            } else {
                std::printf("Matched files %zu / %zu\nRead %" PRIu64 " bytes\n", checked, total, bytes);
            }
            std::puts("\nB  Cancel");
            consoleUpdate(nullptr);
        }
        return true;
    }

    void CheckFiles(PadState &pad) {
        char app_text[11]{};
        if (!ReadInput("Steam AppID", "Enter the AppID of the installed game", app_text, sizeof(app_text), true, 1)) {
            std::puts("AppID entry ended.");
            return;
        }
        const std::string_view app_input(app_text);
        std::uint32_t app_id = 0;
        const auto end = app_input.data() + app_input.size();
        const auto parsed = std::from_chars(app_input.data(), end, app_id);
        if (parsed.ec != std::errc{} || parsed.ptr != end || app_id == 0 || app_input.front() == '0') {
            std::puts("Enter a positive Steam AppID without leading zeroes.");
            return;
        }

        char pin[65]{};
        if (!ReadInput("Reference SHA-256", "Use the 64 hex digits saved separately from the SD card",
                pin, sizeof(pin), false, 64)) {
            std::puts("Reference checksum entry ended.");
            return;
        }
        ProgressDisplay display{pad};
        const auto result = exodus::steam_integrity::VerifySteamInstallation("sdmc:", app_id, pin,
            UpdateProgress, &display);
        consoleClear();
        std::puts(ResultText(result.status));
        if (result.total_files != 0) {
            std::printf("Matched files %zu / %zu\nRead %" PRIu64 " bytes\n", result.checked_files,
                result.total_files, result.compared_bytes);
        }
        if (!result.path.empty())
            std::printf("File\n%s\n", result.path.c_str());
    }

}

int main() {
    consoleInit(nullptr);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    std::puts("EXODUS Steam File Check\n\nA  Check installed game files\n+  Exit");

    while (appletMainLoop()) {
        padUpdate(&pad);
        const auto pressed = padGetButtonsDown(&pad);
        if (pressed & HidNpadButton_Plus)
            break;
        if (pressed & HidNpadButton_A) {
            consoleClear();
            CheckFiles(pad);
            std::puts("\nA  Run another check\n+  Exit");
        }
        consoleUpdate(nullptr);
    }
    consoleExit(nullptr);
    return 0;
}
