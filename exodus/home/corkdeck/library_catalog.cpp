// SPDX-License-Identifier: GPL-2.0-only
#include "library_catalog.h"
#include "../ulaunch/catalog_refresh.hpp"
#include <new>
#include <utility>
#ifndef _WIN32
#include "evcs_discovery.h"
#endif
#ifdef __SWITCH__
#include "live_inventory.hpp"
#endif
using namespace exodus::home;
struct exodus_library {
    Catalog catalog;
    const char *discovery="Not requested";
    size_t first_game=SIZE_MAX;
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
            ? ReadRefreshedCatalog(path, SteamSnapshotsPath, exodus_live_inventory)
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
