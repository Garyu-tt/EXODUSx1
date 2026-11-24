// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "../ulaunch/catalog.hpp"
#include "launch_route.h"
#include <switch.h>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <memory>
#include <set>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

extern "C" {
#include "../../runtime/autorun_probe.h"
}

namespace {
using namespace exodus::home;
struct ExodusNsGuard { ~ExodusNsGuard() { nsExit(); } };
struct ExodusDirClose { void operator()(DIR *d) const { if (d) closedir(d); } };

inline bool exodus_scan_titles(Catalog &catalog)
{
    if (R_FAILED(nsInitialize())) return false;
    ExodusNsGuard guard;
    std::set<u64> ids;
    NsApplicationRecord records[32];
    auto control = std::make_unique<NsApplicationControlData>();
    s32 offset = 0;
    for (;;) {
        s32 count = 0;
        if (R_FAILED(nsListApplicationRecord(records, 32, offset, &count)) ||
            count < 0 || count > 32 || count > 4096 - offset) return false;
        if (!count) break;
        for (s32 i = 0; i < count; ++i) {
            u64 id = records[i].application_id;
            if (!id || !ids.insert(id).second) return false;
            char key[17];
            snprintf(key, sizeof(key), "%016llx", (unsigned long long)id);
            std::string name = key;
            u64 size = 0;
            NacpLanguageEntry *language = nullptr;
            if (R_SUCCEEDED(nsGetApplicationControlData(NsApplicationControlSource_Storage,
                id, control.get(), sizeof(*control), &size)) &&
                size >= sizeof(control->nacp) &&
                R_SUCCEEDED(nsGetApplicationDesiredLanguage(&control->nacp, &language)) &&
                language && language->name[0] &&
                memchr(language->name, 0, sizeof(language->name)) &&
                utf8_to_utf32(nullptr, (const uint8_t *)language->name, 0) >= 0)
                name = language->name;
            for (char &ch : name) if ((unsigned char)ch < 32 || ch == 127) ch = ' ';
            catalog.entries.push_back({Provider::Switch, std::move(name), key, "", 0});
        }
        offset += count;
    }
    return true;
}

inline void exodus_scan_nros(Catalog &catalog, const std::string &prefix, bool descend,
                             size_t &budget)
{
    std::string directory = "sdmc:/switch/" + prefix;
    std::unique_ptr<DIR, ExodusDirClose> dir(opendir(directory.c_str()));
    if (!dir) return;
    while (budget && catalog.entries.size() < CatalogEntryLimit) {
        auto *item = readdir(dir.get());
        if (!item) break;
        --budget;
        std::string name = item->d_name;
        if (name.empty() || name[0] == '.' || name.find_first_of("/\\:\"") != std::string::npos)
            continue;
        std::string id = prefix + name;
        std::string path = "sdmc:/switch/" + id;
        if (path.size() >= 512) continue;
        struct stat st{};
        if (stat(path.c_str(), &st) != 0) continue;
        if (descend && S_ISDIR(st.st_mode)) {
            exodus_scan_nros(catalog, id + "/", false, budget);
        } else if (S_ISREG(st.st_mode)) {
            char checked[512];
            if (id == "exodus-shell/exodus-shell.nro" ||
                !exodus_nro_path(id.c_str(), checked) ||
                exodus_autorun_probe(checked) != EXODUS_AUTORUN_READY ||
                utf8_to_utf32(nullptr, (const uint8_t *)name.c_str(), 0) < 0) continue;
            name.resize(name.size() - 4);
            for (char &ch : name) if ((unsigned char)ch < 32 || ch == 127) ch = ' ';
            catalog.entries.push_back({Provider::Homebrew, std::move(name), std::move(id), "", 0});
        }
    }
}

inline Catalog exodus_live_inventory()
{
    Catalog catalog{CatalogStatus::Ready, {}};
    // A failed NS scan must not publish an incomplete set of registered games.
    bool ns_ok = exodus_scan_titles(catalog);
    if (!ns_ok) catalog.entries.clear();
    size_t budget = 4096;
    exodus_scan_nros(catalog, "", true, budget);
    if (!ns_ok && catalog.entries.empty()) catalog.status = CatalogStatus::Unavailable;
    std::stable_sort(catalog.entries.begin(), catalog.entries.end(), [](const Entry &a, const Entry &b) {
        return a.provider == b.provider ? a.title < b.title : a.provider < b.provider;
    });
    return catalog;
}
}
