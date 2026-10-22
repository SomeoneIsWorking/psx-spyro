#include "render/screen_overlays.h"

#include "blend.h"
#include "core.h"
#include "core/session.h"
#include "frame_state.h"
#include "gp0_command.h"
#include "gp0_primitive_decode.h"
#include "lucent/log.h"
#include "render/field/guest_packet.h"
#include "render/state_producer_override.h"
#include "render/terrain/guest_depth_bins.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace spyro::screen_overlays {
namespace {

constexpr std::uint32_t kScreenWidth = 0x200;
constexpr std::uint32_t kBarBytes = guest_packet::kF4Bytes;
// The semi-transparency modes in which colour 0 leaves the screen as it is.
constexpr std::uint32_t kAdditive = 1;
constexpr std::uint32_t kSubtractive = 2;

constexpr std::uint32_t position(std::uint32_t x, std::uint32_t y) {
  return ((y & 0xFFFFu) << 16) | (x & 0xFFFFu);
}

using Quad = psx::gpu::PacketBuffer<5>;

psx::present::DrawPrimitive quad(const Quad &words) {
  const auto primitive = psx::gpu::decodePacketPrimitive(words.view());
  if (!primitive) {
    lucent::error("overlays", "the overlay quad 0x{:08X} does not decode", words.word(0));
    std::abort();
  }
  return *primitive;
}

bool redrawing(Core &core) {
  return session(core).fieldRedraw.redrawing;
}

} // namespace

void drawTint(Core &core, const TintFacts &facts) {
  if (redrawing(core)) {
    return;
  }
  const FullscreenTint::State state{.blend = core.r[4],
                                    .r = static_cast<std::uint8_t>(core.r[5]),
                                    .g = static_cast<std::uint8_t>(core.r[6]),
                                    .b = static_cast<std::uint8_t>(core.r[7])};
  const auto site = core.emission.instance(core.r[31]);
  core.frameStates.save(core.emission.current(), state);
  (void)psx::cpu::callOriginalOrPropagate(core, facts.entry);
}

bool drawLetterbox(Core &core, const LetterboxFacts &facts) {
  if (redrawing(core)) {
    return true;
  }
  const std::uint32_t first = core.mem_r32(facts.primitiveCursor);
  const auto site = core.emission.instance(core.r[31]);
  if (!psx::cpu::callOriginalOrPropagate(core, facts.entry)) {
    return false;
  }
  const CinematicLetterbox::State state{.height =
                                            static_cast<std::int32_t>(core.mem_r32(facts.height))};
  if (state.height == 0) {
    return true;
  }
  for (std::uint32_t bar = 0; bar < 2; ++bar) {
    const auto part = core.emission.element(bar);
    core.emission.bindPacket(first + bar * kBarBytes);
  }
  core.frameStates.save(core.emission.current(), state);
  return true;
}

void FullscreenTint::render(const psx::present::StateInterval &states,
                            psx::present::PrimitiveSink &sink) const {
  const float t = states.t;
  const State after = psx::present::stateAs<State>(states.to);
  State shown = after;
  if (const State before = psx::present::stateAs<State>(states.from); before.blend == after.blend) {
    shown.r = static_cast<std::uint8_t>(psx::present::lerpInt(before.r, after.r, t));
    shown.g = static_cast<std::uint8_t>(psx::present::lerpInt(before.g, after.g, t));
    shown.b = static_cast<std::uint8_t>(psx::present::lerpInt(before.b, after.b, t));
  }
  const std::uint32_t colour = (guest_packet::kF4Code | guest_packet::kSemiCode) |
                               (std::uint32_t{shown.b} << 16) | (std::uint32_t{shown.g} << 8) |
                               shown.r;
  psx::present::DrawPrimitive primitive = quad(Quad(std::array{colour,
                                                           position(0, facts_.top),
                                                           position(kScreenWidth, facts_.top),
                                                           position(0, facts_.bottom),
                                                           position(kScreenWidth, facts_.bottom)}));
  psx::gpu::applyTexPageAttribute(primitive.state, static_cast<std::uint16_t>(after.blend << 5));
  sink.emit(depth_bins::kOverlaySlot, primitive);
}

bool FullscreenTint::rests(std::span<const std::byte> drawn, std::span<std::byte> into) const {
  State rest = psx::present::stateAs<State>(drawn);
  if (rest.blend != kAdditive && rest.blend != kSubtractive) {
    return false;
  }
  rest.r = 0;
  rest.g = 0;
  rest.b = 0;
  std::memcpy(into.data(), &rest, sizeof(rest));
  return true;
}

void CinematicLetterbox::render(const psx::present::StateInterval &states,
                                psx::present::PrimitiveSink &sink) const {
  const State before = psx::present::stateAs<State>(states.from);
  const State after = psx::present::stateAs<State>(states.to);
  const float shown = static_cast<float>(before.height) +
                      static_cast<float>(after.height - before.height) * states.t;
  // A corner on the bar's moving edge, `y` down the screen.
  const auto edge = [](std::uint32_t x, float y) {
    return psx::gte::ProjectedVertex::nearest({static_cast<float>(x), y});
  };
  const float topEdge = static_cast<float>(facts_.top) + shown;
  const float bottomEdge = static_cast<float>(facts_.bottom) - shown;
  Quad top;
  top.store(0, guest_packet::kF4Code);
  top.store(1, position(0, facts_.top));
  top.store(2, position(kScreenWidth, facts_.top));
  top.storeVertex(3, edge(0, topEdge));
  top.storeVertex(4, edge(kScreenWidth, topEdge));
  Quad bottom;
  bottom.store(0, guest_packet::kF4Code);
  bottom.storeVertex(1, edge(0, bottomEdge));
  bottom.storeVertex(2, edge(kScreenWidth, bottomEdge));
  bottom.store(3, position(0, facts_.bottom));
  bottom.store(4, position(kScreenWidth, facts_.bottom));
  sink.emit(depth_bins::kOverlaySlot, quad(top));
  sink.emit(depth_bins::kOverlaySlot, quad(bottom));
}

bool CinematicLetterbox::rests(std::span<const std::byte>, std::span<std::byte> into) const {
  const State rest{};
  std::memcpy(into.data(), &rest, sizeof(rest));
  return true;
}

void installTint(Core &core, const TintFacts &facts, psx::cpu::NativeFunction draw) {
  installProducerOverride(
      core, facts.entry, facts.overrideName, draw, std::make_unique<FullscreenTint>(facts));
}

void installLetterbox(Core &core, const LetterboxFacts &facts, psx::cpu::NativeFunction draw) {
  installProducerOverride(
      core, facts.entry, facts.overrideName, draw, std::make_unique<CinematicLetterbox>(facts));
}

} // namespace spyro::screen_overlays
