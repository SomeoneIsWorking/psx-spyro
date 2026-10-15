// The terrain drawer's classification pass (SCUS_944.25 80023C0C..800245CC): which sectors are
// drawn, and by which pass.

#include "guest_terrain_passes.h"

#include "core.h"
#include "gte_registers.h"
#include "guest_render_globals.h"
#include "guest_terrain_animation.h"
#include "guest_terrain_frame.h"
#include "guest_terrain_sphere.h"
#include "guest_word.h"

#include <array>

namespace spyro::guest_terrain {
namespace {

namespace gte = psx::gte;

constexpr std::uint32_t kGroupCopyEnd = 0xFu; // SCUS_944.25 800244FC: whole 16-byte rows

namespace sector {
constexpr std::uint32_t kCentreXY = 0x00; // x in the high halfword, y in the low, camera units / 16
constexpr std::uint32_t kCentreZ = 0x04;  // z in the high halfword; flags 0xF000; radius 0x0FFF
constexpr std::uint32_t kAnimationMarks = 0x18; // four bytes, 0xFF once animated this frame
} // namespace sector

constexpr std::uint32_t kRadiusMask = 0x0FFFu;
constexpr std::uint32_t kFlagMask = 0xF000u;
constexpr std::uint32_t kNoFarList = 0x2000u;   // 80023DDC
constexpr std::uint32_t kNearFar = 0x8000u;     // 80023DE8: listed far from 0x160, not 0x760
constexpr std::uint32_t kTranslucent = 0x5000u; // 80023E2C
constexpr std::uint32_t kNoNearList = 0x4000u;  // 80023E40

constexpr std::int32_t kFarListDepth = 0x760;
constexpr std::int32_t kNearFarListDepth = 0x160;
constexpr std::int32_t kDetailListDepth = 0x7C0;
constexpr std::int32_t kTranslucentListDepth = 0x600;
constexpr std::int32_t kCloseDepth = 0x100;

// List-entry bits, ORed into the 4-aligned sector pointer.
constexpr std::uint32_t kEdgeTouching = 1;
constexpr std::uint32_t kClose = 2;

// Which lists took the sector: the byte that replaces its scratchpad visibility byte.
constexpr std::uint32_t kTakenNear = 1;
constexpr std::uint32_t kTakenFar = 2;

using spyro::guest_word::asSigned;

constexpr std::uint32_t asWord(std::int32_t value) {
  return static_cast<std::uint32_t>(value);
}

class SectorClassifier {
public:
  explicit SectorClassifier(TerrainFrame &frame)
      : frame_(frame), core_(frame.core), memory_(frame.memory) {}

  void run();

private:
  // A sector the visibility table marks, and that table's byte for it.
  struct Visible {
    std::uint32_t sector = 0;
    std::uint32_t visibilityByte = 0;
  };

  void classify(const Visible &visible);

