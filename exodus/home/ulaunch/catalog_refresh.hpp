// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "catalog.hpp"

#include <cstdint>
#include <optional>

namespace exodus::home {

    inline constexpr char HomeCatalogPath[] = "sdmc:/ulaunch/exodus-catalog.json";
    inline constexpr char SteamSnapshotsPath[] = "sdmc:/switch/exodus-steam-sync/snapshots";

    bool IsSteamCatalog(const Catalog &catalog);
    std::optional<std::uint64_t> PublishSteamSnapshot(const char *directory, std::string_view steam,
                                                      const char *base_path = HomeCatalogPath);
    Catalog ApplySteamSnapshots(Catalog current, const char *snapshot_directory);
    // Without a merge callback, fallback inventory is used only if the base path is genuinely absent.
    // Opt-in native merger receives a valid external catalog before Steam snapshot selection.
    Catalog ReadRefreshedCatalog(const char *base_path, const char *snapshot_directory,
                                 Catalog (*missing_base)() = nullptr,
                                 void (*merge_live)(Catalog &, const Catalog &) = nullptr);

}
