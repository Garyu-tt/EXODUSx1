/* Standalone EXODUS settings proof: uses the original CorkDeck drawing/input
 * engine, not the Wine launcher. No system writes or executable dispatch. */
#define SDL_MAIN_HANDLED
#include "launcher_ui.h"
#include "horizon_settings.h"
#include "library_catalog.h"
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

/* Eight cards per page; the donor renderer and input system stay in charge.
 * Snapshots are metadata only. No provider is implicitly executable. */
static void show_library(struct ui *ui, const char *path)
{
    struct exodus_library *library = exodus_library_open(path);
    size_t count = exodus_library_count(library), selection = 0;
    if (!count) {
        const char *status = exodus_library_status(library);
        ui_message(ui, "EXODUS Library",
                   strcmp(status, "Ready") == 0 ? "Empty catalog" : status);
        exodus_library_close(library);
        return;
    }
    const struct ui_hint hints[] = {
        {UI_A, "Details"}, {UI_B, "Back"}, {UI_L, "Previous"}, {UI_R, "Next"}
    };
    ui_start_screen(ui);
    while (ui_begin_frame(ui)) {
        struct ui_input input;
        while (ui_poll(ui, &input)) {
            size_t previous = selection;
            if (input.button == UI_B) goto done;
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
                char detail[256];
                snprintf(detail, sizeof(detail), "Provider: %s\nMetadata snapshot only. Launch unavailable.",
                         exodus_library_provider(library, selection));
                ui_message(ui, exodus_library_title(library, selection), detail);
                break;
            }
            if (previous != selection) ui_sound(ui, LAUNCHER_SOUND_MOVE);
        }
        if (!ui->running) break;
        char page[64];
        snprintf(page, sizeof(page), "Page %zu / %zu", selection / 8 + 1, (count + 7) / 8);
        ui_background(ui);
        ui_header(ui, "EXODUS Library", page);
        size_t first = selection / 8 * 8;
        for (size_t n = first; n < count && n < first + 8; ++n) {
            int col = (int)((n - first) % 4), row = (int)((n - first) / 4);
            int x = 84 + 280 * col, y = 132 + 244 * row;
            ui_panel(ui, x, y, 264, 220);
            if (n == selection) ui_outline(ui, x, y, 264, 220, 6, 3, ui->selection);
            ui_text_fit(ui, ui->small, x + 12, y + 18, 240,
                        exodus_library_provider(library, n), ui->dim, 0);
            ui_text_fit(ui, ui->normal, x + 12, y + 160, 240,
                        exodus_library_title(library, n), ui->text, n == selection);
        }
        ui_footer(ui, hints, 4);
        ui_present(ui);
        ui_wait(ui);
    }
done:
    exodus_library_close(library);
}

int main(int argc, char **argv)
{
    struct ui ui;
    struct ui_list list = {0};
    struct ui_row rows[9];
    const char *groups[] = {"System", "Connectivity", "Hardware"};
    int group = 0;
    int smoke = argc == 3 && strcmp(argv[2], "--smoke") == 0;
    int library_mode = argc == 4 && strcmp(argv[2], "--library") == 0;
    size_t size = 0;
    if (argc != 2 && !smoke && !library_mode) {
        fprintf(stderr, "usage: %s FONT.ttf [--smoke | --library CATALOG.json]\n", argv[0]);
        return 2;
    }
    void *font = load_font(argv[1], &size);
    if (!font) { fprintf(stderr, "Cannot read font\n"); return 1; }
    if (!ui_init(&ui, font, size, 0)) {
        fprintf(stderr, "CorkDeck UI: %s\n", ui_error());
        free(font);
        return 1;
    }
    SDL_SetWindowTitle(ui.window, "EXODUS Settings");
    settings_snapshot(rows);
    if (smoke) {
        if (!ui_begin_frame(&ui)) {
            ui_quit(&ui);
            free(font);
            return 1;
        }
        ui_background(&ui);
        ui_header(&ui, "EXODUS Settings", "Standalone UI");
        ui_panel(&ui, 84, 140, 1112, 120);
        ui_text(&ui, ui.normal, 110, 160, rows[0].label, ui.text);
        ui_text(&ui, ui.normal, 110, 200, rows[0].value, ui.value);
        ui_present(&ui);
        puts("EXODUS standalone donor UI frame rendered");
    } else if (library_mode) {
        SDL_SetWindowTitle(ui.window, "EXODUS Library");
        show_library(&ui, argv[3]);
    } else {
        enum ui_action action;
        do {
            settings_snapshot(rows);
            action = ui_settings_run(&ui, &list, "EXODUS Settings",
                                     "Read-only preview", groups, 3, rows, 9, 0, &group);
        } while (ui.running && action != UI_ACTION_BACK && action != UI_ACTION_QUIT);
    }
    ui_quit(&ui);
    free(font);
    return 0;
}
