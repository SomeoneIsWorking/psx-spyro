#include "core/debug/warp.h"

#include <lucent/log.h>

#include <utility>

namespace spyro {

std::string Warp::arm(Core &core, std::uint32_t level, Placement placement) {
  const bool placeNow = placement.ready && placement.ready(core);
  level_ = placeNow ? std::nullopt : std::optional<std::uint32_t>(level);
  placement_ = std::move(placement);
  if (placeNow) {
    return lucent::format("ok: armed in level {}; it applies at the next frame", level);
  }
  return lucent::format("ok: warp armed for level {}; it starts from the title screen", level);
}

void Warp::applyArmed(Core &core, std::uint32_t frame) {
  if (level_ && titleScreenIdle(core)) {
    const std::uint32_t level = *level_;
    level_.reset();
    startLevel(core, level);
    lucent::info("warp", "level {} started from the title screen at f{}", level, frame);
    return;
  }
  if (!level_ && placement_.apply && placement_.ready(core)) {
    const std::string result = placement_.apply(core);
    placement_ = {};
    lucent::info("warp", "placement at f{}: {}", frame, result);
  }
}

} // namespace spyro
