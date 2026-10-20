#include "spyro1/core/frame/boot_sequence.h"
#include "spyro1/core/guest_functions.h"
#include "spyro1/core/guest_globals.h"

#include "core.h"
#include "guest_call.h"
#include "core/field_owner.h"
#include "spyro1/core/frame/guest_update_budget.h"
#include "spyro1/core/frame/skip_map.h"

#include <cstdlib>
#include <lucent/log.h>

namespace spyro1 {
namespace {

using spyro1::guest::kCopyWords;
using spyro1::guest::kFill;

constexpr std::uint32_t kBootStackBytes = 72u;
constexpr std::uint32_t kRect24 = 24u;
constexpr std::uint32_t kRect32 = 32u;
constexpr std::uint32_t kLogoBytes = 0x5A000u;
constexpr std::uint32_t kLogoHoldFields = 0xD2u;

void call(Core &core,
          std::uint32_t address,
          std::uint32_t a0 = 0,
          std::uint32_t a1 = 0,
          std::uint32_t a2 = 0,
          std::uint32_t a3 = 0) {
  psx::cpu::dispatchGuestWithArgumentsToReturn(
      core,
      address,
      std::array<std::uint32_t, 4>{a0, a1, a2, a3},
      psx::cpu::ExecutionBudget::fromCycles(kGuestUpdateCycles),
      "boot-native");
}

void writeRect(Core &core,
               std::uint32_t address,
               std::int16_t x,
               std::int16_t y,
               std::int16_t width,
               std::int16_t height) {
  core.mem_w16(address + 0u, static_cast<std::uint16_t>(x));
  core.mem_w16(address + 2u, static_cast<std::uint16_t>(y));
  core.mem_w16(address + 4u, static_cast<std::uint16_t>(width));
  core.mem_w16(address + 6u, static_cast<std::uint16_t>(height));
}

} // namespace

BootSequence::BootSequence(spyro::FieldOwner &fields, SkipMap &skipMap) : fields_(fields), skipMap_(skipMap) {}

void BootSequence::initialize(Core &core) {
  if (initialized_) {
    lucent::error("boot-native", "Spyro 1 boot sequence initialized twice");
    std::abort();
  }
  initialized_ = true;
  originalStack_ = core.r[29];
  core.r[29] -= kBootStackBytes;
  skipMap_.bootSequenceBegin();

  call(core, 0x800122A8u);
  call(core, 0x80012460u);
  call(core, 0x800123C8u);
  call(core, 0x8005595Cu);
  call(core, 0x80012480u);
  call(core, 0x8001256Cu);
  call(core, 0x80062350u);
  call(core, 0x80062618u, 0x100u, 0x78u);
  call(core, 0x80062638u, 0x155u);
  core.mem_w32(0x800785D8u, 0x8007DDE8u);
  core.mem_w32(0x800785DCu, 0x8007DDE8u);
  firstHoldStart_ = static_cast<std::uint32_t>(fields_.counter());
  call(core, 0x8005F6C8u, 0u);
  core.mem_w8(0x80076F4Du, 1u);
  core.mem_w8(0x80076FD1u, 1u);
  call(core, kFill, core.mem_r32(0x800785D8u), 0u, kLogoBytes);
  lucent::info(
      "boot-native", "native boot 0x800127C0/0x8001286C initialized at field {}", firstHoldStart_);
}

bool BootSequence::complete() const {
  return phase_ == Phase::Complete;
}

void BootSequence::deliverField(Core &core, const char *site) {
  if (!fields_.deliver({.site = site, .present = true, .pace = true})) {
    lucent::error("boot-native", "field owner refused boot field at {}", site);
    std::abort();
  }
}

void BootSequence::drawLogoField(Core &core,
                                 std::uint32_t source,
                                 std::uint32_t destination,
                                 int offset) {
  call(core, 0x80017F24u, source, destination, static_cast<std::uint32_t>(offset));
  const std::uint32_t rect = core.r[29] + kRect24;
  writeRect(core, rect, 0, 0, 0x300, 0xF0);
  call(core, 0x8005FA28u, rect, destination);
  call(core, 0x8005F764u, 0u);
  deliverField(core, "boot-logo");
  call(core, 0x80060030u, 0x80076FC0u);
}

void BootSequence::loadAssets(Core &core) {
  call(core, 0x8001250Cu);
  core.mem_w32(core.r[29] + 16u, 600u);
  call(core,
       0x80016500u,
       core.mem_r32(0x80076B90u),
       core.mem_r32(0x800113A0u),
       0x800u,
       core.mem_r32(0x8007A6E8u));
  call(core, kCopyWords, 0x80076C00u, core.mem_r32(0x800113A0u), 0x1D0u);
  assetBase_ = 0x801C0000u - core.mem_r32(0x800755A4u);
  call(core,
       0x80016500u,
       core.mem_r32(0x80076B90u),
       assetBase_,
       0x40000u,
       core.mem_r32(0x80076C00u) + core.mem_r32(0x8007A6E8u));
  call(core,
       0x80016500u,
       core.mem_r32(0x80076B90u),
       core.mem_r32(0x800113A0u),
       core.mem_r32(0x8007A6E4u),
       core.mem_r32(0x8007A6E0u));
  logoSource_ = assetBase_ - core.mem_r32(0x8007A6D4u);
  call(core,
       0x80016500u,
       core.mem_r32(0x80076B90u),
       logoSource_,
       core.mem_r32(0x8007A6D4u),
       core.mem_r32(0x8007A6D0u));
  logoDestination_ = logoSource_ - kLogoBytes;
}

void BootSequence::finalize(Core &core) {
  call(core, 0x8005F6C8u, 0u);
  core.mem_w8(0x80076F4Du, 0u);
  core.mem_w8(0x80076FD1u, 0u);
  call(core, 0x80060030u, 0x80076FC0u);

  std::uint32_t rect = core.r[29] + kRect32;
  writeRect(core, rect, 0, 0, 0x200, 0x1E0);
  call(core, 0x8005F8F8u, rect, 0u, 0u, 0u);
  call(core, 0x8005F764u, 0u);
  call(core, 0x8005F6C8u, 1u);
  rect = core.r[29] + kRect24;
  writeRect(core, rect, 0x200, 0, 0x200, 0x100);
  call(core, 0x8005FA28u, rect, assetBase_);
  call(core, 0x8005F764u, 0u);
  core.mem_w32(0x800785FCu, 0x80200000u - core.mem_r32(0x800755A4u));
  call(core, 0x8005B7D8u);
  call(core, 0x8002D338u);
  core.mem_w32(0x80075918u, 0xFu);
  call(core, 0x8002D170u);
  core.mem_w32(spyro1::guest::kDeltaTime, 2u);
  core.mem_w32(spyro1::guest::kUnprocessedFrames, 0u);
  core.mem_w32(0x800785CCu, 0x8000u);
  skipMap_.bootSequenceEnd();
  core.r[29] = originalStack_;
  fields_.armHostClock();
  lucent::info("boot-native", "native boot reached the gameplay frame boundary");
}

void BootSequence::leaveFirstPresentationHold(Core &core) {
  // The natural expiry's route; the hold starts after the boot assets load, so no press skips loading.
  call(core, kFill, core.mem_r32(0x800785D8u), 0u, kLogoBytes);
  phase_ = Phase::FadeFirstOut;
}

void BootSequence::leaveSecondPresentationHold() {
  // The second hold starts after the stage 3->10 loader completes.
  phase_ = Phase::FadeSecondOut;
}

void BootSequence::observePress() {
  // Past every hold, a press has nothing left to end.
  if (phase_ == Phase::FadeSecondOut || phase_ == Phase::Finalize || phase_ == Phase::Complete) {
    return;
  }
  pressLatch_.observe(fields_.presentationSkipPressed());
}

bool BootSequence::step(Core &core) {
  if (!initialized_) {
    lucent::error("boot-native", "boot step called before initialization");
    std::abort();
  }

  // One step per delivered field, so the edge is read once per field.
  observePress();

  // Zero-field transitions fold into the adjacent step rather than becoming frames of their own.
  for (;;) {
    switch (phase_) {
    case Phase::FadeFirstIn:
      drawLogoField(core, 0x8006FCF4u, core.mem_r32(0x800785D8u), -0xE0 + iteration_ * 0x20);
      if (iteration_++ == 0) {
        call(core, 0x8005F6C8u, 1u);
      }
      if (iteration_ == 8) {
        iteration_ = 0;
        phase_ = Phase::LoadAssets;
      }
      return false;
    case Phase::LoadAssets:
      loadAssets(core);
      phase_ = Phase::HoldFirst;
      break;
    case Phase::HoldFirst:
      if (pressLatch_.take()) {
        lucent::info("boot-native", "Start/Cross ends Spyro 1's first presentation hold");
        leaveFirstPresentationHold(core);
        break;
      }
      if (static_cast<std::uint32_t>(fields_.counter()) - firstHoldStart_ < kLogoHoldFields) {
        deliverField(core, "boot-first-hold");
        return false;
      }
      leaveFirstPresentationHold(core);
      break;
    case Phase::FadeFirstOut:
      drawLogoField(core, 0x8006FCF4u, core.mem_r32(0x800785D8u), -(iteration_ + 1) * 0x20);
      if (++iteration_ == 8) {
        iteration_ = 0;
        secondHoldStart_ = static_cast<std::uint32_t>(fields_.counter());
        call(core, kFill, logoDestination_, 0u, kLogoBytes);
        phase_ = Phase::FadeSecondIn;
      }
      return false;
    case Phase::FadeSecondIn:
      drawLogoField(core, logoSource_, logoDestination_, -0xE0 + iteration_ * 0x20);
      if (++iteration_ == 8) {
        iteration_ = 0;
        core.mem_w32(spyro1::guest::kLoadStage, 3u);
        core.mem_w32(0x8007566Cu, 0u);
        phase_ = Phase::AdvanceLoadState;
      }
      return false;
    case Phase::AdvanceLoadState:
      call(core, 0x80014564u);
      if (core.mem_r32(spyro1::guest::kLoadStage) < 10u) {
        deliverField(core, "boot-load-state");
        return false;
      }
      phase_ = Phase::HoldSecond;
      break;
    case Phase::HoldSecond:
      if (pressLatch_.take()) {
        lucent::info("boot-native", "Start/Cross ends Spyro 1's second presentation hold");
        leaveSecondPresentationHold();
        break;
      }
      if (static_cast<std::uint32_t>(fields_.counter()) - secondHoldStart_ < kLogoHoldFields) {
        deliverField(core, "boot-second-hold");
        return false;
      }
      leaveSecondPresentationHold();
      break;
    case Phase::FadeSecondOut:
      drawLogoField(core, logoSource_, logoDestination_, -(iteration_ + 1) * 0x20);
      if (++iteration_ == 8) {
        phase_ = Phase::Finalize;
      }
      return false;
    case Phase::Finalize:
      finalize(core);
      phase_ = Phase::Complete;
      return true;
    case Phase::Complete:
      return true;
    }
  }
}

} // namespace spyro1
