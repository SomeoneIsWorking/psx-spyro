#include "spyro1/render/title_sprites.h"

#include "core.h"
#include "core/session.h"
#include "frame_state.h"
#include "gp0_primitive_decode.h"
#include "lucent/log.h"
#include "native_dispatch.h"
#include "render/terrain/guest_depth_bins.h"
#include "spyro1/render/terrain_facts.h"

#include <cstdlib>
#include <span>
#include <tuple>
#include <memory>

namespace spyro1 {
namespace {

// The object: the call site's low address bits and the sprite's column, since a menu row draws
// several sprites from one site.
constexpr std::uint32_t kSiteBits = 22;
constexpr std::uint32_t kSiteMask = (1u << kSiteBits) - 1u;

void drawSpriteOverride(Core *core) {
  const auto x = static_cast<std::int32_t>(core->r[4]);
  const auto y = static_cast<std::int32_t>(core->r[5]);
  const std::uint32_t packet = core->mem_r32(terrain::kRenderGlobals.primitiveCursor);
  const auto sprite = core->emission.instance((core->r[31] & kSiteMask) |
                                              (static_cast<std::uint32_t>(x) << kSiteBits));
  if (!psx::cpu::callOriginalOrPropagate(*core, TitleSprite::kEntry)) {
    return;
  }
  TitleSprite::State state{.x = x, .y = y};
  for (std::uint32_t word = 0; word < state.words.size(); ++word) {
    state.words[word] = core->mem_r32(packet + 4u * (word + 1u));
  }
  core->frameStates.save(core->emission.current(), state);
}

} // namespace

void TitleSprite::render(const psx::present::StateInterval &states,
                         psx::present::PrimitiveSink &sink) const {
  const State after = psx::present::stateAs<State>(states.to);
  psx::gpu::PacketBuffer<std::tuple_size_v<decltype(after.words)>> packet(std::span(after.words));
  const State before = psx::present::stateAs<State>(states.from);
  // Texture words (UV, CLUT, texpage) equal: the same sprite moved; anything else is drawn as it is
  // now.
  if (before.words[2] == after.words[2] && before.words[4] == after.words[4] &&
      before.words[6] == after.words[6] && before.words[8] == after.words[8]) {
    const float dx = static_cast<float>(before.x - after.x) * (1.0f - states.t);
    const float dy = static_cast<float>(before.y - after.y) * (1.0f - states.t);
    // The FT4's corner words.
    for (const std::size_t corner : {1u, 3u, 5u, 7u}) {
      const GtePlace place = psx::gte::ProjectedVertex::whole(after.words[corner]).place();
      packet.storeVertex(corner, psx::gte::ProjectedVertex::nearest({place.x + dx, place.y + dy}));
    }
  }
  auto primitive = psx::gpu::decodePacketPrimitive(packet.view());
  if (!primitive) {
    lucent::error(
        "title-sprite", "the title sprite packet 0x{:08X} does not decode", after.words[0]);
    std::abort();
  }
  sink.emit(spyro::depth_bins::kOverlaySlot, *primitive);
}

void registerTitleSpriteProducer(Core &core) {
  spyro::session(core).stateProducers.install(TitleSprite::kEntry, std::make_unique<TitleSprite>());
}

void titleOverlayLanded(Core &core, std::uint32_t destination, std::uint32_t length) {
  if (destination != TitleSprite::kOverlayBase || length <= TitleSprite::kEntry - destination ||
      core.mem_r32(destination) != TitleSprite::kOverlayId) {
    return;
  }
  psx::cpu::installNativeOverride(core,
                                  TitleSprite::kEntry,
                                  "spyro1-title-sprite",
                                  drawSpriteOverride,
                                  psx::present::Producer{});
}

} // namespace spyro1
