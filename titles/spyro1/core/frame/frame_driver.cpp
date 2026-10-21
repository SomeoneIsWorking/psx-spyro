#include "spyro1/core/frame/frame_driver.h"

#include "core.h"
#include "core/session.h"
#include "game.h"
#include "guest_call.h"
#include "spyro1/core/frame/guest_update_budget.h"

#include <algorithm>
#include <cstdlib>
#include <lucent/log.h>

namespace spyro1 {
namespace {

using guest::kDeltaTime;
using guest::kGp;
using guest::kStateSwitch;
using guest::kUnprocessedFrames;

constexpr std::uint32_t kStaticConstructors = 0x8005B988u;
constexpr std::uint32_t kFrameUpdate = 0x8003385Cu;
constexpr std::uint32_t kVblankCounter = 0x800749E0u;
constexpr std::uint32_t kRootHandlers = 0x80073928u;
constexpr std::uint32_t kInputLatchOpen = kGp + 0x604u;
constexpr std::int32_t kDeltaTimeMin = 2;
constexpr std::int32_t kDeltaTimeMax = 4;

} // namespace

FrameDriver::FrameDriver(Game &game)
    : fields_(game,
              spyro::FieldOwnerFacts{.titleName = "Spyro 1",
                                     .fieldCounter = kVblankCounter,
                                     .rootHandlerSlot = kRootHandlers,
                                     // lw at 0x8001F090 in the draw 0x8001ED5C.
                                     .drawWaitFlag = 0x80075784u},
              &skipMap_),
      boot_(fields_, skipMap_), transitions_(fields_), cut_(kFrameCutFacts) {}

void FrameDriver::initialize(Core &core) {
  fields_.publish();
  psx::cpu::callGuestNow(core, "static-constructors", kStaticConstructors);
  boot_.initialize(core);
  lucent::info(
      "frameloop",
      "FrameDriver initialized; guest main 0x{:08X} and boot body 0x{:08X} are not dispatched",
      0x80012204u,
      0x800127C0u);
}

bool FrameDriver::runGuestUpdate(Core &core) {
  // Before the update reads the transition globals, so a cancel releases its hold this frame.
  transitions_.observe(core);

  core.mem_w8(kInputLatchOpen, 0);
  psx::cpu::dispatchGuestWithArgumentsToReturn(
      core,
      kFrameUpdate,
      {},
      psx::cpu::ExecutionBudget::fromCycles(kGuestUpdateCycles),
      "frame-update");
  core.mem_w8(kInputLatchOpen, 1);
  // main.c: g_DeltaTime = clamp(g_UnprocessedFrames, 2, 4); g_UnprocessedFrames = 0.
  const auto unprocessed = static_cast<std::int32_t>(core.mem_r32(kUnprocessedFrames));
  core.mem_w32(kDeltaTime,
               static_cast<std::uint32_t>(std::clamp(unprocessed, kDeltaTimeMin, kDeltaTimeMax)));
  const bool stateSwitch = core.mem_r32(kStateSwitch) != 0;
  core.mem_w32(kUnprocessedFrames, 0);
  return stateSwitch;
}

void FrameDriver::stepFrame(Core &core, std::uint32_t) {
  if (!boot_.complete() && !boot_.step(core)) {
    return;
  }

  GpuPerf &perf = core.game->perf;
  perf.frameBegin();
  const std::uint32_t frame = ++gameplayFrame_;
  fields_.beginLogicFrame();
  core.game->timing.logicFrame = frame;
  core.rsub.otAttr.beginLogicFrame(frame);
  perf.markPre();

  // main.c draws only when g_StateSwitch is clear; a product step is one drawn iteration plus the
  // draw-less ones the guest chains before it.
  perf.phaseBegin(GpuPerf::Phase::GameLogic);
  spyro::session(core).warp->applyArmed(core, frame);
  while (runGuestUpdate(core)) {
    if (fields_.fieldsThisLogicFrame() > kGuestUpdateFields) {
      lucent::error("frameloop",
                    "product step {} chained draw-less guest iterations across {} fields; "
                    "g_StateSwitch never cleared",
                    frame,
                    fields_.fieldsThisLogicFrame());
      std::abort();
    }
  }
  perf.phaseEnd(GpuPerf::Phase::GameLogic);

  perf.phaseBegin(GpuPerf::Phase::Present);
  draw_.run(core, fields_);
  cut_.onFrameDrawn(core);
  cut_.onFrameTail(core);
  fields_.presentFrame();
  perf.phaseEnd(GpuPerf::Phase::Present);
  perf.frameEnd();
}

bool FrameDriver::pastBootPrefix() const {
  return boot_.complete();
}

} // namespace spyro1
