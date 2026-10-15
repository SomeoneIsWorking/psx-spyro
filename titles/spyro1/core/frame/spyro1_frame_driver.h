#pragma once

#include "frame_cut.h"
#include "game_runtime.h"
#include "guest_globals.h"
#include "handoff_store_observer.h"
#include "spyro1_boot_sequence.h"
#include "spyro1_field_scheduler.h"
#include "spyro1_frame_draw.h"
#include "spyro1_transition_skip.h"
#include "stage_update_observer.h"

#include <cstdint>

class Game;

namespace spyro1 {

// GamestateDraw switches on g_Gamestate; Spyro 1's camera machine is not read.
inline constexpr spyro::FrameCutFacts kFrameCutFacts{.gameState = spyro::guest::kGamestate,
                                                     .levelId = spyro::guest::kLevelId};

class Spyro1FrameDriver final : public psx::game::FrameDriver {
public:
  Spyro1FrameDriver(Game &game, bool observeStageUpdate, bool observeHandoffStores);
  ~Spyro1FrameDriver() override;

  void initialize(Core &core);
  void stepFrame(Core &core, std::uint32_t frame) override;

  // True once the boot logos have handed over to the retail main loop.
  [[nodiscard]] bool bootComplete() const;

  FieldScheduler &fields();
  const FieldScheduler &fields() const;
  [[nodiscard]] const spyro::FrameCut &cut() const {
    return cut_;
  }

private:
  // One retail main-loop iteration up to its draw gate: the guest update, then g_DeltaTime and
  // g_UnprocessedFrames bookkeeping. Returns the guest's g_StateSwitch, which skips the draw.
  bool runGuestUpdate(Core &core);

  FieldScheduler fields_;
  BootSequence boot_;
  TransitionSkip transitions_;
  StageUpdateObserver stageObserver_;
  HandoffStoreObserver handoffStoreObserver_;
  FrameDraw draw_;
  spyro::FrameCut cut_;
  std::uint32_t gameplayFrame_ = 0;
};

Spyro1FrameDriver &frameDriver(Core &core);
const Spyro1FrameDriver &frameDriver(const Core &core);

} // namespace spyro1
