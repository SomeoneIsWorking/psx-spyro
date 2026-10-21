#include "spyro1/render/screen_overlays.h"

#include "blend.h"
#include "core.h"
#include "frame_state.h"
#include "gp0_command.h"
#include "gp0_primitive_decode.h"
#include "lucent/log.h"
#include "native_dispatch.h"
#include "render/field/guest_packet.h"
#include "render/state_producer_override.h"
#include "render/terrain/guest_depth_bins.h"
#include "spyro1/render/terrain_facts.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace spyro1 {
namespace {

namespace guest_packet = spyro::guest_packet;

constexpr psx::present::OtSlot kOverlaySlot{spyro::depth_bins::kOverlayTable, 0};
constexpr std::uint32_t kScreenWidth = 0x200;
constexpr std::uint32_t kScreenHeight = 0xF0;
constexpr std::uint32_t kTintTop = 8;
constexpr std::uint32_t kTintBottom = 0xE8;
// The tint's POLY_F4 follows its 3-word DR_MODE; each letterbox bar is a 6-word POLY_F4.
constexpr std::uint32_t kBarBytes = guest_packet::kF4Bytes;
// The semi-transparency modes in which colour 0 leaves the screen as it is.
constexpr std::uint32_t kAdditive = 1;
constexpr std::uint32_t kSubtractive = 2;

constexpr std::uint32_t position(std::uint32_t x, std::uint32_t y) {
  return ((y & 0xFFFFu) << 16) | (x & 0xFFFFu);
}

psx::present::DrawPrimitive quad(const std::array<std::uint32_t, 5> &words) {
  const auto primitive = psx::gpu::decodePacketPrimitive(words);
  if (!primitive) {
    lucent::error("overlays", "the overlay quad 0x{:08X} does not decode", words[0]);
    std::abort();
  }
  return *primitive;
}

void tintOverride(Core *core) {
  const FullscreenTint::State state{.blend = core->r[4],
                                    .r = static_cast<std::uint8_t>(core->r[5]),
                                    .g = static_cast<std::uint8_t>(core->r[6]),
                                    .b = static_cast<std::uint8_t>(core->r[7])};
  const auto site = core->emission.instance(core->r[31]);
  core->frameStates.save(core->emission.current(), state);
  (void)psx::cpu::callOriginalOrPropagate(*core, FullscreenTint::kEntry);
}

void letterboxOverride(Core *core) {
  const std::uint32_t first = core->mem_r32(terrain::kRenderGlobals.primitiveCursor);
  const auto site = core->emission.instance(core->r[31]);
  (void)psx::cpu::callOriginalOrPropagate(*core, CinematicLetterbox::kEntry);
  for (std::uint32_t bar = 0; bar < 2; ++bar) {
    const auto part = core->emission.element(bar);
    core->emission.bindPacket(first + bar * kBarBytes);
  }
  const CinematicLetterbox::State state{
      .height = static_cast<std::int32_t>(core->mem_r32(CinematicLetterbox::kBarHeight))};
  core->frameStates.save(core->emission.current(), state);
}

} // namespace

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
  psx::present::DrawPrimitive primitive = quad({colour,
                                                position(0, kTintTop),
                                                position(kScreenWidth, kTintTop),
                                                position(0, kTintBottom),
                                                position(kScreenWidth, kTintBottom)});
  psx::gpu::applyTexPageAttribute(primitive.state, static_cast<std::uint16_t>(after.blend << 5));
  sink.emit(kOverlaySlot, primitive);
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
  const float t = states.t;
  const State before = psx::present::stateAs<State>(states.from);
  const State after = psx::present::stateAs<State>(states.to);
  const auto height = static_cast<std::uint32_t>(after.height);
  const float shown =
      static_cast<float>(before.height) + static_cast<float>(after.height - before.height) * t;
  psx::present::DrawPrimitive top = quad({guest_packet::kF4Code,
                                          position(0, 0),
                                          position(kScreenWidth, 0),
                                          position(0, height),
                                          position(kScreenWidth, height)});
  psx::present::DrawPrimitive bottom = quad({guest_packet::kF4Code,
                                             position(0, kScreenHeight - height),
                                             position(kScreenWidth, kScreenHeight - height),
                                             position(0, kScreenHeight),
                                             position(kScreenWidth, kScreenHeight)});
  const float bottomEdge = static_cast<float>(kScreenHeight) - shown;
  for (std::size_t corner = 0; corner < 2; ++corner) {
    psx::present::RecordVertex &topEdge = top.vertices[corner + 2];
    psx::present::placeVertex(topEdge, static_cast<float>(topEdge.x), shown);
    psx::present::RecordVertex &bottomTop = bottom.vertices[corner];
    psx::present::placeVertex(bottomTop, static_cast<float>(bottomTop.x), bottomEdge);
  }
  sink.emit(kOverlaySlot, top);
  sink.emit(kOverlaySlot, bottom);
}

void registerScreenOverlayOverrides(Core &core) {
  spyro::installProducerOverride(core,
                                 FullscreenTint::kEntry,
                                 "spyro1-fullscreen-tint",
                                 tintOverride,
                                 std::make_unique<FullscreenTint>());
  spyro::installProducerOverride(core,
                                 CinematicLetterbox::kEntry,
                                 "spyro1-cinematic-letterbox",
                                 letterboxOverride,
                                 std::make_unique<CinematicLetterbox>());
}

} // namespace spyro1
