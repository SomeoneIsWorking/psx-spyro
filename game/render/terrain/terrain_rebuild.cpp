#include "render/terrain/terrain_rebuild.h"

#include "blend.h"
#include "gte_registers.h"
#include "render/field/guest_word.h"
#include "lucent/log.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace spyro::guest_terrain {
namespace {

// One field's terrain allocates at most 55,672 bytes of packets (SCUS_944.25).
constexpr std::uint32_t kMinArenaBytes = 0x10000u;

} // namespace

CameraState readCamera(Core &core, const guest_render_globals::Globals &globals) {
  CameraState state;
  state.angles[0] = core.mem_r16s(globals.cameraAngles);
  state.angles[1] = core.mem_r16s(globals.cameraAngles + 2);
  state.angles[2] = core.mem_r16s(globals.cameraAngles + 4);
  for (std::uint32_t i = 0; i < 3; ++i) {
    state.position[i] = core.mem_r32(globals.cameraPosition + 4 * i);
  }
  return state;
}

InBetweenCamera cameraBetween(Core &core,
                              const TerrainImage &image,
                              const CameraState &from,
                              const CameraState &to,
                              float t) {
  const float at = std::clamp(t, 0.0f, 1.0f);
  const guest_camera::Angles angles =
      guest_camera::interpolate({from.angles[0], from.angles[1], from.angles[2]},
                                {to.angles[0], to.angles[1], to.angles[2]},
                                at);
  const guest_camera::Matrices matrices = image.camera(core, angles);
  InBetweenCamera camera{.rotation = matrices.projection,
                         .viewRotation = matrices.view,
                         .angles = {static_cast<std::int16_t>(guest_camera::wholeAngle(angles.aroundX)),
                                    static_cast<std::int16_t>(guest_camera::wholeAngle(angles.aroundY)),
                                    static_cast<std::int16_t>(guest_camera::wholeAngle(angles.aroundZ))}};
  for (std::uint32_t i = 0; i < 3; ++i) {
    camera.position[i] = psx::present::lerpWord(from.position[i], to.position[i], at);
  }
  return camera;
}

std::vector<std::uint8_t> unionVisibility(std::span<const std::uint8_t> from,
                                          std::span<const std::uint8_t> to) {
  std::vector<std::uint8_t> out(to.size(), 0);
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i] = static_cast<std::uint8_t>(to[i] != 0 || (i < from.size() && from[i] != 0));
  }
  return out;
}

Rebuild::Rebuild(Core &core,
                 const TerrainImage &image,
                 const InBetweenCamera &camera,
                 const FieldInputs &field,
                 psx::present::PrimitiveSink &places)
    : gte_(core.gte()), memory_(core, host_), camera_(camera) {
  const FieldWindows windows = image.windows(core);
  arena_ = windows.inBetweenArena;
  if (arena_.bytes < kMinArenaBytes) {
    lucent::error("terrrebuild",
                  "refusing a terrain in-between: its packet arena runs from 0x{:08X} for {} "
                  "bytes, and one field's terrain needs more than {}",
                  arena_.address,
                  arena_.bytes,
                  kMinArenaBytes);
    std::abort();
  }

  // Seeding from the guest would hand the traversal the last real field's lists and bins.
  for (const GuestSpan &range : windows.owned) {
    host_.zero(range.address, range.bytes);
  }
  host_.zero(arena_.address, arena_.bytes);
  host_.overlay(field.readWords);

  for (std::size_t i = 0; i < field.visibleSectors.size(); ++i) {
    if (field.visibleSectors[i] != 0) {
      memory_.mem_w8(kScratchpad + static_cast<std::uint32_t>(i), field.visibleSectors[i]);
      ++visible_;
    }
  }
  drawer_.emplace(core,
                  image,
                  memory_,
                  FrameMode::InBetween,
                  InBetweenWindow{.camera = &camera_,
                                  .places = &places,
                                  .argument = field.argument,
                                  .arenaBase = arena_.address,
                                  .packetBudget = field.packetBudget});
  const TerrainFrame &ran = drawer_->run();
  tableBase_ = windows.tableBase;
  linkedBins_ = &ran.linkedBins();
}

void Rebuild::emit(psx::present::PrimitiveSink &sink) const {
  psx::present::PacketChainWalker walker(memory_, psx::present::PacketBound::within(arena_));
  psx::present::DrawModeTracker drawMode;
  for (auto bin = static_cast<std::uint32_t>(linkedBins_->size()); bin-- > 0;) {
    if ((*linkedBins_)[bin] != 0) {
      walker.emitChain(memory_.mem_r32(tableBase_ + bin * depth_bins::kBinStride + depth_bins::kBinFirstOffset),
                       psx::present::OtSlot{depth_bins::kTable, bin},
                       drawMode,
                       sink);
    }
  }
}

} // namespace spyro::guest_terrain
