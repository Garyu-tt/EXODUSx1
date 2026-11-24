// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace exodus::home {

    inline constexpr std::size_t CatalogByteLimit = 2 * 1024 * 1024;
    inline constexpr std::size_t CatalogEntryLimit = 4096;
    inline constexpr std::size_t TitleCharacterLimit = 24;

    enum class CatalogStatus {
        Ready,
        Unavailable,
        Invalid,
        TooLarge,
    };

    enum class Provider {
        Steam,
        Switch,
        Homebrew,
        Other,
    };

    struct Entry {
        Provider provider;
        std::string title;
        std::string id;
    };

    struct Catalog {
        CatalogStatus status;
        std::vector<Entry> entries;
        bool steam_snapshot_applied = false;
    };

    Catalog ParseCatalog(std::string_view text, std::string *normalized_utf8 = nullptr);
    Catalog ReadCatalog(const char *path);
    std::optional<std::uint32_t> SteamApplicationId(const Entry &entry);
    std::optional<std::uint64_t> SwitchApplicationId(const Entry &entry);
    std::string CatalogLabel();
    void ShowCatalog();

}
