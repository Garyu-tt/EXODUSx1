/* Standalone EXODUS settings proof: uses the original CorkDeck drawing/input
 * engine, not the Wine launcher. No system settings writes; guarded launch requests. */
#define SDL_MAIN_HANDLED
#include "launcher_ui.h"
#include "horizon_settings.h"
#include "library_catalog.h"
#include "native_launch.h"
#include "library_filter.h"
#include "shell_menu.h"
#include "control_metrics.h"
#include "launcher_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FONT_LIMIT (8 * 1024 * 1024)

static void *load_font(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    void *bytes = malloc(FONT_LIMIT + 1);
    if (!bytes) { fclose(f); return NULL; }
    *size = fread(bytes, 1, FONT_LIMIT + 1, f);
    int ok = !ferror(f) && *size > 0 && *size <= FONT_LIMIT;
    fclose(f);
    if (!ok) { free(bytes); return NULL; }
    return bytes;
}

static void set_row(struct ui_row *row, int group, const char *label,
                    const char *value, const char *help)
{
    memset(row, 0, sizeof(*row));
    row->group = group;
    row->kind = UI_ROW_INFO;
    snprintf(row->label, sizeof(row->label), "%s", label);
    snprintf(row->value, sizeof(row->value), "%s", value);
    row->help = help;
}

static void settings_snapshot(struct ui_row rows[9])
{
    struct exodus_settings s;
    exodus_read_settings(&s);
#ifndef __SWITCH__
    int percentage = -1;
    SDL_PowerState state = SDL_GetPowerInfo(NULL, &percentage);
    if (state == SDL_POWERSTATE_NO_BATTERY)
        snprintf(s.battery, sizeof(s.battery), "No battery");
    else if (percentage >= 0 && percentage <= 100)
        snprintf(s.battery, sizeof(s.battery), "%d%%", percentage);
#endif
    set_row(&rows[0], 0, "Platform", SDL_GetPlatform(), "Reported by SDL.");
    set_row(&rows[1], 0, "Firmware", s.firmware, "Horizon set:sys.");
    set_row(&rows[2], 0, "Battery", s.battery, "Horizon PSM, or SDL on host.");
    set_row(&rows[3], 0, "Runtime", "Standalone", "No Wine launcher linked.");
    set_row(&rows[4], 1, "Network", s.network, "NIFM connection status.");
    set_row(&rows[5], 1, "Wi-Fi", s.wifi, "NIFM wireless enable state.");
    set_row(&rows[6], 1, "Bluetooth", s.bluetooth, "System Bluetooth enable flag.");
    set_row(&rows[7], 2, "Brightness", s.brightness, "LBL brightness setting, not measured luminance.");
    set_row(&rows[8], 2, "Performance", s.performance, "APM mode; no clock or voltage writes.");
}

/* Eight visible covers at most; cache rebuilt only on filter/page changes. */
struct cover_slot { SDL_Texture *texture; int width, height; };

static void clear_covers(struct cover_slot covers[8])
{
    for (int i = 0; i < 8; ++i) {
        if (covers[i].texture) SDL_DestroyTexture(covers[i].texture);
        covers[i] = (struct cover_slot){0};
    }
}

static void load_cover(struct ui *ui, const char *path, struct cover_slot *slot)
{
    struct launcher_icon icon = {0};
    if (!exodus_artwork_path(path) || !launcher_image_load(path, &icon)) return;
    if (launcher_icon_fit(&icon, 256)) {
        slot->texture = SDL_CreateTexture(ui->renderer, SDL_PIXELFORMAT_RGBA32,
                                          SDL_TEXTUREACCESS_STATIC, icon.width, icon.height);
        if (slot->texture && SDL_UpdateTexture(slot->texture, NULL, icon.data, icon.width * 4)) {
            SDL_DestroyTexture(slot->texture);
            slot->texture = NULL;
        }
        slot->width = icon.width;
        slot->height = icon.height;
    }
    launcher_icon_free(&icon);
}

static size_t filter_cards(const struct exodus_library *library, enum exodus_filter filter,
                           const unsigned char *favorites, struct exodus_card_index *cards)
{
    size_t count = 0;
    for (size_t n = 0; n < exodus_library_count(library); ++n) {
        uint64_t recent = exodus_library_last_played(library, n);
        if (exodus_filter_matches(filter, exodus_library_provider(library, n), favorites[n], recent))
            cards[count++] = (struct exodus_card_index){n, recent};
    }
    if (filter == FILTER_RECENT) qsort(cards, count, sizeof(*cards), exodus_recent_compare);
    return count;
}

