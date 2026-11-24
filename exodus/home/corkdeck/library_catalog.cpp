// SPDX-License-Identifier: GPL-2.0-only
#include "library_catalog.h"
#include "evcs_discovery.h"
#include "../ulaunch/catalog.hpp"
#include <new>
#include <utility>
#ifdef __SWITCH__
#include "live_inventory.hpp"
#endif
using namespace exodus::home;
struct exodus_library { Catalog catalog; const char *discovery = "Not requested"; };
struct evcs_catalog_pending {
    const Catalog &base;
    std::vector<Entry> entries;
    size_t bytes = 0;
};
static int evcs_catalog_emit(const char *path, enum evcs_kind kind, uint64_t bytes, void *user)
{
    (void)kind; (void)bytes;
    auto &p = *static_cast<evcs_catalog_pending *>(user);
    try {
        for (const auto &e : p.base.entries)
            if (e.provider == Provider::Other && e.id == path) return 1;
        const char *name = strrchr(path, '/');
        name = name ? name + 1 : path;
        size_t cost = strlen(path) + strlen(name);
        if (p.base.entries.size() + p.entries.size() >= CatalogEntryLimit ||
            cost > CatalogByteLimit - p.bytes) return 0;
        // A suffix is discovery only, never a verified provider or launch target.
        p.entries.push_back({Provider::Other, name, path});
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
    for (auto &entry : pending.entries) entries.push_back(std::move(entry));
    if (!entries.empty()) lib.catalog.status = CatalogStatus::Ready;
    lib.discovery = "Games scan complete";
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
