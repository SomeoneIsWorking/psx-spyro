#include "spyro1/core/frame/flyby_card.h"

#include "core.h"
#include "game.h"
#include "spyro1/core/guest_globals.h"

namespace spyro1 {
namespace {

// The card camera (TSS_Setup: yaw 0x400, no pitch or roll) looks along +y: world x is screen x.
constexpr std::uint32_t kSpyroX = guest::kSpyro;
constexpr std::uint32_t kCameraX = guest::kCameraPosition;

bool cardShowsSpyro(Core &core) {
  return core.mem_r32(guest::kGamestate) == guest::kGamestateTitleScreen &&
         core.mem_r32(guest::kTitlescreenMode) == guest::kTitleModeDemo &&
         core.mem_r32(guest::kTitlescreenStage) == guest::kTitleStageActive;
}

} // namespace

FlybyCardPlacement::FlybyCardPlacement(Core &core) : core_(core) {
  const psx::present::GuestProjectionPlan &plan = core.game->guestDisplay.plan();
  if (!plan.widescreen() || !cardShowsSpyro(core)) {
    return;
  }
  const std::uint32_t x = core.mem_r32(kSpyroX);
  const std::int64_t camera = static_cast<std::int32_t>(core.mem_r32(kCameraX));
  const std::int64_t across = static_cast<std::int32_t>(x) - camera;
  const std::int64_t widened =
      across * plan.projectionExtent.width / plan.nativeProjectionExtent.width;
  retailX_ = x;
  core.mem_w32(kSpyroX, static_cast<std::uint32_t>(camera + widened));
}

FlybyCardPlacement::~FlybyCardPlacement() {
  if (retailX_) {
    core_.mem_w32(kSpyroX, *retailX_);
  }
}

} // namespace spyro1
