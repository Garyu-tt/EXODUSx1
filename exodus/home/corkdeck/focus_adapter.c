// SPDX-License-Identifier: GPL-2.0-only
#include "focus_indicator.h"
#include "launcher_ui.h"

void ui_focus_border(struct ui *ui, int x, int y, int width, int height, int radius,
                     int thickness, SDL_Color base, SDL_Color highlight)
{
    (void)radius;
    exodus_draw_focus_frame(ui->renderer, x, y, width, height, thickness, base, highlight);
}

void ui_outline(struct ui *ui, int x, int y, int width, int height, int radius,
                int thickness, SDL_Color color)
{
    ui_focus_border(ui, x, y, width, height, radius, thickness, color, color);
}
