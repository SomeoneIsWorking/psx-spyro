#include "spyro1/core/frame/frame_draw.h"

#include "core/field_owner.h"
#include "spyro1/core/frame/guest_update_budget.h"

#include <cstdint>
#include <cstdlib>
#include <lucent/log.h>

namespace spyro1 {
namespace {

// Called by main 0x80012204 at 0x8001227C; it returns past that jal's delay slot.
constexpr std::uint32_t kFrameDraw = 0x8001ED5Cu;
constexpr std::uint32_t kFrameDrawReturn = 0x80012284u;

} // namespace

void FrameDraw::run(Core &core, spyro::FieldOwner &fields) {
  call_.begin(core,
              "frame-draw",
              psx::cpu::GuestCallSite{kFrameDraw, kFrameDrawReturn},
              psx::cpu::kUnboundedCallTurns);
  for (std::uint32_t turn = 0; turn < kGuestUpdateFields; ++turn) {
    const psx::cpu::CallStep step = call_.advance();
    if (step.outcome == psx::cpu::CallOutcome::Returned) {
      return;
    }
    if (step.outcome == psx::cpu::CallOutcome::Refused) {
      lucent::error("frameloop",
                    "GamestateDraw 0x{:08X} refused at pc 0x{:08X}: {}",
                    kFrameDraw,
                    step.guestPc,
                    step.detail);
      std::abort();
    }
    if (step.reason == psx::cpu::ExecutionExitReason::FrameBoundary &&
        !fields.deliver({.site = "frame-draw", .present = false, .pace = false})) {
      lucent::error("frameloop", "field owner refused GamestateDraw's field wait");
      std::abort();
    }
  }
  lucent::error("frameloop",
                "GamestateDraw 0x{:08X} did not return within {} host turns; resume 0x{:08X}",
                kFrameDraw,
                kGuestUpdateFields,
                call_.resumePc());
  std::abort();
}

} // namespace spyro1
