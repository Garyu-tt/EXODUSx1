/* Offline library presentation policy. No networking or filesystem writes. */
#ifndef EXODUS_LIBRARY_FILTER_H
#define EXODUS_LIBRARY_FILTER_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

enum exodus_filter { FILTER_ALL, FILTER_SWITCH, FILTER_HOMEBREW, FILTER_STEAM,
                     FILTER_WINDOWS, FILTER_OTHER, FILTER_FAVORITES, FILTER_RECENT, FILTER_COUNT };

static inline const char *exodus_filter_name(enum exodus_filter filter)
{
    static const char *const names[] = {"All", "Switch", "Homebrew", "Steam",
        "CorkDeck", "Other", "Favorites", "Recently played"};
    return filter >= 0 && filter < FILTER_COUNT ? names[filter] : "All";
}

static inline int exodus_filter_matches(enum exodus_filter filter, const char *provider,
                                        int favorite, uint64_t last_played)
{
    if (filter == FILTER_ALL) return 1;
    if (filter == FILTER_FAVORITES) return favorite != 0;
    if (filter == FILTER_RECENT) return last_played != 0;
    return !strcmp(exodus_filter_name(filter), provider);
}

struct exodus_card_index { size_t index; uint64_t last_played; };
static inline int exodus_recent_compare(const void *a, const void *b)
{
    const struct exodus_card_index *x = a, *y = b;
    if (x->last_played != y->last_played) return x->last_played > y->last_played ? -1 : 1;
    return (x->index > y->index) - (x->index < y->index);
}

/* Allow only existing artwork paths on known SD roots. Do not interpret URLs
 * from catalogs or permit traversal. On host these are relative fixture paths. */
static inline int exodus_artwork_path(const char *path)
{
    if (!path || strlen(path) >= 512) return 0;
    const char *segment;
    if (!strncmp(path, "sdmc:/switch/wine/", 18)) segment = path + 18;
    else if (!strncmp(path, "sdmc:/ulaunch/", 14)) segment = path + 14;
    else return 0;
    for (const char *p = segment; ; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c == '/' || c == 0) {
            size_t n = (size_t)(p - segment);
            if (!n || (n == 1 && segment[0] == '.') ||
                (n == 2 && segment[0] == '.' && segment[1] == '.')) return 0;
            if (!c) break;
            segment = p + 1;
        } else if (c < 32 || c == 127 || c == '\\' || c == ':' || c == '"') return 0;
    }
    size_t len = strlen(path);
    return (len > 4 && (!strcmp(path + len - 4, ".png") || !strcmp(path + len - 4, ".jpg"))) ||
           (len > 5 && !strcmp(path + len - 5, ".jpeg"));
}
#endif
