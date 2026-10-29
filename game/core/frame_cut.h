// Whether a sealed record is a cut, for the 60 fps in-between.
//
// A record is a cut when its scene is not the previous record's: another game state (the per-field
// draw's switch operand, with the level-load state's fades over the field counted as the field) or
// another level, or, where the title names its camera machine, when the camera was re-placed inside
// one scene. The Spyro 2/3 camera controllers (SCUS_944.25 FUN_8001FA58, SCUS_944.67 FUN_800144B4)
// place the camera only at three steps of the same mode machine, each after the guest's own
// full-screen fade:
//   mode 9    sub-state 0 -> 1 (bit 7 aside): the saved camera
//   mode 0xB  sub-state 0 -> 1: the player and camera at the respawn position
//   mode 10   sub-state 0 -> 1: the scripted camera begins, and
//             sub-state 0x80 -> 0x81: it ends and the gameplay camera returns
// Every other mode change keeps the camera where it was. A cutscene camera is not the controller's:
// the cutscene overlays (state 6; Spyro 2 0x8007490C, Spyro 3's blend test at 0x80077E0C) play a
// track of keys, one per two ticks, and blend toward the next key on an odd tick only when both are
// one shot (FrameCut::sameShot); a record whose camera fails that test against the previous one is
// a cut. The scene and the camera are sampled when the draw returns, right after its DrawOTag
// walked the table, because the draw that walks a table often resumes a step after the one that
// built it.
#pragma once

#include <array>
#include <cstdint>
#include <optional>

class Core;

namespace spyro {

// The camera controller's mode machine: the guest word addresses in the facts, their values when
// sampled.
struct CameraMachineWords {
  std::uint32_t mode = 0;
  std::uint32_t subState = 0;
};

// A cutscene state whose camera a key track places: the state, and the camera's position (three
// words) and angles (three halfwords) the track writes.
struct ScriptedCameraFacts {
  std::uint32_t gameState = 0;
  std::uint32_t position = 0;
  std::uint32_t angles = 0;
};

// The level-load state: it fades out over the field, loads behind black, and fades in over the
// loaded field, which the field state then continues. Its draw draws the field scene at two of its
// sub-states.
struct LevelLoadFacts {
  std::uint32_t gameState = 0;
  std::uint32_t subState = 0;
  std::uint32_t fieldGameState = 0;
  std::array<std::uint32_t, 2> fieldSubStates{};
};

// The guest words a title's cut is read from. Spyro 1's camera machine is not read.
struct FrameCutFacts {
  std::uint32_t gameState = 0;
  std::uint32_t levelId = 0;
  std::optional<CameraMachineWords> camera;
  std::optional<ScriptedCameraFacts> scriptedCamera;
  std::optional<LevelLoadFacts> levelLoad;
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

  // A cutscene camera when sampled.
  struct ScriptedCamera {
    std::int32_t position[3]{};
    std::int16_t angles[3]{};
  };

  // The scene a walked game state shows: a level-load record drawing the field is the field state's
  // scene.
  [[nodiscard]] std::uint32_t sceneState(Core &core) const;
  // Whether the camera controller placed the camera between two samples of its mode machine.
  [[nodiscard]] static bool cameraPlaced(const CameraMachineWords &before,
                                         const CameraMachineWords &after);
  // The track player's own test for blending two keys (0x80074F4C..0x80074F74): the position moved
  // less than 0x100 (0x8001BA20 over x, y, z) and the third angle at most 0xFF (0x8001BF00's fold).
  [[nodiscard]] static bool sameShot(const ScriptedCamera &before, const ScriptedCamera &after);

  FrameCutFacts facts_;
  std::optional<SceneIdentity> walked_; // the last table walked this step
  std::optional<SceneIdentity> sealed_; // the last table a sealed record holds
  CameraMachineWords walkedCamera_;
  CameraMachineWords sealedCamera_;
  std::optional<ScriptedCamera> walkedShot_; // set while the walked scene is the cutscene state
  std::optional<ScriptedCamera> sealedShot_;
  bool cut_ = true;
};

} // namespace spyro
