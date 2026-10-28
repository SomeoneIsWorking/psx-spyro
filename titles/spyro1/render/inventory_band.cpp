#include "spyro1/render/inventory_band.h"

#include "core.h"
#include "core/session.h"
#include "emission_scope.h"
#include "frame_state.h"
#include "gp0_primitive_decode.h"
#include "render/field/record_blend.h"
#include "render/terrain/guest_depth_bins.h"
#include "spyro1/render/shaded_line.h"

#include <lucent/log.h>

#include <array>
#include <cstdlib>
#include <memory>
#include <optional>
#include <type_traits>

namespace spyro1::inventory_band {
namespace {

// The POLY_F4 after its tag: colour with the command, then the corners left-top, right-top,
// left-bottom, right-bottom.
using BandWords = std::array<std::uint32_t, 5>;
static_assert(std::is_trivially_copyable_v<BandWords>);

constexpr std::uint32_t kScreenWidth = 0x200;

// A corner word with its x moved to the band's left edge plus `offset`.
std::uint32_t atColumn(std::uint32_t corner, std::uint32_t left, std::uint32_t offset) {
  return (corner & 0xFFFF0000u) | ((left + offset) & 0xFFFFu);
}

} // namespace

void linked(Core &core, std::uint32_t packet) {
  if (core.r[31] != InventoryBandProducer::kLinkReturn) {
    return;
  }
  const psx::present::EmissionScope::Guard scope(
      core.emission, InventoryBandProducer::kHandler, shaded_line::callObject(core), 0);
  core.emission.bindPacket(packet);
  BandWords words{};
  for (std::uint32_t word = 0; word < words.size(); ++word) {
    words[word] = core.mem_r32(packet + 4u * (word + 1u));
  }
  core.frameStates.save(core.emission.current(), words);
}

void InventoryBandProducer::render(const psx::present::StateInterval &states,
                                   psx::present::PrimitiveSink &sink) const {
  const BandWords before = psx::present::stateAs<BandWords>(states.from);
  BandWords shown = psx::present::stateAs<BandWords>(states.to);
  shown[0] = spyro::record_blend::rgbBetween(before[0], shown[0], states.t);
  for (std::uint32_t corner = 1; corner < shown.size(); ++corner) {
    shown[corner] = psx::present::lerpHalves(before[corner], shown[corner], states.t);
  }
  const std::uint32_t left = shown[1] & 0xFFFFu;
  shown[2] = atColumn(shown[2], left, kScreenWidth);
  shown[4] = atColumn(shown[4], left, kScreenWidth);
  std::optional<psx::present::DrawPrimitive> primitive =
      psx::gpu::decodePacketPrimitive({.words = shown, .places = {}});
  if (!primitive) {
    lucent::error("inventory-band", "the saved word 0x{:08X} is not a polygon", shown[0]);
    std::abort();
  }
  sink.emit(spyro::depth_bins::kOverlaySlot, *primitive);
}

void registerProducer(Core &core) {
  spyro::session(core).stateProducers.install(InventoryBandProducer::kHandler,
                                              std::make_unique<InventoryBandProducer>());
}

} // namespace spyro1::inventory_band
