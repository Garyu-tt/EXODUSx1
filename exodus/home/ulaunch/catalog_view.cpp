// SPDX-License-Identifier: GPL-2.0-only
#include "catalog_refresh.hpp"

#include <algorithm>
#include <ul/menu/smi/smi_Commands.hpp>
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

        const Entry *SelectTitle(const std::vector<const Entry *> &entries, const char *caption) {
            std::string content;
            std::vector<std::string> options;
            for (const auto *choice : entries) {
                if (!content.empty())
                    content += '\n';
                const auto number = std::to_string(options.size() + 1);
                content += number + ". [" + CatalogText(ProviderLabel(choice->provider)) + "] " + choice->title;
                options.push_back(number);
            }
            options.push_back(CatalogText("cancel"));
            const auto selected = g_MenuApplication->DisplayDialog(CatalogText(caption), content, options, true);
            if (selected < 0 || static_cast<std::size_t>(selected) >= entries.size())
                return nullptr;
            return entries[static_cast<std::size_t>(selected)];
        }

        bool LaunchTitle(const Entry &choice) {
            const auto &menu = g_MenuApplication->GetLayout<ul::menu::ui::MainMenuLayout>();
            if (choice.provider == Provider::Homebrew)
                return menu->TryLaunchCatalogHomebrew(choice.id);
            const auto id = SwitchApplicationId(choice);
            return id && menu->TryLaunchCatalogApplication(*id);
        }

        void ShowSteamLaunchStatus(const Entry &choice, bool steam_snapshot_applied) {
            const auto id = SteamApplicationId(choice);
            if (!id)
                return;

            ul::smi::CatalogSteamLaunchInfo info{};
            const auto rc = ul::menu::smi::QueryCatalogSteamLaunchStatus(*id, info);
            const auto source = steam_snapshot_applied ? "exodus_catalog_steam_saved_library"
                : "exodus_catalog_steam_manual_library";
            const auto status = R_SUCCEEDED(rc) ? "exodus_catalog_steam_status_pending"
                : "exodus_catalog_steam_status_unavailable";
            const auto content = CatalogText("exodus_catalog_steam_appid") + " " + std::to_string(*id)
                + "\n" + CatalogText(source) + "\n\n" + CatalogText(status);
            g_MenuApplication->DisplayDialog(choice.title, content, {CatalogText("ok")}, true);
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
            std::vector<const Entry *> launchable;
            std::vector<const Entry *> steam_titles;
            for (auto index = first; index < end; ++index) {
                if (index > first)
                    content += '\n';
                const auto &entry = catalog.entries[index];
                content += "[" + CatalogText(ProviderLabel(entry.provider)) + "] " + entry.title;
                const auto id = SwitchApplicationId(entry);
                if (id && main_menu->CanLaunchCatalogApplication(*id)) {
                    launchable.push_back(&entry);
                } else if (entry.provider == Provider::Homebrew && main_menu->CanLaunchCatalogHomebrew(entry.id)) {
                    launchable.push_back(&entry);
                } else if (SteamApplicationId(entry)) {
                    steam_titles.push_back(&entry);
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
            const auto steam_option = options.size();
            if (!steam_titles.empty())
                options.push_back(CatalogText("exodus_catalog_steam_status"));
            options.push_back(CatalogText("exodus_catalog_close"));

            const auto selected = g_MenuApplication->DisplayDialog(page_title, content, options, true);
            if (selected < 0 || static_cast<std::size_t>(selected) >= options.size() - 1)
                return;
            if (!launchable.empty() && static_cast<std::size_t>(selected) == launch_option) {
                const auto *choice = SelectTitle(launchable, "exodus_catalog_select_title");
                if (choice && LaunchTitle(*choice))
                    return;
            } else if (!steam_titles.empty() && static_cast<std::size_t>(selected) == steam_option) {
                const auto *choice = SelectTitle(steam_titles, "exodus_catalog_select_steam_title");
                if (choice)
                    ShowSteamLaunchStatus(*choice, catalog.steam_snapshot_applied);
            } else if (has_previous && selected == 0) {
                --page;
            } else {
                ++page;
            }
        }
    }

}
