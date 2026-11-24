// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <array>
#include <cstddef>
#include <optional>

namespace exodus::home {

    inline constexpr std::size_t NroPageBytes = 0x1000;

    struct NroLoadSegment {
        std::size_t offset;
        std::size_t length;
    };

    // The image includes the three file-backed segments. The final mapping also
    // includes BSS and page padding, all of which must fit in the allocated heap.
    inline constexpr std::optional<std::size_t> NroMappedSize(
        const std::array<NroLoadSegment, 3> &segments, std::size_t prefix_size,
        std::size_t image_size, std::size_t bss_size, std::size_t heap_size) {
        if (image_size <= prefix_size || image_size > heap_size || bss_size > heap_size - image_size
            || segments.front().length < prefix_size)
            return std::nullopt;

        std::size_t offset = 0;
        for (const auto &segment : segments) {
            if (segment.offset != offset || offset % NroPageBytes != 0 || segment.length == 0
                || segment.length > image_size - offset)
                return std::nullopt;
            offset += segment.length;
        }
        if (offset != image_size)
            return std::nullopt;

        const auto occupied = image_size + bss_size;
        const auto padding = (NroPageBytes - occupied % NroPageBytes) % NroPageBytes;
        if (padding > heap_size - occupied)
            return std::nullopt;
        return occupied + padding;
    }

}
