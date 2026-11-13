#include "render/crop_layers.h"

#include "core.h"
#include "game.h"
#include "ordering_table.h"

#include <algorithm>

namespace spyro {
namespace {

bool cropping(Core &core) {
  return core.game->guestDisplay.plan().crop.active();
}

} // namespace

bool WholeFrameUi::covers(Core &core) const {
  return !states.empty() && std::ranges::find(states, core.mem_r32(gameState)) != states.end();
}

void CropLayers::linked(Core &core, std::uint32_t packet) {
  if (cropping(core)) {
    linking_.push_back(psx::gpu::mainRamOffsetOf(packet));
  }
}

void CropLayers::linkedChain(Core &core,
                             const psx::present::EmitMemory &memory,
                             std::uint32_t first,
                             std::uint32_t last) {
  if (!cropping(core)) {
    return;
  }
  for (std::uint32_t packet = first;; packet = psx::gpu::guestAddressOf(memory.mem_r32(packet))) {
    linking_.push_back(psx::gpu::mainRamOffsetOf(packet));
    if (packet == last) {
      return;
    }
  }
}

void CropLayers::secondPass() {
  secondPass_ = true;
}

void CropLayers::flattened(bool wholeFrame) {
  if (secondPass_) {
    secondPass_ = false;
    drawn_.insert(drawn_.end(), linking_.begin(), linking_.end());
  } else {
    wholeFrame_ = wholeFrame;
    drawn_.swap(linking_);
  }
  linking_.clear();
  std::ranges::sort(drawn_);
}

bool CropLayers::uiPacket(std::uint32_t packet) const {
  return wholeFrame_ || std::ranges::binary_search(drawn_, psx::gpu::mainRamOffsetOf(packet));
}

} // namespace spyro
