#pragma once

#include "core/field_owner.h"
#include "core/frame_cut.h"
#include "game_runtime.h"
#include "machine_state.h"
#include "resumable_guest_call.h"

#include <cstdint>
#include <optional>

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
  FieldOwnerFacts field;
  FrameCutFacts cut;
  std::uint64_t bootStepFieldLimit = kDefaultBootStepFieldLimit;
  std::uint64_t bootStepLimit = kDefaultBootStepLimit;
};

// One product step of a title whose retail executable IS its boot. The host owns the FIELD: every
// guest VSync comes back as a frame-boundary exit this driver satisfies with one delivered field,
// so one step presents exactly one field. The host-turn clock stays unarmed because the guest's
// own VSync is the only field source the boot uses. The call in flight is machine state: a save
// taken while it is suspended resumes it, not the live one.
class BootPrefixFrameDriver final : public psx::game::FrameDriver,
                                    public psx::state::NativeStatePort {
public:
  BootPrefixFrameDriver(Game &game, const BootPrefixFacts &facts);

  void initialize();
  void stepFrame(Core &core, std::uint32_t frame) override;

  // True once the retail boot prefix (logos, publisher cards, loading) has returned.
  [[nodiscard]] bool pastBootPrefix() const override {
    return phase_ == Phase::MainLoop;
  }

  [[nodiscard]] const FrameCut &cut() const {
    return cut_;
  }

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
  // Why one retail call ended: it returned, ran out of host turns, or hit the field bound.
  enum class CallProgress : std::uint8_t { Returned, TurnBudgetReached, FieldBoundReached };
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
  void stepMainLoop(Core &core);
  [[nodiscard]] std::uint32_t entryOf(LoopCall call) const;
  [[nodiscard]] const char *siteOf(LoopCall call) const;
  void reportStop(Core &core,
                  const psx::cpu::ResumableGuestCall &call,
                  const psx::cpu::CallStep &step) const;

  Core &core_;
  BootPrefixFacts facts_;
  FieldOwner fields_;
  FrameCut cut_;
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
