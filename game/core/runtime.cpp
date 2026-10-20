#include "core/runtime.h"

#include "core.h"
#include "core/session.h"
#include "core/title_logo.h"
#include "frame_pacer.h"
#include "game.h"

#include <cstdio>
#include <cstring>

namespace spyro {
namespace {

// No rectangle, so nothing is read and nothing is drawn.
constexpr TitleLogoFacts kNoLogoFacts{};

} // namespace

const TitleLogoFacts &Runtime::logoFacts() const {
  return kNoLogoFacts;
}

std::unique_ptr<psx::game::TitleSessionState> Runtime::createSession(Game &) {
  auto created = std::make_unique<Session>();
  created->warp = createWarp();
  created->titleState = createTitleState();
  return created;
}

const psx::present::StateProducer *Runtime::stateProducer(Core &core,
                                                          std::uint32_t producer) const {
  return session(core).stateProducers.find(producer);
}

void Runtime::pacePresentation(Core &core, psx::frame::FieldCadence cadence) {
  core.game->framePacer.waitPresentedFields(core, cadence);
}

std::optional<psx::host::LogoImage> Runtime::panelLogo(Core &core) const {
  return extractPanelLogo(core, logoFacts());
}

std::unique_ptr<TitleState> Runtime::createTitleState() const {
  return nullptr;
}

bool Runtime::interpolates() const {
  return true;
}

const psx::present::GuestWidescreenProjection *Runtime::guestWidescreenProjection() const {
  return &aspectPolicy_;
}

bool Runtime::titleCommand(Core &core, const psx::game::TitleCommandLine &input, std::FILE *reply) {
  if (std::strcmp(input.command, "warp") != 0) {
    return false;
  }
  std::fprintf(reply, "%s\n", session(core).warp->arm(input.args).c_str());
  return true;
}

void Runtime::reportRun(Core &core) const {
  core.rsub.census.report("spyro run-complete");
}

} // namespace spyro
