#include "core/draw_section.h"

#include "core.h"
#include "guest_redraw.h"
#include "ordering_table.h"
#include "render/terrain/guest_depth_bins.h"

#include <lucent/log.h>

#include <array>

namespace spyro {

bool DrawSection::replays(Core &core) const {
  return core.mem_r32(facts_.gate) == 0 && replaysState(gameState(core));
}

std::uint32_t DrawSection::gameState(Core &core) const {
  return core.mem_r32(facts_.gameState);
}

bool DrawSection::step(psx::present::GuestRedraw &redraw,
                       std::uint32_t entry,
                       std::span<const std::uint32_t> args) {
  const bool returned = redraw.dispatch(entry, args);
  lucent::debug("replay", "section step 0x{:08X}: {}", entry, returned ? "returned" : "refused");
  return returned;
}

bool DrawSection::dispatch(psx::present::GuestRedraw &redraw, std::uint32_t flags) const {
  const std::array<std::uint32_t, 1> args{flags};
  return step(redraw, facts_.dispatcher, args);
}

std::optional<std::uint32_t> DrawSection::flatten(Core &core,
                                                  psx::present::GuestRedraw &redraw) const {
  const std::array<std::uint32_t, 1> bins{depth_bins::kBins};
  if (!step(redraw, facts_.flatten, bins)) {
    return std::nullopt;
  }
  return core.r[2];
}

std::optional<std::uint32_t>
DrawSection::flattenFrame(Core &core, psx::present::GuestRedraw &redraw, std::uint32_t) const {
  return flatten(core, redraw);
}

std::optional<std::uint32_t> DrawSection::draw(Core &core,
                                               psx::present::GuestRedraw &redraw) const {
  const std::uint32_t buffer =
      core.mem_r32(facts_.currentBuffer) == facts_.buffer0 ? facts_.buffer1 : facts_.buffer0;
  const std::uint32_t cursor = core.mem_r32(buffer + kBufferPool);
  redraw.writeWord(facts_.primitiveCursor, cursor);
  redraw.writeWord(facts_.binMark, core.mem_r32(facts_.orderingTable) + 0x1000u);
  redraw.writeWord(facts_.scratchEnd, cursor + core.mem_r32(facts_.scratchSize));
  redraw.writeWord(facts_.currentBuffer, buffer);
  const std::array<std::uint32_t, 3> camera{
      facts_.cameraAngles, facts_.viewRotation, facts_.projectionRotation};
  const std::array<std::uint32_t, 2> position{facts_.drawCameraPosition, facts_.cameraPosition};
  if (!step(redraw, facts_.cameraBuilder, camera) || !step(redraw, facts_.positionCopy, position)) {
    return std::nullopt;
  }
  const std::uint32_t state = gameState(core);
  if (!drawState(core, redraw, state)) {
    return std::nullopt;
  }
  return flattenFrame(core, redraw, state);
}

bool DrawSection::linked(std::uint32_t head) const {
  const std::uint32_t offset = psx::gpu::mainRamOffsetOf(head);
  return offset < psx::gpu::mainRamOffsetOf(facts_.buffer0) ||
         offset >= psx::gpu::mainRamOffsetOf(facts_.buffer1 + kBufferBytes);
}

} // namespace spyro
