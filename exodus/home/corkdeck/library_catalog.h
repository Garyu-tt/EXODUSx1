/* Read-only C boundary for the existing EXODUS catalog parser. */
#ifndef EXODUS_LIBRARY_CATALOG_H
#define EXODUS_LIBRARY_CATALOG_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct exodus_library;
struct exodus_library *exodus_library_open(const char *path);
/* Optional read-only Games inventory, entries remain nonlaunchable Other. */
struct exodus_library *exodus_library_open_with_games(const char *path, const char *root);
const char *exodus_library_native_inventory_status(const struct exodus_library *library);
const char *exodus_library_discovery_status(const struct exodus_library *library);
/* Read-only on-demand Horizon metadata for registered title candidates. */
int exodus_library_native_detail(const struct exodus_library *library, size_t index, char *out, size_t cap);
/* Probe selected discovered NSP directory; never authorizes launch. */
int exodus_library_archive_detail(const struct exodus_library *library, size_t index, char *out, size_t cap);
/* Grouped presentation only. Raw index space and launch policy are unchanged. */
size_t exodus_library_group_representative(const struct exodus_library *library, size_t index);
/* Stable owner-declared group key; never a launch ID. Returns 0 for orphan/unclassified. */
int exodus_library_declared_group_key(const struct exodus_library *library, size_t index, char *out, size_t cap);
int exodus_library_is_group_card(const struct exodus_library *library, size_t index);
int exodus_library_is_discovered(const struct exodus_library *library, size_t index);
size_t exodus_library_group_size(const struct exodus_library *library, size_t index);
void exodus_library_close(struct exodus_library *library);
const char *exodus_library_status(const struct exodus_library *library);
size_t exodus_library_count(const struct exodus_library *library);
const char *exodus_library_id(const struct exodus_library *library, size_t index);
const char *exodus_library_artwork(const struct exodus_library *library, size_t index);
uint64_t exodus_library_last_played(const struct exodus_library *library, size_t index);
const char *exodus_library_title(const struct exodus_library *library, size_t index);
const char *exodus_library_provider(const struct exodus_library *library, size_t index);
#ifdef __cplusplus
}
#endif
#endif
