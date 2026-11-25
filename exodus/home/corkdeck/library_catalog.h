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
/* Read-only inventory; discovered paths have no execution backend. */
struct exodus_library *exodus_library_open_with_games(const char *path,const char *root);
const char *exodus_library_discovery_status(const struct exodus_library *library);
int exodus_library_is_discovered(const struct exodus_library *library,size_t index);
/* Informational owner-declared grouping only; raw file indices are preserved. */
size_t exodus_library_group_representative(const struct exodus_library *library,size_t index);
size_t exodus_library_group_size(const struct exodus_library *library,size_t index);
/* Preference identity only, zero for undeclared or rejected groups. */
uint64_t exodus_library_declared_group_id(const struct exodus_library *library,size_t index);
/* Read-only native NS metadata; never an entitlement or launch gate. */
int exodus_library_native_detail(const struct exodus_library *library,size_t index,char *out,size_t cap);
/* Selected discovered file diagnostics only; never returns launch permission. */
int exodus_library_archive_detail(const struct exodus_library *library,size_t index,char *out,size_t cap);
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
