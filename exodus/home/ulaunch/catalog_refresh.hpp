// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "catalog.hpp"

#include <cstdint>
#include <optional>

namespace exodus::home {

    inline constexpr char SteamSnapshotsPath[] = "sdmc:/switch/exodus-steam-sync/snapshots";

    bool IsSteamCatalog(const Catalog &catalog);
    std::optional<std::uint64_t> PublishSteamSnapshot(const char *directory, std::string_view steam);
    Catalog ReadRefreshedCatalog(const char *base_path, const char *snapshot_directory);

}