  TerrainFrame &frame_;
  Core &core_;
  guest_memory::WorkingMemory &memory_;
  SphereCamera camera_;
  std::uint32_t detailCursor_ = 0;
  std::uint32_t translucentCursor_ = 0;
  std::uint32_t farCursor_ = 0;
};

void SectorClassifier::run() {
  for (std::uint32_t i = 0; i < gte::kRotationWords; ++i) {
    core_.gte().writeControl(gte::kRotation + i, frame_.classificationRotationWord(i));
  }
  core_.gte().writeControl(gte::kTranslationX, 0);
  core_.gte().writeControl(gte::kTranslationY, 0);
  core_.gte().writeControl(gte::kTranslationZ, 0);
  camera_ = {.x = asSigned(frame_.positionWord(0)) >> 4,
             .y = asSigned(frame_.positionWord(1)) >> 4,
             .z = asSigned(frame_.positionWord(2)) >> 4};
  const std::uint32_t count = memory_.r32(frame_.facts.classify.sectorCount);
  std::uint32_t slot = memory_.r32(frame_.facts.classify.sectorTable);
  const std::uint32_t end = kScratchpad + count;
  detailCursor_ = frame_.scratch + kDetailList;
  farCursor_ = frame_.scratch + kFarList;
  translucentCursor_ = frame_.scratch + kTranslucentList;

  for (std::uint32_t byte = kScratchpad; byte != end; ++byte) {
    slot += 4;
    if (memory_.r8(byte) != 0) {
      classify({.sector = memory_.r32(slot - 4), .visibilityByte = byte});
    }
  }
  memory_.w32(translucentCursor_, 0);
  memory_.w32(farCursor_, 0);
  memory_.w32(detailCursor_, 0);

  // 800244F0: the moby visibility walk (each moby's group byte at +0x52) is the guest's own, so a
  // real field must publish these bytes; an in-between field classifies from what it was handed.
  if (frame_.realField()) {
    const std::uint32_t copyEnd = (end + kGroupCopyEnd) & ~kGroupCopyEnd;
    std::uint32_t from = kScratchpad;
    std::uint32_t to = frame_.globals.visibilityGroups;
    do {
      for (std::uint32_t word = 0; word < 16; word += 4) {
        core_.mem_w32(to + word, memory_.r32(from + word));
      }
      from += 16;
      to += 16;
    } while (from != copyEnd);
  }
}

// 80023CB0..80023E88.
void SectorClassifier::classify(const Visible &visible) {
  const auto [sector, visibilityByte] = visible;
  memory_.w8(visibilityByte, 0);
  const std::uint32_t centreXY = memory_.r32(sector + sector::kCentreXY);
  const std::uint32_t centreZ = memory_.r32(sector + sector::kCentreZ);
  const std::uint32_t flags = centreZ & kFlagMask;
  const std::uint32_t r = centreZ & kRadiusMask;
  const SectorSphere sphere =
      testSectorSphere(core_,
                       {.centreXY = centreXY, .centreZ = centreZ, .radius = r},
                       camera_,
                       guest_moby_frustum::kRetailSlope);
  if (!sphere.visible) {
    return;
  }
  const std::uint32_t z = sphere.z;
  std::uint32_t marks = memory_.r32(sector + sector::kAnimationMarks);
  std::uint32_t entry = sector | kEdgeTouching;
  if (sphere.inside) {
    entry ^= kEdgeTouching;
  }

  // 80023DD4: which lists take it, and which halves of the animation marks it may claim.
  std::uint32_t claim = 0xFFFFFFFFu;
  std::uint32_t taken = 0;
  if ((flags & kNoFarList) != 0) {
    claim = 0xFFFF0000u;
  } else {
    const std::int32_t farDepth = (flags & kNearFar) != 0 ? kNearFarListDepth : kFarListDepth;
    if (asSigned(z + r - asWord(farDepth)) > 0) {
      const std::uint32_t farEntry =
          asSigned(z - r - asWord(kFarListDepth)) > 0 ? entry : entry | kClose;
      memory_.w32(farCursor_, farEntry);
      farCursor_ += 4;
      taken |= kTakenFar;
      claim = 0xFFFF0000u;
    }
  }
  const std::uint32_t nearSide = z - r;
  if (asSigned(nearSide - asWord(kDetailListDepth)) < 0) {
    if ((flags & kTranslucent) == 0) {
      if (asSigned(nearSide - asWord(kCloseDepth)) < 0) {
        entry += kClose;
      }
      memory_.w32(detailCursor_, entry);
      detailCursor_ += 4;
      taken |= kTakenNear;
      claim &= 0xFFFFu;
    } else if (asSigned(nearSide - asWord(kTranslucentListDepth)) < 0) {
      if ((flags & kNoNearList) != 0) {
        claim &= 0xFFFFu;
      } else {
        if (asSigned(nearSide - asWord(kCloseDepth)) < 0) {
          entry += kClose;
        }
        translucentCursor_ += 4;
        memory_.w32(translucentCursor_ - 4, entry);
        taken |= kTakenNear;
      }
    }
  }
  marks |= claim;
  memory_.w8(visibilityByte, static_cast<std::uint8_t>(taken));
  // 80023E8C..80024314: each slot whose mark byte is still clear animates once, and each of the
  // four writes a word of the level's own sector data, so an in-between field must not advance the
  // animation a second time inside one field.
  if (!frame_.realField() || marks == 0xFFFFFFFFu) {
    return;
  }
  animateSector(frame_, frame_.facts.classify.animation, sector, marks);
}

} // namespace

void classifySectors(TerrainFrame &frame) {
  SectorClassifier(frame).run();
}

} // namespace spyro::guest_terrain
