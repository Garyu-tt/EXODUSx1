// SPDX-License-Identifier: GPL-2.0-only
#include "library_catalog.h"
#include "evcs_discovery.h"
#include "evcs_pfs0.h"
#include "evcs_manifest.h"
#include "launch_route.h"
extern "C" {
#include "../../runtime/autorun_probe.h"
}
#include "../ulaunch/catalog.hpp"
#include <new>
#include <utility>
#ifdef __SWITCH__
#include "live_inventory.hpp"
#endif
using namespace exodus::home;
struct exodus_library { Catalog catalog; const char *discovery = "Not requested"; size_t first_game = SIZE_MAX;
    std::vector<evcs_content> graph{};
    std::vector<size_t> graph_indices{};
    bool graph_valid = true;
};
struct evcs_catalog_pending {
    const Catalog &base;
    std::vector<Entry> entries;
    size_t bytes = 0;
};
static int evcs_catalog_emit(const char *path, enum evcs_kind kind, uint64_t bytes, void *user)
{
    (void)bytes;
    auto &p = *static_cast<evcs_catalog_pending *>(user);
    try {
        Provider provider = Provider::Other;
        char resolved[512];
        // Only the actual console namespace is eligible. Host paths stay inventory-only.
        if (kind == EVCS_NRO && !strncmp(path, "sdmc:/Games/", 12) &&
            exodus_nro_path(path, resolved) && !strcmp(path, resolved) &&
            exodus_autorun_probe(path) == EXODUS_AUTORUN_READY)
            provider = Provider::Homebrew;
        for (const auto &e : p.base.entries)
            if (e.provider == provider && e.id == path) return 1;
        const char *name = strrchr(path, '/');
        name = name ? name + 1 : path;
        size_t cost = strlen(path) + strlen(name);
        if (p.base.entries.size() + p.entries.size() >= CatalogEntryLimit ||
            cost > CatalogByteLimit - p.bytes) return 0;
        // Archives remain inventory-only; NRO is checked again at handoff.
        p.entries.push_back({provider, name, path});
        p.bytes += cost;
        return 1;
    } catch (...) { return 0; }
}
static void evcs_catalog_merge(exodus_library &lib, const char *root)
{
    if (!root) return;
    if (lib.catalog.status != CatalogStatus::Ready &&
        lib.catalog.status != CatalogStatus::Unavailable) {
        lib.discovery = "Base catalog invalid"; return;
    }
    evcs_catalog_pending pending{lib.catalog, {}, 0};
    const auto rc = evcs_discover(root, evcs_catalog_emit, &pending);
    if (rc != EVCS_OK) {
        lib.discovery = rc == EVCS_IO ? "Games folder unavailable" : "Games scan incomplete";
        return;
    }
    auto &entries = lib.catalog.entries;
    entries.reserve(entries.size() + pending.entries.size());
    lib.first_game = entries.size();
    for (auto &entry : pending.entries) entries.push_back(std::move(entry));
    if (!entries.empty()) lib.catalog.status = CatalogStatus::Ready;
    lib.discovery = "Games scan complete";
    // Sidecars describe discovered files only. Never derive launch routes from them.
    for (size_t i=lib.first_game; i<entries.size(); ++i) {
        const auto &e=entries[i];
        enum evcs_kind kind;
        if (e.provider != Provider::Other || !evcs_kind_of(e.id.c_str(), &kind) ||
            (kind != EVCS_NSP && kind != EVCS_XCI)) continue;
        evcs_content item{};
        const auto status=evcs_manifest_read(e.id.c_str(), &item);
        if (status == EVCS_MANIFEST_MISSING) continue;
        if (status != EVCS_MANIFEST_OK) { lib.graph_valid=false; break; }
        lib.graph.push_back(item);
        lib.graph_indices.push_back(i);
    }
    if (evcs_graph_validate(lib.graph.data(),lib.graph.size()) != EVCS_GRAPH_OK)
        lib.graph_valid=false;
    if (!lib.graph_valid) {
        lib.graph.clear(); lib.graph_indices.clear();
        lib.discovery="Games scan complete; metadata rejected";
    }
}
extern "C" {
struct exodus_library *exodus_library_open(const char *path)
{
#ifdef __SWITCH__
    if (path && std::string_view(path) == "sdmc:/ulaunch/exodus-catalog.json")
        return exodus_library_open_with_games(path, "sdmc:/Games");
#endif
    return exodus_library_open_with_games(path, nullptr);
}
struct exodus_library *exodus_library_open_with_games(const char *path, const char *root)
{
    if (!path) return nullptr;
    try {
        auto catalog = ReadCatalog(path);
#ifdef __SWITCH__
        if (catalog.status == CatalogStatus::Unavailable &&
            std::string_view(path) == "sdmc:/ulaunch/exodus-catalog.json")
            catalog = exodus_live_inventory();
#endif
        exodus_library result{std::move(catalog)};
        evcs_catalog_merge(result, root);
        return new exodus_library{std::move(result)};
    }
    catch (...) { return nullptr; }
}
const char *exodus_library_discovery_status(const struct exodus_library *library)
{
    return library ? library->discovery : "Catalog unavailable";
}
int exodus_library_archive_detail(const struct exodus_library *library, size_t index,
                                  char *out, size_t cap)
{
    if (!library || !out || !cap || index < library->first_game ||
        index >= library->catalog.entries.size()) return 0;
    const auto &entry = library->catalog.entries[index];
    enum evcs_kind kind;
    if (entry.provider != Provider::Other ||
        !evcs_kind_of(entry.id.c_str(), &kind) || kind != EVCS_NSP) return 0;
    struct stat st;
    if (lstat(entry.id.c_str(), &st) || !S_ISREG(st.st_mode) || st.st_size <= 0) {
        snprintf(out, cap, "NSP unavailable. Not launchable.");
        return 1;
    }
    FILE *f = fopen(entry.id.c_str(), "rb");
    struct evcs_pfs0_info info;
    auto rc = f ? evcs_pfs0_probe(f, (uint64_t)st.st_size, &info) : EVCS_PFS0_IO;
    if (f && fclose(f)) rc = EVCS_PFS0_IO;
    if (rc == EVCS_PFS0_OK)
        snprintf(out, cap, "NSP directory: %lu files, %lu NCA, %lu metadata candidates.\nNo verified launch backend.",
                 (unsigned long)info.entries, (unsigned long)info.nca, (unsigned long)info.meta_nca);
    else
        snprintf(out, cap, "NSP directory %s. Not launchable.",
                 rc == EVCS_PFS0_LIMIT ? "exceeds scan limits" :
                 rc == EVCS_PFS0_IO ? "unreadable" : "invalid");
    if (rc == EVCS_PFS0_OK && library->graph_valid) {
        for (size_t j=0;j<library->graph_indices.size();++j) {
            if (library->graph_indices[j]!=index) continue;
            const auto &item=library->graph[j];
            const auto group=evcs_graph_group(library->graph.data(),library->graph.size(),item.application_id);
            size_t used=strlen(out);
            if (used<cap)
                snprintf(out+used,cap-used,"\nOwner metadata (unverified): base %s, update %s, %lu DLC.",
                         group.base==SIZE_MAX ? "missing" : "listed",
                         group.update==SIZE_MAX ? "none" : "listed",
                         (unsigned long)group.dlc_count);
            break;
        }
    }
    return 1;
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
