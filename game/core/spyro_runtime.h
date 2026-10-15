#pragma once

#include "game_runtime.h"
#include "guest_widescreen_projection.h"
#include "title_logo_facts.h"

#include <cstdint>
#include <memory>
#include <optional>

namespace spyro {

enum class SpyroTitle : std::uint8_t { Spyro1, Spyro2, Spyro3 };

// The behaviour Spyro 1/2/3 share; each serial's runtime derives from it.
class SpyroRuntime : public psx::game::GameRuntime {
public:
  SpyroTitle title() const;

  // Where the title's own logo lands in VRAM and how it is encoded; the base has none.
  virtual const TitleLogoFacts &logoFacts() const;

  std::unique_ptr<psx::game::TitleSessionState> createSession(Game &game) final;
  const psx::present::StateProducer *stateProducer(Core &core, std::uint32_t producer) const final;
  // The field owner already delivered the simulated fields, so presentation only waits.
  void pacePresentation(Core &core, int guestFields, int parts) final;
  std::optional<psx::host::LogoImage> panelLogo(Core &core) const final;
  // The player's aspect; the canvas holds the margins and each title widens its own culls.
  const GuestWidescreenProjection *guestWidescreenProjection() const final;
  void reportRun(Core &core) const final;

protected:
  explicit SpyroRuntime(SpyroTitle title);

private:
  SpyroTitle title_;
  GuestWidescreenProjection aspectPolicy_;
};

} // namespace spyro
