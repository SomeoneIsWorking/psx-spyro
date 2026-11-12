#include "spyro1/core/frame/frame_driver.h"

#include "core.h"
#include "core/session.h"
#include "game.h"
#include "guest_call.h"
#include "spyro1/core/frame/guest_update_budget.h"
#include "spyro1/core/native_state.h"

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
                                     .rootHandlerSlot = kRootHandlers},
              &skipMap_),
      boot_(fields_, skipMap_), transitions_(fields_), motion_(game.core, nativeState(game.core)),
      cut_(kFrameCutFacts) {}

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
  spyro::Warp &warp = *spyro::session(core).warp;
  warp.applyArmed(core, frame);
  if (const std::optional<spyro::Warp::Entry> entry = warp.takeEntry()) {
    psx::cpu::callGuestNow(core, "debug-entry", entry->function, entry->argument);
  }
  bool switched = false;
  while (runGuestUpdate(core)) {
    // g_StateSwitch: a load, respawn, exit or cutscene start re-placed the scene.
    switched = true;
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
  // A new draw stage re-placed the scene as a state switch does.
  const std::uint32_t stage = DrawSection::stage(core);
  if (switched || stage != stage_) {
    replay_.forget();
  }
  stage_ = stage;
  if (section_.replays(core)) {
    replay_.beforeDraw(core);
  } else {
    replay_.forget();
  }
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

bool FrameDriver::save(psx::platform::BlobWriter &out, std::string &error) const {
  if (!boot_.complete()) {
    error = lucent::format("{}: the boot sequence is still running; a state is taken past it",
                           sectionName());
    return false;
  }
  out.u32(gameplayFrame_);
  return true;
}

bool FrameDriver::load(psx::platform::BlobReader &in, std::string &) {
  stagedFrame_ = in.u32();
  return true;
}

void FrameDriver::restored(Core &) {
  if (!boot_.complete()) {
    boot_.adoptCompleted();
  }
  transitions_.dropHeldPress();
  if (!stagedFrame_) {
    lucent::error("frameloop", "{}: restored without a loaded gameplay frame", sectionName());
    std::abort();
  }
  gameplayFrame_ = *stagedFrame_;
  stagedFrame_.reset();
  replay_.forget();
}

} // namespace spyro1
