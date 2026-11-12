#include "core/boot_prefix_frame_driver.h"

#include "core.h"
#include "core/session.h"
#include "game.h"

#include "blob_codec.h"
#include "execution_exit.h"

#include <cstdlib>
#include <lucent/log.h>

namespace spyro {
namespace {

// The boot prefix is finite but long: it brings up the display, walks the loader chain and
// dispatches the loaded module.
constexpr std::uint32_t kGuestRamLow = 0x80000000u;
constexpr std::uint32_t kGuestRamHigh = 0x80200000u;

// A turn runs one field of guest cycles, so a running host clock delivers a field within two; more
// full turns than this without one means the clock is held.
constexpr std::uint32_t kTurnsWithoutField = 8;

} // namespace

BootPrefixFrameDriver::BootPrefixFrameDriver(Game &game,
                                             const BootPrefixFacts &facts,
                                             const DrawSection &section,
                                             const DrawMotionFacts &motion)
    : core_(game.core), facts_(facts), fields_(game, facts.field), cut_(facts.cut),
      logoHolds_(facts.logoHolds), section_(section), motion_(game.core, motion),
      replay_(section_, motion_.tracks(), motion_.sources()) {}

void BootPrefixFrameDriver::initialize() {
  if (initialized_) {
    lucent::error("boot-native", "{} frame driver initialized twice", facts_.field.titleName);
    std::abort();
  }
  initialized_ = true;
  if (facts_.deltaClampFields < 2) {
    lucent::error("boot-native", "{} names no frame delta clamp", facts_.field.titleName);
    std::abort();
  }
  fields_.publish();
  fields_.armHostClock();
  if (!enterCall(facts_.bootPrefix)) {
    // Refused before any guest work; the run is stalled by name rather than aborted.
    bootStalled_ = true;
    core_.game->run.requestEnd();
    return;
  }
  lucent::info("boot-native",
               "{} enters the retail boot prefix 0x{:08X}; fields follow the guest clock",
               facts_.field.titleName,
               facts_.bootPrefix);
}

bool BootPrefixFrameDriver::enterCall(std::uint32_t entry) {
  // A host-entered call has no guest caller to return to, so `$r[31]` carries whatever the host
  // last had there and the executor ends the call when the guest PC EQUALS it. A host pointer
  // cannot equal a KSEG0 guest PC, which is what makes the call finite.
  const std::uint32_t returnPc = core_.r[31];
  if (returnPc >= kGuestRamLow && returnPc < kGuestRamHigh) {
    lucent::error(
        "boot-native",
        "{}: refusing to enter {} with `$r[31]` = 0x{:08X} inside guest RAM; the call would "
        "stop at an address it never reaches",
        facts_.field.titleName,
        facts_.callName,
        returnPc);
    return false;
  }
  // The turn cap is the title's bound; this driver ends a call on its own field and step bounds.
  call_.begin(core_,
              facts_.callName,
              psx::cpu::GuestCallSite{entry, returnPc},
              psx::cpu::kUnboundedCallTurns);
  return true;
}

void BootPrefixFrameDriver::reportStop(Core &core,
                                       const psx::cpu::ResumableGuestCall &call,
                                       const psx::cpu::CallStep &step) const {
  lucent::error("frameloop",
                "{} stopped: phase={} step={} call 0x{:08X} active={} turns={} cycles={} "
                "exit={} at guest pc=0x{:08X} ra=0x{:08X} sp=0x{:08X} fields={} presents={}",
                facts_.field.titleName,
                phase_ == Phase::Boot ? "boot" : "main-loop",
                steps_,
                call.entry(),
                call.pending() ? 1 : 0,
                call.turns(),
                call.cycles(),
                step.detail.empty() ? "-" : step.detail,
                step.guestPc,
                core.r[31],
                core.r[29],
                fields_.fields(),
                fields_.presents());
}

BootPrefixFrameDriver::TurnEnd BootPrefixFrameDriver::advanceTurn(Core &core, const char *site) {
  const psx::cpu::CallStep step = call_.advance();
  if (step.outcome == psx::cpu::CallOutcome::Returned) {
    return TurnEnd::Returned;
  }
  if (step.outcome == psx::cpu::CallOutcome::Refused) {
    reportStop(core, call_, step);
    std::abort();
  }
  if (step.reason != psx::cpu::ExecutionExitReason::FrameBoundary) {
    return TurnEnd::Ran;
  }
  if (!fields_.deliver({.site = site, .present = false, .pace = false})) {
    lucent::error("frameloop",
                  "{}: field owner refused the wait's field at {}",
                  facts_.field.titleName,
                  site);
    std::abort();
  }
  return call_.resumePc() == facts_.fadeFrameWait ? TurnEnd::FadeFrame : TurnEnd::Waited;
}

void BootPrefixFrameDriver::requireClock(Core &core, std::uint32_t turnsWithoutField) const {
  if (turnsWithoutField <= kTurnsWithoutField) {
    return;
  }
  lucent::error("frameloop",
                "{} step {} ran {} full turns without a field at resume 0x{:08X} pc=0x{:08X}; the "
                "host clock is held",
                facts_.field.titleName,
                steps_,
                turnsWithoutField,
                call_.resumePc(),
                core.pc);
  std::abort();
}

void BootPrefixFrameDriver::stepBoot(Core &core) {
  if (bootStalled_) {
    return;
  }
  const std::uint64_t fieldsBefore = fields_.fields();
  for (std::uint32_t idle = 0; fields_.fields() == fieldsBefore;) {
    const std::uint64_t fieldsAtTurn = fields_.fields();
    const TurnEnd end = advanceTurn(core, "boot");
    if (end == TurnEnd::Returned) {
      phase_ = Phase::MainLoop;
      lucent::info("boot-native",
                   "{}'s retail boot prefix returned after {} field(s); the per-frame update "
                   "0x{:08X} and draw 0x{:08X} now own each step",
                   facts_.field.titleName,
                   fields_.fields(),
                   facts_.frameUpdate,
                   facts_.frameDraw);
      return;
    }
    idle = fields_.fields() != fieldsAtTurn ? 0u : idle + (end == TurnEnd::Ran ? 1u : 0u);
    requireClock(core, idle);
  }
  logoHolds_.observe(core, call_, fields_.presentationSkipPressed());
  if (fields_.fields() >= facts_.bootFieldLimit) {
    // A boot prefix that spends its whole field budget without returning is not making progress, so
    // the run ends by name instead of spinning on it.
    bootStalled_ = true;
    lucent::error("frameloop",
                  "{}'s boot prefix took {} field(s) without returning (bound {}) at resume "
                  "0x{:08X}; ending the run",
                  facts_.field.titleName,
                  fields_.fields(),
                  facts_.bootFieldLimit,
                  call_.resumePc());
    core.game->run.requestEnd();
  }
}

std::uint32_t BootPrefixFrameDriver::entryOf(LoopCall call) const {
  return call == LoopCall::Draw ? facts_.frameDraw : facts_.frameUpdate;
}

const char *BootPrefixFrameDriver::siteOf(LoopCall call) const {
  switch (call) {
  case LoopCall::Update:
    return "frame-update";
  case LoopCall::Draw:
    return "frame-draw";
  case LoopCall::Entry:
    return "debug-entry";
  }
  return "frame-update";
}

void BootPrefixFrameDriver::stepMainLoop(Core &core) {
  // Resumes whichever call the previous step left suspended: resuming a suspended draw as the
  // update would leave the update unreachable after its first iteration.
  const std::uint64_t fieldsBefore = fields_.fields();
  for (std::uint32_t idle = 0;;) {
    if (!call_.pending()) {
      if (loopCall_ == LoopCall::Update) {
        fields_.beginLogicFrame();
        Warp &warp = *session(core).warp;
        warp.applyArmed(core, static_cast<std::uint32_t>(steps_));
        if (const std::optional<Warp::Entry> entry = warp.takeEntry()) {
          core.r[4] = entry->argument;
          loopCall_ = LoopCall::Entry;
          if (!enterCall(entry->function)) {
            std::abort();
          }
        }
      }
      if (loopCall_ == LoopCall::Draw) {
        beforeDraw(core);
      }
      if (!call_.pending() && !enterCall(entryOf(loopCall_))) {
        std::abort();
      }
    }
    const std::uint64_t fieldsAtTurn = fields_.fields();
    const TurnEnd end = advanceTurn(core, siteOf(loopCall_));
    switch (end) {
    case TurnEnd::Returned:
      if (loopCall_ == LoopCall::Draw) {
        cut_.onFrameDrawn(core);
        loopCall_ = LoopCall::Update;
        return;
      }
      // The update of the same logic frame follows an entry, without a second frame begin.
      loopCall_ = loopCall_ == LoopCall::Entry ? LoopCall::Update : LoopCall::Draw;
      break;
    case TurnEnd::FadeFrame:
      return;
    case TurnEnd::Ran:
    case TurnEnd::Waited:
      break;
    }
    // Past the guest's delta clamp the frame is a stall, and each further field shows what VRAM
    // holds, as the display would.
    if (fields_.fields() != fieldsBefore &&
        static_cast<int>(fields_.fieldsThisLogicFrame()) > facts_.deltaClampFields) {
      return;
    }
    idle = fields_.fields() != fieldsAtTurn ? 0u : idle + (end == TurnEnd::Ran ? 1u : 0u);
    requireClock(core, idle);
  }
}

void BootPrefixFrameDriver::beforeDraw(Core &core) {
  const std::uint32_t state = section_.gameState(core);
  if (const std::optional<std::string_view> reason = section_.declines(core)) {
    ++declined_[*reason];
    replay_.forget();
    sampledState_.reset();
    return;
  }
  if (sampledState_ != state) {
    replay_.forget();
  }
  sampledState_ = state;
  replay_.beforeDraw(core);
}

void BootPrefixFrameDriver::reportDeclined() const {
  for (const auto &[reason, frames] : declined_) {
    lucent::info("replay", "run-end: {} drawn frame(s) not replayed: {}", frames, reason);
  }
}

bool BootPrefixFrameDriver::save(psx::platform::BlobWriter &out, std::string &) const {
  out.u8(static_cast<std::uint8_t>(phase_));
  out.u8(static_cast<std::uint8_t>(loopCall_));
  out.boolean(bootStalled_);
  call_.save(out);
  return true;
}

bool BootPrefixFrameDriver::load(psx::platform::BlobReader &in, std::string &error) {
  const std::uint8_t phase = in.u8();
  const std::uint8_t loopCall = in.u8();
  Resume resume{.bootStalled = in.boolean()};
  if (!resume.call.load(core_, in, error)) {
    return false;
  }
  if (phase > static_cast<std::uint8_t>(Phase::MainLoop) ||
      loopCall > static_cast<std::uint8_t>(LoopCall::Entry)) {
    error = lucent::format(
        "{}: phase {} and loop call {} name no driver state", sectionName(), phase, loopCall);
    return false;
  }
  resume.phase = static_cast<Phase>(phase);
  resume.loopCall = static_cast<LoopCall>(loopCall);
  staged_ = std::move(resume);
  return true;
}

void BootPrefixFrameDriver::restored(Core &) {
  if (!staged_) {
    lucent::error("frameloop", "{}: restored without a loaded driver state", sectionName());
    std::abort();
  }
  phase_ = staged_->phase;
  loopCall_ = staged_->loopCall;
  bootStalled_ = staged_->bootStalled;
  call_ = std::move(staged_->call);
  staged_.reset();
  replay_.forget();
  marginSectors_.clear();
  sampledState_.reset();
}

void BootPrefixFrameDriver::stepFrame(Core &core, std::uint32_t frame) {
  if (!initialized_) {
    lucent::error(
        "frameloop", "{} step {} ran before boot initialization", facts_.field.titleName, frame);
    std::abort();
  }
  ++steps_;
  if (phase_ == Phase::Boot) {
    stepBoot(core);
  }
  // The boot's last field and the first logic frame share a step.
  if (phase_ == Phase::MainLoop) {
    stepMainLoop(core);
  }
  // The guest's last command of the step has executed; the record is sealed next.
  cut_.onFrameTail(core);
  fields_.presentFrame();
  // Without a logic-frame clock OtAttr never drops its packet spans and attributes reused pool
  // words to stale writers.
  core.rsub.otAttr.beginLogicFrame(frame);

  lucent::debug("boot",
                "{} step {} phase={} call_active={} resume=0x{:08X} turns={} cycles={} "
                "fields={} presents={}",
                facts_.field.titleName,
                frame,
                phase_ == Phase::Boot ? "boot" : "main-loop",
                call_.pending() ? 1 : 0,
                call_.resumePc(),
                call_.turns(),
                call_.cycles(),
                fields_.fields(),
                fields_.presents());
}

} // namespace spyro
