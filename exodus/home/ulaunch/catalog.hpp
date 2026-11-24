// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <cstddef>
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
    };

    struct Catalog {
        CatalogStatus status;
        std::vector<Entry> entries;
    };

    Catalog ParseCatalog(std::string_view text, std::string *normalized_utf8 = nullptr);
    Catalog ReadCatalog(const char *path);
    std::string CatalogLabel();
    void ShowCatalog();

}
