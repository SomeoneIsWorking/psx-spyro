// inventory_band.h — the shaded band behind each inventory row as a state producer: a
// semi-transparent POLY_F4 the inventory handler 0x8001A40C writes inline at x D_800757CC to
// D_800757CC + 0x1FF and links at 0x8001BEC0 into the overlay pair (0x800168DC appends to the list
// at [0x8007581C]). It renders the guest's quad; on a widened canvas a band written across the
// screen from column 0 continues across the canvas.
#pragma once

#include "state_producer.h"

#include <cstdint>

class Core;

namespace spyro1::inventory_band {

class InventoryBandProducer final : public psx::present::StateProducer {
public:
  // The producer id: the handler that draws the band.
  static constexpr std::uint32_t kHandler = 0x8001A40Cu;
  // Return address of the band's AddPrimToOT.
  static constexpr std::uint32_t kLinkReturn = 0x8001BEC8u;

  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;
};

// AddPrimToOT 0x800168DC linked `packet`: when the call is the band's, key the packet and save what
// it draws.
void linked(Core &core, std::uint32_t packet);

void registerProducer(Core &core);

} // namespace spyro1::inventory_band
