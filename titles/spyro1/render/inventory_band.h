// inventory_band.h — the shaded band behind each inventory row: a semi-transparent POLY_F4 the
// menu draw 0x8001A40C writes inline at x D_800757CC to D_800757CC + 0x1FF and links at 0x8001BEC0
// into the overlay pair (0x800168DC appends to the list at [0x8007581C]). On a widened canvas a
// band from column 0 has its right edge moved onto the buffer's, so the record's margin rule
// continues it across the canvas with no unbanded column at 511.
#pragma once

#include <cstdint>

class Core;

namespace spyro1::inventory_band {

// Return address of the band's AddPrimToOT.
inline constexpr std::uint32_t kLinkReturn = 0x8001BEC8u;

// AddPrimToOT 0x800168DC linked `packet`: when the call is the band's on a widened canvas, its
// right edge.
void linked(Core &core, std::uint32_t packet);

} // namespace spyro1::inventory_band
