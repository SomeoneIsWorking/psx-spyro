#include "core/debug/warp.h"

#include <lucent/log.h>

#include <utility>

namespace spyro {

std::string Warp::arm(Core &core, std::uint32_t level, Placement placement) {
  const bool placeNow = placement.ready && placement.ready(core);
  level_ = placeNow ? std::nullopt : std::optional<std::uint32_t>(level);
  target_ = level;
  issued_ = false;
  left_ = false;
  changes_ = 0;
  placement_ = std::move(placement);
  if (placeNow) {
    return lucent::format("ok: armed in level {}; it applies at the next frame", level);
  }
  return lucent::format("ok: warp armed for level {}", level);
}

void Warp::applyArmed(Core &core, std::uint32_t frame) {
  if (level_) {
    enterArmed(core, frame);
    return;
  }
  if (!placement_.apply && !placement_.enter) {
    return;
  }
  if (placement_.ready(core)) {
    entry_ = placement_.enter;
    const std::string result =
        placement_.apply ? placement_.apply(core)
                         : lucent::format("entry 0x{:08X}({}) before the update",
                                          placement_.enter->function,
                                          placement_.enter->argument);
    placement_ = {};
    lucent::info("warp", "placement at f{}: {}", frame, result);
    return;
  }
  if (levelChangeReady(core) && !playing(core, target_)) {
    placement_ = {};
    lucent::warn("warp", "placement dropped at f{}: play left level {} first", frame, target_);
  }
}

void Warp::enterArmed(Core &core, std::uint32_t frame) {
  const std::uint32_t level = *level_;
  if (titleScreenIdle(core)) {
    startLevel(core, level);
    issued_ = true;
    left_ = true;
    lucent::info("warp", "level {} started from the title screen at f{}", level, frame);
    return;
  }
  if (!levelChangeReady(core)) {
    left_ = issued_;
    return;
  }
  if (issued_ && !left_) {
    return;
  }
  if (issued_ && playing(core, level)) {
    level_.reset();
    lucent::info("warp", "level {} reached at f{} after {} change(s)", level, frame, changes_);
    return;
  }
  if (changes_ == kMaxChanges) {
    level_.reset();
    placement_ = {};
    lucent::error("warp",
                  "level {} not reached after {} changes; warp dropped at f{}",
                  level,
                  changes_,
                  frame);
    return;
  }
  changeLevel(core, level);
  issued_ = true;
  left_ = false;
  ++changes_;
  lucent::info("warp", "change {} toward level {} from gameplay at f{}", changes_, level, frame);
}

} // namespace spyro
