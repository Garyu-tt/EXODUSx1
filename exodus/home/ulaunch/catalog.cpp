// SPDX-License-Identifier: GPL-2.0-only
#include "catalog.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <set>
#include <utility>
#include <json.hpp>

namespace exodus::home {

    namespace {

        using JSON = nlohmann::json;
        constexpr std::size_t CatalogDepth = 3;
        constexpr std::size_t EntryFields = 8;

        template <typename Character>
        CatalogStatus CheckStructure(std::basic_string_view<Character> text) {
            bool quoted = false;
            bool escaped = false;
            std::size_t depth = 0;
            std::size_t separators = 0;
            for (const Character ch : text) {
                if (ch == '\0')
                    return CatalogStatus::Invalid;
                if (quoted) {
                    if (escaped)
                        escaped = false;
                    else if (ch == '\\')
                        escaped = true;
                    else if (ch == '"')
                        quoted = false;
                } else if (ch == '"') {
                    quoted = true;
                } else if (ch == '[' || ch == '{') {
                    if (++depth > CatalogDepth)
                        return CatalogStatus::Invalid;
                } else if (ch == ']' || ch == '}') {
                    if (depth == 0)
                        return CatalogStatus::Invalid;
                    --depth;
                } else if (ch == ',' && ++separators > CatalogEntryLimit * EntryFields) {
                    return CatalogStatus::TooLarge;
                }
            }
            return !quoted && depth == 0 ? CatalogStatus::Ready : CatalogStatus::Invalid;
        }

        bool TextField(const JSON &object, const char *key, bool nullable = false) {
            const auto field = object.find(key);
            return field != object.end()
                && ((nullable && field->is_null())
                    || (field->is_string() && !field->get_ref<const std::string &>().empty()));
        }

        bool ValidEntry(const JSON &entry) {
            if (!entry.is_object() || entry.size() != EntryFields)
                return false;
            for (const auto *key : {"provider", "provider_id", "name",
                                   "installation_state", "compatibility_state"}) {
                if (!TextField(entry, key))
                    return false;
            }
            if (!TextField(entry, "artwork", true) || !TextField(entry, "launch_mode", true))
                return false;
            const auto last_played = entry.find("last_played");
            return last_played != entry.end()
                && (last_played->is_null() || last_played->is_number_unsigned()
                    || (last_played->is_number_integer() && last_played->get<std::int64_t>() >= 0));
        }

        Provider ProviderFromId(std::string_view id) {
            if (id == "steam")
                return Provider::Steam;
            if (id == "switch")
                return Provider::Switch;
            if (id == "switch_homebrew")
                return Provider::Homebrew;
            if (id == "corkdeck")
                return Provider::CorkDeck;
            return Provider::Other;
        }

        std::string DisplayTitle(std::string_view title) {
            // The JSON parser has checked UTF-8. Keep whole characters and one display line.
            std::string display;
            std::size_t offset = 0;
            for (std::size_t characters = 0; offset < title.size(); ++characters) {
                if (characters == TitleCharacterLimit) {
                    display += "…";
                    break;
                }
                const auto first = static_cast<unsigned char>(title[offset]);
                auto end = offset + 1;
                while (end < title.size() && (static_cast<unsigned char>(title[end]) & 0xc0) == 0x80)
                    ++end;
                const auto character = title.substr(offset, end - offset);
                if (first < 0x20 || first == 0x7f ||
                    (first == 0xc2 && end - offset == 2 && static_cast<unsigned char>(title[offset + 1]) <= 0x9f) ||
                    character == "\xe2\x80\xa8" || character == "\xe2\x80\xa9") {
                    display += ' ';
                } else {
                    display.append(character);
                }
                offset = end;
            }
            return display;
        }

    }

