#include "core/runtime.h"

#include "core.h"
#include "core/session.h"
#include "core/stick_camera.h"
#include "frame_pacer.h"
#include "game.h"
#include "render/terrain/terrain_vegetation.h"

#include <array>

namespace spyro {
namespace {

const std::array<psx::game::TitleIntSetting, 2> kTitleSettings{
    rightStickCameraSetting(), guest_terrain::moreVegetationSetting()};

} // namespace

std::unique_ptr<psx::game::TitleSessionState> Runtime::createSession(Game &) {
  auto created = std::make_unique<Session>();
  created->warp = createWarp();
  created->titleState = createTitleState();
  return created;
}

std::optional<double> Runtime::presentDeadline(Core &core, psx::frame::FieldCadence cadence) {
  return core.game->framePacer.presentedFieldsDeadline(core, cadence);
}

std::optional<psx::present::ArtScene> Runtime::panelArt(psx::cd::Disc &disc) const {
  return wadWordmark(disc, wordmark());
}

std::unique_ptr<TitleState> Runtime::createTitleState() const {
  return nullptr;
}

bool Runtime::interpolates() const {
  return true;
}

std::span<const psx::game::TitleIntSetting> Runtime::titleIntSettings() const {
  return kTitleSettings;
}

const psx::present::GuestWidescreenProjection *Runtime::guestWidescreenProjection() const {
  return this;
}

const psx::present::CropUiLayers *Runtime::cropUiLayers(Core &core) const {
  return &session(core).layers;
}

const psx::present::OutlineGlyphs *Runtime::outlineGlyphs(Core &core) const {
  return &session(core).layers;
}

} // namespace spyro
