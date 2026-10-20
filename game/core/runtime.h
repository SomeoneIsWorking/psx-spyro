#pragma once

#include "core/debug/warp.h"
#include "core/session.h"
#include "core/title_logo_facts.h"
#include "game_runtime.h"
#include "guest_widescreen_projection.h"

#include <cstdint>
#include <memory>
#include <optional>

namespace spyro {

// The behaviour Spyro 1/2/3 share; each serial's runtime derives from it.
class Runtime : public psx::game::GameRuntime {
public:
  // Where the title's own logo lands in VRAM and how it is encoded; the base has none.
  virtual const TitleLogoFacts &logoFacts() const;

  std::unique_ptr<psx::game::TitleSessionState> createSession(Game &game) final;
  const psx::present::StateProducer *stateProducer(Core &core, std::uint32_t producer) const final;
  // The field owner already delivered the simulated fields, so presentation only waits.
  void pacePresentation(Core &core, psx::frame::FieldCadence cadence) final;
  std::optional<psx::host::LogoImage> panelLogo(Core &core) const final;
  // The player's aspect; the canvas holds the margins and each title widens its own culls.
  const GuestWidescreenProjection *guestWidescreenProjection() const final;
  void reportRun(Core &core) const final;
  bool interpolates() const final;
  // `warp <level>`, armed on the session's warp.
  bool
  titleCommand(Core &core, const psx::game::TitleCommandLine &input, std::FILE *reply) override;

protected:
  Runtime() = default;

  // The title's warp: its level ids, idle title screen and level-start owner.
  virtual std::unique_ptr<Warp> createWarp() const = 0;
  // The title's own per-Core state; none by default.
  virtual std::unique_ptr<TitleState> createTitleState() const;

private:
  GuestWidescreenProjection aspectPolicy_;
};

} // namespace spyro
