#include "page_snapshot.h"

#include <string.h>

enum { STANDALONE = 0, REFERENCE_XOR = 1, CRC_BYTES = 4 };

static uint32_t page_crc32(const uint8_t *page)
{
    /* Reflected IEEE CRC-32, little-endian on the wire. */
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < EXODUS_PAGE_BYTES; ++i) {
        crc ^= page[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? UINT32_C(0xedb88320) : 0);
    }
    return ~crc;
}

size_t exodus_snapshot_encode(const uint8_t *page, const uint8_t *reference,
                              uint8_t *frame)
{
    frame[0] = STANDALONE;
    size_t full_size = exodus_page_encode_adaptive(page, frame + 1);
    if (reference == NULL)
        return 1 + full_size;

    uint8_t delta[EXODUS_PAGE_BYTES], encoded_delta[EXODUS_PAGE_BYTES];
    for (size_t i = 0; i < sizeof(delta); ++i)
        delta[i] = page[i] ^ reference[i];
    size_t delta_size = exodus_page_encode_adaptive(delta, encoded_delta);
    if (CRC_BYTES + delta_size >= full_size)
        return 1 + full_size;

    frame[0] = REFERENCE_XOR;
    uint32_t crc = page_crc32(reference);
    for (unsigned i = 0; i < CRC_BYTES; ++i)
        frame[1 + i] = (uint8_t)(crc >> (8 * i));
    memcpy(frame + 1 + CRC_BYTES, encoded_delta, delta_size);
    return 1 + CRC_BYTES + delta_size;
}

bool exodus_snapshot_decode(const uint8_t *frame, size_t size,
                            const uint8_t *reference, uint8_t *page)
{
    if (frame == NULL || page == NULL || size < 2 || size > EXODUS_SNAPSHOT_MAX_BYTES)
        return false;
    if (frame[0] == STANDALONE)
        return exodus_page_decode(frame + 1, size - 1, page);
    if (frame[0] != REFERENCE_XOR || size <= 1 + CRC_BYTES || reference == NULL)
        return false;

    uint32_t expected = 0;
    for (unsigned i = 0; i < CRC_BYTES; ++i)
        expected |= (uint32_t)frame[1 + i] << (8 * i);
    if (page_crc32(reference) != expected)
        return false;

    uint8_t delta[EXODUS_PAGE_BYTES];
    if (!exodus_page_decode(frame + 1 + CRC_BYTES, size - 1 - CRC_BYTES, delta))
        return false;
    for (size_t i = 0; i < sizeof(delta); ++i)
        page[i] = reference[i] ^ delta[i];
    return true;
}
