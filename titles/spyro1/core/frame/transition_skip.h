#pragma once

#include "core/press_latch.h"

#include <cstdint>

class Core;

namespace spyro1 {

// The observable state a transition screen's cancellation decision is made from.
struct TransitionState {
  std::uint32_t stage = 0;
  std::uint32_t levelTransHudActive = 0;
  // The flyby cards are one branch of one gamestate; only this quadruple identifies them.
  std::uint32_t titleMode = 0;
  std::uint32_t titleState = 0;
  std::uint32_t demoType = 0;
  std::uint32_t loadStage = 0;
  // The loader's music fade (guest_globals.h kCdMusicFlags, kSpuMusicFade*).
  std::uint32_t musicFlags = 0;
  std::int32_t musicFadeTarget = -1;
  std::int32_t musicFadeStep = 0;
  // GS_ACTOR_CUTSCENE's substate and timer (open-spyro actor_cutscene.h).
  std::uint32_t actorCutsceneSubstate = 0;
  std::uint32_t actorCutsceneTimer = 0;
  // GS_Cutscene's index, and its layout's tick as this update will see it (g_DeltaTime added) and
  // duration; read only while the state is GS_Cutscene.
  std::uint32_t cutsceneIdx = 0;
  std::int32_t cutsceneTick = 0;
  std::int32_t cutsceneDuration = 0;
  // GS_Dragon's m_State, m_CutsceneTicks and camera track length.
  std::uint32_t dragonState = 0;
  std::int32_t dragonTicks = 0;
  std::int32_t dragonCameraLength = 0;
  bool skipPressed = false;
};

enum class Cancellation : std::uint8_t {
  None,
  LevelTransitionTally,
  LoadingMusicFade,
  ReturnHomeSequence,
  CutsceneTransitionFlyby,
  CutsceneTransitionIntroCard,
  BalloonistCruise,
  IntroCutscene,
  DragonSpeech,
};

// The balloon's cruise (ACS_CRUISE) owns the screen while its level streams.
bool balloonistCruising(const TransitionState &state);

// True while "IN THE WORLD OF DRAGONS..." or "THE ADVENTURE BEGINS/CONTINUES..." owns the screen,
// loaded or not; the attract demo's flyby is excluded.
bool flybyCardUp(const TransitionState &state);

// The intro cutscene before tick 241, where its own skip refuses Start and Cross.
bool introCutsceneRefusesPress(const TransitionState &state);

// GS_Dragon's speech (m_State 4) before its camera track ends.
bool dragonSpeaking(const TransitionState &state);

// Which transition screen, if any, the current state authorises cancelling.
Cancellation classify(const TransitionState &state);

// Cancels a presentation-only transition screen on a Start or Cross edge in g_Pad.m_Down, the word
// the guest's update reads, through the screen's own guest terminal; a screen whose terminal cannot
// be dispatched (stage 9) is absent. The press that cancels is the screen's: it leaves g_Pad's edge
// words, and a button still held makes no new edge, so the state the screen hands over to does not
// read it.
class TransitionSkip {
public:
  // Called once per logic frame, before the guest frame update reads the transition globals.
  void observe(Core &core);

  std::uint32_t cancellations() const {
    return cancellations_;
  }
  // A press this process latched belongs to no screen of a loaded state.
  void dropHeldPress() {
    flybyPress_.clear();
    cruisePress_.clear();
  }

private:
  void cancel(Core &core, Cancellation cancellation, const TransitionState &state);

  std::uint32_t cancellations_ = 0;
  // A press while a flyby card's load streams, held until its load gate opens; dropped with the
  // card.
  spyro::PressLatch flybyPress_;
  // A press during the cruise, held until the stream reaches its terminal stage.
  spyro::PressLatch cruisePress_;
};

} // namespace spyro1
