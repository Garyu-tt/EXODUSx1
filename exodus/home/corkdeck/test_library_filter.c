// SPDX-License-Identifier: GPL-2.0-only
#include "library_filter.h"
#include <assert.h>
int main(void)
{
    assert(exodus_filter_matches(FILTER_ALL, "Switch", 0, 0));
    assert(exodus_filter_matches(FILTER_SWITCH, "Switch", 0, 0));
    assert(!exodus_filter_matches(FILTER_SWITCH, "Steam", 1, 1));
    assert(exodus_filter_matches(FILTER_FAVORITES, "Steam", 1, 0));
    assert(!exodus_filter_matches(FILTER_FAVORITES, "Steam", 0, 9));
    assert(exodus_filter_matches(FILTER_RECENT, "Steam", 0, 1));
    struct exodus_card_index items[] = {{0, 10}, {1, 100}, {2, 100}};
    qsort(items, 3, sizeof(items[0]), exodus_recent_compare);
    assert(items[0].index == 1 && items[1].index == 2 && items[2].index == 0);
    assert(exodus_artwork_path("sdmc:/switch/wine/covers/a.png"));
    assert(exodus_artwork_path("sdmc:/ulaunch/a.jpeg"));
    assert(exodus_artwork_path("sdmc:/switch/wine/covers/a.PNG"));
    assert(exodus_artwork_path("sdmc:/ulaunch/a.JpG"));
    assert(exodus_artwork_path("sdmc:/ulaunch/a.JPEG"));
    assert(!exodus_artwork_path("sdmc:/switch/wine/../a.png"));
    assert(!exodus_artwork_path("sdmc:/switch/wine/../a.PNG"));
    assert(!exodus_artwork_path("sdmc:/switch/wine//a.png"));
    assert(!exodus_artwork_path("sdmc:/switch/wine/folder.PNG/cover"));
    assert(!exodus_artwork_path("sdmc:/switch/wine/cover.PNG.bak"));
    assert(!exodus_artwork_path("sdmc:/ulaunch/a.exe"));
    assert(!exodus_artwork_path("https://example.org/a.png"));
    assert(!exodus_artwork_path("sdmc:/other/a.png"));
    return 0;
}