static int show_library(struct ui *ui, const char *path)
{
    struct exodus_library *library = exodus_library_open(path);
    size_t total = exodus_library_count(library), selection = 0, count = total;
    int handoff = 0;
    if (!total) {
        const char *status = exodus_library_status(library);
        ui_message(ui, "EXODUS Library", strcmp(status, "Ready") == 0 ? "Empty catalog" : status);
        exodus_library_close(library);
        return 0;
    }
    unsigned char *favorites = calloc(total, 1);
    struct exodus_card_index *cards = calloc(total, sizeof(*cards));
    struct cover_slot covers[8] = {{0}};
    size_t cached_page = (size_t)-1;
    enum exodus_filter filter = FILTER_ALL;
    if (!favorites || !cards) {
        ui_message(ui, "EXODUS Library", "Not enough memory");
        goto done;
    }
    count = filter_cards(library, filter, favorites, cards);
    const struct ui_hint hints[] = {
        {UI_A, "Open"}, {UI_B, "Back"}, {UI_L, "Previous"}, {UI_R, "Next"},
        {UI_X, "Favorite"}, {UI_Y, "Filter"}
    };
    ui_start_screen(ui);
    while (ui_begin_frame(ui)) {
        struct ui_input input;
        while (ui_poll(ui, &input)) {
            size_t previous = selection;
            if (input.button == UI_B) goto done;
            if (input.button == UI_Y) {
                filter = (enum exodus_filter)((filter + 1) % FILTER_COUNT);
                count = filter_cards(library, filter, favorites, cards);
                selection = 0;
                cached_page = (size_t)-1;
                continue;
            }
            if (!count) continue;
            size_t index = cards[selection].index;
            if (input.button == UI_X) {
                favorites[index] = !favorites[index];
                if (filter == FILTER_FAVORITES) {
                    count = filter_cards(library, filter, favorites, cards);
                    if (selection >= count) selection = count ? count - 1 : 0;
                    cached_page = (size_t)-1;
                }
                continue;
            }
            if (input.button == UI_LEFT && selection > 0) selection--;
            if (input.button == UI_RIGHT && selection + 1 < count) selection++;
            if (input.button == UI_UP && selection >= 4) selection -= 4;
            if (input.button == UI_DOWN && selection + 4 < count) selection += 4;
            if (input.button == UI_L && selection >= 8) selection -= 8;
            if (input.button == UI_R && selection / 8 < (count - 1) / 8) {
                selection += 8;
                if (selection >= count) selection = count - 1;
            }
            if (input.button == UI_A) {
                char detail[640];
                const char *provider = exodus_library_provider(library, index);
                const char *id = exodus_library_id(library, index);
                enum exodus_route route = exodus_route_for(provider, id);
#ifdef __SWITCH__
                char result[256];
                if (route == EXODUS_ROUTE_SWITCH || route == EXODUS_ROUTE_HOMEBREW ||
                    route == EXODUS_ROUTE_WINDOWS || route == EXODUS_ROUTE_STEAM) {
                    snprintf(detail, sizeof(detail), "Target: %s\n%.500s", exodus_route_name(route), id);
                    if (ui_confirm(ui, exodus_library_title(library, index), detail,
                                   route == EXODUS_ROUTE_STEAM ? "Open library" : "Launch")) {
                        if (exodus_launch(route, id, result)) { handoff = 1; goto done; }
                        ui_message(ui, "Launch unavailable", result);
                    }
                    break;
                }
#endif
                snprintf(detail, sizeof(detail), "%s\nNo active launch backend.", exodus_route_name(route));
                ui_message(ui, exodus_library_title(library, index), detail);
                break;
            }
            if (previous != selection) ui_sound(ui, LAUNCHER_SOUND_MOVE);
        }
        if (!ui->running) break;
        size_t first = selection / 8 * 8;
        if (cached_page != first) {
            clear_covers(covers);
            for (size_t n = first; n < count && n < first + 8; ++n)
                load_cover(ui, exodus_library_artwork(library, cards[n].index), &covers[n - first]);
            cached_page = first;
        }
        char page[96];
        snprintf(page, sizeof(page), "%s | %zu items | %zu / %zu", exodus_filter_name(filter),
                 count, count ? selection / 8 + 1 : 0, (count + 7) / 8);
        ui_background(ui);
        ui_header(ui, "EXODUS Library", page);
        if (!count) ui_text(ui, ui.normal, 84, 180, "No matching entries. Press Y to change filter.", ui->text);
        for (size_t n = first; n < count && n < first + 8; ++n) {
            size_t index = cards[n].index;
            int col = (int)((n - first) % 4), row = (int)((n - first) / 4);
            int x = 84 + 280 * col, y = 132 + 244 * row;
            ui_panel(ui, x, y, 264, 220);
            if (n == selection) ui_outline(ui, x, y, 264, 220, 6, 3, ui->selection);
            ui_text_fit(ui, ui->small, x + 12, y + 12, 200,
                        exodus_library_provider(library, index), ui->dim, 0);
            if (favorites[index]) ui_text(ui, ui.small, x + 230, y + 12, "*", ui->text);
            struct cover_slot *cover = &covers[n - first];
            if (cover->texture) {
                float scale = 116.0f / cover->height;
                if (cover->width * scale > 240) scale = 240.0f / cover->width;
                int w = (int)(cover->width * scale), h = (int)(cover->height * scale);
                if (w < 1) w = 1;
                if (h < 1) h = 1;
                SDL_Rect dst = {x + (264-w)/2, y + 40 + (116-h)/2, w, h};
                SDL_RenderCopy(ui->renderer, cover->texture, NULL, &dst);
            }
            ui_text_fit(ui, ui->normal, x + 12, y + 168, 240,
                        exodus_library_title(library, index), ui->text, n == selection);
        }
        ui_footer(ui, hints, 6);
        ui_present(ui);
        ui_wait(ui);
    }
done:
    clear_covers(covers);
    free(cards);
    free(favorites);
    exodus_library_close(library);
    return handoff;
}

