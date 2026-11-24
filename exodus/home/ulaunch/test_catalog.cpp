// SPDX-License-Identifier: GPL-2.0-only
#include "catalog.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>

using namespace exodus::home;

namespace {

    std::string MakeEntry(const std::string &provider, const std::string &id, const std::string &name) {
        return "{\"provider\":\"" + provider + "\",\"provider_id\":\"" + id + "\",\"name\":\"" + name
            + "\",\"artwork\":null,\"installation_state\":\"unknown\",\"last_played\":null,"
              "\"compatibility_state\":\"unknown\",\"launch_mode\":null}";
    }

    std::string MakeCatalog(const std::string &entries) {
        return "{\"schema_version\":1,\"entries\":[" + entries + "]}";
    }

}

int main(int argc, char **argv) {
    const auto steam = MakeEntry("steam", "42", "Portal");
    const auto native = MakeEntry("switch", "42", "Caf\\u00e9\\n[deluxe]");
    const auto homebrew = MakeEntry("switch_homebrew", "tools/example.nro", "Tool \\\\ \\\"A\\\"");
    const auto combined = ParseCatalog(MakeCatalog(steam + "," + native + "," + homebrew));
    assert(combined.status == CatalogStatus::Ready);
    assert(combined.entries.size() == 3);
    assert(combined.entries[0].provider == Provider::Steam && combined.entries[0].title == "Portal");
    assert(combined.entries[1].provider == Provider::Switch && combined.entries[1].title == "Café [deluxe]");
    assert(combined.entries[2].provider == Provider::Homebrew && combined.entries[2].title == "Tool \\ \"A\"");

    std::string name;
    for (std::size_t index = 0; index < TitleCharacterLimit; ++index)
        name += "é";
    const auto long_name = ParseCatalog(MakeCatalog(MakeEntry("another_provider", "1", name + "🔧")));
    assert(long_name.status == CatalogStatus::Ready);
    assert(long_name.entries[0].provider == Provider::Other);
    assert(long_name.entries[0].title == name + "…");
    const auto empty = ParseCatalog(MakeCatalog(""));
    assert(empty.status == CatalogStatus::Ready && empty.entries.empty());

    const auto invalid_schema = ParseCatalog("{\"schema_version\":2,\"entries\":[]}");
    assert(invalid_schema.status == CatalogStatus::Invalid);
    assert(ParseCatalog("{\"schema_version\":true,\"entries\":[]}").status == CatalogStatus::Invalid);
    assert(ParseCatalog("{\"schema_version\":1,\"schema_version\":1,\"entries\":[]}").status == CatalogStatus::Invalid);
    assert(ParseCatalog(MakeCatalog(steam + "," + steam)).status == CatalogStatus::Invalid);
    assert(ParseCatalog(MakeCatalog("{\"name\":\"incomplete\"}")).status == CatalogStatus::Invalid);
    assert(ParseCatalog(MakeCatalog(MakeEntry("steam", "1", "\xff"))).status == CatalogStatus::Invalid);
    assert(ParseCatalog(MakeCatalog("[[[{}]]]")).status == CatalogStatus::Invalid);
    assert(ParseCatalog(MakeCatalog(steam.substr(0, steam.size() - 1))).status == CatalogStatus::Invalid);
    assert(ParseCatalog(MakeCatalog("") + '\0' + "trailing").status == CatalogStatus::Invalid);
    assert(ParseCatalog(std::string(CatalogByteLimit + 1, ' ')).status == CatalogStatus::TooLarge);
    std::string full_catalog;
    for (std::size_t number = 0; number < CatalogEntryLimit; ++number) {
        if (number != 0)
            full_catalog += ',';
        full_catalog += MakeEntry("steam", std::to_string(number), "Game");
    }
    const auto at_limit = ParseCatalog(MakeCatalog(full_catalog));
    assert(at_limit.status == CatalogStatus::Ready && at_limit.entries.size() == CatalogEntryLimit);
    assert(ParseCatalog(MakeCatalog(full_catalog + "," + steam)).status == CatalogStatus::TooLarge);
    assert(ReadCatalog("").status == CatalogStatus::Unavailable);
    if (argc == 2) {
        const auto from_file = ReadCatalog(argv[1]);
        assert(from_file.status == CatalogStatus::Ready);
        for (const auto provider : {Provider::Switch, Provider::Steam, Provider::Homebrew}) {
            assert(std::any_of(from_file.entries.begin(), from_file.entries.end(),
                [provider](const Entry &entry) { return entry.provider == provider; }));
        }
        std::cout << "Read " << from_file.entries.size() << " entries from the merged catalog\n";
    }
    std::cout << "EXODUS HOME catalog checks passed\n";
}
