// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <cstddef>
#include <string_view>

namespace exodus::home {

    inline constexpr std::string_view HomebrewSdMount = "sdmc:";
    inline constexpr std::string_view HomebrewAppletDirectory = "sdmc:/switch/";

    // path_capacity includes the NUL in uLoader's path buffer.
    inline bool IsCanonicalHomebrewAppletId(std::string_view id, std::size_t path_capacity) {
        if (path_capacity <= HomebrewAppletDirectory.size()
            || id.size() >= path_capacity - HomebrewAppletDirectory.size())
            return false;

        constexpr std::string_view invalid_characters = R"(\:*?"<>|)";
        for (const unsigned char character : id) {
            if (character < 0x20 || character == 0x7f
                || invalid_characters.find(character) != std::string_view::npos)
                return false;
        }

        for (;;) {
            const auto slash = id.find('/');
            const auto part = id.substr(0, slash);
            // A trailing dot or space can be normalized away by FAT tooling.
            if (part.empty() || part.back() == '.' || part.back() == ' ')
                return false;
            if (slash == std::string_view::npos) {
                if (part.size() <= 4)
                    return false;
                const auto suffix = part.substr(part.size() - 4);
                return suffix[0] == '.'
                    && (suffix[1] == 'n' || suffix[1] == 'N')
                    && (suffix[2] == 'r' || suffix[2] == 'R')
                    && (suffix[3] == 'o' || suffix[3] == 'O');
            }
            id.remove_prefix(slash + 1);
        }
    }

    inline bool HomebrewAppletPathMatches(std::string_view canonical_id, std::string_view path) {
        return path.substr(0, HomebrewAppletDirectory.size()) == HomebrewAppletDirectory
            && path.substr(HomebrewAppletDirectory.size()) == canonical_id;
    }

}
