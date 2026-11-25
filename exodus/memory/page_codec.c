#include "page_codec.h"

#include <string.h>

enum {
    BLOCK_BYTES = 64,
    BLOCKS = EXODUS_PAGE_BYTES / BLOCK_BYTES,
    MASK_BYTES = BLOCKS / 8,
    WORD_BYTES = 4,
    PAGE_WORDS = EXODUS_PAGE_BYTES / WORD_BYTES,
    WORD_RUN_VERSION = 1,
    WORD_RUN_REPEAT = 128,
    WORD_RUN_MAX = 128
};

static bool next_word_matches(const uint8_t *page, size_t word)
{
    return word + 1 < PAGE_WORDS &&
           memcmp(page + word * WORD_BYTES, page + (word + 1) * WORD_BYTES, WORD_BYTES) == 0;
}

static size_t encode_word_runs(const uint8_t *page, uint8_t *encoded, size_t fallback_size)
{
    size_t offset = 1;
    encoded[0] = WORD_RUN_VERSION;

    for (size_t word = 0; word < PAGE_WORDS;) {
        bool repeated = next_word_matches(page, word);
        size_t count = 1;
        if (repeated) {
            count = 2;
            while (count < WORD_RUN_MAX && next_word_matches(page, word + count - 1))
                ++count;
        } else {
            while (count < WORD_RUN_MAX && word + count < PAGE_WORDS &&
                   !next_word_matches(page, word + count))
                ++count;
        }

        size_t payload_size = repeated ? WORD_BYTES : count * WORD_BYTES;
        if (1 + payload_size >= fallback_size - offset)
            return 0;
        uint8_t control = (uint8_t)(count - 1);
        if (repeated)
            control |= WORD_RUN_REPEAT;
        encoded[offset++] = control;
        memcpy(encoded + offset, page + word * WORD_BYTES, payload_size);
        offset += payload_size;
        word += count;
    }

    /* Sparse and raw lengths are even. Padding can never reach the even
     * fallback size because the unpadded size is already smaller. */
    if (offset % 2 == 0)
        encoded[offset++] = 0;
    return offset;
}

static size_t encode_page(const uint8_t *page, uint8_t *encoded, bool adaptive)
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

    size_t fallback_size = used == BLOCKS ? EXODUS_PAGE_BYTES : MASK_BYTES + used * BLOCK_BYTES;
    if (adaptive && used != 0) {
        size_t size = encode_word_runs(page, encoded, fallback_size);
        if (size != 0)
            return size;
    }

    if (used == BLOCKS) {
        memcpy(encoded, page, EXODUS_PAGE_BYTES);
        return fallback_size;
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

size_t exodus_page_encode(const uint8_t *page, uint8_t *encoded)
{
    return encode_page(page, encoded, false);
}

size_t exodus_page_encode_adaptive(const uint8_t *page, uint8_t *encoded)
{
    return encode_page(page, encoded, true);
}

static void restore_repeated_words(uint8_t *destination, const uint8_t *source, size_t count)
{
    uint32_t word;
    /* Keep the stored byte order and allow unaligned source and destination. */
    memcpy(&word, source, sizeof(word));
    const uint32_t block[] = {word, word, word, word};
    const size_t block_words = sizeof(block) / sizeof(word);
    while (count >= block_words) {
        memcpy(destination, block, sizeof(block));
        destination += sizeof(block);
        count -= block_words;
    }
    while (count > 0) {
        memcpy(destination, &word, sizeof(word));
        destination += sizeof(word);
        --count;
    }
}

static bool read_word_runs(const uint8_t *encoded, size_t encoded_size, uint8_t *page)
{
    if (encoded[0] != WORD_RUN_VERSION)
        return false;

    size_t offset = 1;
    for (size_t word = 0; word < PAGE_WORDS;) {
        if (offset == encoded_size)
            return false;
        uint8_t control = encoded[offset++];
        size_t count = (control & (WORD_RUN_MAX - 1)) + 1;
        bool repeated = (control & WORD_RUN_REPEAT) != 0;
        size_t payload_size = repeated ? WORD_BYTES : count * WORD_BYTES;
        if ((repeated && count < 2) || count > PAGE_WORDS - word ||
            payload_size > encoded_size - offset)
            return false;

        if (page) {
            uint8_t *destination = page + word * WORD_BYTES;
            if (repeated)
                restore_repeated_words(destination, encoded + offset, count);
            else
                memcpy(destination, encoded + offset, payload_size);
        }
        offset += payload_size;
        word += count;
    }

    return offset == encoded_size ||
           (offset % 2 == 0 && offset + 1 == encoded_size && encoded[offset] == 0);
}

bool exodus_page_decode(const uint8_t *encoded, size_t encoded_size, uint8_t *page)
{
    if (encoded_size > EXODUS_PAGE_BYTES)
        return false;
    if (encoded_size == EXODUS_PAGE_BYTES) {
        memcpy(page, encoded, EXODUS_PAGE_BYTES);
        return true;
    }
    if (encoded_size % 2 != 0) {
        /* Validate all commands and padding before touching the output page. */
        return read_word_runs(encoded, encoded_size, NULL) &&
               read_word_runs(encoded, encoded_size, page);
    }
    if (encoded_size <= MASK_BYTES) {
        static const uint8_t zero_mask[MASK_BYTES] = {0};
        if (encoded_size != MASK_BYTES || memcmp(encoded, zero_mask, MASK_BYTES) != 0)
            return false;
        memset(page, 0, EXODUS_PAGE_BYTES);
        return true;
    }
    if ((encoded_size - MASK_BYTES) % BLOCK_BYTES != 0)
        return false;

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
