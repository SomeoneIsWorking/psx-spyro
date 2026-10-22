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

constexpr std::uint32_t kBootTurnsPerStep = 4;
constexpr std::uint32_t kFrameTurnsPerStep = 8;
// A step ends at the first display wait a call stops at, so this only bounds a loop whose
// iteration waits for nothing.
constexpr int kMaxLoopCallsPerStep = 4;

} // namespace

BootPrefixFrameDriver::BootPrefixFrameDriver(Game &game, const BootPrefixFacts &facts)
    : core_(game.core), facts_(facts), fields_(game, facts.field), cut_(facts.cut) {}

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
  if (!enterCall(facts_.bootPrefix)) {
    // Refused before any guest work; the run is stalled by name rather than aborted.
    bootStalled_ = true;
    core_.game->run.requestEnd();
    return;
  }
  lucent::info("boot-native",
               "{} enters the retail boot prefix 0x{:08X}; libetc VSync stays a frame boundary",
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

void BootPrefixFrameDriver::deliverField(const char *site, bool present, bool pace) {
  if (!fields_.deliver({.site = site, .present = present, .pace = pace})) {
    lucent::error(
        "frameloop", "{}: field owner refused the field at {}", facts_.field.titleName, site);
    std::abort();
  }
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

BootPrefixFrameDriver::CallProgress
BootPrefixFrameDriver::runCall(Core &core,
                               psx::cpu::ResumableGuestCall &call,
                               std::uint32_t maxTurns,
                               const char *site,
                               std::uint64_t fieldBound) {
  for (std::uint32_t turn = 0; turn < maxTurns;) {
    const psx::cpu::CallStep step = call.advance();
    if (step.outcome == psx::cpu::CallOutcome::Returned) {
      return CallProgress::Returned;
    }
    if (step.outcome == psx::cpu::CallOutcome::Refused) {
      reportStop(core, call, step);
      std::abort();
    }
    if (step.reason == psx::cpu::ExecutionExitReason::FrameBoundary) {
      // A guest display wait is one field. The one reaching the bound is the step's own, delivered
      // by the step; the others are real time and the guest's own vblank work, not presented, and a
      // boot call paces each. The fade loop draws a frame per field, so its wait is the step's own.
      if (call.resumePc() == facts_.fadeFrameWait) {
        return CallProgress::FadeFrame;
      }
      if (fields_.fields() + 1u >= fieldBound) {
        return CallProgress::FieldBoundReached;
      }
      deliverField(site, false, phase_ == Phase::Boot);
      continue;
    }
    // A suspension that is not a display wait is one host turn without a field: the step's turn
    // budget, not the field bound.
    return CallProgress::TurnBudgetReached;
  }
  return CallProgress::TurnBudgetReached;
}

void BootPrefixFrameDriver::stepBoot(Core &core) {
  const std::uint64_t fieldsBefore = fields_.fields();
  // Checked first, before any guest work, because a guest that polls instead of asking for fields
  // never reaches the delivery site the field bound lives on.
  if (bootSteps_ >= facts_.bootStepLimit) {
    bootStalled_ = true;
    lucent::error("frameloop",
                  "{}'s boot prefix ran {} step(s) without returning and delivered {} "
                  "field(s) in total (step bound {}); it is polling rather than waiting, so the "
                  "field bound cannot see it -- ending the run at resume 0x{:08X}",
                  facts_.field.titleName,
                  bootSteps_,
                  fields_.fields(),
                  facts_.bootStepLimit,
                  call_.resumePc());
    core.game->run.requestEnd();
    // Not an early return: a step presents its one field whatever the guest did.
  }
  // The bound is a budget for THIS call, not the step's running total: a boot that legitimately
  // delivers 200 fields across ten steps must not be refused at step ten for having delivered 200.
  // A refused-at-entry boot holds no call at all, which is what distinguishes it here.
  if (bootStalled_ && call_.entry() == 0u) {
    return;
  }
  const auto progress =
      bootStalled_ ? CallProgress::TurnBudgetReached
                   : runCall(core, call_, kBootTurnsPerStep, "boot", facts_.bootStepFieldLimit);
  if (progress == CallProgress::Returned) {
    phase_ = Phase::MainLoop;
    lucent::info("boot-native",
                 "{}'s retail boot prefix returned after {} step(s) and {} field(s); the "
                 "per-frame update 0x{:08X} and draw 0x{:08X} now own each step",
                 facts_.field.titleName,
                 steps_,
                 fields_.fields(),
                 facts_.frameUpdate,
                 facts_.frameDraw);
  } else if (progress == CallProgress::FieldBoundReached) {
    // A boot prefix that spends its whole field budget without returning is not making progress, so
    // the run ends by name instead of spinning on it.
    bootStalled_ = true;
    lucent::error("frameloop",
                  "{}'s boot prefix delivered {} field(s) in one call without returning and "
                  "gave up on the guest at resume 0x{:08X} (bound {}); ending the run",
                  facts_.field.titleName,
                  fields_.fields() - fieldsBefore,
                  call_.resumePc(),
                  facts_.bootStepFieldLimit);
    core.game->run.requestEnd();
  } else {
    ++bootSteps_;
  }
}

std::uint32_t BootPrefixFrameDriver::entryOf(LoopCall call) const {
  return call == LoopCall::Update ? facts_.frameUpdate : facts_.frameDraw;
}

const char *BootPrefixFrameDriver::siteOf(LoopCall call) const {
  return call == LoopCall::Update ? "frame-update" : "frame-draw";
}

BootPrefixFrameDriver::LoopStop BootPrefixFrameDriver::stepMainLoop(Core &core) {
  // One step advances the retail loop to its next field or to the return of the frame's draw,
  // resuming whichever call the previous step left suspended. Resuming a suspended draw as the
  // update would leave the update unreachable after its first iteration.
  for (int calls = 0; calls < kMaxLoopCallsPerStep; ++calls) {
    if (!call_.pending() && loopCall_ == LoopCall::Update) {
      fields_.beginLogicFrame();
      session(core).warp->applyArmed(core, static_cast<std::uint32_t>(steps_));
    }
    if (!call_.pending() && !enterCall(entryOf(loopCall_))) {
      return LoopStop::Field;
    }
    const CallProgress progress =
        runCall(core, call_, kFrameTurnsPerStep, siteOf(loopCall_), fields_.fields() + 1u);
    if (progress == CallProgress::FadeFrame) {
      return LoopStop::FadeFrame;
    }
    if (progress != CallProgress::Returned) {
      return LoopStop::Field;
    }
    if (loopCall_ == LoopCall::Draw) {
      cut_.onFrameDrawn(core);
      loopCall_ = LoopCall::Update;
      return LoopStop::FrameDrawn;
    }
    loopCall_ = LoopCall::Draw;
  }
  return LoopStop::Field;
}

void BootPrefixFrameDriver::presentField(Core &core, std::uint32_t frame, const char *site) {
  // The guest's last command of the step has executed; the record is sealed next.
  cut_.onFrameTail(core);
  deliverField(site, true, true);
  // Without a logic-frame clock OtAttr never drops its packet spans and attributes reused pool
  // words to stale writers.
  core.rsub.otAttr.beginLogicFrame(frame);
}

void BootPrefixFrameDriver::deferField(Core &core, const char *site) {
  deliverField(site, false, false);
  core.game->presentation.commitUnpresented();
}

bool BootPrefixFrameDriver::save(psx::platform::BlobWriter &out, std::string &) const {
  out.u8(static_cast<std::uint8_t>(phase_));
  out.u8(static_cast<std::uint8_t>(loopCall_));
  out.boolean(bootStalled_);
  out.u64(bootSteps_);
  call_.save(out);
  return true;
}

bool BootPrefixFrameDriver::load(psx::platform::BlobReader &in, std::string &error) {
  const std::uint8_t phase = in.u8();
  const std::uint8_t loopCall = in.u8();
  Resume resume{.bootStalled = in.boolean(), .bootSteps = in.u64()};
  if (!resume.call.load(core_, in, error)) {
    return false;
  }
  if (phase > static_cast<std::uint8_t>(Phase::MainLoop) ||
      loopCall > static_cast<std::uint8_t>(LoopCall::Draw)) {
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
  phase_ = staged_->phase;
  loopCall_ = staged_->loopCall;
  bootStalled_ = staged_->bootStalled;
  bootSteps_ = staged_->bootSteps;
  call_ = std::move(staged_->call);
  staged_.reset();
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
    presentField(core, frame, "boot-tail");
  } else {
    switch (stepMainLoop(core)) {
    case LoopStop::FrameDrawn:
      cut_.onFrameTail(core);
      fields_.presentFrame();
      core.rsub.otAttr.beginLogicFrame(frame);
      break;
    case LoopStop::Field:
      // A field inside a logic frame passes unpresented, so the frame is presented over all its
      // fields with in-betweens. Past the guest's delta clamp the frame is a stall, and each
      // further field shows what VRAM holds, as the display would.
      if (static_cast<int>(fields_.fieldsThisLogicFrame()) < facts_.deltaClampFields) {
        deferField(core, siteOf(loopCall_));
      } else {
        presentField(core, frame, siteOf(loopCall_));
      }
      break;
    case LoopStop::FadeFrame:
      presentField(core, frame, siteOf(loopCall_));
      break;
    }
  }

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
