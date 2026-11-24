/* SPDX-License-Identifier: GPL-2.0-only */

#include "focus_indicator.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static void expect(int condition, const char *message)
{
    if (condition)
        return;

    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}

static SDL_Color read_pixel(SDL_Renderer *renderer, int x, int y)
{
    const SDL_Rect rect = {x, y, 1, 1};
    Uint8 rgba[4];
    expect(SDL_RenderReadPixels(renderer, &rect, SDL_PIXELFORMAT_RGBA32,
                                rgba, sizeof(rgba)) == 0, SDL_GetError());
    return (SDL_Color){rgba[0], rgba[1], rgba[2], rgba[3]};
}

static int colors_equal(SDL_Color left, SDL_Color right)
{
    return left.r == right.r && left.g == right.g &&
           left.b == right.b && left.a == right.a;
}

int main(void)
{
    const SDL_Color black = {0, 0, 0, 255};
    const SDL_Color base = {255, 0, 0, 255};
    const SDL_Color highlight = {0, 255, 0, 255};
    const SDL_Color saved = {13, 27, 39, 61};

    expect(SDL_Init(0) == 0, SDL_GetError());
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 12, 10, 32,
                                                          SDL_PIXELFORMAT_RGBA32);
    expect(surface != NULL, SDL_GetError());
    SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(surface);
    expect(renderer != NULL, SDL_GetError());

    expect(SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255) == 0, SDL_GetError());
    expect(SDL_RenderClear(renderer) == 0, SDL_GetError());
    expect(SDL_SetRenderDrawColor(renderer, saved.r, saved.g, saved.b, saved.a) == 0,
           SDL_GetError());
    expect(SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD) == 0,
           SDL_GetError());

    exodus_draw_focus_frame(renderer, 2, 2, 7, 6, 2, base, highlight);
    expect(colors_equal(read_pixel(renderer, 2, 2), highlight), "Wrong corner color");
    expect(colors_equal(read_pixel(renderer, 5, 3), highlight), "Wrong top edge");
    expect(colors_equal(read_pixel(renderer, 5, 6), highlight), "Wrong bottom edge");
    expect(colors_equal(read_pixel(renderer, 3, 4), base), "Wrong left edge");
    expect(colors_equal(read_pixel(renderer, 7, 5), base), "Wrong right edge");
    expect(colors_equal(read_pixel(renderer, 4, 4), black), "Changed frame interior");
    expect(colors_equal(read_pixel(renderer, 9, 7), black), "Changed frame exterior");

    exodus_draw_focus_frame(renderer, 10, 2, 1, 6, INT_MAX,
                           (SDL_Color){255, 0, 0, 128}, highlight);
    const SDL_Color side = read_pixel(renderer, 10, 3);
    expect((side.r == 127 || side.r == 128) && side.g == 0 && side.b == 0,
           "Overdrew a translucent one-pixel edge");
    expect(colors_equal(read_pixel(renderer, 10, 7), highlight),
           "Wrong one-pixel edge endpoint");

    exodus_draw_focus_frame(renderer, 0, 0, 2, 2, 0, base, highlight);
    exodus_draw_focus_frame(renderer, 0, 0, -2, 2, 1, base, highlight);
    exodus_draw_focus_frame(renderer, INT_MAX, 0, 2, 2, 1, base, highlight);
    expect(colors_equal(read_pixel(renderer, 0, 0), black), "Drew an empty frame");

    SDL_Color current;
    SDL_BlendMode current_mode;
    expect(SDL_GetRenderDrawColor(renderer, &current.r, &current.g,
                                  &current.b, &current.a) == 0, SDL_GetError());
    expect(colors_equal(current, saved), "Draw color was not restored");
    expect(SDL_GetRenderDrawBlendMode(renderer, &current_mode) == 0, SDL_GetError());
    expect(current_mode == SDL_BLENDMODE_ADD, "Blend mode was not restored");

    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(surface);
    SDL_Quit();
    return EXIT_SUCCESS;
}
