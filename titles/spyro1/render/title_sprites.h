// title_sprites.h — the title overlay's sprite drawer TitlescreenDrawSprite 0x8007CD38 (spyro-1
// src/overlays/titlescreen.c) as a state producer: the logo sliding in and out, PRESS START, the
// menu borders and the memory card screens.
//
// The drawer writes one POLY_FT4 at the primitive cursor and links it with AddPrimToOT 0x800168DC
// into the overlay pair. It lives in the title overlay (psyq overlay id 48, loaded to 0x8007AA38),
// whose range every level overlay reuses, so its override is installed against each landing of that
// image.
#pragma once

#include "state_producer.h"

#include <array>
#include <cstdint>
#include <type_traits>

class Core;

namespace spyro1 {

class TitleSprite final : public psx::present::StateProducer {
public:
  static constexpr std::uint32_t kEntry = 0x8007CD38u;
  static constexpr std::uint32_t kOverlayBase = 0x8007AA38u;
  static constexpr std::uint32_t kOverlayId = 48u;

  // The sprite's corner as the caller passed it, and the packet the body wrote, tag excluded.
  struct State {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::array<std::uint32_t, 9> words{};
  };
  static_assert(std::is_trivially_copyable_v<State>);

  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;
};

// The render, once per session.
void registerTitleSpriteProducer(Core &core);
// A loader read of `length` bytes landed at `destination`: when it is the title overlay, its drawer
// becomes the producer for that image.
void titleOverlayLanded(Core &core, std::uint32_t destination, std::uint32_t length);

} // namespace spyro1
