// guest_depth_bins.h — which depth bin of the ordering table each packet of the frame was linked
// into, taken before the guest flattens the bins into one chain.
//
// The flatten (SCUS_942.28 0x80016784, SCUS_944.25 FUN_8001B2A8, SCUS_944.67 FUN_8004E664, one
// body) concatenates the table's 8-byte (head, first) bins, deepest first, into the single chain
// DrawOTag walks, so after it the bins are gone. Each title's flatten override reads them first and
// hands each packet its bin through `core.otTables`: terrain, mobys and HUD alike.
#pragma once

#include "emit_memory.h"
#include "frame_record.h"
#include "render/field/guest_render_globals.h"

#include <cstdint>

class Core;

namespace spyro::depth_bins {

// The ordering table's name in `core.otTables`; the terrain producer emits into its bins.
inline constexpr std::uint16_t kTable = 0;
// The overlay pair's: one slot, drawn after every bin.
inline constexpr std::uint16_t kOverlayTable = 1;
inline constexpr psx::present::OtSlot kOverlaySlot{kOverlayTable, 0};
// A second pass's table: the same bins refilled after the first flatten, drawn after its chain.
inline constexpr std::uint16_t kSecondPassTable = 2;
// 0x580 (head, first) word pairs, the count both draws clear and ask the flatten for; a bin's chain
// starts at its first-linked packet.
inline constexpr std::uint32_t kBins = 0x580;
inline constexpr std::uint32_t kBinStride = 8;
inline constexpr std::uint32_t kBinFirstOffset = 4;

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

// The overlay pair the guest published.
[[nodiscard]] Bins overlayBins(Core &core, const guest_render_globals::Globals &globals);

// The packets below `below` that no other producer keyed, which the walk keys to the open producer
// scope.
struct Claim {
  std::uint32_t below = 0;
};

// Names the table in `core.otTables` and gives every packet linked into a bin that bin's slot.
void assignBins(Core &core, const Bins &bins, Claim claim = {});

// assignBins for the overlay pair, which the flatten chains after `published`'s bins. When it holds
// no packet, its slot is placed after the last of them, where its packets would have been drawn.
void assignOverlay(Core &core, const Bins &published, const Bins &overlay, Claim claim = {});

// Links the packets `first`..`last` as the new head of the (head, first) pair at `slot`; returns
// the old head.
std::uint32_t linkIntoBin(const psx::present::EmitMemory &memory,
                          std::uint32_t slot,
                          std::uint32_t first,
                          std::uint32_t last);

} // namespace spyro::depth_bins
