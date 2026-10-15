#include "spyro_runtime.h"

#include "core.h"
#include "frame_pacer.h"
#include "game.h"
#include "spyro_session.h"
#include "title_logo.h"

namespace spyro {
namespace {

// No rectangle, so nothing is read and nothing is drawn.
constexpr TitleLogoFacts kNoLogoFacts{};

} // namespace

SpyroRuntime::SpyroRuntime(SpyroTitle title) : title_(title) {}

SpyroTitle SpyroRuntime::title() const {
  return title_;
}

const TitleLogoFacts &SpyroRuntime::logoFacts() const {
  return kNoLogoFacts;
}

std::unique_ptr<psx::game::TitleSessionState> SpyroRuntime::createSession(Game &) {
  return std::make_unique<SpyroSession>();
}

const psx::present::StateProducer *SpyroRuntime::stateProducer(Core &core,
                                                               std::uint32_t producer) const {
  return session(core).stateProducers.find(producer);
}

void SpyroRuntime::pacePresentation(Core &core, int guestFields, int parts) {
  core.game->framePacer.waitPresentedFields(core, guestFields, parts);
}

std::optional<psx::host::LogoImage> SpyroRuntime::panelLogo(Core &core) const {
  return extractPanelLogo(core, logoFacts());
}

const GuestWidescreenProjection *SpyroRuntime::guestWidescreenProjection() const {
  return &aspectPolicy_;
}

void SpyroRuntime::reportRun(Core &core) const {
  // These counters are incremented inside the framework's guest-OT classifier, which a run whose
  // producers are all guest code never executes; a zero here means "never ran", not "no coverage".
  render_depth_coverage_report(&core, "run-complete");
  core.rsub.census.report("spyro run-complete");
}

} // namespace spyro
