#include "core/debug/warp.h"

#include <lucent/log.h>

namespace spyro {
std::string Warp::arm(std::uint32_t level) {
  level_ = level;
  return lucent::format("ok: warp armed for level {}; it starts from the title screen", level);
}

void Warp::applyArmed(Core &core, std::uint32_t frame) {
  if (!level_ || !titleScreenIdle(core)) {
    return;
  }
  const std::uint32_t level = *level_;
  level_.reset();
  startLevel(core, level);
  lucent::info("warp", "level {} started from the title screen at f{}", level, frame);
}

} // namespace spyro
