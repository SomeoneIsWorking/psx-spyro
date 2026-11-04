// intro_card.h — the title intro card continued sideways in its own model space into the 16:9
// margins.
#pragma once

#include "core.h"
#include "render/field/guest_render_globals.h"

#include <cstdint>
#include <span>

namespace spyro::intro_card {

// One row of the card's front face: its outermost vertex on each side.
struct Row {
  std::uint8_t left = 0;
  std::uint8_t right = 0;
};

struct Facts {
  std::uint32_t drawer = 0; // the moby drawer the walk hands its render list to
  const char *overrideName = "";
  std::uint32_t deltaTable = 0; // the drawer's byte-code delta words
  std::uint32_t gameState = 0;
  std::uint32_t titleGameState = 0;
  std::uint16_t mesh = 0;      // the card's mesh index in the title level
  std::span<const Row> rows{}; // top to bottom
};

// The drawer, then the card's extension when the title screen lists it on a widened canvas.
void draw(Core &core, const Facts &facts, const guest_render_globals::Globals &globals);

// An override is a plain `void (*)(Core *)`, so each image's facts are template arguments.
template <const Facts &facts, const guest_render_globals::Globals &globals>
void drawOverride(Core *core) {
  draw(*core, facts, globals);
}

} // namespace spyro::intro_card
