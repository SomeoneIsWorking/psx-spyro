#include "guest_terrain_mesh.h"

#include "core.h"
#include "gte_registers.h"
#include "guest_terrain_frame.h"

namespace spyro::guest_terrain {
namespace {

// The GTE register and command numbers are shared vocabulary (psxport gte_registers.h), not this
// pass's.
namespace gte = psx::gte;

constexpr std::int32_t asSigned(std::uint32_t value) {
  return static_cast<std::int32_t>(value);
}

constexpr std::uint32_t kFarDepth = 0x600;   // 80024800: at or past this, the plain outcodes
constexpr std::uint32_t kCloseDepth = 0x100; // 80024810: nearer than this may be re-projected
constexpr std::uint32_t kRefineSpan = 0x100; // 80024824: |MAC1|, |MAC2| under this
constexpr std::uint32_t kRefineMask = 0xFFF0FFF0u;
constexpr std::uint32_t kOverflowSeen = 0x80000000u;

// The sector origin, camera-relative, in the units the packed vertex offsets subtract from.
struct SectorOrigin {
  std::uint32_t z;
  std::uint32_t x;
  std::uint32_t y;
};

struct UnpackedVertex {
  std::uint32_t z;
  std::uint32_t xy;
};

UnpackedVertex unpack(std::uint32_t word, const SectorOrigin &origin, NearPass pass) {
  const std::uint32_t height = pass == NearPass::Detail ? (word << 3) & 0x1FFCu : word & 0x3FFu;
  const std::uint32_t z = ((word >> 19) & 0x1FFCu) + origin.z;
  const std::uint32_t x = origin.x - ((word >> 8) & 0x1FFCu);
  const std::uint32_t y = origin.y - height;
  return UnpackedVertex{z, x + (y << 16)};
}

void load(Core &core, const UnpackedVertex &vertex) {
  core.gte().writeData(gte::kVz0, vertex.z);
  core.gte().writeData(gte::kVxy0, vertex.xy);
}

class SectorProjector {
public:
  SectorProjector(TerrainFrame &frame, std::uint32_t sector, NearPass pass);

  ProjectedSector run(std::uint32_t sectorClass);

private:
  void projectInside();
  void projectEdge();
  void projectClose();
  std::uint32_t edgeOutcode(std::uint32_t sxy);
  void advance();
  // One projected vertex's scratch words: its screen word (or outcode) and its depth.
  struct Projected {
    std::uint32_t word = 0;
    std::uint32_t depth = 0;
  };

  void store(const Projected &projected);

