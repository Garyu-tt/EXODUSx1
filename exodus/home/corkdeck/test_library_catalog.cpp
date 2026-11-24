// SPDX-License-Identifier: GPL-2.0-only
#include "library_catalog.h"
#include "native_catalog_merge.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <string>

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
    char native_detail[256];
    assert(!exodus_library_native_detail(lib,0,native_detail,sizeof(native_detail)));
    assert(!exodus_library_native_detail(lib,1,native_detail,sizeof(native_detail))); // invalid title ID
    assert(!exodus_library_native_detail(lib,5,native_detail,sizeof(native_detail)));
    assert(!strcmp(exodus_library_title(lib, 5), ""));
    assert(!strcmp(exodus_library_provider(lib, 5), "Unknown"));
    exodus_library_close(lib);
    // Folder discovery is read-only, and even an NRO suffix is not proof of launchability.
    const std::string root = std::string(argv[1]) + ".games";
    assert(std::filesystem::create_directory(root));
    const std::string game = root + "/Game.nsp";
    const std::string homebrew = root + "/Test.NRO";
    { std::ofstream f(game); f << "fixture"; }
    { std::ofstream f(homebrew); f << "fixture"; }
    { std::ofstream f(root + "/partial.xci.part"); f << "fixture"; }
    lib = exodus_library_open_with_games(argv[1], root.c_str());
    assert(lib && exodus_library_count(lib) == 7);
    assert(!strcmp(exodus_library_discovery_status(lib), "Games scan complete"));
    for (size_t i = 5; i < 7; ++i) {
        assert(!strcmp(exodus_library_provider(lib, i), "Other"));
        const std::string id = exodus_library_id(lib, i);
        assert(id == game || id == homebrew);
    }
    assert(strcmp(exodus_library_id(lib, 5), exodus_library_id(lib, 6)));
    exodus_library_close(lib);
    assert(std::filesystem::file_size(game) == 7);
    lib = exodus_library_open_with_games(argv[1], (root + "/missing").c_str());
    assert(lib && exodus_library_count(lib) == 5);
    assert(!strcmp(exodus_library_discovery_status(lib), "Games folder unavailable"));
    exodus_library_close(lib);
    assert(std::filesystem::remove_all(root) == 4);
    // Explicit sidecars attach informational graph records, not launch permission.
    assert(std::filesystem::create_directory(root));
    {
        unsigned char pfs[60]={};
        memcpy(pfs,"PFS0",4);
        pfs[4]=1; pfs[8]=16; pfs[24]=4;
        memcpy(pfs+40,"a.nca",6);
        std::ofstream f(game,std::ios::binary);
        f.write(reinterpret_cast<const char *>(pfs),sizeof(pfs));
        assert(f.good());
    }
    const auto update=root+"/Update.nsp";
    { std::ofstream f(update); f << "fixture"; }
    { std::ofstream f(game+".evcs"); f << "EVCS1 0000000000000001 0000000000000001 0 base\n"; }
    { std::ofstream f(update+".evcs"); f << "EVCS1 0000000000000001 0000000000000002 1 update\n"; }
    lib=exodus_library_open_with_games(argv[1],root.c_str());
    assert(lib && exodus_library_count(lib)==7);
    char detail[512];
    size_t game_index=5;
    if (std::string(exodus_library_id(lib,5))!=game) game_index=6;
    assert(!strcmp(exodus_library_provider(lib,game_index),"Other"));
    const size_t update_index = game_index == 5 ? 6 : 5;
    assert(exodus_library_is_discovered(lib,game_index));
    assert(!exodus_library_is_discovered(lib,0));
    assert(exodus_library_is_group_card(lib,game_index));
    assert(!exodus_library_is_group_card(lib,update_index));
    assert(exodus_library_group_size(lib,game_index)==2);
    assert(exodus_library_group_size(lib,update_index)==2);
    assert(exodus_library_group_size(lib,0)==1);
    assert(exodus_library_group_size(lib,7)==0);
    assert(exodus_library_archive_detail(lib,game_index,detail,sizeof(detail)));
    assert(strstr(detail,"Owner metadata (unverified): base listed, update listed, 0 DLC."));
    exodus_library_close(lib);
    { std::ofstream f(update+".evcs"); f << "EVCS1 0000000000000001 0000000000000001 0 base\n"; }
    lib=exodus_library_open_with_games(argv[1],root.c_str());
    assert(lib && exodus_library_count(lib)==7);
    assert(strstr(exodus_library_discovery_status(lib),"metadata rejected"));
    assert(exodus_library_is_group_card(lib,5) && exodus_library_is_group_card(lib,6));
    assert(exodus_library_group_size(lib,5)==1);
    exodus_library_close(lib);
    assert(std::filesystem::remove_all(root)==5);
    // Split archives stay inventory-only. A declared group key survives renaming.
    assert(std::filesystem::create_directory(root));
    const auto split=root+"/Split.nsp";
    const auto patch=root+"/Patch.nsp";
    const auto moved=root+"/Renamed.nsp";
    assert(std::filesystem::create_directory(split));
    { std::ofstream f(split+"/00"); f<<"1234"; }
    { std::ofstream f(split+"/01"); f<<"12"; }
    { std::ofstream f(patch); f<<"dummy"; }
    { std::ofstream f(split+".evcs"); f<<"EVCS1 0000000000000010 0000000000000010 0 base\n"; }
    { std::ofstream f(patch+".evcs"); f<<"EVCS1 0000000000000010 0000000000000011 1 update\n"; }
    lib=exodus_library_open_with_games(argv[1],root.c_str());
    assert(lib && exodus_library_count(lib)==7);
    size_t split_index=5;
    if (std::string(exodus_library_id(lib,5))!=split) split_index=6;
    size_t patch_index=split_index==5?6:5;
    assert(exodus_library_group_representative(lib,split_index)==split_index);
    assert(exodus_library_group_representative(lib,patch_index)==split_index);
    char key[64], key2[64];
    assert(exodus_library_declared_group_key(lib,split_index,key,sizeof(key)));
    assert(!strcmp(key,"evcs-declared:0000000000000010"));
    assert(exodus_library_declared_group_key(lib,patch_index,key2,sizeof(key2)) && !strcmp(key,key2));
    assert(!exodus_library_declared_group_key(lib,0,key2,sizeof(key2)));
    assert(!exodus_library_declared_group_key(lib,split_index,key2,5) && !key2[0]);
    assert(exodus_library_archive_detail(lib,split_index,detail,sizeof(detail)) && strstr(detail,"2 contiguous parts, 6 bytes"));
    assert(strstr(detail,"Not launchable"));
    assert(!strcmp(exodus_library_provider(lib,split_index),"Other"));
    assert(std::filesystem::remove(split+"/01"));
    assert(exodus_library_archive_detail(lib,split_index,detail,sizeof(detail)) && strstr(detail,"invalid"));
    exodus_library_close(lib);
    { std::ofstream f(split+"/01"); f<<"12"; }
    std::filesystem::rename(split,moved);
    std::filesystem::rename(split+".evcs",moved+".evcs");
    lib=exodus_library_open_with_games(argv[1],root.c_str());
    assert(lib && exodus_library_count(lib)==7);
    for (size_t n=5;n<7;++n)
        assert(exodus_library_declared_group_key(lib,n,key2,sizeof(key2)) && !strcmp(key,key2));
    assert(exodus_library_group_representative(lib,99)==SIZE_MAX);
    exodus_library_close(lib);
    assert(std::filesystem::file_size(moved+"/00")==4);
    assert(std::filesystem::remove_all(root)==7);
    // Host mirror of the console namespace; no execution of fixture bytes.
    const auto base_path = std::filesystem::absolute(argv[1]);
    const auto original_cwd = std::filesystem::current_path();
    assert(std::filesystem::create_directory(root));
    std::filesystem::current_path(root);
    assert(std::filesystem::create_directories("sdmc:/Games"));
    {
        unsigned char nro[0x3000] = {};
        memcpy(nro + 0x10, "NRO0", 4);
        auto put32 = [&](size_t off, uint32_t n) {
            for (unsigned j = 0; j < 4; ++j) nro[off+j] = static_cast<unsigned char>(n >> (8*j));
        };
        put32(0x18, sizeof(nro));
        for (unsigned j = 0; j < 3; ++j) {
            put32(0x20 + j*8, j*0x1000);
            put32(0x24 + j*8, 0x1000);
        }
        std::ofstream f("sdmc:/Games/valid.NRO", std::ios::binary);
        f.write(reinterpret_cast<const char *>(nro), sizeof(nro));
        assert(f.good());
    }
    { std::ofstream f("sdmc:/Games/broken.nro"); f << "bad"; }
    { std::ofstream f("sdmc:/Games/backup.xci"); f << "fixture"; }
    lib = exodus_library_open_with_games(base_path.c_str(), "sdmc:/Games");
    assert(lib && exodus_library_count(lib) == 8);
    unsigned native_candidates = 0, inventory_only = 0;
    for (size_t i = 5; i < 8; ++i) {
        const std::string id = exodus_library_id(lib, i);
        if (id == "sdmc:/Games/valid.NRO") {
            assert(!strcmp(exodus_library_provider(lib, i), "Homebrew"));
            ++native_candidates;
        } else {
            assert(!strcmp(exodus_library_provider(lib, i), "Other"));
            ++inventory_only;
        }
    }
    assert(native_candidates == 1 && inventory_only == 2);
    exodus_library_close(lib);
    std::filesystem::current_path(original_cwd);
    assert(std::filesystem::remove_all(root) == 6);
    { std::ofstream f(argv[1]); f << "{}"; }
    lib = exodus_library_open(argv[1]);
    assert(lib && !strcmp(exodus_library_status(lib), "Invalid catalog"));
    assert(exodus_library_count(lib) == 0);
    exodus_library_close(lib);
    {
        std::ofstream f(argv[1]);
        f << R"({"schema_version":1,"entries":[{"provider":"switch","provider_id":"0100000000001234","name":"Native","artwork":null,"installation_state":"unknown","last_played":null,"compatibility_state":"unknown","launch_mode":null}]})";
    }
    lib=exodus_library_open(argv[1]);
    assert(lib && exodus_library_count(lib)==1);
    assert(exodus_library_native_detail(lib,0,native_detail,sizeof(native_detail)));
    assert(strstr(native_detail,"unavailable on host"));
    exodus_library_close(lib);
    std::remove(argv[1]);
    return 0;
}
