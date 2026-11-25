// SPDX-License-Identifier: GPL-2.0-only
#include "library_catalog.h"
#include "../ulaunch/catalog_refresh.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

#ifndef _WIN32
static void verify_default_catalog_refresh(const char *fixture)
{
    namespace fs = std::filesystem;
    using namespace exodus::home;
    const auto original = fs::current_path();
    auto root = fs::absolute(fixture);
    root += ".steam-refresh";
    assert(fs::create_directory(root));
    fs::current_path(root);

    const char *base = "sdmc:/ulaunch/exodus-catalog.json";
    const std::string old_steam = R"({"provider":"steam","provider_id":"10","name":"Old",)"
        R"("artwork":null,"installation_state":"unknown","last_played":null,"compatibility_state":"unknown","launch_mode":null})";
    const std::string homebrew = R"({"provider":"switch_homebrew","provider_id":"test.nro","name":"Homebrew",)"
        R"("artwork":null,"installation_state":"unknown","last_played":null,"compatibility_state":"unknown","launch_mode":null})";
    const std::string base_json = R"({"schema_version":1,"entries":[)" + old_steam + "," + homebrew + "]}";
    assert(fs::create_directories(fs::path(base).parent_path()));
    assert(fs::create_directories(fs::path(SteamSnapshotsPath).parent_path()));
    { std::ofstream catalog(base); assert(catalog << base_json); }
    { std::ofstream catalog("manual.json"); assert(catalog << base_json); }
    const std::string snapshot =
        R"({"schema_version":1,"entries":[{"provider":"steam","provider_id":"11","name":"New",)"
        R"("artwork":null,"installation_state":"unknown","last_played":null,"compatibility_state":"unknown","launch_mode":null}]})";
    assert(PublishSteamSnapshot(SteamSnapshotsPath, snapshot));

    auto *library = exodus_library_open(base);
    assert(library && exodus_library_count(library) == 2);
    assert(!std::strcmp(exodus_library_title(library, 0), "New"));
    assert(!std::strcmp(exodus_library_id(library, 1), "test.nro"));
    exodus_library_close(library);

    library = exodus_library_open("manual.json");
    assert(library && exodus_library_count(library) == 2);
    assert(!std::strcmp(exodus_library_title(library, 0), "Old"));
    exodus_library_close(library);

    { std::ofstream catalog(base); assert(catalog << "{}"); }
    library = exodus_library_open(base);
    assert(library && !std::strcmp(exodus_library_status(library), "Invalid catalog"));
    exodus_library_close(library);

    assert(fs::remove(base));
    library = exodus_library_open(base);
    assert(library && exodus_library_count(library) == 1);
    assert(!std::strcmp(exodus_library_title(library, 0), "New"));
    exodus_library_close(library);

    auto fallback = ApplySteamSnapshots(
        {CatalogStatus::Ready, {{Provider::Switch, "Native", "0000000000000001", "", 0}}},
        SteamSnapshotsPath);
    assert(fallback.steam_snapshot_applied && fallback.entries.size() == 2);
    assert(fallback.entries[0].title == "New" && fallback.entries[1].title == "Native");

    // The shared reader must not use fallback inventory for a non-regular base.
    assert(fs::create_directory(base));
    library = exodus_library_open(base);
    assert(library && !std::strcmp(exodus_library_status(library), "Catalog unavailable"));
    assert(exodus_library_count(library) == 0);
    exodus_library_close(library);
    auto live = +[]() -> Catalog {
        return {CatalogStatus::Ready, {{Provider::Switch, "Native", "0000000000000001", "", 0}}};
    };
    assert(ReadRefreshedCatalog(base, SteamSnapshotsPath, live).status == CatalogStatus::Unavailable);
    assert(fs::remove(base));
    const auto with_live = ReadRefreshedCatalog(base, SteamSnapshotsPath, live);
    assert(with_live.steam_snapshot_applied && with_live.entries.size() == 2);
    assert(with_live.entries[0].title == "New" && with_live.entries[1].title == "Native");
    Catalog full{CatalogStatus::Ready, {}};
    for (std::size_t i = 0; i < CatalogEntryLimit; ++i)
        full.entries.push_back({Provider::Switch, "Local", std::to_string(i), "", 0});
    const auto retained = ApplySteamSnapshots(std::move(full), SteamSnapshotsPath);
    assert(!retained.steam_snapshot_applied && retained.entries.size() == CatalogEntryLimit);

    fs::current_path(original);
    fs::remove_all(root);
}
#endif

int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *providers[] = {"steam", "switch", "switch_homebrew", "corkdeck", "other"};
    const char *labels[] = {"Steam", "Switch", "Homebrew", "CorkDeck", "Other"};
    {
        std::ofstream f(argv[1]);
        f << R"({"schema_version":1,"entries":[)";
        for (int i = 0; i < 5; ++i) {
            if (i) f << ',';
            f << R"({"provider":")" << providers[i] << R"(","provider_id":"1","name":"Game","artwork":null,"installation_state":"unknown","last_played":null,"compatibility_state":"unknown","launch_mode":null})";
        }
        f << "]}";
        assert(f.good());
    }
    auto *lib = exodus_library_open(argv[1]);
    assert(lib && exodus_library_count(lib) == 5);
    for (size_t i = 0; i < 5; ++i) {
        assert(!strcmp(exodus_library_provider(lib, i), labels[i]));
        assert(!strcmp(exodus_library_title(lib, i), "Game"));
    }
    assert(!strcmp(exodus_library_title(lib, 5), ""));
    assert(!strcmp(exodus_library_provider(lib, 5), "Unknown"));
    exodus_library_close(lib);
    { std::ofstream f(argv[1]); f << "{}"; }
    lib = exodus_library_open(argv[1]);
    assert(lib && !strcmp(exodus_library_status(lib), "Invalid catalog"));
    assert(exodus_library_count(lib) == 0);
    exodus_library_close(lib);
    std::remove(argv[1]);
#ifndef _WIN32
    verify_default_catalog_refresh(argv[1]);
#endif
    return 0;
}