static void show_settings(struct ui *ui)
{
    struct ui_list list = {0};
    struct ui_row rows[9];
    const char *groups[] = {"System", "Connectivity", "Hardware"};
    int group = 0;
    enum ui_action action;
    do {
        settings_snapshot(rows);
        action = ui_settings_run(ui, &list, "EXODUS Settings",
                                 "Read-only preview", groups, 3, rows, 9, 0, &group);
    } while (ui->running && action != UI_ACTION_BACK && action != UI_ACTION_QUIT);
}

static int open_tool(struct ui *ui, const struct exodus_page_info *page)
{
#ifdef __SWITCH__
    if (appletGetAppletType() != AppletType_Application || !envHasNextLoad()) {
        ui_message(ui, page->name, "Full application mode and a compatible loader are required.");
        return 0;
    }
    if (exodus_autorun_probe(page->nro) != EXODUS_AUTORUN_READY) {
        ui_message(ui, page->name, "Tool NRO missing or invalid. Install the corresponding EXODUS build.");
        return 0;
    }
    if (!ui_confirm(ui, page->name, page->nro, "Open")) return 0;
    Result rc = envSetNextLoad(page->nro, page->nro);
    if (R_SUCCEEDED(rc)) return 1;
    char detail[64];
    snprintf(detail, sizeof(detail), "Handoff failed: 0x%08lx", (unsigned long)rc);
    ui_message(ui, page->name, detail);
#else
    ui_message(ui, page->name, "Native Switch tool; unavailable on host.");
#endif
    return 0;
}

static void show_control_center(struct ui *ui)
{
    struct exodus_metrics metrics;
    struct exodus_settings settings;
    const struct ui_hint hints[] = {{UI_X, "Refresh"}, {UI_B, "Back"}};
    int refresh = 1;
    ui_start_screen(ui);
    while (ui_begin_frame(ui)) {
        struct ui_input input;
        while (ui_poll(ui, &input)) {
            if (input.button == UI_B || input.button == UI_PLUS) return;
            if (input.button == UI_X) refresh = 1;
        }
        if (!ui->running) return;
        if (refresh) {
            exodus_read_settings(&settings);
            exodus_read_metrics(&metrics);
            refresh = 0;
        }
        const char *labels[] = {"Battery", "Network", "Wi-Fi", "Bluetooth",
                                "Brightness", "Performance", "Process memory",
                                "SD used / capacity", "SoC temperature", "Board temperature"};
        const char *values[] = {settings.battery, settings.network, settings.wifi,
                                settings.bluetooth, settings.brightness, settings.performance,
                                metrics.memory, metrics.storage, metrics.soc, metrics.board};
        ui_background(ui);
        ui_header(ui, "Control Center", "Read-only snapshot");
        for (int n = 0; n < 10; ++n) {
            int x = 84 + (n % 2) * 568, y = 116 + (n / 2) * 98;
            ui_panel(ui, x, y, 548, 88);
            ui_text(ui, ui->small, x + 18, y + 8, labels[n], ui->dim);
            ui_text_fit(ui, ui->normal, x + 18, y + 42, 512, values[n], ui->text, 0);
        }
        ui_footer(ui, hints, 2);
        ui_present(ui);
        ui_wait(ui);
    }
}

