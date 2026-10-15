// Whether a sealed record is a cut, for the 60 fps in-between.
//
// A record is a cut when its scene is not the previous record's: another game state (the per-field
// draw's switch operand) or another level, or, where the title names its camera machine, when the
// camera was re-placed inside one scene. The Spyro 2/3 camera controllers (SCUS_944.25
// FUN_8001FA58, SCUS_944.67 FUN_800144B4) place the camera only at three steps of the same mode
// machine, each after the guest's own full-screen fade:
//   mode 9    sub-state 0 -> 1 (bit 7 aside): the saved camera
//   mode 0xB  sub-state 0 -> 1: the player and camera at the respawn position
//   mode 10   sub-state 0 -> 1: the scripted camera begins, and
//             sub-state 0x80 -> 0x81: it ends and the gameplay camera returns
// Every other mode change keeps the camera where it was. The scene and the camera are sampled when
// the draw returns, right after its DrawOTag walked the table, because the draw that walks a table
// often resumes a step after the one that built it.
#pragma once

#include <cstdint>
#include <optional>

class Core;

namespace spyro {

// The camera controller's mode machine words.
struct CameraMachineWords {
  std::uint32_t mode = 0;
  std::uint32_t subState = 0;
};

// The guest words a title's cut is read from. Spyro 1's camera machine is not read.
struct FrameCutFacts {
  std::uint32_t gameState = 0;
  std::uint32_t levelId = 0;
  std::optional<CameraMachineWords> camera;
};

// Owned by the title's frame driver, so a new session's first record is a cut.
class FrameCut final {
public:
  explicit FrameCut(const FrameCutFacts &facts) : facts_(facts) {}

  // The per-frame draw returned; its last act walked the ordering table it built.
  void onFrameDrawn(Core &core);
  // The step's guest work is done and its record is about to be sealed.
  void onFrameTail(Core &core);
  // Whether the record sealed after the latest frame tail is a cut from the one before it.
  [[nodiscard]] bool isCut() const {
    return cut_;
  }

private:
  struct SceneIdentity {
    std::uint32_t gameState = 0;
    std::uint32_t levelId = 0;
    bool operator==(const SceneIdentity &) const = default;
  };

  struct CameraMachine {
    std::uint32_t mode = 0;
    std::uint32_t subState = 0;
  };

  // Whether the camera controller placed the camera between two samples of its mode machine.
  [[nodiscard]] static bool cameraPlaced(const CameraMachine &before, const CameraMachine &after);

  FrameCutFacts facts_;
  std::optional<SceneIdentity> walked_; // the last table walked this step
  std::optional<SceneIdentity> sealed_; // the last table a sealed record holds
  CameraMachine walkedCamera_;
  CameraMachine sealedCamera_;
  bool cut_ = true;
};

} // namespace spyro
