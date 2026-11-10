#pragma once

#include "core/field_owner.h"
#include "core/frame_cut.h"
#include "game_runtime.h"
#include "machine_state.h"
#include "replay_frame.h"
#include "spyro1/core/frame/boot_sequence.h"
#include "spyro1/core/frame/draw_motion.h"
#include "spyro1/core/frame/draw_section.h"
#include "spyro1/core/frame/frame_draw.h"
#include "spyro1/core/frame/skip_map.h"
#include "spyro1/core/frame/transition_skip.h"
#include "spyro1/core/guest_globals.h"

#include <cstdint>
#include <optional>

class Game;

namespace spyro1 {

// GamestateDraw switches on g_Gamestate; the re-placements are declared (draw_cuts.h).
inline constexpr spyro::FrameCutFacts kFrameCutFacts{.gameState = guest::kGamestate,
                                                     .levelId = guest::kLevelId};

class FrameDriver final : public psx::game::FrameDriver, public psx::state::NativeStatePort {
public:
  explicit FrameDriver(Game &game);

  void initialize(Core &core);
  void stepFrame(Core &core, std::uint32_t frame) override;
  [[nodiscard]] bool pastBootPrefix() const override;

  [[nodiscard]] const spyro::FrameCut &cut() const {
    return cut_;
  }
  // The guest re-placed the camera or Spyro: this record is a cut and no sample pairs across it.
  void declareCut() {
    cut_.declare();
    replay_.forget();
  }
  // GS_Playing's and the title screen's in-betweens, replayed from the draw section.
  [[nodiscard]] psx::present::ReplayFrame &replay() {
    return replay_;
  }

  // A state is taken past the boot sequence; its guest state lives in RAM, the gameplay frame here.
  [[nodiscard]] const char *sectionName() const override {
    return "spyro1.frame-driver";
  }
  [[nodiscard]] std::uint32_t version() const override {
    return 1;
  }
  bool save(psx::platform::BlobWriter &out, std::string &error) const override;
  bool load(psx::platform::BlobReader &in, std::string &error) override;
  void restored(Core &core) override;

private:
  // One retail main-loop iteration up to its draw gate; returns g_StateSwitch, which skips the
  // draw.
  bool runGuestUpdate(Core &core);

  SkipMap skipMap_;
  spyro::FieldOwner fields_;
  BootSequence boot_;
  TransitionSkip transitions_;
  FrameDraw draw_;
  DrawSection section_;
  DrawMotion motion_;
  psx::present::ReplayFrame replay_{section_, motion_.tracks(), motion_.sources()};
  spyro::FrameCut cut_;
  std::uint32_t gameplayFrame_ = 0;
  std::optional<std::uint32_t>
      stagedFrame_; // a loaded state's gameplay frame until every section is in
};

} // namespace spyro1
