// spyro1_ordering_table.h — the slot each packet of a Spyro 1 frame was linked into, named before
// LinkOTPrimitives flattens the table.
//
// SCUS_942.28 links packets two ways: AddPrimToOT 0x800168DC appends to the (head, first) pair
// at [0x8007581C], the screen overlays' pair, and 0x800168A0 to a depth bin of the table at
// [0x80075820]. LinkOTPrimitives 0x80016784($a0 bins) chains the bins deepest first, then the
// overlay pair, into the one list DrawOTag walks.
#pragma once

#include <cstdint>

class Core;

namespace spyro1 {

class OrderingTable {
public:
  static constexpr std::uint32_t kFlatten = 0x80016784u;
  static constexpr std::uint32_t kBinTablePointer = 0x80075820u;
  static constexpr std::uint32_t kOverlayPairPointer = 0x8007581Cu;
  // `core.otTables` ids: the depth bins share Spyro 2/3's table 0.
  static constexpr std::uint16_t kOverlayTable = 1;

  // Names both tables and assigns every linked packet its slot.
  static void assignSlots(Core &core, std::uint32_t bins);
  static void registerOverrides(Core &core);
};

} // namespace spyro1
