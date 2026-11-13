#include "spyro1/render/crop_layers.h"

#include "core.h"
#include "game.h"
#include "ordering_table.h"

#include <algorithm>

namespace spyro1 {
namespace {

bool cropping(Core &core) {
  return core.game->guestDisplay.plan().crop.active();
}

} // namespace

void CropLayers::linked(Core &core, std::uint32_t packet, std::uint32_t returnAddress) {
  if (!cropping(core) ||
      std::ranges::find(kWholeScreenLinks, returnAddress) != kWholeScreenLinks.end()) {
    return;
  }
  linking_.push_back(psx::gpu::mainRamOffsetOf(packet));
}

void CropLayers::screenSpace(Core &core,
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

void CropLayers::flattened() {
  drawn_.swap(linking_);
  linking_.clear();
  std::ranges::sort(drawn_);
}

bool CropLayers::uiPacket(std::uint32_t packet) const {
  return std::ranges::binary_search(drawn_, psx::gpu::mainRamOffsetOf(packet));
}

} // namespace spyro1
