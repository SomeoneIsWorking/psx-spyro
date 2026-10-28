#include "spyro1/render/shaded_line.h"

#include "core.h"
#include "core/session.h"
#include "emission_scope.h"
#include "frame_state.h"
#include "game.h"
#include "gp0_primitive_decode.h"
#include "native_dispatch.h"
#include "render/field/record_blend.h"
#include "render/terrain/guest_depth_bins.h"
#include "spyro1/core/guest_globals.h"
#include "spyro1/render/terrain_facts.h"

#include <lucent/log.h>

#include <algorithm>
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

// Word index of a return address in 2 MB of RAM; the turn and the page fill the bits above it.
constexpr std::uint32_t kSiteBits = 19;
constexpr std::uint32_t kTurnBits = 8;

CallSites &callSites(Core &core) {
  return static_cast<CallSites &>(*spyro::session(core).titleState);
}

// The page joins the key because a page change moves every inventory line by a whole page.
std::uint32_t lineObject(Core &core) {
  const std::uint32_t site = core.r[31];
  const std::uint32_t turn = callSites(core).turn(core.game->timing.logicFrame, site);
  const std::uint32_t page = core.mem_r32(guest::kInventoryPage);
  return ((site >> 2) & ((1u << kSiteBits) - 1u)) | ((turn & ((1u << kTurnBits) - 1u)) << kSiteBits) |
         (page << (kSiteBits + kTurnBits));
}

void drawLine(Core *core) {
  Core &guest = *core;
  const std::uint32_t packet = guest.mem_r32(terrain::kRenderGlobals.primitiveCursor);
  const psx::present::EmissionScope::Guard scope(guest.emission, ShadedLineProducer::kEntry, lineObject(guest), 0);
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

std::uint32_t CallSites::turn(std::uint32_t frame, std::uint32_t site) {
  if (frame != frame_) {
    frame_ = frame;
    calls_.clear();
  }
  const auto found = std::ranges::find(calls_, site, &std::pair<std::uint32_t, std::uint32_t>::first);
  if (found == calls_.end()) {
    calls_.emplace_back(site, 1u);
    return 0;
  }
  return found->second++;
}

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
  shown[1] = psx::present::lerpHalves(before[1], shown[1], states.t);
  shown[2] = colourBetween(before[2], shown[2], states.t);
  shown[3] = psx::present::lerpHalves(before[3], shown[3], states.t);
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
