#include "spyro1/core/frame/level_transition_update.h"
#include "spyro1/core/guest_globals.h"

#include "core.h"
#include "core/loader_drain.h"
#include "guest_call.h"
#include "native_dispatch.h"

#include <cstdint>
#include <string_view>

namespace spyro1 {
namespace {

using guest::kGamestate;
using guest::kLevelTransHudActive;
using guest::kLoadStage;

constexpr std::uint32_t kLevelTransitionUpdate = 0x8002DF9Cu;
constexpr std::uint32_t kUpdateSpyroEnterReturnHome = 0x8004A7ECu;
constexpr std::uint32_t kCameraUpdate = 0x80037BD4u;
constexpr std::uint32_t kTransitionTally = 0x8002DA74u;
constexpr std::uint32_t kLoadLevel = 0x80015370u;
constexpr std::uint32_t kGamestateLevelTransition = 1u;
// LoadLevel's stage 11 on waits for the tally to end.
constexpr std::int32_t kStageTallyGate = 11;
// Stage 12 waits for the camera to settle on Spyro; 13 is the load's terminal.
constexpr std::int32_t kStageCameraSettle = 12;
// D_8007576C: stage 5's VRAM half, one LoadImage a call.
constexpr std::uint32_t kVramHalf = 0x8007576Cu;
constexpr std::int32_t kStageVramTransfer = 5;
constexpr std::string_view kOwner = "level-transition-update";

std::int32_t loadStage(Core &core) {
  return static_cast<std::int32_t>(core.mem_r32(kLoadStage));
}

bool loadOpen(Core &core) {
  return loadStage(core) < kStageTallyGate || core.mem_r32(kLevelTransHudActive) == 0u;
}

std::int64_t loadProgress(Core &core) {
  const std::int32_t stage = loadStage(core);
  const std::int64_t half = stage == kStageVramTransfer ? core.mem_r32(kVramHalf) : 0u;
  return (static_cast<std::int64_t>(stage) << 8) + half;
}

void levelTransitionUpdate(Core *core) {
  psx::cpu::callGuestNow(*core, kOwner, kUpdateSpyroEnterReturnHome);
  psx::cpu::callGuestNow(*core, kOwner, kCameraUpdate);
  psx::cpu::callGuestNow(*core, kOwner, kTransitionTally);
  if (!loadOpen(*core)) {
    return;
  }
  spyro::drainLoader(
      [core] {
        psx::cpu::callGuestNow(*core, kOwner, kLoadLevel, 1u);
      },
      [core] {
        return loadProgress(*core);
      },
      [core] {
        return core->mem_r32(kGamestate) == kGamestateLevelTransition &&
               loadStage(*core) < kStageCameraSettle && loadOpen(*core);
      });
}

} // namespace

void registerLevelTransitionUpdate(Core &core) {
  psx::cpu::installNativeOverride(
      core, kLevelTransitionUpdate, "level_transition_update", levelTransitionUpdate);
}

} // namespace spyro1
