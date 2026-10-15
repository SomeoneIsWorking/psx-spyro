// guest_depth_bins.h — which depth bin of the ordering table each packet of the frame was linked
// into, taken before the guest flattens the bins into one chain.
//
// The flatten (SCUS_942.28 0x80016784, SCUS_944.25 FUN_8001B2A8, SCUS_944.67 FUN_8004E664, one
// body) concatenates the table's 8-byte (head, first) bins, deepest first, into the single chain
// DrawOTag walks, so after it the bins are gone. Each title's flatten override reads them first and
// hands each packet its bin through `core.otTables`: terrain, mobys and HUD alike.
#pragma once

#include "frame_record.h"
#include "render/field/guest_render_globals.h"

#include <cstdint>

class Core;

namespace spyro::depth_bins {

// The ordering table's name in `core.otTables`; the terrain producer emits into its bins.
inline constexpr std::uint16_t kTable = 0;
// 512 (head, first) word pairs; a bin's chain starts at its first-linked packet.
inline constexpr std::uint32_t kBins = 512;
inline constexpr std::uint32_t kBinStride = 8;
inline constexpr std::uint32_t kBinFirstOffset = 4;
inline constexpr std::uint32_t kTableBytes = kBins * kBinStride;

// One ordering table's (head, first) pairs: `named` from `base`, the first `linked` of them holding
// this frame's packets.
struct Bins {
  std::uint16_t table = kTable;
  std::uint32_t base = 0;
  std::uint32_t named = 0;
  std::uint32_t linked = 0;
};

// The S2/S3 table at the base the guest published, linked up to its mark.
[[nodiscard]] Bins publishedBins(Core &core, const guest_render_globals::Globals &globals);

// Names the table in `core.otTables` and gives every packet linked into a bin that bin's slot.
void assignBins(Core &core, const Bins &bins);

} // namespace spyro::depth_bins
