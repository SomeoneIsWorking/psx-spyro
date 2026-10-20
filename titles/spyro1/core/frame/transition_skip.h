#pragma once

#include "spyro1/core/frame/press_latch.h"

#include <cstdint>

class Core;

namespace spyro {
class FieldOwner;
}

namespace spyro1 {

// The observable state a transition screen's cancellation decision is made from.
struct TransitionState {
  std::uint32_t stage = 0;
  std::uint32_t levelTransHudActive = 0;
  // The flyby card is one branch of one gamestate; only this quadruple identifies it.
  std::uint32_t titleMode = 0;
  std::uint32_t titleState = 0;
  std::uint32_t demoType = 0;
  std::uint32_t loadStage = 0;
  bool skipPressed = false;
};

enum class Cancellation : std::uint8_t {
  None,
  LevelTransitionTally,
  ReturnHomeSequence,
  CutsceneTransitionFlyby,
};

// True while the "THE ADVENTURE BEGINS/CONTINUES..." flyby owns the screen, loaded or not.
bool flybyCardUp(const TransitionState &state);

// Which transition screen, if any, the current state authorises cancelling.
Cancellation classify(const TransitionState &state);

// Cancels a presentation-only transition screen on Start or Cross through the screen's own guest
// terminal; a screen whose terminal cannot be dispatched (stage 9, docs/issues/0138) is absent.
class TransitionSkip {
public:
  explicit TransitionSkip(spyro::FieldOwner &fields);

  // Called once per logic frame, before the guest frame update reads the transition globals.
  void observe(Core &core);

  std::uint32_t cancellations() const {
    return cancellations_;
  }

private:
  spyro::FieldOwner &fields_;
  std::uint32_t cancellations_ = 0;
  // A press while the flyby's level streams, held until g_LoadStage reaches 13; dropped with the card.
  PressLatch flybyPress_;
};

} // namespace spyro1
