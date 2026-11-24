// SPDX-License-Identifier: GPL-2.0-only
#include "catalog_refresh.hpp"

#include <algorithm>
#include <ul/menu/ui/ui_MenuApplication.hpp>

extern ul::menu::ui::GlobalSettings g_GlobalSettings;
extern ul::menu::ui::MenuApplication::Ref g_MenuApplication;

namespace exodus::home {

    namespace {

        constexpr std::size_t ItemsPerPage = 6;
        constexpr char CatalogPath[] = "sdmc:/ulaunch/exodus-catalog.json";

        std::string CatalogText(const char *key) {
            const auto &language = g_GlobalSettings.main_lang;
            const auto translated = language.find(key);
            if (translated != language.end() && translated->is_string()
                && !translated->get_ref<const std::string &>().empty()) {
                return translated->get<std::string>();
            }
            return g_GlobalSettings.default_lang.value(key, key);
        }

        const char *ProviderLabel(Provider provider) {
            switch (provider) {
                case Provider::Steam: return "exodus_catalog_steam";
                case Provider::Switch: return "exodus_catalog_switch";
                case Provider::Homebrew: return "exodus_catalog_homebrew";
                case Provider::Other: return "exodus_catalog_other";
            }
            return "exodus_catalog_other";
        }

        const char *StatusMessage(CatalogStatus status) {
            switch (status) {
                case CatalogStatus::Unavailable: return "exodus_catalog_unavailable";
                case CatalogStatus::Invalid: return "exodus_catalog_invalid";
                case CatalogStatus::TooLarge: return "exodus_catalog_too_large";
                case CatalogStatus::Ready: return "exodus_catalog_empty";
            }
            return "exodus_catalog_unavailable";
        }

        struct LaunchChoice {
            std::uint64_t id;
            std::string title;
        };

        bool SelectTitle(const std::vector<LaunchChoice> &launchable) {
            std::string content;
            std::vector<std::string> options;
            for (const auto &choice : launchable) {
                if (!content.empty())
                    content += '\n';
                const auto number = std::to_string(options.size() + 1);
                content += number + ". " + choice.title;
                options.push_back(number);
            }
            options.push_back(CatalogText("cancel"));
            const auto selected = g_MenuApplication->DisplayDialog(
                CatalogText("exodus_catalog_select_title"), content, options, true);
            if (selected < 0 || static_cast<std::size_t>(selected) >= launchable.size())
                return false;
            const auto &choice = launchable[static_cast<std::size_t>(selected)];
            return g_MenuApplication->GetLayout<ul::menu::ui::MainMenuLayout>()
                ->TryLaunchCatalogApplication(choice.id);
        }

    }

    std::string CatalogLabel() {
        return CatalogText("exodus_catalog");
    }

    void ShowCatalog() {
        const auto catalog = ReadRefreshedCatalog(CatalogPath, SteamSnapshotsPath);
        const auto title = CatalogLabel();
        if (catalog.status != CatalogStatus::Ready || catalog.entries.empty()) {
            g_MenuApplication->DisplayDialog(
                title, CatalogText(StatusMessage(catalog.status)), {CatalogText("ok")}, true);
            return;
        }

        const auto &main_menu = g_MenuApplication->GetLayout<ul::menu::ui::MainMenuLayout>();
        const auto page_count = (catalog.entries.size() + ItemsPerPage - 1) / ItemsPerPage;
        for (std::size_t page = 0; ; ) {
            const auto first = page * ItemsPerPage;
            const auto end = std::min(first + ItemsPerPage, catalog.entries.size());
            std::string content;
            std::vector<LaunchChoice> launchable;
            for (auto index = first; index < end; ++index) {
                if (index > first)
                    content += '\n';
                const auto &entry = catalog.entries[index];
                content += "[" + CatalogText(ProviderLabel(entry.provider)) + "] " + entry.title;
                const auto id = SwitchApplicationId(entry);
                if (id && main_menu->CanLaunchCatalogApplication(*id)) {
                    launchable.push_back({*id, entry.title});
                }
            }
            const auto page_title = title + " " + std::to_string(page + 1) + "/" + std::to_string(page_count);
            const bool has_previous = page != 0;
            std::vector<std::string> options;
            if (has_previous)
                options.push_back(CatalogText("exodus_catalog_previous"));
            if (page + 1 < page_count)
                options.push_back(CatalogText("exodus_catalog_next"));
            const auto launch_option = options.size();
            if (!launchable.empty())
                options.push_back(CatalogText("exodus_catalog_launch"));
            options.push_back(CatalogText("exodus_catalog_close"));

            const auto selected = g_MenuApplication->DisplayDialog(page_title, content, options, true);
            if (selected < 0 || static_cast<std::size_t>(selected) >= options.size() - 1)
                return;
            if (!launchable.empty() && static_cast<std::size_t>(selected) == launch_option) {
                if (SelectTitle(launchable))
                    return;
            } else if (has_previous && selected == 0) {
                --page;
            } else {
                ++page;
            }
        }
    }

}
