#include "page_codec.h"

#include <string.h>

enum {
    BLOCK_BYTES = 64,
    BLOCKS = EXODUS_PAGE_BYTES / BLOCK_BYTES,
    MASK_BYTES = BLOCKS / 8
};

size_t exodus_page_encode(const uint8_t *page, uint8_t *encoded)
{
    static const uint8_t zero[BLOCK_BYTES] = {0};
    uint8_t mask[MASK_BYTES] = {0};
    size_t used = 0;

    for (size_t i = 0; i < BLOCKS; ++i) {
        if (memcmp(page + i * BLOCK_BYTES, zero, BLOCK_BYTES) != 0) {
            mask[i / 8] |= (uint8_t)(1u << (i % 8));
            ++used;
        }
    }

    if (used == BLOCKS) {
        memcpy(encoded, page, EXODUS_PAGE_BYTES);
        return EXODUS_PAGE_BYTES;
    }

    memcpy(encoded, mask, MASK_BYTES);
    size_t offset = MASK_BYTES;
    for (size_t i = 0; i < BLOCKS; ++i) {
        if (mask[i / 8] & (1u << (i % 8))) {
            memcpy(encoded + offset, page + i * BLOCK_BYTES, BLOCK_BYTES);
            offset += BLOCK_BYTES;
        }
    }
    return offset;
}

bool exodus_page_decode(const uint8_t *encoded, size_t encoded_size, uint8_t *page)
{
    if (encoded_size == EXODUS_PAGE_BYTES) {
        memcpy(page, encoded, EXODUS_PAGE_BYTES);
        return true;
    }
    if (encoded_size < MASK_BYTES || encoded_size > EXODUS_PAGE_BYTES ||
        (encoded_size - MASK_BYTES) % BLOCK_BYTES != 0) {
        return false;
    }

    size_t expected = MASK_BYTES;
    for (size_t i = 0; i < MASK_BYTES; ++i) {
        unsigned bits = encoded[i];
        while (bits != 0) {
            bits &= bits - 1;
            expected += BLOCK_BYTES;
        }
    }
    if (expected != encoded_size) {
        return false;
    }

    size_t offset = MASK_BYTES;
    for (size_t i = 0; i < BLOCKS; ++i) {
        uint8_t *block = page + i * BLOCK_BYTES;
        if (encoded[i / 8] & (1u << (i % 8))) {
            memcpy(block, encoded + offset, BLOCK_BYTES);
            offset += BLOCK_BYTES;
        } else {
            memset(block, 0, BLOCK_BYTES);
        }
    }
    return true;
}
