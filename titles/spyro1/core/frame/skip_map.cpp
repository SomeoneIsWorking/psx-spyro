#include "spyro1/core/frame/skip_map.h"

#include "core.h"
#include "spyro1/core/guest_globals.h"

#include <cstdlib>
#include <lucent/log.h>

namespace spyro1 {
namespace {

// titlescreen.c:100-106: a press cannot skip the card below this tick.
constexpr std::uint32_t kCardSkipTick = 300u;
constexpr std::uint32_t kNoCardLayout = 0xFFFFFFFFu;
constexpr std::uint32_t kSummaryFields = 600u;

struct Sample {
  std::uint32_t gamestate = 0;
  std::uint32_t loadStage = 0;
  std::uint32_t titleMode = 0;
  std::uint32_t titleStage = 0;
  std::uint32_t titleSubState = 0;
  bool cardLayoutResident = false;
  std::uint32_t cardTick = 0;
  bool cardSkippable = false;
};

Sample read(Core &core) {
  Sample sample{.gamestate = core.mem_r32(guest::kGamestate),
                .loadStage = core.mem_r32(guest::kLoadStage),
                .titleMode = core.mem_r32(guest::kTitlescreenMode),
                .titleStage = core.mem_r32(guest::kTitlescreenStage),
                .titleSubState = core.mem_r32(guest::kTitlescreenSubState)};
  const std::uint32_t layout = core.mem_r32(guest::kCutsceneLayout);
  sample.cardLayoutResident = layout != 0u;
  if (sample.cardLayoutResident) {
    sample.cardTick = core.mem_r32(layout + guest::kCutsceneLayoutCurrentTick);
    sample.cardSkippable = sample.cardTick >= kCardSkipTick;
  }
  return sample;
}

} // namespace

void SkipMap::bootSequenceBegin() {
  if (bootSequenceActive_) {
    lucent::error("skipmap", "boot sequence observation armed twice");
    std::abort();
  }
  bootSequenceActive_ = true;
  lucent::debug("skipmap", "observing Start edges during guest boot function 0x800127C0");
}

void SkipMap::bootSequenceEnd() {
  bootSequenceActive_ = false;
}

void SkipMap::onField(Core &core, bool startEdge) {
  const Sample sample = read(core);
  ++fields_;
  const bool bootActive = bootSequenceActive_;
  bootActive ? ++bootFields_ : ++stageFields_;
  if (startEdge) {
    ++startEdges_;
  }
  const bool changed = sample.gamestate != previousGamestate_ || sample.loadStage != previousLoadStage_ ||
                       sample.titleMode != previousTitleMode_ || sample.titleStage != previousTitleStage_ ||
                       sample.titleSubState != previousTitleSubState_ || bootActive != previousBootActive_;
  if (startEdge || changed) {
    lucent::debug("skipmap",
                  "field={} start_edge={} region={} load_stage={} gamestate={} "
                  "title[mode={} state={} substate={}] card_tick={} card_skippable={} edges={}",
                  fields_,
                  startEdge ? 1 : 0,
                  bootActive ? "boot" : "stage",
                  sample.loadStage,
                  sample.gamestate,
                  sample.titleMode,
                  sample.titleStage,
                  sample.titleSubState,
                  sample.cardLayoutResident ? sample.cardTick : kNoCardLayout,
                  sample.cardSkippable ? 1 : 0,
                  startEdges_);
  }
  if (fields_ % kSummaryFields == 0u) {
    lucent::debug("skipmap",
                  "scanned {} fields: start_edges={} boot_fields={} stage_fields={} current={}",
                  fields_,
                  startEdges_,
                  bootFields_,
                  stageFields_,
                  bootActive ? "boot" : "stage");
  }
  previousGamestate_ = sample.gamestate;
  previousTitleMode_ = sample.titleMode;
  previousTitleStage_ = sample.titleStage;
  previousTitleSubState_ = sample.titleSubState;
  previousLoadStage_ = sample.loadStage;
  previousBootActive_ = bootActive;
}

} // namespace spyro1
