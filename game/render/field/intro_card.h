// intro_card.h — the title screen's opening "Developed by Insomniac" card at 16:9.
//
// The card is a moby of the title level (Spyro 2 record 3, mesh 3; Spyro 3 record 0, mesh 0), a
// thick textured board on a post whose own animation swings it (Spyro 2) or recedes it (Spyro 3)
// in front of a still camera. Retail authored it to just cover the 4:3 view, so a widened canvas
// shows the title level past its sides while it is face-on. This owner wraps the moby drawer and,
// after it, continues the card's front face sideways in the card's own model space by the share of
// its width the canvas adds, as black quads projected through the card's entry: they turn and
// recede with it, and the logo is not stretched. Nothing is drawn at 4:3.
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
template <const Facts &facts, const guest_render_globals::Globals &globals> void drawOverride(Core *core) {
  draw(*core, facts, globals);
}

} // namespace spyro::intro_card
