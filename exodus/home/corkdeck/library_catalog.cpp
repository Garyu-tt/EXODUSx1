// SPDX-License-Identifier: GPL-2.0-only
#include "library_catalog.h"
#include "../ulaunch/catalog_refresh.hpp"
#include <new>
#include <utility>
#ifndef _WIN32
#include "evcs_discovery.h"
#include "evcs_pfs0.h"
#include "evcs_manifest.h"
#include <map>
#endif
#ifdef __SWITCH__
#include "live_inventory.hpp"
#include "native_catalog_merge.hpp"
#endif
using namespace exodus::home;
struct exodus_library {
    Catalog catalog;
    const char *discovery="Not requested";
    size_t first_game=SIZE_MAX;
    std::vector<size_t> representatives{};
    std::vector<size_t> group_sizes{};
    std::vector<uint64_t> declared_ids{};
};
#ifndef _WIN32
struct InventoryPending {
    const Catalog &base;
    std::vector<Entry> entries;
    size_t bytes=0;
};
static int inventory_emit(const char *path, enum evcs_kind kind, uint64_t bytes, void *user)
{
    (void)kind; (void)bytes;
    auto &pending=*static_cast<InventoryPending *>(user);
    try {
        for (const auto &entry:pending.base.entries)
            if (entry.provider==Provider::Other && entry.id==path) return 1;
        const char *name=strrchr(path,'/');
        name=name?name+1:path;
        size_t cost=strlen(path)+strlen(name);
        if (pending.base.entries.size()+pending.entries.size()>=CatalogEntryLimit ||
            cost>CatalogByteLimit-pending.bytes) return 0;
        // A filesystem path is never a registered native title or executable route.
        pending.entries.push_back({Provider::Other,name,path});
        pending.bytes+=cost;
        return 1;
    } catch (...) { return 0; }
}
static void group_declared_inventory(exodus_library &lib)
{
    std::vector<evcs_content> records;
    std::vector<size_t> indices;
    const auto &entries=lib.catalog.entries;
    for (size_t i=lib.first_game;i<entries.size();++i) {
        enum evcs_kind kind;
        if (!evcs_kind_of(entries[i].id.c_str(),&kind) ||
            (kind!=EVCS_NSP && kind!=EVCS_XCI)) continue;
        evcs_content item{};
        auto rc=evcs_manifest_read(entries[i].id.c_str(),&item);
        if (rc==EVCS_MANIFEST_MISSING) continue;
        if (rc!=EVCS_MANIFEST_OK) {
            lib.discovery="Games scan complete; metadata rejected"; return;
        }
        records.push_back(item);
        indices.push_back(i);
    }
    if (evcs_graph_validate(records.data(),records.size())!=EVCS_GRAPH_OK) {
        lib.discovery="Games scan complete; metadata rejected"; return;
    }
    std::map<uint64_t,size_t> bases;
    for (size_t j=0;j<records.size();++j) {
        const auto &item=records[j];
        if (item.role!=EVCS_BASE) continue;
        auto found=bases.find(item.application_id);
        if (found==bases.end() || records[found->second].version<item.version)
            bases[item.application_id]=j;
    }
    if (bases.empty()) return;
    std::vector<size_t> reps(entries.size()),sizes(entries.size(),0);
    std::vector<uint64_t> ids(entries.size(),0);
    for (size_t i=0;i<entries.size();++i) reps[i]=i;
    for (size_t j=0;j<records.size();++j) {
        const auto found=bases.find(records[j].application_id);
        if (found!=bases.end()) {
            reps[indices[j]]=indices[found->second];
            ids[indices[j]]=records[j].application_id;
        }
    }
    for (size_t r:reps) ++sizes[r];
    lib.representatives=std::move(reps);
    lib.group_sizes=std::move(sizes);
    lib.declared_ids=std::move(ids);
}
static void merge_games_inventory(exodus_library &lib,const char *base_path,const char *root)
{
    if (!root) return;
    if (lib.catalog.status!=CatalogStatus::Ready) {
        struct stat st{};
        if (lib.catalog.status!=CatalogStatus::Unavailable ||
            lstat(base_path,&st)==0 || errno!=ENOENT) {
            lib.discovery="Base catalog unavailable or invalid"; return;
        }
    }
    InventoryPending pending{lib.catalog,{},0};
    for (const auto &entry:lib.catalog.entries) {
        for (size_t n:{entry.id.size(),entry.title.size(),entry.artwork.size()}) {
            if (n>CatalogByteLimit-pending.bytes) {
                lib.discovery="Catalog byte limit reached"; return;
            }
            pending.bytes+=n;
        }
    }
    auto rc=evcs_discover(root,inventory_emit,&pending);
    if (rc!=EVCS_OK) {
        lib.discovery=rc==EVCS_IO?"Games folder unavailable":"Games scan incomplete";
        return;
    }
    auto &entries=lib.catalog.entries;
    entries.reserve(entries.size()+pending.entries.size());
    lib.first_game=entries.size();
    for (auto &entry:pending.entries) entries.push_back(std::move(entry));
    if (!entries.empty()) lib.catalog.status=CatalogStatus::Ready;
    lib.discovery="Games scan complete";
    group_declared_inventory(lib);
}
#endif
extern "C" {
struct exodus_library *exodus_library_open(const char *path)
{
#ifdef __SWITCH__
    if (path && std::string_view(path)==HomeCatalogPath)
        return exodus_library_open_with_games(path,"sdmc:/Games");
#endif
    return exodus_library_open_with_games(path,nullptr);
}
struct exodus_library *exodus_library_open_with_games(const char *path,const char *root)
{
    if (!path) return nullptr;
    try {
        auto catalog = std::string_view(path) == HomeCatalogPath
#ifdef __SWITCH__
            ? ReadRefreshedCatalog(path, SteamSnapshotsPath, exodus_live_inventory,
                +[](Catalog &current, const Catalog &live) { (void)MergeNativeCatalog(current, live); })
#else
            ? ReadRefreshedCatalog(path, SteamSnapshotsPath)
#endif
            : ReadCatalog(path);
        exodus_library result{std::move(catalog)};
#ifndef _WIN32
        merge_games_inventory(result,path,root);
#else
        if (root) result.discovery="Games inventory unavailable on this host";
#endif
        return new exodus_library{std::move(result)};
    }
    catch (...) { return nullptr; }
}
const char *exodus_library_discovery_status(const struct exodus_library *library)
{
    return library?library->discovery:"Catalog unavailable";
}
int exodus_library_is_discovered(const struct exodus_library *library,size_t index)
{
    return library && index>=library->first_game && index<library->catalog.entries.size();
}
size_t exodus_library_group_representative(const struct exodus_library *library,size_t index)
{
    if (!library || index>=library->catalog.entries.size()) return SIZE_MAX;
    return library->representatives.empty()?index:library->representatives[index];
}
size_t exodus_library_group_size(const struct exodus_library *library,size_t index)
{
    const size_t rep=exodus_library_group_representative(library,index);
    if (rep==SIZE_MAX) return 0;
    return library->group_sizes.empty()?1:library->group_sizes[rep];
}
uint64_t exodus_library_declared_group_id(const struct exodus_library *library,size_t index)
{
    if (!library || index>=library->declared_ids.size()) return 0;
    return library->declared_ids[index];
}
int exodus_library_archive_detail(const struct exodus_library *library,size_t index,char *out,size_t cap)
{
    if (!out || !cap) return 0;
    out[0]=0;
    if (!exodus_library_is_discovered(library,index)) return 0;
#ifndef _WIN32
    const auto &entry=library->catalog.entries[index];
    enum evcs_kind kind;
    if (!evcs_kind_of(entry.id.c_str(),&kind)) return 0;
    const char *format=kind==EVCS_NSP?"NSP":kind==EVCS_XCI?"XCI":kind==EVCS_NRO?"NRO":"NCA";
    struct stat st{};
    if (lstat(entry.id.c_str(),&st)) {
        snprintf(out,cap,"%s unavailable. Not launchable.",format); return 1;
    }
    if (S_ISDIR(st.st_mode) && (kind==EVCS_NSP || kind==EVCS_XCI)) {
        unsigned visited=0;
        struct evcs_split_info info{};
        if (evcs_split_probe(entry.id.c_str(),&visited,EVCS_MAX_VISITED,&info)==EVCS_SPLIT_OK)
            snprintf(out,cap,"Split %s: %u contiguous parts, %llu bytes.\nCompleteness unverified. Not launchable.",
                format,info.parts,(unsigned long long)info.bytes);
        else snprintf(out,cap,"Split %s unavailable or invalid. Not launchable.",format);
        return 1;
    }
    if (!S_ISREG(st.st_mode) || st.st_size<=0) {
        snprintf(out,cap,"%s unavailable or invalid. Not launchable.",format); return 1;
    }
    if (kind!=EVCS_NSP) {
        snprintf(out,cap,"%s discovered: %llu bytes.\nNo verified launch backend.",format,(unsigned long long)st.st_size);
        return 1;
    }
    FILE *stream=fopen(entry.id.c_str(),"rb");
    struct evcs_pfs0_info info{};
    auto rc=stream?evcs_pfs0_probe(stream,(uint64_t)st.st_size,&info):EVCS_PFS0_IO;
    if (stream && fclose(stream)) rc=EVCS_PFS0_IO;
    if (rc==EVCS_PFS0_OK)
        snprintf(out,cap,"NSP directory: %u files, %u NCA, %u metadata candidates.\nNo verified launch backend.",
            (unsigned)info.entries,(unsigned)info.nca,(unsigned)info.meta_nca);
    else snprintf(out,cap,"NSP directory %s. Not launchable.",
        rc==EVCS_PFS0_LIMIT?"exceeds scan limits":rc==EVCS_PFS0_IO?"unreadable":"invalid");
    return 1;
#else
    return 0;
#endif
}
void exodus_library_close(struct exodus_library *library) { delete library; }
const char *exodus_library_status(const struct exodus_library *library)
{
    if (!library) return "Cannot allocate catalog";
    switch (library->catalog.status) {
        case CatalogStatus::Ready: return "Ready";
        case CatalogStatus::Unavailable: return "Catalog unavailable";
        case CatalogStatus::Invalid: return "Invalid catalog";
        case CatalogStatus::TooLarge: return "Catalog too large";
    }
    return "Unknown status";
}
size_t exodus_library_count(const struct exodus_library *library)
{
    return library ? library->catalog.entries.size() : 0;
}
const char *exodus_library_id(const struct exodus_library *library, size_t index)
{
    return index < exodus_library_count(library) ? library->catalog.entries[index].id.c_str() : "";
}
const char *exodus_library_artwork(const struct exodus_library *library, size_t index)
{
    return index < exodus_library_count(library) ? library->catalog.entries[index].artwork.c_str() : "";
}
uint64_t exodus_library_last_played(const struct exodus_library *library, size_t index)
{
    return index < exodus_library_count(library) ? library->catalog.entries[index].last_played : 0;
}
const char *exodus_library_title(const struct exodus_library *library, size_t index)
{
    return index < exodus_library_count(library) ? library->catalog.entries[index].title.c_str() : "";
}
struct exodus_execution_target exodus_library_execution_target(const struct exodus_library *library, size_t index)
{
    if (index>=exodus_library_count(library)) return {EXODUS_BACKEND_NONE,EXODUS_TARGET_NONE};
    switch (library->catalog.entries[index].provider) {
        case Provider::Switch: return {EXODUS_BACKEND_HORIZON,EXODUS_TARGET_TITLE_ID};
        case Provider::Homebrew: return {EXODUS_BACKEND_HORIZON,EXODUS_TARGET_NRO};
        case Provider::CorkDeck: return {EXODUS_BACKEND_CORKDECK,EXODUS_TARGET_PE};
        case Provider::Steam: return {EXODUS_BACKEND_CORKDECK,EXODUS_TARGET_STEAM_LIBRARY};
        default: return {EXODUS_BACKEND_NONE,EXODUS_TARGET_NONE};
    }
}
const char *exodus_library_provider(const struct exodus_library *library, size_t index)
{
    if (index >= exodus_library_count(library)) return "Unknown";
    switch (library->catalog.entries[index].provider) {
        case Provider::Steam: return "Steam";
        case Provider::Switch: return "Switch";
        case Provider::Homebrew: return "Homebrew";
        case Provider::CorkDeck: return "CorkDeck";
        case Provider::Other: return "Other";
    }
    return "Other";
}
}
