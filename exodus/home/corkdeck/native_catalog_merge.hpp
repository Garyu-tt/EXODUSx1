// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "../ulaunch/catalog.hpp"
#include "launch_route.h"
#include <set>
#include <utility>

namespace exodus::home {
enum class NativeMergeResult { Ready, Unavailable, Invalid, Limit };

inline std::pair<Provider,std::string> NativeCatalogKey(const Entry &e)
{
    char key[512];
    uint64_t id=0;
    if (e.provider==Provider::Switch && exodus_title_id(e.id.c_str(),&id)) {
        snprintf(key,sizeof(key),"%016llx",(unsigned long long)id);
        return {e.provider,key};
    }
    if (e.provider==Provider::Homebrew && exodus_nro_path(e.id.c_str(),key))
        return {e.provider,key};
    return {e.provider,e.id};
}

// Native inventory contributes identities, never launch authorization.
// Build a scratch catalog and commit only when every limit is satisfied.
// Existing external entries retain their presentation metadata.
inline NativeMergeResult MergeNativeCatalog(Catalog &external, const Catalog &live)
{
    if (external.status!=CatalogStatus::Ready && external.status!=CatalogStatus::Unavailable)
        return NativeMergeResult::Invalid;
    if (live.status!=CatalogStatus::Ready) return NativeMergeResult::Unavailable;
    Catalog merged{CatalogStatus::Ready,{},external.steam_snapshot_applied};
    std::set<std::pair<Provider,std::string>> seen;
    size_t bytes=0;
    auto append=[&](const Entry &e) {
        auto key=NativeCatalogKey(e);
        if (!seen.insert(std::move(key)).second) return true;
        if (merged.entries.size()>=CatalogEntryLimit) return false;
        for (size_t n : {e.id.size(),e.title.size(),e.artwork.size()}) {
            if (n>CatalogByteLimit-bytes) return false;
            bytes+=n;
        }
        merged.entries.push_back(e);
        return true;
    };
    for (const auto &e:external.entries)
        if (!append(e)) return NativeMergeResult::Limit;
    for (const auto &e:live.entries) {
        if (e.provider!=Provider::Switch && e.provider!=Provider::Homebrew) continue;
        if (!append(e)) return NativeMergeResult::Limit;
    }
    external=std::move(merged);
    return NativeMergeResult::Ready;
}
}
