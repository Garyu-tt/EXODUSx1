/* Standalone EXODUS settings proof: uses the original CorkDeck drawing/input
 * engine, not the Wine launcher. No system writes or executable dispatch. */
#define SDL_MAIN_HANDLED
#include "launcher_ui.h"
#include "horizon_settings.h"
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

int main(int argc, char **argv)
{
    struct ui ui;
    struct ui_list list = {0};
    struct ui_row rows[9];
    const char *groups[] = {"System", "Connectivity", "Hardware"};
    int group = 0;
    int smoke = argc == 3 && strcmp(argv[2], "--smoke") == 0;
    size_t size = 0;
    if (argc != 2 && !smoke) {
        fprintf(stderr, "usage: %s FONT.ttf [--smoke]\n", argv[0]);
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
