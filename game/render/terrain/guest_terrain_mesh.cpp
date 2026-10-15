#include "render/terrain/guest_terrain_mesh.h"

#include "core.h"
#include "gte_registers.h"
#include "render/field/guest_word.h"
#include "render/terrain/guest_terrain_frame.h"

namespace spyro::guest_terrain {
namespace {

namespace gte = psx::gte;

using spyro::guest_word::asSigned;

constexpr std::uint32_t kFarDepth = 0x600;   // at or past this, the plain outcodes
constexpr std::uint32_t kCloseDepth = 0x100; // nearer than this may be re-projected
constexpr std::uint32_t kRefineSpan = 0x100; // |MAC1|, |MAC2| under this
constexpr std::uint32_t kRefineMask = 0xFFF0FFF0u;

} // namespace

SectorProjector::SectorProjector(TerrainFrame &frame)
    : frame_(frame), slot_(kScratchpad), depthSlot_(kVertexDepths) {}

ProjectedSector SectorProjector::project(std::uint32_t entry) {
  const std::uint32_t sectorClass = entry & kSectorClassMask;
  const std::uint32_t sector = entry ^ sectorClass;
  guest_memory::WorkingMemory &memory = frame_.memory;
  Core &core = frame_.core;
  const std::uint32_t camera = cameraRegister();
  const std::uint32_t zx = memory.r32(sector + near_sector::kOriginZX);
  const std::uint32_t y = memory.r32(sector + near_sector::kOriginY);
  originZ_ = (zx >> 14) - core.gte().readControl(camera);
  originX_ = core.gte().readControl(camera + 1) - ((zx & 0xFFFFu) << 2);
  originY_ = core.gte().readControl(camera + 2) - (y >> 14);
  const std::uint32_t layout = memory.r32(sector + near_sector::kLayout);
  const std::uint32_t first = sector + near_sector::kData + ((layout >> 22) & 0x3FCu);
  end_ = first + 4 * ((layout & 0xFFu) + 2);
  next_ = memory.r32(first + 4);
  load(unpack(memory.r32(first)));
  cursor_ = first + 8;

  ProjectedSector result;
  result.vertexEnd = end_;
  if ((sectorClass & kCloseSector) != 0) {
    projectClose();
    result.clipped = true;
    result.overflowed = overflow_;
  } else if (sectorClass != 0) {
    projectEdge();
    result.clipped = true;
  } else {
    projectInside();
    result.drawn = true;
    return result;
  }
  result.drawn = (common_ & 0xFu) == 0;
  return result;
}

SectorProjector::UnpackedVertex SectorProjector::unpack(std::uint32_t word) const {
  const std::uint32_t z = ((word >> 19) & 0x1FFCu) + originZ_;
  const std::uint32_t x = originX_ - ((word >> 8) & 0x1FFCu);
  const std::uint32_t y = originY_ - height(word);
  return UnpackedVertex{z, x + (y << 16)};
}

void SectorProjector::load(const UnpackedVertex &vertex) {
  frame_.core.gte().writeData(gte::kVz0, vertex.z);
  frame_.core.gte().writeData(gte::kVxy0, vertex.xy);
}

// Unpack the vertex read one ahead into VZ0/VXY0 for the next RTPS, and read the one after it.
void SectorProjector::advance() {
  const UnpackedVertex vertex = unpack(next_);
  next_ = frame_.memory.r32(cursor_);
  cursor_ += 4;
  load(vertex);
}

void SectorProjector::store(const Projected &projected) {
  frame_.memory.w32(slot_, projected.word);
  frame_.memory.w16(depthSlot_, static_cast<std::uint16_t>(projected.depth));
  slot_ += 4;
  depthSlot_ += 2;
}

void SectorProjector::projectInside() {
  Core &core = frame_.core;
  do {
    core.gte().execute(gte::kRtps);
    advance();
    const std::uint32_t sxy = core.gte().readData(gte::kSxy2);
    const std::uint32_t depth = core.gte().readData(gte::kSz3);
    frame_.v0 = sxy;
    frame_.v1 = depth;
    store({sxy, depth});
  } while (cursor_ != end_);
}

// The edge outcode, also used for close vertices at or past kFarDepth.
std::uint32_t SectorProjector::edgeOutcode(std::uint32_t sxy) {
  std::uint32_t code = sxy << 5;
  if (frame_.bounds.aboveTop(sxy)) {
    code += 1;
  }
  if (frame_.bounds.belowBottom(sxy)) {
    code += 2;
  }
  if (frame_.bounds.outsideHorizontal(sxy)) {
    code += 0xC;
  }
  frame_.v0 = code;
  frame_.v1 = sxy & 0xFE00u;
  common_ &= code;
  return code;
}

void SectorProjector::projectEdge() {
  Core &core = frame_.core;
  do {
    core.gte().execute(gte::kRtps);
    advance();
    const std::uint32_t sxy = core.gte().readData(gte::kSxy2);
    const std::uint32_t depth = core.gte().readData(gte::kSz3);
    store({edgeOutcode(sxy), depth});
  } while (cursor_ != end_);
}

void SectorProjector::projectClose() {
  Core &core = frame_.core;
  std::uint32_t depthSum = 0;
  bool lastDeep = false;
  do {
    core.gte().execute(gte::kRtps);
    const UnpackedVertex vertex = unpack(next_);
    next_ = frame_.memory.r32(cursor_);
    cursor_ += 4;
    const std::uint32_t depth = core.gte().readData(gte::kSz3);
    std::uint32_t sxy = core.gte().readData(gte::kSxy2);
    lastDeep = depth >= kFarDepth;
    if (sumsDepth(lastDeep)) {
      depthSum += depth;
    }
    if (lastDeep) {
      load(vertex);
      store({edgeOutcode(sxy), depth});
      continue;
    }
    if (depth < kCloseDepth && refines(depth)) {
      const std::uint32_t mac1 = core.gte().readData(gte::kMac1) + kRefineSpan;
      const std::uint32_t mac2 = core.gte().readData(gte::kMac2) + kRefineSpan;
      if (asSigned(mac1) > 0 && asSigned(mac1 - 2 * kRefineSpan) < 0 && asSigned(mac2) > 0 &&
          asSigned(mac2 - 2 * kRefineSpan) < 0) {
        core.gte().writeData(gte::kVz0, core.gte().readData(gte::kVz0) << 4);
        core.gte().writeData(gte::kVxy0, (core.gte().readData(gte::kVxy0) << 4) & kRefineMask);
        core.gte().execute(gte::kRtps);
        sxy = core.gte().readData(gte::kSxy2);
      }
    }
    std::uint32_t code = sxy << 5;
    if (reportsOverflow() && asSigned(core.gte().readControl(gte::kFlag)) < 0) {
      code += kOverflowOutcode;
      overflow_ = true;
    }
    load(vertex);
    if (frame_.bounds.aboveTop(sxy)) {
      code += 1;
    }
    if (frame_.bounds.belowBottom(sxy)) {
      code += 2;
    }
    if (frame_.bounds.atOrLeftOfLeft(sxy)) {
      code += 4;
    }
    if (frame_.bounds.atOrRightOfRight(sxy)) {
      code += 8;
    }
    frame_.v0 = code;
    frame_.v1 = (sxy << 16) - 0x2000000u;
    common_ &= code;
    store({code, depth});
  } while (cursor_ != end_);
  // A sector whose depths sum to zero is behind the camera; one whose last vertex was deep is not
  // given that test.
  if (testsBehind() && (common_ & 0xFu) == 0 && !lastDeep && depthSum == 0) {
    common_ = 0xFu;
  }
}

namespace {

// SCUS_944.25 800245D0: the camera in the light matrix's first three words.
class DetailProjector final : public SectorProjector {
public:
  explicit DetailProjector(TerrainFrame &frame) : SectorProjector(frame) {}

private:
  [[nodiscard]] std::uint32_t height(std::uint32_t word) const override {
    return (word << 3) & 0x1FFCu;
  }
  [[nodiscard]] std::uint32_t cameraRegister() const override {
    return gte::kLight0;
  }
  [[nodiscard]] bool refines(std::uint32_t depth) const override {
    return depth != 0;
  }
  [[nodiscard]] bool reportsOverflow() const override {
    return true;
  }
  [[nodiscard]] bool sumsDepth(bool) const override {
    return true;
  }
  [[nodiscard]] bool testsBehind() const override {
    return true;
  }
};

// SCUS_944.25 80025440.
class TranslucentProjector final : public SectorProjector {
public:
  explicit TranslucentProjector(TerrainFrame &frame) : SectorProjector(frame) {}

private:
  [[nodiscard]] std::uint32_t height(std::uint32_t word) const override {
    return word & 0x3FFu;
  }
  [[nodiscard]] std::uint32_t cameraRegister() const override {
    return gte::kLight0;
  }
  [[nodiscard]] bool refines(std::uint32_t depth) const override {
    return depth != 0;
  }
  [[nodiscard]] bool reportsOverflow() const override {
    return false;
  }
  [[nodiscard]] bool sumsDepth(bool deep) const override {
    return !deep;
  }
  [[nodiscard]] bool testsBehind() const override {
    return true;
  }
};

} // namespace

ProjectedSector projectSector(TerrainFrame &frame, std::uint32_t entry, NearPass pass) {
  if (pass == NearPass::Detail) {
    return DetailProjector(frame).project(entry);
  }
  return TranslucentProjector(frame).project(entry);
}

} // namespace spyro::guest_terrain
