#include "spyro1/render/shaded_line.h"

#include "core.h"
#include "core/session.h"
#include "emission_scope.h"
#include "frame_state.h"
#include "gp0_primitive_decode.h"
#include "native_dispatch.h"
#include "render/field/record_blend.h"
#include "render/terrain/guest_depth_bins.h"
#include "spyro1/render/terrain_facts.h"

#include <lucent/log.h>

#include <array>
#include <cstdlib>
#include <memory>
#include <optional>
#include <type_traits>

namespace spyro1::shaded_line {
namespace {

// The LINE_G2 after its tag: colour 0 with the command, xy 0, colour 1, xy 1.
using LineWords = std::array<std::uint32_t, 4>;
static_assert(std::is_trivially_copyable_v<LineWords>);

constexpr std::uint32_t kColourMask = 0x00FFFFFFu;

// Screen positions fit 16 bits each.
constexpr std::uint32_t pointKey(std::uint32_t x, std::uint32_t y) {
  return (x & 0xFFFFu) | (y << 16);
}

void drawLine(Core *core) {
  Core &guest = *core;
  const std::uint32_t packet = guest.mem_r32(terrain::kRenderGlobals.primitiveCursor);
  const psx::present::EmissionScope::Guard scope(guest.emission,
                                                 ShadedLineProducer::kEntry,
                                                 pointKey(guest.r[4], guest.r[5]),
                                                 pointKey(guest.r[6], guest.r[7]));
  if (!psx::cpu::callOriginalOrPropagate(guest, ShadedLineProducer::kEntry)) {
    return;
  }
  LineWords words{};
  for (std::uint32_t word = 0; word < words.size(); ++word) {
    words[word] = guest.mem_r32(packet + 4u * (word + 1u));
  }
  guest.frameStates.save(guest.emission.current(), words);
}

std::uint32_t colourBetween(std::uint32_t from, std::uint32_t to, float t) {
  std::uint32_t shown = to & ~kColourMask;
  for (std::uint32_t shift = 0; shift < 24; shift += 8) {
    const auto channel = spyro::record_blend::byteBetween(
        static_cast<std::uint8_t>(from >> shift), static_cast<std::uint8_t>(to >> shift), t);
    shown |= static_cast<std::uint32_t>(channel) << shift;
  }
  return shown;
}

} // namespace

psx::present::StateProducer::Composition
ShadedLineProducer::composition(const psx::present::StateInterval &states) const {
  return psx::present::stateAs<LineWords>(states.from) ==
                 psx::present::stateAs<LineWords>(states.to)
             ? Composition::Recorded
             : Composition::Render;
}

void ShadedLineProducer::render(const psx::present::StateInterval &states,
                                psx::present::PrimitiveSink &sink) const {
  const LineWords before = psx::present::stateAs<LineWords>(states.from);
  LineWords shown = psx::present::stateAs<LineWords>(states.to);
  shown[0] = colourBetween(before[0], shown[0], states.t);
  shown[2] = colourBetween(before[2], shown[2], states.t);
  std::optional<psx::present::DrawPrimitive> primitive =
      psx::gpu::decodePacketPrimitive({.words = shown, .places = {}});
  if (!primitive) {
    lucent::error("shaded-line", "the saved word 0x{:08X} is not a line", shown[0]);
    std::abort();
  }
  sink.emit(spyro::depth_bins::kOverlaySlot, *primitive);
}

void registerOverrides(Core &core) {
  spyro::session(core).stateProducers.install(ShadedLineProducer::kEntry,
                                              std::make_unique<ShadedLineProducer>());
  psx::cpu::installNativeOverride(core, ShadedLineProducer::kEntry, "spyro1-shaded-line", drawLine);
}

} // namespace spyro1::shaded_line
