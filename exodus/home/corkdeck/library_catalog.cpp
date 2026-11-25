// SPDX-License-Identifier: GPL-2.0-only
#include "library_catalog.h"
#include "../ulaunch/catalog.hpp"
#include <new>
#include <utility>
#ifdef __SWITCH__
#include "live_inventory.hpp"
#include "unified_catalog.hpp"
#endif
using namespace exodus::home;
struct exodus_library { Catalog catalog; };
extern "C" {
struct exodus_library *exodus_library_open(const char *path)
{
    if (!path) return nullptr;
    try {
        auto catalog = ReadCatalog(path);
#ifdef __SWITCH__
        if (std::string_view(path) == "sdmc:/ulaunch/exodus-catalog.json" &&
            (catalog.status == CatalogStatus::Ready || catalog.status == CatalogStatus::Unavailable)) {
            auto live = exodus_live_inventory();
            if (live.status == CatalogStatus::Ready) {
                std::set<std::pair<Provider,std::string>> known;
                for (const auto &e:catalog.entries) known.emplace(e.provider,e.id);
                for (auto &e:live.entries)
                    if (catalog.entries.size()<CatalogEntryLimit && known.emplace(e.provider,e.id).second)
                        catalog.entries.push_back(std::move(e));
                catalog.status=CatalogStatus::Ready;
            }
            MergeProvider(catalog, ReadCorkDeck("sdmc:/switch/wine/launcher-library-v2.ini"), Provider::CorkDeck);
            auto nonsteam=std::count_if(catalog.entries.begin(),catalog.entries.end(),
                [](const Entry &e){return e.provider!=Provider::Steam;});
            MergeProvider(catalog,ReadSteamLatest("sdmc:/switch/exodus-steam-sync/snapshots",
                CatalogEntryLimit-nonsteam),Provider::Steam);
        }
#endif
        return new exodus_library{std::move(catalog)};
    }
    catch (...) { return nullptr; }
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
