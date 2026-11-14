// title_state.h — Spyro 3's per-Core state its natives own: the HUD widget being drawn and the
// sprite fonts as the level's tables lay them out.
#pragma once

#include "core/session.h"
#include "spyro3/render/hud_anchor.h"
#include "spyro3/render/sprite_fonts.h"

class Core;

namespace spyro3 {

class TitleState final : public spyro::TitleState {
public:
  hud_anchor::DrawState hud;
  sprite_fonts::Fonts fonts;
};

TitleState &titleState(Core &core);

} // namespace spyro3
