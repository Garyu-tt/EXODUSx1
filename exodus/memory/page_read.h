#ifndef EXODUS_PAGE_READ_H
#define EXODUS_PAGE_READ_H
#include "page_dedup_store.h"
/* Read one 4-byte word without materializing the page. Output is in stored
 * byte order; invalid records or indices leave it unchanged. Keep output
 * separate from all backing buffers and serialize access with writers. */
bool exodus_page_read_word(const uint8_t *encoded, size_t size, size_t word,
                           uint8_t out[4]);
bool exodus_page_store_read_word(const struct exodus_page_store *store,
                                 size_t index, size_t word, uint8_t out[4]);
bool exodus_page_dedup_store_read_word(const struct exodus_page_dedup_store *store,
                                       size_t index, size_t word, uint8_t out[4]);
#endif
