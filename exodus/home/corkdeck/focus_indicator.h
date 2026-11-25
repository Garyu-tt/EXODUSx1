/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef EXODUS_FOCUS_INDICATOR_H
#define EXODUS_FOCUS_INDICATOR_H

#include <SDL.h>

void exodus_draw_focus_frame(SDL_Renderer *renderer, int x, int y,
                             int width, int height, int thickness,
                             SDL_Color base, SDL_Color highlight);

#endif
