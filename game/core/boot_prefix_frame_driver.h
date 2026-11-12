#pragma once

#include "core/draw_motion.h"
#include "core/draw_section.h"
#include "core/field_owner.h"
#include "core/frame_cut.h"
#include "core/logo_hold.h"
#include "game_runtime.h"
#include "machine_state.h"
#include "replay_frame.h"
#include "resumable_guest_call.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string_view>

class Core;
class Game;

namespace spyro {

// The fields the boot prefix may take before the run calls it stalled.
inline constexpr std::uint64_t kDefaultBootFieldLimit = 1024;

// Titles whose retail executable IS their boot; each address is measured from that executable.
struct BootPrefixFacts {
  const char *callName = "";
  std::uint32_t bootPrefix = 0;
  std::uint32_t frameUpdate = 0;
  std::uint32_t frameDraw = 0;
  // The fields the update clamps its frame delta to; a logic frame longer than this is a stall.
  int deltaClampFields = 0;
  // The display wait (VSync return) of the fade loop before a load, which swaps and draws one frame
  // per field.
  std::uint32_t fadeFrameWait = 0;
  // The boot logos' holds a press ends; empty when none is recovered.
  LogoHoldFacts logoHolds;
  FieldOwnerFacts field;
  FrameCutFacts cut;
  std::uint64_t bootFieldLimit = kDefaultBootFieldLimit;
};

// One product step of a title whose retail executable IS its boot. Fields come from the guest
// clock: the host turn delivers one each time the guest's work crosses a field edge, and a guest
// VSync wait (a frame-boundary exit) takes the next one. A boot step presents its field. A
// main-loop step runs one logic frame, update then draw, and presents it over its fields when the
// draw returns; the draw's own wait loop sets the cadence. A field of the fade loop before a load,
// or of a logic frame past the guest's delta clamp, is a step of its own. The call in flight is
// machine state: a save taken while it is suspended resumes it, not the live one. Before each draw
// the replay samples what moves and replays the in-betweens of the frame it ends.
class BootPrefixFrameDriver final : public psx::game::FrameDriver,
                                    public psx::state::NativeStatePort {
public:
  BootPrefixFrameDriver(Game &game,
                        const BootPrefixFacts &facts,
                        const DrawSection &section,
                        const DrawMotionFacts &motion);

  void initialize();
  void stepFrame(Core &core, std::uint32_t frame) override;

  // True once the retail boot prefix (logos, publisher cards, loading) has returned.
  [[nodiscard]] bool pastBootPrefix() const override {
    return phase_ == Phase::MainLoop;
  }

  [[nodiscard]] const FrameCut &cut() const {
    return cut_;
  }
  // The draw body's in-betweens, replayed from its section.
  [[nodiscard]] psx::present::ReplayFrame &replay() {
    return replay_;
  }
  [[nodiscard]] DrawMotion &motion() {
    return motion_;
  }
  // Run end: the drawn frames the section declined, by the guest condition that kept each off.
  void reportDeclined() const;

  [[nodiscard]] const char *sectionName() const override {
    return "spyro.frame-driver";
  }
  [[nodiscard]] std::uint32_t version() const override {
    return 2;
  }
  bool save(psx::platform::BlobWriter &out, std::string &error) const override;
  bool load(psx::platform::BlobReader &in, std::string &error) override;
  void restored(Core &core) override;

private:
  enum class Phase : std::uint8_t { Boot, MainLoop };
  // How one host turn of a retail call ended.
  enum class TurnEnd : std::uint8_t { Returned, Ran, Waited, FadeFrame };
  // The two retail calls of one main-loop iteration, in the order game main runs them, and a debug
  // entry the warp armed, run before the update as one more call.
  enum class LoopCall : std::uint8_t { Update, Draw, Entry };
  // What a loaded state replaces once every machine section is in place.
  struct Resume {
    Phase phase = Phase::Boot;
    LoopCall loopCall = LoopCall::Update;
    bool bootStalled = false;
    psx::cpu::ResumableGuestCall call;
  };

  // One host turn of `call_`; a VSync wait takes its field here, the host turn the others.
  TurnEnd advanceTurn(Core &core, const char *site);
  // Aborts once the step has run more full turns than a running guest clock allows without a field.
  void requireClock(Core &core, std::uint32_t turnsWithoutField) const;
  // False when `$r[31]` would end the call in the middle of itself; no guest work runs.
  bool enterCall(std::uint32_t entry);
  void stepBoot(Core &core);
  // Runs to the return of the frame's draw, or to a field the step presents on its own.
  void stepMainLoop(Core &core);
  // The draw is about to run: the replay samples, or forgets when the body cannot be replayed or
  // the game state changed since the last sample.
  void beforeDraw(Core &core);
  [[nodiscard]] std::uint32_t entryOf(LoopCall call) const;
  [[nodiscard]] const char *siteOf(LoopCall call) const;
  void reportStop(Core &core,
                  const psx::cpu::ResumableGuestCall &call,
                  const psx::cpu::CallStep &step) const;

  Core &core_;
  BootPrefixFacts facts_;
  FieldOwner fields_;
  FrameCut cut_;
  LogoHoldSkip logoHolds_;
  const DrawSection &section_;
  DrawMotion motion_;
  psx::present::ReplayFrame replay_;
  std::optional<std::uint32_t> sampledState_;
  std::map<std::string_view, std::uint64_t> declined_;
  psx::cpu::ResumableGuestCall call_;
  Phase phase_ = Phase::Boot;
  // Which retail call `call_` is, once the main loop runs: a call suspended at a display wait is
  // resumed later, so the step cannot assume it holds the update.
  LoopCall loopCall_ = LoopCall::Update;
  bool initialized_ = false;
  bool bootStalled_ = false;
  std::uint64_t steps_ = 0;
  std::optional<Resume> staged_;
};

} // namespace spyro
