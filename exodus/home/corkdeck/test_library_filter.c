#include "library_filter.h"
#include <assert.h>
int main(void)
{
    assert(exodus_filter_matches(FILTER_ALL, "Switch", 0, 0));
    assert(!strcmp(exodus_filter_name(FILTER_FILES), "Game files"));
    assert(exodus_filter_matches(FILTER_SWITCH, "Switch", 0, 0));
    assert(!exodus_filter_matches(FILTER_SWITCH, "Steam", 1, 1));
    assert(exodus_filter_matches(FILTER_FAVORITES, "Steam", 1, 0));
    assert(!exodus_filter_matches(FILTER_FAVORITES, "Steam", 0, 9));
    assert(exodus_filter_matches(FILTER_RECENT, "Steam", 0, 1));
    assert(exodus_section_matches(SECTION_APPLICATIONS,"Homebrew"));
    assert(!exodus_section_matches(SECTION_LIBRARY,"Homebrew"));
    assert(exodus_section_matches(SECTION_LIBRARY,"Switch"));
    assert(exodus_section_matches(SECTION_LIBRARY,"Other"));
    assert(exodus_section_matches(SECTION_LIBRARY,"Steam"));
    assert(!exodus_section_matches(SECTION_APPLICATIONS,"Switch"));
    assert(!exodus_section_matches(SECTION_APPLICATIONS,"Other"));
    assert(!exodus_section_matches(SECTION_LIBRARY,NULL));
    assert(exodus_section_next_filter(SECTION_LIBRARY,FILTER_SWITCH)==FILTER_STEAM);
    assert(exodus_section_next_filter(SECTION_APPLICATIONS,FILTER_ALL)==FILTER_FAVORITES);
    assert(exodus_section_next_filter(SECTION_APPLICATIONS,FILTER_FAVORITES)==FILTER_RECENT);
    assert(exodus_section_next_filter(SECTION_APPLICATIONS,FILTER_RECENT)==FILTER_ALL);
    assert(!exodus_section_filter_allowed(SECTION_APPLICATIONS,FILTER_FILES));
    struct exodus_card_index items[] = {{0, 10}, {1, 100}, {2, 100}};
    qsort(items, 3, sizeof(items[0]), exodus_recent_compare);
    assert(items[0].index == 1 && items[1].index == 2 && items[2].index == 0);
    assert(exodus_artwork_path("sdmc:/switch/wine/covers/a.png"));
    assert(exodus_artwork_path("sdmc:/ulaunch/a.jpeg"));
    assert(!exodus_artwork_path("sdmc:/switch/wine/../a.png"));
    assert(!exodus_artwork_path("sdmc:/switch/wine//a.png"));
    assert(!exodus_artwork_path("sdmc:/ulaunch/a.exe"));
    assert(!exodus_artwork_path("https://example.org/a.png"));
    assert(!exodus_artwork_path("sdmc:/other/a.png"));
    return 0;
}
