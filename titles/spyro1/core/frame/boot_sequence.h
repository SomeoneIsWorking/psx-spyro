#pragma once

#include "spyro1/core/frame/press_latch.h"

#include <cstdint>

class Core;

namespace spyro {
class FieldOwner;
}

namespace spyro1 {

class SkipMap;

// Resumable transcription of Spyro 1 boot 0x800127C0/0x8001286C: each incomplete step is one field,
// and the zero-field asset and finalisation work folds into an adjacent step.
class BootSequence {
public:
  BootSequence(spyro::FieldOwner &fields, SkipMap &skipMap);

  void initialize(Core &core);
  bool step(Core &core);
  bool complete() const;

private:
  enum class Phase : std::uint8_t {
    FadeFirstIn,
    LoadAssets,
    HoldFirst,
    FadeFirstOut,
    FadeSecondIn,
    AdvanceLoadState,
    HoldSecond,
    FadeSecondOut,
    Finalize,
    Complete,
  };

  void deliverField(Core &core, const char *site);
  void drawLogoField(Core &core, std::uint32_t source, std::uint32_t destination, int offset);
  void loadAssets(Core &core);
  void finalize(Core &core);
  void leaveFirstPresentationHold(Core &core);
  void leaveSecondPresentationHold();
  void observePress();

  spyro::FieldOwner &fields_;
  SkipMap &skipMap_;
  Phase phase_ = Phase::FadeFirstIn;
  std::uint32_t originalStack_ = 0;
  std::uint32_t firstHoldStart_ = 0;
  std::uint32_t secondHoldStart_ = 0;
  std::uint32_t assetBase_ = 0;
  std::uint32_t logoSource_ = 0;
  std::uint32_t logoDestination_ = 0;
  // An edge from a fade or the stage 3->10 loader, held for the next hold phase to take.
  PressLatch pressLatch_;
  int iteration_ = 0;
  bool initialized_ = false;
};

} // namespace spyro1
