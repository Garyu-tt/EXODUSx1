// SPDX-License-Identifier: GPL-2.0-only
#include "loader_nro_bounds.hpp"

#include <limits>

using exodus::home::NroLoadSegment;
using exodus::home::NroMappedSize;

constexpr std::size_t prefix = 0x80;
constexpr std::size_t image = 0x3800;
constexpr std::size_t heap = 0x5000;
constexpr std::array<NroLoadSegment, 3> segments{{{0, 0x1000}, {0x1000, 0x1000}, {0x2000, 0x1800}}};

static_assert(NroMappedSize(segments, prefix, image, 0, heap) == 0x4000);
static_assert(NroMappedSize(segments, prefix, image, 0x1000, heap) == heap);
static_assert(!NroMappedSize(segments, prefix, prefix, 0, heap));
static_assert(!NroMappedSize(segments, prefix, image, 0, image - 1));
static_assert(!NroMappedSize(segments, prefix, image, 0x1801, heap));
static_assert(!NroMappedSize(segments, prefix, image, 0x1000, heap - 1));
static_assert(!NroMappedSize(segments, prefix, image, 0, image));
static_assert(!NroMappedSize(segments, prefix, image + 1, 0, heap));
static_assert(!NroMappedSize(segments, prefix, image - 1, 0, heap));
static_assert(!NroMappedSize(segments, prefix, image, std::numeric_limits<std::size_t>::max(), heap));
static_assert(!NroMappedSize(segments, prefix, image,
    std::numeric_limits<std::size_t>::max() - image, std::numeric_limits<std::size_t>::max()));
static_assert(!NroMappedSize({{{0, 0x1000}, {0x1000, 0}, {0x2000, 0x1800}}}, prefix, image, 0, heap));
static_assert(!NroMappedSize({{{0, 0x1000}, {0x1200, 0x1000}, {0x2200, 0x1800}}}, prefix, image, 0, heap));

int main() {}
