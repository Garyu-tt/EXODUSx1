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

    std::string EncodeUtf16Le(std::u16string_view text) {
        std::string bytes = "\xff\xfe";
        for (const auto unit : text) {
            bytes.push_back(static_cast<char>(unit & 0xff));
            bytes.push_back(static_cast<char>(unit >> 8));
        }
        return bytes;
    }

    void CheckUtf16Le() {
        const std::u16string source = uR"({"schema_version":1,"entries":[{
            "provider":"steam","provider_id":"42","name":"Café 東京 🕹",
            "artwork":null,"installation_state":"unknown","last_played":null,
            "compatibility_state":"unknown","launch_mode":null}]})";
        const auto bytes = EncodeUtf16Le(source);
        const auto catalog = ParseCatalog(bytes);
        assert(catalog.status == CatalogStatus::Ready && catalog.entries.size() == 1);
        assert(catalog.entries[0].provider == Provider::Steam
            && catalog.entries[0].title == "Café 東京 🕹");

        assert(ParseCatalog(bytes.substr(0, bytes.size() - 1)).status == CatalogStatus::Invalid);
        const auto icon = source.find(u"🕹");
        assert(icon != std::u16string::npos);
        auto unpaired_high = source;
        unpaired_high.erase(icon + 1, 1);
        assert(ParseCatalog(EncodeUtf16Le(unpaired_high)).status == CatalogStatus::Invalid);
        auto unpaired_low = source;
        unpaired_low.erase(icon, 1);
        assert(ParseCatalog(EncodeUtf16Le(unpaired_low)).status == CatalogStatus::Invalid);
        assert(ParseCatalog(EncodeUtf16Le(std::u16string(1, 0xd800))).status == CatalogStatus::Invalid);

        auto raw_null = source;
        raw_null[icon] = u'\0';
        raw_null[icon + 1] = u' ';
        assert(ParseCatalog(EncodeUtf16Le(raw_null)).status == CatalogStatus::Invalid);
        assert(ParseCatalog(EncodeUtf16Le(uR"({"schema_version":2,"entries":[]})")).status == CatalogStatus::Invalid);
        assert(ParseCatalog(EncodeUtf16Le(std::u16string(CatalogByteLimit / 2, u' '))).status == CatalogStatus::TooLarge);
    }

    void CheckSwitchApplicationIds() {
        const std::string id = "0100aBcDeF123456";
        constexpr std::uint64_t expected = 0x0100abcdef123456ULL;
        const auto text = MakeCatalog(MakeEntry("switch", id, "Native")
            + "," + MakeEntry("steam", id, "Steam"));
        const auto catalog = ParseCatalog(text);
        assert(catalog.status == CatalogStatus::Ready && catalog.entries.size() == 2);
        assert(catalog.entries[0].id == id && catalog.entries[1].id == id);
        assert(SwitchApplicationId(catalog.entries[0]) == expected);
        assert(!SwitchApplicationId(catalog.entries[1]));

        const auto utf16 = ParseCatalog(EncodeUtf16Le(std::u16string(text.begin(), text.end())));
        assert(utf16.status == CatalogStatus::Ready && utf16.entries.size() == 2);
        assert(utf16.entries[0].id == id && SwitchApplicationId(utf16.entries[0]) == expected);
        assert(!SwitchApplicationId(utf16.entries[1]));

        assert(SwitchApplicationId({Provider::Switch, "", "FFFFFFFFFFFFFFFF"}) == 0xffffffffffffffffULL);
        assert(SwitchApplicationId({Provider::Switch, "", "0000000000000001"}) == 1);
        assert(!SwitchApplicationId({Provider::Homebrew, "", id}));
        for (const auto *invalid : {"", "0000000000000000", "0100abcdef12345", "0100abcdef1234567",
                                    "0100abcdef12345g", " 100abcdef123456", "+100abcdef123456",
                                    "-100abcdef123456", "0x00abcdef123456", "0000000000000１"}) {
            assert(!SwitchApplicationId({Provider::Switch, "", invalid}));
        }

        const auto nul_id = MakeCatalog(MakeEntry("switch", "0100abcdef12345\\u0000", "Game"));
        assert(ParseCatalog(nul_id).status == CatalogStatus::Invalid);
        const auto missing_id = MakeCatalog(R"({"provider":"switch","name":"Game","artwork":null,
            "installation_state":"unknown","last_played":null,"compatibility_state":"unknown","launch_mode":null})");
        assert(ParseCatalog(missing_id).status == CatalogStatus::Invalid);
    }

}

int main(int argc, char **argv) {
    CheckUtf16Le();
    CheckSwitchApplicationIds();
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
