#include "core/frame_cut.h"

#include "core.h"

#include <lucent/log.h>

namespace spyro {
namespace {

// The camera controller's modes that place the camera, and bit 7 of mode 9's sub-state, which the
// controller keeps through the placement (0x80 -> 0x81).
inline constexpr std::uint32_t kModeSavedCamera = 9;
inline constexpr std::uint32_t kModeScripted = 10;
inline constexpr std::uint32_t kModeRespawn = 0xB;
inline constexpr std::uint32_t kScriptEnding = 0x80;
inline constexpr std::uint32_t kSubStateFlag = 0x80;
// The track player's blend limits; the length test is compared squared.
inline constexpr std::int64_t kShotStepSquared = std::int64_t{0x100} * 0x100;
inline constexpr std::int32_t kShotTurn = 0xFF;
inline constexpr std::uint32_t kAngleMask = 0xFFF;
inline constexpr std::int32_t kAngleHalfTurn = 0x800;
inline constexpr std::int32_t kAngleFullTurn = 0x1000;

} // namespace

bool FrameCut::cameraPlaced(const CameraMachineWords &before, const CameraMachineWords &after) {
  if (before.mode != after.mode || before.subState == after.subState) {
    return false;
  }
  switch (before.mode) {
  case kModeSavedCamera:
    return (before.subState & ~kSubStateFlag) == 0 && (after.subState & ~kSubStateFlag) != 0;
  case kModeRespawn:
    return before.subState == 0;
  case kModeScripted:
    return before.subState == 0 || before.subState == kScriptEnding;
  default:
    return false;
  }
}

bool FrameCut::sameShot(const ScriptedCamera &before, const ScriptedCamera &after) {
  std::int64_t squared = 0;
  for (int axis = 0; axis < 3; ++axis) {
    const std::int64_t step = std::int64_t{after.position[axis]} - before.position[axis];
    squared += step * step;
  }
  auto turn = static_cast<std::int32_t>(
      static_cast<std::uint32_t>(after.angles[2] - before.angles[2]) & kAngleMask);
  if (turn > kAngleHalfTurn) {
    turn -= kAngleFullTurn;
  }
  return squared < kShotStepSquared && turn >= -kShotTurn && turn <= kShotTurn;
}

std::uint32_t FrameCut::sceneState(Core &core) const {
  const std::uint32_t state = core.mem_r32(facts_.gameState);
  if (!facts_.levelLoad || state != facts_.levelLoad->gameState) {
    return state;
  }
  const std::uint32_t subState = core.mem_r32(facts_.levelLoad->subState);
  for (const std::uint32_t field : facts_.levelLoad->fieldSubStates) {
    if (subState == field) {
      return facts_.levelLoad->fieldGameState;
    }
  }
  return state;
}

void FrameCut::onFrameDrawn(Core &core) {
  walked_ = SceneIdentity{
      .gameState = sceneState(core),
      .levelId = core.mem_r32(facts_.levelId),
  };
  if (facts_.camera) {
    walkedCamera_ = CameraMachineWords{
        .mode = core.mem_r32(facts_.camera->mode),
        .subState = core.mem_r32(facts_.camera->subState),
    };
  }
  walkedShot_.reset();
  if (facts_.scriptedCamera && walked_->gameState == facts_.scriptedCamera->gameState) {
    ScriptedCamera shot;
    for (std::uint32_t axis = 0; axis < 3; ++axis) {
      shot.position[axis] =
          static_cast<std::int32_t>(core.mem_r32(facts_.scriptedCamera->position + axis * 4));
      shot.angles[axis] =
          static_cast<std::int16_t>(core.mem_r16(facts_.scriptedCamera->angles + axis * 2));
    }
    walkedShot_ = shot;
  }
}

void FrameCut::onFrameTail(Core &) {
  // A record that walked no table is no scene change; the presenter never shows it.
  if (!walked_.has_value()) {
    cut_ = false;
    return;
  }
  const bool shotChanged = sealedShot_ && walkedShot_ && !sameShot(*sealedShot_, *walkedShot_);
  cut_ = !sealed_.has_value() || *sealed_ != *walked_ ||
         cameraPlaced(sealedCamera_, walkedCamera_) || shotChanged;
  const ScriptedCamera shot = walkedShot_.value_or(ScriptedCamera{});
  lucent::debug("cut",
                "cut={} state={} level={} camera={}/{} shot={} at=({}, {}, {}) angles=({}, {}, {})",
                cut_,
                walked_->gameState,
                walked_->levelId,
                walkedCamera_.mode,
                walkedCamera_.subState,
                shotChanged,
                shot.position[0],
                shot.position[1],
                shot.position[2],
                shot.angles[0],
                shot.angles[1],
                shot.angles[2]);
  sealed_ = walked_;
  sealedCamera_ = walkedCamera_;
  sealedShot_ = walkedShot_;
  walked_.reset();
}

} // namespace spyro
