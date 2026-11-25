#ifndef EXODUS_PAGE_CODEC_H
#define EXODUS_PAGE_CODEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { EXODUS_PAGE_BYTES = 4096 };
enum { EXODUS_PAGE_REPEATED_TILE_VERSION = 2, EXODUS_PAGE_BDI_VERSION = 3 };

/* Supply two separate buffers of EXODUS_PAGE_BYTES bytes. Retain only the
 * returned number of encoded bytes and record that length with the page. */
size_t exodus_page_encode(const uint8_t *page, uint8_t *encoded);

/* As above, also trying full-page 8-/16-byte tile repeats 32-bit word runs and BDI blocks.
 * Decode adaptive output with this version of exodus_page_decode. */
size_t exodus_page_encode_adaptive(const uint8_t *page, uint8_t *encoded);

/* Supply encoded_size readable bytes and a separate writable page buffer.
 * Invalid encodings leave the page untouched. */
bool exodus_page_decode(const uint8_t *encoded, size_t encoded_size, uint8_t *page);

/* Read a word from a validated V3 BDI page without full decoding. */
bool exodus_page_bdi_read_word(const uint8_t *encoded, size_t size, size_t word, uint8_t out[4]);

#endif