  TerrainFrame &frame_;
  Core &core_;
  TerrainMemory &memory_;
  NearPass pass_;
  SectorOrigin origin_{};
  std::uint32_t cursor_ = 0; // t7: the next vertex word to read
  std::uint32_t end_ = 0;    // s0
  std::uint32_t next_ = 0;   // at: the vertex word read one ahead
  std::uint32_t slot_ = kScratchpad;
  std::uint32_t depthSlot_ = kVertexDepths;
  std::uint32_t common_ = 0xFFFFFFFFu; // t0: the outcode bits every vertex shares
  std::uint32_t overflow_ = 0;         // s1
};

SectorProjector::SectorProjector(TerrainFrame &frame, std::uint32_t sector, NearPass pass)
    : frame_(frame), core_(frame.core), memory_(frame.memory), pass_(pass) {
  const std::uint32_t zx = memory_.r32(sector + near_sector::kOriginZX);
  const std::uint32_t y = memory_.r32(sector + near_sector::kOriginY);
  origin_.z = (zx >> 14) - frame.core.gte().readControl(gte::kLight0);
  origin_.x = frame.core.gte().readControl(gte::kLight1) - ((zx & 0xFFFFu) << 2);
  origin_.y = frame.core.gte().readControl(gte::kLight2) - (y >> 14);
  const std::uint32_t layout = memory_.r32(sector + near_sector::kLayout);
  const std::uint32_t first = sector + near_sector::kData + ((layout >> 22) & 0x3FCu);
  end_ = first + 4 * ((layout & 0xFFu) + 2);
  next_ = memory_.r32(first + 4);
  load(core_, unpack(memory_.r32(first), origin_, pass_));
  cursor_ = first + 8;
}

ProjectedSector SectorProjector::run(std::uint32_t sectorClass) {
  ProjectedSector result;
  result.vertexEnd = end_;
  if ((sectorClass & kCloseSector) != 0) {
    projectClose();
    result.clipped = true;
    result.overflowed = overflow_ != 0;
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

// Unpack the vertex read one ahead into VZ0/VXY0 for the next RTPS, and read the one after it.
void SectorProjector::advance() {
  const UnpackedVertex vertex = unpack(next_, origin_, pass_);
  next_ = memory_.r32(cursor_);
  cursor_ += 4;
  load(core_, vertex);
}

void SectorProjector::store(const Projected &projected) {
  memory_.w32(slot_, projected.word);
  memory_.w16(depthSlot_, static_cast<std::uint16_t>(projected.depth));
  slot_ += 4;
  depthSlot_ += 2;
}

// 800246B0.
void SectorProjector::projectInside() {
  do {
    core_.gte().execute(gte::kRtps);
    advance();
    const std::uint32_t sxy = core_.gte().readData(gte::kSxy2);
    const std::uint32_t depth = core_.gte().readData(gte::kSz3);
    frame_.v0 = sxy;
    frame_.v1 = depth;
    store({sxy, depth});
  } while (cursor_ != end_);
}

// 80024760: the edge outcode, also used for close vertices at or past kFarDepth.
std::uint32_t SectorProjector::edgeOutcode(std::uint32_t sxy) {
  std::uint32_t code = sxy << 5;
  if (aboveTop(sxy)) {
    code += 1;
  }
  if (belowBottom(sxy)) {
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

// 80024718.
void SectorProjector::projectEdge() {
  do {
    core_.gte().execute(gte::kRtps);
    advance();
    const std::uint32_t sxy = core_.gte().readData(gte::kSxy2);
    const std::uint32_t depth = core_.gte().readData(gte::kSz3);
    store({edgeOutcode(sxy), depth});
  } while (cursor_ != end_);
}

// 800247B4 (detail) and 80025608 (translucent).
void SectorProjector::projectClose() {
  std::uint32_t depthSum = 0;
  bool lastDeep = false;
  do {
    core_.gte().execute(gte::kRtps);
    const UnpackedVertex vertex = unpack(next_, origin_, pass_);
    next_ = memory_.r32(cursor_);
    cursor_ += 4;
    const std::uint32_t depth = core_.gte().readData(gte::kSz3);
    std::uint32_t sxy = core_.gte().readData(gte::kSxy2);
    lastDeep = depth >= kFarDepth;
    if (pass_ == NearPass::Detail || !lastDeep) {
      depthSum += depth;
    }
    if (lastDeep) {
      load(core_, vertex);
      store({edgeOutcode(sxy), depth});
      continue;
    }
    if (depth != 0 && depth < kCloseDepth) {
      const std::uint32_t mac1 = core_.gte().readData(gte::kMac1) + kRefineSpan;
      const std::uint32_t mac2 = core_.gte().readData(gte::kMac2) + kRefineSpan;
      if (asSigned(mac1) > 0 && asSigned(mac1 - 2 * kRefineSpan) < 0 && asSigned(mac2) > 0 &&
          asSigned(mac2 - 2 * kRefineSpan) < 0) {
        core_.gte().writeData(gte::kVz0, core_.gte().readData(gte::kVz0) << 4);
        core_.gte().writeData(gte::kVxy0, (core_.gte().readData(gte::kVxy0) << 4) & kRefineMask);
        core_.gte().execute(gte::kRtps);
        sxy = core_.gte().readData(gte::kSxy2);
      }
    }
    std::uint32_t code = sxy << 5;
    if (pass_ == NearPass::Detail && asSigned(core_.gte().readControl(gte::kFlag)) < 0) {
      code += kOverflowOutcode;
      overflow_ = kOverflowSeen;
    }
    load(core_, vertex);
    if (aboveTop(sxy)) {
      code += 1;
    }
    if (belowBottom(sxy)) {
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
  // 800248DC: an all-deep-free sector whose depths sum to zero is behind the camera. A sector
  // whose last vertex was deep is not given that test (80024940).
  if ((common_ & 0xFu) == 0 && !lastDeep && depthSum == 0) {
    common_ = 0xFu;
  }
}

} // namespace

ProjectedSector
projectSector(TerrainFrame &frame, std::uint32_t sector, std::uint32_t sectorClass, NearPass pass) {
  return SectorProjector(frame, sector, pass).run(sectorClass);
}

} // namespace spyro::guest_terrain
