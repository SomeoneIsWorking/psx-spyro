#include "render/packet_layers.h"

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

void PacketLayers::Layer::link(const psx::present::EmitMemory &memory,
                               std::uint32_t first,
                               std::uint32_t last) {
  for (std::uint32_t packet = first;; packet = psx::gpu::guestAddressOf(memory.mem_r32(packet))) {
    linking.push_back(psx::gpu::mainRamOffsetOf(packet));
    if (packet == last) {
      return;
    }
  }
}

void PacketLayers::Layer::flatten(bool joinsFrame) {
  if (joinsFrame) {
    drawn.insert(drawn.end(), linking.begin(), linking.end());
  } else {
    drawn.swap(linking);
  }
  linking.clear();
  std::ranges::sort(drawn);
}

bool PacketLayers::Layer::holds(std::uint32_t packet) const {
  return std::ranges::binary_search(drawn, psx::gpu::mainRamOffsetOf(packet));
}

void PacketLayers::linked(Core &core, std::uint32_t packet) {
  if (cropping(core)) {
    ui_.linking.push_back(psx::gpu::mainRamOffsetOf(packet));
  }
}

void PacketLayers::linkedChain(Core &core,
                               const psx::present::EmitMemory &memory,
                               std::uint32_t first,
                               std::uint32_t last) {
  if (cropping(core)) {
    ui_.link(memory, first, last);
  }
}

void PacketLayers::linkedGlyphs(const psx::present::EmitMemory &memory,
                                std::uint32_t first,
                                std::uint32_t last) {
  glyphs_.link(memory, first, last);
}

void PacketLayers::secondPass() {
  secondPass_ = true;
}

void PacketLayers::flattened(bool wholeFrame) {
  if (!secondPass_) {
    wholeFrame_ = wholeFrame;
  }
  ui_.flatten(secondPass_);
  glyphs_.flatten(secondPass_);
  secondPass_ = false;
}

bool PacketLayers::uiPacket(std::uint32_t packet) const {
  return wholeFrame_ || ui_.holds(packet);
}

bool PacketLayers::glyphPacket(std::uint32_t packet) const {
  return glyphs_.holds(packet);
}

} // namespace spyro
