/* SPDX-License-Identifier: GPL-2.0-only */

#include "focus_indicator.h"

#include <limits.h>

void exodus_draw_focus_frame(SDL_Renderer *renderer, int x, int y,
                             int width, int height, int thickness,
                             SDL_Color base, SDL_Color highlight)
{
    SDL_Color saved_color;
    SDL_BlendMode saved_blend_mode;

    if (!renderer || width <= 0 || height <= 0 || thickness <= 0)
        return;

    /* Keep the exclusive edges representable as SDL_Rect coordinates. */
    width = SDL_min(width, INT_MAX - SDL_max(x, 0));
    height = SDL_min(height, INT_MAX - SDL_max(y, 0));
    if (width == 0 || height == 0)
        return;

    thickness = SDL_min(thickness, SDL_max(1, SDL_min(width, height) / 2));

    const int middle_height = height - 2 * thickness;
    const SDL_Rect horizontal[] = {
        {x, y, width, thickness},
        {x, y + height - thickness, width, thickness},
    };
    const SDL_Rect vertical[] = {
        {x, y + thickness, thickness, middle_height},
        {x + width - thickness, y + thickness, thickness, middle_height},
    };

    if (SDL_GetRenderDrawColor(renderer, &saved_color.r, &saved_color.g,
                               &saved_color.b, &saved_color.a) != 0 ||
        SDL_GetRenderDrawBlendMode(renderer, &saved_blend_mode) != 0)
        return;

    /* Each pixel belongs to one edge, including when the colors have alpha. */
    if (SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND) == 0) {
        if (middle_height > 0 &&
            SDL_SetRenderDrawColor(renderer, base.r, base.g, base.b, base.a) == 0)
            SDL_RenderFillRects(renderer, vertical, width > thickness ? 2 : 1);

        if (SDL_SetRenderDrawColor(renderer, highlight.r, highlight.g,
                                   highlight.b, highlight.a) == 0)
            SDL_RenderFillRects(renderer, horizontal, height > thickness ? 2 : 1);
    }

    SDL_SetRenderDrawColor(renderer, saved_color.r, saved_color.g,
                           saved_color.b, saved_color.a);
    SDL_SetRenderDrawBlendMode(renderer, saved_blend_mode);
}
