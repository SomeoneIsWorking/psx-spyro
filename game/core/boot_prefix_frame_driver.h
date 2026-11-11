#pragma once

#include "core/draw_motion.h"
#include "core/draw_section.h"
#include "core/field_owner.h"
#include "core/frame_cut.h"
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

// Two bounds catch different stalls: a guest that answers every display wait with another one
// burns the field limit, a guest that never asks for a field only exhausts the turn budget.
inline constexpr std::uint64_t kDefaultBootStepFieldLimit = 1024;
inline constexpr std::uint64_t kDefaultBootStepLimit = 480;

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
  FieldOwnerFacts field;
  FrameCutFacts cut;
  std::uint64_t bootStepFieldLimit = kDefaultBootStepFieldLimit;
  std::uint64_t bootStepLimit = kDefaultBootStepLimit;
};

// One product step of a title whose retail executable IS its boot. The host owns the FIELD: every
// guest VSync comes back as a frame-boundary exit this driver satisfies with one delivered field.
// A boot step presents its last field. A main-loop step ends at a field or at the return of the
// frame's draw, which presents the frame over its fields. The host-turn clock stays unarmed because
// the guest's own VSync is the only field source the boot uses. The call in flight is machine
// state: a save taken while it is suspended resumes it, not the live one. Before each draw the
// replay samples what moves and replays the in-betweens of the frame it ends.
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
    return 1;
  }
  bool save(psx::platform::BlobWriter &out, std::string &error) const override;
  bool load(psx::platform::BlobReader &in, std::string &error) override;
  void restored(Core &core) override;

private:
  enum class Phase : std::uint8_t { Boot, MainLoop };
  // Why one retail call ended: it returned, ran out of host turns, hit the field bound, or waits
  // for the field that shows a frame of the fade loop.
  enum class CallProgress : std::uint8_t {
    Returned,
    TurnBudgetReached,
    FieldBoundReached,
    FadeFrame
  };
  // Where a main-loop step stopped: at the return of the frame's draw, at a field inside the
  // frame, or at a field of the fade loop.
  enum class LoopStop : std::uint8_t { FrameDrawn, Field, FadeFrame };
  // The two retail calls of one main-loop iteration, in the order game main runs them.
  enum class LoopCall : std::uint8_t { Update, Draw };
  // What a loaded state replaces once every machine section is in place.
  struct Resume {
    Phase phase = Phase::Boot;
    LoopCall loopCall = LoopCall::Update;
    bool bootStalled = false;
    std::uint64_t bootSteps = 0;
    psx::cpu::ResumableGuestCall call;
  };

  // Fields this ONE call may have the host deliver before the step calls it stalled; checked here,
  // because a guest that never returns would never reach a bound checked after the loop.
  CallProgress runCall(Core &core,
                       psx::cpu::ResumableGuestCall &call,
                       std::uint32_t maxTurns,
                       const char *site,
                       std::uint64_t fieldBound);
  // False when `$r[31]` would end the call in the middle of itself; no guest work runs.
  bool enterCall(std::uint32_t entry);
  void deliverField(const char *site, bool present, bool pace);
  void stepBoot(Core &core);
  [[nodiscard]] LoopStop stepMainLoop(Core &core);
  // The draw is about to run: the replay samples, or forgets when the body cannot be replayed or
  // the game state changed since the last sample.
  void beforeDraw(Core &core);
  // The step's field shows the frame drawn so far.
  void presentField(Core &core, std::uint32_t frame, const char *site);
  // The step's field passes unpresented; the step still crosses its presentation fence.
  void deferField(Core &core, const char *site);
  [[nodiscard]] std::uint32_t entryOf(LoopCall call) const;
  [[nodiscard]] const char *siteOf(LoopCall call) const;
  void reportStop(Core &core,
                  const psx::cpu::ResumableGuestCall &call,
                  const psx::cpu::CallStep &step) const;

  Core &core_;
  BootPrefixFacts facts_;
  FieldOwner fields_;
  FrameCut cut_;
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
  std::uint64_t bootSteps_ = 0;
  std::optional<Resume> staged_;
};

} // namespace spyro
