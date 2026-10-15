#include "render/terrain/guest_terrain_far_vertices.h"

#include "core.h"
#include "gte_registers.h"
#include "render/field/guest_word.h"
#include "render/terrain/guest_terrain_frame.h"

namespace spyro::guest_terrain {
namespace {

namespace gte = psx::gte;

using spyro::guest_word::asSigned;

constexpr std::uint32_t kOutAbove = 1;
constexpr std::uint32_t kOutBelow = 2;
constexpr std::uint32_t kOutHorizontal = 4;
constexpr std::uint32_t kOutNear = 8;
constexpr std::uint32_t kOutcodeMask = 0xF;

// z in the top 11 bits, then x, then y.
void loadVertex(Core &core,
                std::uint32_t word,
                const std::array<std::uint32_t, 3> &origin,
                FarHeight height) {
  const std::uint32_t z = (word >> 21) + origin[0];
  const std::uint32_t x = origin[1] - ((word >> 10) & 0x7FFu);
  const std::uint32_t y =
      origin[2] - (height == FarHeight::Doubled ? (word << 1) & 0x7FFu : word & 0x3FFu);
  core.gte().writeData(gte::kVz0, z);
  core.gte().writeData(gte::kVxy0, x + (y << 16));
}

} // namespace

bool projectFarVertices(TerrainFrame &frame,
                        const FarVertices &vertices,
                        const FarClip &clip,
                        FarHeight height) {
  Core &core = frame.core;
  loadVertex(core, frame.memory.r32(vertices.data), vertices.origin, height);
  std::uint32_t cell = kScratchpad;
  std::uint32_t shared = 0xFFFFFFFFu;
  for (std::uint32_t i = 1; i <= vertices.count; ++i) {
    core.gte().execute(gte::kRtps);
    loadVertex(core, frame.memory.r32(vertices.data + 4 * i), vertices.origin, height);
    const std::uint32_t sxy = core.gte().readData(gte::kSxy2);
    const std::uint32_t sz = core.gte().readData(gte::kSz3);
    frame.v0 = sxy;
    frame.v1 = sz;
    std::uint32_t word = sxy;
    if (clip.clipped) {
      word = sxy << kFarOutcodeShift;
      if (clip.edge) {
        if (frame.bounds.aboveTop(sxy)) {
          word += kOutAbove;
        }
        if (frame.bounds.belowBottom(sxy)) {
          word += kOutBelow;
        }
        if (frame.bounds.outsideHorizontal(sxy)) {
          word += kOutHorizontal;
        }
      }
      if (clip.nearBias && asSigned(sz + *clip.nearBias) < 0) {
        word += kOutNear;
      }
      shared &= word;
    }
    frame.memory.w32(cell, word);
    frame.memory.w32(cell + 4, sz);
    cell += kFarCellSize;
  }
  return !clip.edge || (shared & kOutcodeMask) == 0;
}

} // namespace spyro::guest_terrain