    template <typename Character>
    static Catalog ParseCatalogText(std::basic_string_view<Character> text) {
        const auto structure = CheckStructure(text);
        if (structure != CatalogStatus::Ready)
            return {structure, {}};

        std::array<std::set<std::string>, CatalogDepth> fields;
        bool duplicate_field = false;
        const JSON document = JSON::parse(text, [&](int depth, JSON::parse_event_t event, JSON &value) {
            if (event == JSON::parse_event_t::object_start)
                fields[depth].clear();
            if (event == JSON::parse_event_t::key) {
                const auto &key = value.get_ref<const std::string &>();
                if (!fields[depth - 1].insert(key).second)
                    duplicate_field = true;
            }
            return true;
        }, false);
        if (duplicate_field || !document.is_object() || document.size() != 2)
            return {CatalogStatus::Invalid, {}};
        const auto version = document.find("schema_version");
        const auto entries = document.find("entries");
        if (version == document.end() || !version->is_number_integer() || *version != 1
            || entries == document.end() || !entries->is_array())
            return {CatalogStatus::Invalid, {}};
        if (entries->size() > CatalogEntryLimit)
            return {CatalogStatus::TooLarge, {}};

        Catalog catalog{CatalogStatus::Ready, {}};
        catalog.entries.reserve(entries->size());
        std::set<std::pair<std::string, std::string>> identities;
        for (const auto &entry : *entries) {
            if (!ValidEntry(entry))
                return {CatalogStatus::Invalid, {}};
            const auto &provider = entry["provider"].get_ref<const std::string &>();
            const auto &id = entry["provider_id"].get_ref<const std::string &>();
            if (!identities.emplace(provider, id).second)
                return {CatalogStatus::Invalid, {}};
            const auto &name = entry["name"].get_ref<const std::string &>();
            if (id.find('\0') != std::string::npos)
                return {CatalogStatus::Invalid, {}};
            const auto &art = entry["artwork"];
            const auto &last = entry["last_played"];
            std::string artwork = art.is_string() ? art.get<std::string>() : "";
            if (artwork.find('\0') != std::string::npos)
                return {CatalogStatus::Invalid, {}};
            catalog.entries.push_back({ProviderFromId(provider), DisplayTitle(name), id,
                std::move(artwork), last.is_null() ? 0 : last.get<std::uint64_t>()});
        }
        return catalog;
    }

    Catalog ParseCatalog(std::string_view text) {
        if (text.size() > CatalogByteLimit)
            return {CatalogStatus::TooLarge, {}};
        if (text.substr(0, 2) != "\xff\xfe")
            return ParseCatalogText(text);
        if (text.size() % 2 != 0)
            return {CatalogStatus::Invalid, {}};

        // Windows PowerShell 5.1 writes UTF-16LE with a BOM for redirected output.
        std::u16string decoded;
        decoded.reserve((text.size() - 2) / 2);
        bool needs_low_surrogate = false;
        for (std::size_t offset = 2; offset < text.size(); offset += 2) {
            const auto unit = static_cast<char16_t>(static_cast<unsigned char>(text[offset])
                | (static_cast<unsigned char>(text[offset + 1]) << 8));
            const bool low_surrogate = unit >= 0xdc00 && unit <= 0xdfff;
            if (low_surrogate != needs_low_surrogate)
                return {CatalogStatus::Invalid, {}};
            needs_low_surrogate = unit >= 0xd800 && unit <= 0xdbff;
            decoded.push_back(unit);
        }
        if (needs_low_surrogate)
            return {CatalogStatus::Invalid, {}};
        return ParseCatalogText(std::u16string_view(decoded));
    }

    Catalog ReadCatalog(const char *path) {
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input)
            return {CatalogStatus::Unavailable, {}};
        const auto length = input.tellg();
        if (length < 0)
            return {CatalogStatus::Unavailable, {}};
        if (length > static_cast<std::streamoff>(CatalogByteLimit))
            return {CatalogStatus::TooLarge, {}};
        std::string text(static_cast<std::size_t>(length), '\0');
        input.seekg(0);
        if (!input.read(text.data(), text.size()) || input.peek() != std::istream::traits_type::eof() || input.bad())
            return {CatalogStatus::Unavailable, {}};
        return ParseCatalog(text);
    }

}
