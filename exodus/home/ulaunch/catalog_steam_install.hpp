// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace exodus::home {

    enum class SteamInstallEvidence : std::uint32_t {
        NoManifest = 1,
        InvalidManifest = 2,
        IncompleteManifest = 3,
        MissingGameDirectory = 4,
        ManifestAndDirectoryFound = 5,
        Unavailable = 6,
    };

    SteamInstallEvidence ProbeLocalSteamInstallation(std::string_view sd_mount, std::uint32_t app_id);

    // Returns the validated, single component under the Steam common directory.
    std::optional<std::string> FindLocalSteamGameDirectory(std::string_view sd_mount, std::uint32_t app_id);

}
