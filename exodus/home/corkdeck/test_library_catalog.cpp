// SPDX-License-Identifier: GPL-2.0-only
#include "library_catalog.h"
#include "native_catalog_merge.hpp"
#include "../ulaunch/catalog_refresh.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

static void native_merge_fixture()
{
    using namespace exodus::home;
    Catalog external{CatalogStatus::Ready, {
        {Provider::Steam,"Steam game","42"},
        {Provider::Switch,"Saved title","010000000000ABCD","art",123},
        {Provider::Homebrew,"Saved tool","tools/tool.nro"},
        {Provider::Other,"Unknown A","same"},
        {Provider::Other,"Unknown B","same"}
    },true};
    const Catalog live{CatalogStatus::Ready,{
        {Provider::Switch,"Live duplicate","010000000000abcd"},
        {Provider::Switch,"Native title","0100000000001234"},
        {Provider::Homebrew,"Live duplicate","tools/tool.nro"},
        {Provider::Homebrew,"New tool","new.nro"}
    }};
    assert(MergeNativeCatalog(external,live)==NativeMergeResult::Ready);
    assert(external.entries.size()==7 && external.steam_snapshot_applied);
    assert(external.entries[0].provider==Provider::Steam);
    assert(external.entries[1].artwork=="art" && external.entries[1].last_played==123);
    assert(external.entries[5].id=="0100000000001234");
    assert(external.entries[6].id=="new.nro");
    assert(MergeNativeCatalog(external,live)==NativeMergeResult::Ready);
    assert(external.entries.size()==7);
    const Catalog unavailable{CatalogStatus::Unavailable,{}};
    assert(MergeNativeCatalog(external,unavailable)==NativeMergeResult::Unavailable);
    assert(external.entries.size()==7);
    Catalog invalid{CatalogStatus::Invalid,{}};
    assert(MergeNativeCatalog(invalid,live)==NativeMergeResult::Invalid);
    assert(invalid.status==CatalogStatus::Invalid && invalid.entries.empty());
    // Invalid absolute external path must not shadow a valid relative NRO.
    Catalog collision{CatalogStatus::Ready,{{Provider::Homebrew,"Invalid external","sdmc:/switch/tools/demo.nro"}}};
    const Catalog discovered{CatalogStatus::Ready,{{Provider::Homebrew,"Native tool","tools/demo.nro"}}};
    assert(MergeNativeCatalog(collision,discovered)==NativeMergeResult::Ready);
    assert(collision.entries.size()==2);
    assert(exodus_route_for("Homebrew",collision.entries[0].id.c_str())==EXODUS_ROUTE_INVALID);
    assert(exodus_route_for("Homebrew",collision.entries[1].id.c_str())==EXODUS_ROUTE_HOMEBREW);
    assert(MergeNativeCatalog(collision,discovered)==NativeMergeResult::Ready);
    assert(collision.entries.size()==2);
    Catalog absent{CatalogStatus::Unavailable,{}};
    assert(MergeNativeCatalog(absent,live)==NativeMergeResult::Ready);
    assert(absent.entries.size()==4);
    Catalog full{CatalogStatus::Ready,{}};
    for (size_t i=0;i<CatalogEntryLimit;++i)
        full.entries.push_back({Provider::Steam,"Game",std::to_string(i+1)});
    assert(MergeNativeCatalog(full,live)==NativeMergeResult::Limit);
    assert(full.entries.size()==CatalogEntryLimit && full.entries.back().provider==Provider::Steam);
    Catalog huge{CatalogStatus::Ready,{{Provider::Steam,std::string(CatalogByteLimit,'a'),"42"}}};
    assert(MergeNativeCatalog(huge,live)==NativeMergeResult::Limit);
    assert(huge.entries.size()==1 && huge.entries[0].title.size()==CatalogByteLimit);
}
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
    auto inventory = +[]() -> Catalog {
        return {CatalogStatus::Ready, {{Provider::Switch, "Live native", "0100000000001234", "", 0},
                                       {Provider::Homebrew, "Duplicate", "test.nro", "", 0}}};
    };
    auto merge = +[](Catalog &base, const Catalog &live) { (void)MergeNativeCatalog(base, live); };
    const auto combined = ReadRefreshedCatalog(base, SteamSnapshotsPath, inventory, merge);
    assert(combined.steam_snapshot_applied && combined.entries.size()==3);
    assert(combined.entries[0].title=="New" && combined.entries[1].title=="Homebrew");
    assert(combined.entries[2].id=="0100000000001234");

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
    assert(ReadRefreshedCatalog(base, SteamSnapshotsPath, inventory, merge).status==CatalogStatus::Invalid);
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
    assert(ReadRefreshedCatalog(base, SteamSnapshotsPath, inventory, merge).status==CatalogStatus::Unavailable);
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
    native_merge_fixture();
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
