// SCUS_942.28 80025928..800261A0: the sector loop. Each sector's bounding sphere is tested against
// the view; a visible one is marked drawn, put on the far list and, if near enough, the near list,
// and its animation slots the lists leave unclaimed are run.

#include "core.h"
#include "gte_registers.h"
#include "render/field/guest_view_slope.h"
#include "render/field/guest_word.h"
#include "render/terrain/guest_terrain_animation.h"
#include "render/terrain/guest_terrain_sphere.h"
#include "spyro1/render/terrain_passes.h"

namespace spyro1::terrain {
namespace {

namespace gte = psx::gte;
using spyro::guest_memory::WorkingMemory;
using spyro::guest_terrain::SectorSphere;
using spyro::guest_terrain::SphereCamera;

using spyro::guest_word::asSigned;

namespace sector {
constexpr std::uint32_t kCentreXY = 0x00;
constexpr std::uint32_t kCentreZ = 0x04; // z high; flags 0xE000; radius 0x1FFF
constexpr std::uint32_t kAnimationMarks = 0x18;
} // namespace sector

constexpr std::uint32_t kRadiusMask = 0x1FFFu;
constexpr std::uint32_t kFlagMask = 0xE000u;
constexpr std::uint32_t kNoFarList = 0x2000u;  // 80025B54
constexpr std::uint32_t kNoNearList = 0x4000u; // 80025B80
constexpr std::uint32_t kAlwaysFar = 0x8000u;  // 80025B5C
constexpr std::uint32_t kFarMargin = 0x100;
constexpr std::uint32_t kCloseDepth = 0x100;
constexpr std::uint32_t kGroupListEnd = 0xFF;
constexpr std::uint32_t kSectorMarkBytes = 0x100;
constexpr std::uint8_t kDrawn = 0xFF;

// List-entry bits, ORed into the sector address.
constexpr std::uint32_t kEdgeTouching = 1;
constexpr std::uint32_t kClose = 2;

class SectorLister {
public:
  SectorLister(TerrainFrame &frame, const Facts &facts)
      : frame_(frame), facts_(facts), core_(frame.core), memory_(frame.memory) {}

  void run(std::int32_t group);

private:
  void list(std::uint32_t index);

  TerrainFrame &frame_;
  const Facts &facts_;
  Core &core_;
  WorkingMemory &memory_;
  SphereCamera camera_;
  spyro::guest_moby_frustum::HorizontalSlope slope_;
  std::uint32_t nearDepth_ = 0;
  std::uint32_t nearCursor_ = 0;
  std::uint32_t farCursor_ = 0;
  std::uint32_t sectors_ = 0;
};

void SectorLister::run(std::int32_t group) {
  for (std::uint32_t i = 0; i < gte::kRotationWords; ++i) {
    core_.gte().writeControl(gte::kRotation + i, frame_.classificationRotationWord(i));
  }
  core_.gte().writeControl(gte::kTranslationX, 0);
  core_.gte().writeControl(gte::kTranslationY, 0);
  core_.gte().writeControl(gte::kTranslationZ, 0);
  camera_ = {.x = asSigned(frame_.positionWord(0)) >> 4,
             .y = asSigned(frame_.positionWord(1)) >> 4,
             .z = asSigned(frame_.positionWord(2)) >> 4};
  slope_ = spyro::presentedSlope(core_);
  // The marks are read by level code, so they stay retail's.
  if (frame_.realField()) {
    for (std::uint32_t offset = 0; offset < kSectorMarkBytes; offset += 4) {
      core_.mem_w32(facts_.sectorMarks + offset, 0);
    }
  }
  nearDepth_ = memory_.r32(facts_.lodTable + lod::kNearDepth) >> 4;
  nearCursor_ = frame_.scratch + kNearList;
  farCursor_ = frame_.scratch + kFarList;
  sectors_ = memory_.r32(facts_.lodTable + lod::kSectors);
  if (group >= 0) {
    const std::uint32_t lists = memory_.r32(facts_.lodTable + lod::kGroupLists);
    std::uint32_t entry = memory_.r32(lists + static_cast<std::uint32_t>(group) * 4);
    for (;;) {
      const std::uint32_t index = memory_.r8(entry);
      entry += 1;
      if (index == kGroupListEnd) {
        break;
      }
      list(index);
    }
  } else {
    const std::uint32_t count = memory_.r32(facts_.lodTable + lod::kSectorCount);
    for (std::uint32_t index = 0; index < count; ++index) {
      list(index);
    }
  }
  memory_.w32(farCursor_, 0);
  memory_.w32(nearCursor_, 0);
}

// 80025A30.
void SectorLister::list(std::uint32_t index) {
  const std::uint32_t sector = memory_.r32(sectors_ + index * 4);
  const std::uint32_t centreXY = memory_.r32(sector + sector::kCentreXY);
  const std::uint32_t centreZ = memory_.r32(sector + sector::kCentreZ);
  const std::uint32_t flags = centreZ & kFlagMask;
  const std::uint32_t r = centreZ & kRadiusMask;
  const spyro::guest_terrain::SphereWords words{
      .centreXY = centreXY, .centreZ = centreZ, .radius = r};
  const SectorSphere sphere = testSectorSphere(core_, words, camera_, slope_);
  if (!sphere.visible) {
    return;
  }
  // A margin sector is listed but neither marked nor animated: level code reads both.
  const bool retailSees =
      slope_ == spyro::guest_moby_frustum::kRetailSlope ||
      testSectorSphere(core_, words, camera_, spyro::guest_moby_frustum::kRetailSlope).visible;
  const std::uint32_t z = sphere.z;
  std::uint32_t marks = memory_.r32(sector + sector::kAnimationMarks);
  std::uint32_t entry = sector | kEdgeTouching;
  if (sphere.inside) {
    entry ^= kEdgeTouching;
  }
  if (frame_.realField() && retailSees) {
    core_.mem_w8(facts_.sectorMarks + index, kDrawn);
  }

  // 80025B4C: the far list's slot is written whether or not the sector takes it.
  std::uint32_t claim = 0xFFFFFFFFu;
  if ((flags & kNoFarList) == 0) {
    memory_.w32(farCursor_, entry);
    if ((flags & kAlwaysFar) != 0 || asSigned(nearDepth_ - (z + r + kFarMargin)) < 0) {
      farCursor_ += 4;
      claim <<= 16;
    }
  }
  if ((flags & kNoNearList) == 0) {
    const std::uint32_t nearSide = z - r;
    if (asSigned(nearDepth_ - nearSide) > 0) {
      claim &= 0xFFFFu;
      if (asSigned(nearSide - kCloseDepth) < 0) {
        entry += kClose;
      }
      memory_.w32(nearCursor_, entry);
      nearCursor_ += 4;
    }
  }
  marks |= claim;
  // 80025BB0: each claimed slot rewrites the level's own sector data.
  if (!frame_.realField() || !retailSees || marks == 0xFFFFFFFFu) {
    return;
  }
  animateSector(frame_, frame_.facts.classify.animation, sector, marks);
}

} // namespace

void listSectors(TerrainFrame &frame, const Facts &facts, std::int32_t group) {
  SectorLister(frame, facts).run(group);
}

} // namespace spyro1::terrain
