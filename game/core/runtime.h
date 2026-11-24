#pragma once

#include "core/debug/warp.h"
#include "core/session.h"
#include "core/wad_wordmark.h"
#include "game_runtime.h"
#include "guest_widescreen_projection.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>

namespace spyro {

// The behaviour Spyro 1/2/3 share; each serial's runtime derives from it, and is its own widescreen
// projection.
class Runtime : public psx::game::GameRuntime, public psx::present::GuestWidescreenProjection {
public:
  std::unique_ptr<psx::game::TitleSessionState> createSession(Game &game) final;
  // The field owner already delivered the simulated fields, so presentation only waits.
  std::optional<double> presentDeadline(Core &core, psx::frame::FieldCadence cadence) final;
  // The title's wordmark, read from its WAD.
  std::optional<psx::present::ArtScene> panelArt(psx::cd::Disc &disc) const final;
  // The player's aspect; the canvas holds the margins and each title widens its own culls.
  const psx::present::GuestWidescreenProjection *guestWidescreenProjection() const final;
  // The packets the overlay pair's link and the screen-space emitters name
  // (render/packet_layers.h).
  const psx::present::CropUiLayers *cropUiLayers(Core &core) const final;
  // The 2D font glyphs the text layouts name (render/packet_layers.h); a title offering its font
  // draws them.
  const psx::present::OutlineGlyphs *outlineGlyphs(Core &core) const final;
  bool interpolates() const final;
  // The right-stick camera (core/stick_camera.h) and more vegetation
  // (render/terrain/terrain_vegetation.h) rows.
  std::span<const psx::game::TitleIntSetting> titleIntSettings() const final;

protected:
  Runtime() = default;

  // The title's warp: its level ids, idle title screen and level-start owner.
  virtual std::unique_ptr<Warp> createWarp() const = 0;
  // Where the title screen's wordmark is in WAD.WAD.
  virtual const WadWordmark &wordmark() const = 0;
  // The title's own per-Core state; none by default.
  virtual std::unique_ptr<TitleState> createTitleState() const;
};

} // namespace spyro
