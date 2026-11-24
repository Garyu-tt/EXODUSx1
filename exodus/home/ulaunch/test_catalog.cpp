// SPDX-License-Identifier: GPL-2.0-only
#include "catalog.hpp"
#include "catalog_homebrew_provider.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <utility>

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

    void CheckSteamApplicationIds() {
        const auto text = MakeCatalog(MakeEntry("steam", "42", "Steam")
            + "," + MakeEntry("switch", "42", "Native")
            + "," + MakeEntry("switch_homebrew", "42", "Homebrew")
            + "," + MakeEntry("another_provider", "42", "Other"));
        const auto catalog = ParseCatalog(text);
        assert(catalog.status == CatalogStatus::Ready && catalog.entries.size() == 4);
        assert(SteamApplicationId(catalog.entries[0]) == 42);
        for (std::size_t index = 1; index < catalog.entries.size(); ++index)
            assert(!SteamApplicationId(catalog.entries[index]));

        assert(SteamApplicationId({Provider::Steam, "", "1"}) == 1);
        assert(SteamApplicationId({Provider::Steam, "", "4294967295"}) ==
               std::numeric_limits<std::uint32_t>::max());
        for (const auto *invalid : {"", "0", "01", "4294967296", "+42", "-42",
                                    " 42", "42 ", "0x2a", "42.0", "42e0", "４２", "42/nro"}) {
            assert(!SteamApplicationId({Provider::Steam, "", invalid}));
        }
        assert(!SteamApplicationId({Provider::Steam, "", std::string("42\0ignored", 10)}));
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

    void CheckCanonicalHomebrewAppletIds() {
        constexpr std::size_t path_capacity = 512;
        const std::string id = "Tools/Café demo.NRO";
        const auto catalog = ParseCatalog(MakeCatalog(MakeEntry("switch_homebrew", id, "Demo")));
        assert(catalog.status == CatalogStatus::Ready && catalog.entries.size() == 1);
        const auto &entry = catalog.entries[0];
        const std::string path = "sdmc:/switch/" + id;
        assert(entry.provider == Provider::Homebrew);
        assert(IsCanonicalHomebrewAppletId(entry.id, path_capacity));
        assert(HomebrewAppletPathMatches(entry.id, path));
        assert(!HomebrewAppletPathMatches(entry.id, "sdmc:/switch/other.nro"));
        assert(!HomebrewAppletPathMatches(entry.id, "sdmc:/switch/tools/Café demo.NRO"));
        assert(!HomebrewAppletPathMatches(entry.id, "sdmc:/elsewhere/Tools/Café demo.NRO"));
        assert(!HomebrewAppletPathMatches(entry.id, "sdmc:/"));
        assert(!IsCanonicalHomebrewAppletId(entry.id, 0));

        const auto max_id = std::string(244, 'a') + '/' + std::string(249, 'b') + ".nro";
        const auto max_path = "sdmc:/switch/" + max_id;
        assert(max_path.size() + 1 == path_capacity);
        assert(IsCanonicalHomebrewAppletId(max_id, path_capacity));
        assert(!IsCanonicalHomebrewAppletId(max_id, path_capacity - 1));

        for (const auto *invalid : {"", "/tool.nro", "sdmc:/switch/tool.nro", "C:/tool.nro",
                                   "dir\\tool.nro", "../tool.nro", "dir/../tool.nro", "./tool.nro",
                                   "dir/./tool.nro", "dir/.. /tool.nro", "dir./tool.nro", "dir /tool.nro",
                                   "dir//tool.nro", "tool.nro/", "tool.exe", ".nro", "tool.nro ",
                                   "quote\".nro", "pipe|.nro", "wild*.nro", "query?.nro", "angle<.nro",
                                   "angle>.nro", "newline\n.nro", "control\x7f.nro"}) {
            assert(!IsCanonicalHomebrewAppletId(invalid, path_capacity));
        }
        const auto nul_id = std::string("nul") + '\0' + ".nro";
        assert(!IsCanonicalHomebrewAppletId(nul_id, path_capacity));
    }

    void WriteFixture(const std::filesystem::path &path, std::string_view bytes) {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(bytes.data(), bytes.size());
        file.close();
        assert(file.good());
    }

    void PutLe32(std::string &bytes, std::size_t offset, std::uint32_t value) {
        for (std::size_t byte = 0; byte < 4; ++byte)
            bytes[offset + byte] = static_cast<char>((value >> (8 * byte)) & 0xff);
    }

    void CheckHomebrewProvider() {
        namespace fs = std::filesystem;
        constexpr std::uint32_t homebrew_type = 2; // uLaunch EntryType::Homebrew.
        constexpr std::size_t path_capacity = 512;
        const auto temporary = fs::canonical(fs::temp_directory_path());
        auto directory_name = (temporary / "exodus-homebrew-provider-XXXXXX").string();
        if (mkdtemp(directory_name.data()) == nullptr)
            std::abort();
        const fs::path fixture(directory_name);
        const auto grid = fixture / "grid";
        const auto file = fixture / "switch" / "tools" / "Demo with spaces.NRO";
        const std::string id = "tools/Demo with spaces.NRO";
        const std::string registered = "sdmc:/switch/" + id;
        const auto primary = grid / "0.m.json";
        const auto extra = grid / "1.m.json";
        const auto registration = "{\"type\":2,\"nro_path\":\"" + registered
            + "\",\"nro_argv\":\"ignored registration arguments\"}";
        auto resolve = [&] {
            return ResolveRegisteredHomebrewApplet(grid.string(), id, path_capacity, homebrew_type, fixture.string());
        };
        fs::create_directory(grid);
        fs::create_directories(file.parent_path());

        assert(!resolve());
        WriteFixture(primary, registration);
        assert(!resolve());

        // An inert disk fixture with the three page-aligned NRO segments and an asset trailer.
        std::string image(0x3000, '\0');
        PutLe32(image, 0x04, 0x100);
        PutLe32(image, 0x10, 0x304f524e);
        PutLe32(image, 0x18, 0x3000);
        PutLe32(image, 0x24, 0x1000);
        PutLe32(image, 0x28, 0x1000);
        PutLe32(image, 0x2c, 0x1000);
        PutLe32(image, 0x30, 0x2000);
        PutLe32(image, 0x34, 0x1000);
        image.append("ASET fixture");
        WriteFixture(file, image);
        assert(resolve() == registered);
        assert(!ResolveRegisteredHomebrewApplet(grid.string(), id, path_capacity, 1, fixture.string()));
        assert(!ResolveRegisteredHomebrewApplet(grid.string(), "../outside.nro", path_capacity,
            homebrew_type, fixture.string()));

        WriteFixture(extra, registration);
        assert(!resolve());
        WriteFixture(extra, "{\"type\":1,\"nro_path\":\"" + registered + "\"}");
        assert(resolve() == registered);
        WriteFixture(extra, std::string(16 * 1024 + 1, ' '));
        assert(!resolve());
        fs::remove(extra);

        WriteFixture(primary, "{\"type\":2,\"type\":2,\"nro_path\":\"" + registered + "\"}");
        assert(!resolve());
        WriteFixture(primary, registration + '\0' + "trailing");
        assert(!resolve());
        fs::create_directory(grid / "folder");
        WriteFixture(grid / "folder" / "0.m.json", registration);
        fs::remove(primary);
        assert(!resolve());
        WriteFixture(primary, registration);

        WriteFixture(file, "not an NRO");
        assert(!resolve());
        for (const auto &corruption : {std::pair{0x10, 0u}, std::pair{0x18, 0x4000u},
                std::pair{0x28, 0u}, std::pair{0x34, 0x2000u}, std::pair{0x38, 0xffffffffu}}) {
            auto malformed = image;
            PutLe32(malformed, corruption.first, corruption.second);
            WriteFixture(file, malformed);
            assert(!resolve());
        }
        WriteFixture(file, image);
        assert(resolve() == registered);

        const auto alias = fixture / "switch" / "alias";
        std::error_code symlink_error;
        fs::create_directory_symlink(file.parent_path(), alias, symlink_error);
        if (!symlink_error) {
            WriteFixture(extra, R"({"type":2,"nro_path":"sdmc:/switch/alias/Demo with spaces.NRO"})");
            assert(!ResolveRegisteredHomebrewApplet(grid.string(), "alias/Demo with spaces.NRO",
                path_capacity, homebrew_type, fixture.string()));
            fs::remove(extra);
        }
        fs::remove(file);
        assert(!resolve());
        symlink_error.clear();
        const auto linked_target = fixture / "real.nro";
        WriteFixture(linked_target, image);
        fs::create_symlink(linked_target, file, symlink_error);
        if (!symlink_error)
            assert(!resolve());

        assert(fs::canonical(fixture).parent_path() == temporary);
        fs::remove_all(fixture);
    }

}

int main(int argc, char **argv) {
    CheckUtf16Le();
    CheckSteamApplicationIds();
    CheckSwitchApplicationIds();
    CheckCanonicalHomebrewAppletIds();
    CheckHomebrewProvider();
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
