#include "spyro1/core/frame/frame_draw.h"

#include "spyro1/core/runtime/field_scheduler.h"

#include <cstdint>
#include <cstdlib>
#include <lucent/log.h>

namespace spyro1 {
namespace {

// Called by main 0x80012204 at 0x8001227C; it returns past that jal's delay slot.
constexpr std::uint32_t kFrameDraw = 0x8001ED5Cu;
constexpr std::uint32_t kFrameDrawReturn = 0x80012284u;
// Four seconds of fields, the bound one guest update has.
constexpr std::uint32_t kTurnLimit = 240;

} // namespace

void FrameDraw::run(Core &core, FieldScheduler &fields) {
  call_.begin(core, "frame-draw", kFrameDraw, kFrameDrawReturn, psx::cpu::kUnboundedCallTurns);
  for (std::uint32_t turn = 0; turn < kTurnLimit; ++turn) {
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
      lucent::error("frameloop", "field scheduler refused GamestateDraw's field wait");
      std::abort();
    }
  }
  lucent::error("frameloop",
                "GamestateDraw 0x{:08X} did not return within {} host turns; resume 0x{:08X}",
                kFrameDraw,
                kTurnLimit,
                call_.resumePc());
  std::abort();
}

} // namespace spyro1
