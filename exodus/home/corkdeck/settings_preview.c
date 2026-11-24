/* Standalone EXODUS settings proof: uses the original CorkDeck drawing/input
 * engine, not the Wine launcher. No system writes or executable dispatch. */
#define SDL_MAIN_HANDLED
#include "launcher_ui.h"
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

static void settings_snapshot(struct ui_row rows[7])
{
    char battery[64];
    int percentage = -1;
    SDL_PowerState state = SDL_GetPowerInfo(NULL, &percentage);
    if (state == SDL_POWERSTATE_NO_BATTERY)
        snprintf(battery, sizeof(battery), "No battery");
    else if (percentage >= 0 && percentage <= 100)
        snprintf(battery, sizeof(battery), "%d%%", percentage);
    else
        snprintf(battery, sizeof(battery), "Unavailable");
    set_row(&rows[0], 0, "Platform", SDL_GetPlatform(), "Reported by SDL.");
    set_row(&rows[1], 0, "Battery", battery, "Read-only SDL power snapshot.");
    set_row(&rows[2], 0, "Runtime", "Standalone", "No Wine launcher linked.");
    set_row(&rows[3], 1, "Wi-Fi", "Not connected to provider", "Horizon service adapter pending.");
    set_row(&rows[4], 1, "Bluetooth", "Not connected to provider", "Horizon service adapter pending.");
    set_row(&rows[5], 2, "Brightness", "Unsupported", "Native read/write adapter pending.");
    set_row(&rows[6], 2, "Performance", "Unsupported", "No clock or voltage writes.");
}

int main(int argc, char **argv)
{
    struct ui ui;
    struct ui_list list = {0};
    struct ui_row rows[7];
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
                                     "Read-only preview", groups, 3, rows, 7, 0, &group);
        } while (ui.running && action != UI_ACTION_BACK && action != UI_ACTION_QUIT);
    }
    ui_quit(&ui);
    free(font);
    return 0;
}
