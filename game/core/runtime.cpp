#include "core/runtime.h"

#include "core.h"
#include "core/session.h"
#include "core/title_logo.h"
#include "frame_pacer.h"
#include "game.h"

namespace spyro {
namespace {

// No rectangle, so nothing is read and nothing is drawn.
constexpr TitleLogoFacts kNoLogoFacts{};

} // namespace

Runtime::Runtime(Title title) : title_(title) {}

Title Runtime::title() const {
  return title_;
}

const TitleLogoFacts &Runtime::logoFacts() const {
  return kNoLogoFacts;
}

std::unique_ptr<psx::game::TitleSessionState> Runtime::createSession(Game &) {
  return std::make_unique<Session>();
}

const psx::present::StateProducer *Runtime::stateProducer(Core &core,
                                                          std::uint32_t producer) const {
  return session(core).stateProducers.find(producer);
}

void Runtime::pacePresentation(Core &core, int guestFields, int parts) {
  core.game->framePacer.waitPresentedFields(core, guestFields, parts);
}

std::optional<psx::host::LogoImage> Runtime::panelLogo(Core &core) const {
  return extractPanelLogo(core, logoFacts());
}

const GuestWidescreenProjection *Runtime::guestWidescreenProjection() const {
  return &aspectPolicy_;
}

void Runtime::reportRun(Core &core) const {
  // These counters are incremented inside the framework's guest-OT classifier, which a run whose
  // producers are all guest code never executes; a zero here means "never ran", not "no coverage".
  render_depth_coverage_report(&core, "run-complete");
  core.rsub.census.report("spyro run-complete");
}

} // namespace spyro
