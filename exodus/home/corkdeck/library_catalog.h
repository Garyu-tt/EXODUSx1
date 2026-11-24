/* Read-only C boundary for the existing EXODUS catalog parser. */
#ifndef EXODUS_LIBRARY_CATALOG_H
#define EXODUS_LIBRARY_CATALOG_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
struct exodus_library;
struct exodus_library *exodus_library_open(const char *path);
void exodus_library_close(struct exodus_library *library);
const char *exodus_library_status(const struct exodus_library *library);
size_t exodus_library_count(const struct exodus_library *library);
const char *exodus_library_title(const struct exodus_library *library, size_t index);
const char *exodus_library_provider(const struct exodus_library *library, size_t index);
#ifdef __cplusplus
}
#endif
#endif
