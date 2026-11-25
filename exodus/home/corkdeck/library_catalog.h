// SPDX-License-Identifier: GPL-2.0-only
/* Read-only C boundary for the existing EXODUS catalog parser. */
#ifndef EXODUS_LIBRARY_CATALOG_H
#define EXODUS_LIBRARY_CATALOG_H
#include <stddef.h>
#include <stdint.h>
#include "launch_route.h"
#ifdef __cplusplus
extern "C" {
#endif
struct exodus_library;
struct exodus_library *exodus_library_open(const char *path);
void exodus_library_close(struct exodus_library *library);
const char *exodus_library_status(const struct exodus_library *library);
size_t exodus_library_count(const struct exodus_library *library);
const char *exodus_library_id(const struct exodus_library *library, size_t index);
const char *exodus_library_artwork(const struct exodus_library *library, size_t index);
uint64_t exodus_library_last_played(const struct exodus_library *library, size_t index);
const char *exodus_library_title(const struct exodus_library *library, size_t index);
/* Presentation and execution are independent. Target selection is not authorization. */
struct exodus_execution_target exodus_library_execution_target(const struct exodus_library *library, size_t index);
const char *exodus_library_provider(const struct exodus_library *library, size_t index);
#ifdef __cplusplus
}
#endif
#endif
