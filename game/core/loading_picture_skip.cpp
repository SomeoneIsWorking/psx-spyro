#include "core/loading_picture_skip.h"

#include "core.h"

#include <lucent/log.h>

namespace spyro {

void LoadingPictureSkip::observe(Core &core, bool pressed) {
  const bool showing = facts_.gameState != 0u &&
                       core.mem_r32(facts_.gameState) == facts_.loadingState &&
                       core.mem_r32(facts_.subState) == 0u;
  const std::uint32_t count = showing ? core.mem_r32(facts_.counter) : 0u;
  // Count 0 is the previous screen's fade-out while its update loads the picture.
  if (count == 0u) {
    press_.clear();
    return;
  }
  press_.observe(pressed);
  if (!press_.take()) {
    return;
  }
  const std::uint32_t to = landing(core, count);
  if (count >= to) {
    return;
  }
  core.mem_w32(facts_.counter, to);
  lucent::info("transition", "Start/Cross ends the loading picture: count {} -> {}", count, to);
}

std::uint32_t LoadingPictureSkip::landing(Core &core, std::uint32_t count) const {
  const bool autosave = facts_.autosaveCount != 0u && count < facts_.autosaveCount &&
                        core.mem_r8(facts_.autosaveFlags + core.mem_r32(facts_.kind)) != 0u;
  const std::uint32_t latest = autosave ? facts_.autosaveCount - 1u : facts_.landingCount;
  // Each update draws into the other buffer; dropping an even number of them leaves the guest on
  // the buffer it would have reached, whose draw env the load's last frame clears with.
  return latest - ((latest - count) & 1u);
}

} // namespace spyro