static void draw_home(struct ui *ui, size_t selection)
{
    ui_background(ui);
    ui_header(ui, "EXODUS", "Gaming Mode");
    for (size_t n = 0; n < PAGE_COUNT; ++n) {
        int x = 84, y = 120 + (int)n * 94;
        ui_panel(ui, x, y, 1112, 82);
        if (n == selection) ui_outline(ui, x, y, 1112, 82, 6, 3, ui->selection);
        ui_text(ui, ui->normal, x + 22, y + 9, exodus_pages[n].name, ui->text);
        ui_text_fit(ui, ui->small, x + 22, y + 47, 1068,
                    exodus_pages[n].description, ui->dim, 0);
    }
    const struct ui_hint hints[] = {{UI_A, "Open"}, {UI_PLUS, "Control Center"}, {UI_B, "Exit"}};
    ui_footer(ui, hints, 3);
}

static void show_home(struct ui *ui, const char *catalog)
{
    size_t selection = 0;
    ui_start_screen(ui);
    while (ui_begin_frame(ui)) {
        struct ui_input input;
        int changed_screen = 0;
        while (ui_poll(ui, &input)) {
            if (input.button == UI_B) return;
            if (input.button == UI_PLUS) {
                show_control_center(ui);
                changed_screen = 1;
                break;
            }
            if (input.button == UI_UP) selection = exodus_page_move(selection, -1);
            if (input.button == UI_DOWN) selection = exodus_page_move(selection, 1);
            if (input.button != UI_A) continue;
            if (selection == PAGE_EXIT) return;
            if (selection == PAGE_LIBRARY) {
                if (show_library(ui, catalog)) return;
            } else if (selection == PAGE_SETTINGS) {
                show_settings(ui);
            } else if (open_tool(ui, &exodus_pages[selection])) return;
            changed_screen = 1;
            break;
        }
        if (!ui->running) break;
        if (changed_screen) {
            ui_start_screen(ui);
            continue;
        }
        draw_home(ui, selection);
        ui_present(ui);
        ui_wait(ui);
    }
}

int main(int argc, char **argv)
{
    struct ui ui;
    int smoke = argc == 3 && strcmp(argv[2], "--smoke") == 0;
    int library_mode = argc == 4 && strcmp(argv[2], "--library") == 0;
    int shell_mode = argc == 4 && strcmp(argv[2], "--shell") == 0;
    size_t size = 0;
    const char *font_path = argc >= 2 ? argv[1] : NULL;
#ifdef __SWITCH__
    if (argc <= 1) font_path = "sdmc:/switch/exodus-shell/font.ttf";
#endif
    if (!font_path || (argc > 2 && !smoke && !library_mode && !shell_mode)) {
        fprintf(stderr, "usage: %s FONT.ttf [--smoke | --library CATALOG.json | --shell CATALOG.json]\n", argv[0]);
        return 2;
    }
    void *font = load_font(font_path, &size);
    if (!font) { fprintf(stderr, "Cannot read font: %s\n", font_path); return 1; }
    if (!ui_init(&ui, font, size, 0)) {
        fprintf(stderr, "CorkDeck UI: %s\n", ui_error());
        free(font);
        return 1;
    }
    SDL_SetWindowTitle(ui.window, "EXODUS");
    if (smoke) {
        if (!ui_begin_frame(&ui)) {
            ui_quit(&ui);
            free(font);
            return 1;
        }
        draw_home(&ui, 0);
        ui_present(&ui);
        puts("EXODUS standalone donor UI frame rendered");
    } else if (library_mode) {
        SDL_SetWindowTitle(ui.window, "EXODUS Library");
        show_library(&ui, argv[3]);
    } else {
        show_home(&ui, shell_mode ? argv[3] : "sdmc:/ulaunch/exodus-catalog.json");
    }
    ui_quit(&ui);
    free(font);
    return 0;
}
