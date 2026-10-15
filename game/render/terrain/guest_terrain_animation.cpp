#include "guest_terrain_animation.h"

#include "core.h"
#include "gte_registers.h"
#include "guest_terrain_frame.h"

namespace spyro::guest_terrain {
namespace {

namespace gte = psx::gte;

namespace sector {
constexpr std::uint32_t kFlagsHalf = 0x04;   // the low halfword of the centre z word
constexpr std::uint32_t kColourSkip = 0x10;  // u8: first colour word slot 1 animates
constexpr std::uint32_t kLayout = 0x14;      // packed offsets of the sector's data arrays
constexpr std::uint32_t kOverlaySkip = 0x17; // u8: first word slot 2 animates
constexpr std::uint32_t kAnimationMarks = 0x18;
constexpr std::uint32_t kData = 0x1C;
} // namespace sector

namespace animation {
constexpr std::uint32_t kKeyIndex = 0x02; // u8: the current key
constexpr std::uint32_t kTarget = 0x05;   // u8: first word slot 0/2 animates
constexpr std::uint32_t kStride = 0x06;   // u16: bytes per frame
constexpr std::uint32_t kDataOffset = 0x08;
constexpr std::uint32_t kKeys = 0x0C; // 8 bytes per key: fraction, from and toward at +4, +5, +6
} // namespace animation

// An INTPL packed vector: x in bits 21..31, y in 10..20, z in 0..9.
struct PackedVector {
  std::uint32_t x;
  std::uint32_t y;
  std::uint32_t z;
};

PackedVector unpack(std::uint32_t word) {
  return PackedVector{word >> 21, (word >> 10) & 0x7FFu, word & 0x3FFu};
}

void loadFarColour(Core &core, std::uint32_t colour) {
  core.gte().writeControl(gte::kFarRed, (colour << 4) & 0xFF0u);
  core.gte().writeControl(gte::kFarGreen, (colour >> 4) & 0xFF0u);
  core.gte().writeControl(gte::kFarBlue, (colour >> 12) & 0xFF0u);
}

class SectorAnimation {
public:
  SectorAnimation(TerrainFrame &frame, const AnimationLayout &layout, std::uint32_t sector)
      : core_(frame.core), memory_(frame.memory), layout_(layout), sector_(sector) {}

  void run(std::uint32_t marks);

private:
  // A vector slot: which slot, the key it selects, the sector flag it drives and where its words
  // land.
  struct VectorSlot {
    std::uint32_t slot = 0;
    std::uint32_t index = 0;
    std::uint32_t flagBit = 0;
    std::uint32_t target = 0;
  };

  void animateVectorSlot(const VectorSlot &vector);
  void animateColourSlot(std::uint32_t slot, std::uint32_t index);
  void animatePairSlot(std::uint32_t slot, std::uint32_t index);
  std::uint32_t definition(std::uint32_t slot, std::uint32_t index);

  Core &core_;
  guest_memory::WorkingMemory &memory_;
  const AnimationLayout &layout_;
  std::uint32_t sector_;
};

void SectorAnimation::run(std::uint32_t marks) {
  const auto index = [marks](std::uint32_t slot) {
    return (marks >> (8 * slot)) & 0xFFu;
  };
  if (index(0) < 0x80) {
    animateVectorSlot({.slot = 0,
                       .index = index(0),
                       .flagBit = layout_.vectorFlags[0],
                       .target = sector_ + sector::kData});
  }
  if (index(1) < 0x80) {
    animateColourSlot(1, index(1));
  }
  if (index(2) < 0x80) {
    animateVectorSlot(
        {.slot = 2,
         .index = index(2),
         .flagBit = layout_.vectorFlags[1],
         .target = sector_ + sector::kData + memory_.r8(sector_ + sector::kOverlaySkip) * 4});
  }
  if (index(3) < 0x80) {
    animatePairSlot(3, index(3));
  }
}

// Claims the slot's mark byte, then returns the slot's definition.
std::uint32_t SectorAnimation::definition(std::uint32_t slot, std::uint32_t index) {
  core_.mem_w8(sector_ + sector::kAnimationMarks + slot, 0xFF);
  return memory_.r32(memory_.r32(layout_.tables[slot]) + index * 4);
}

// Slots 0 and 2. The key's top byte also sets or clears the slot's sector flag, where the image
// has one.
void SectorAnimation::animateVectorSlot(const VectorSlot &vector) {
  const auto [slot, index, flagBit, target] = vector;
  const std::uint32_t def = definition(slot, index);
  const std::uint32_t key =
      memory_.r32(def + animation::kKeys + memory_.r8(def + animation::kKeyIndex) * 8 + 4);
  if (flagBit != 0) {
    std::uint32_t flagsHalf = memory_.r16(sector_ + sector::kFlagsHalf) & ~flagBit & 0xFFFFu;
    if ((key >> 24) != 0) {
      flagsHalf |= flagBit;
    }
    core_.mem_w16(sector_ + sector::kFlagsHalf, static_cast<std::uint16_t>(flagsHalf));
  }
  const std::uint32_t fraction = key & 0xFFu;
  const std::uint32_t stride = memory_.r16(def + animation::kStride);
  std::uint32_t to = target;
  if (layout_.definitionOffsetsTarget) {
    to += memory_.r8(def + animation::kTarget) * 4;
  }
  const std::uint32_t data = def + memory_.r32(def + animation::kDataOffset);
  std::uint32_t from = data + ((key >> 8) & 0xFFu) * stride;
  if (fraction == 0) {
    const std::uint32_t end = from + stride;
    do {
      core_.mem_w32(to, memory_.r32(from));
      from += 4;
      to += 4;
    } while (from != end);
    return;
  }
  core_.gte().writeData(gte::kIr0, fraction << 4);
  std::uint32_t toward = data + ((key >> 16) & 0xFFu) * stride;
  const std::uint32_t end = from + stride + 4;
  std::uint32_t near = memory_.r32(from);
  from += 4;
  std::uint32_t far = memory_.r32(toward);
  toward += 4;
  PackedVector v = unpack(near);
  core_.gte().writeData(gte::kIr1, v.x);
  core_.gte().writeData(gte::kIr2, v.y);
  core_.gte().writeData(gte::kIr3, v.z);
  do {
    const PackedVector f = unpack(far);
    core_.gte().writeControl(gte::kFarRed, f.x);
    core_.gte().writeControl(gte::kFarGreen, f.y);
    core_.gte().writeControl(gte::kFarBlue, f.z);
    near = memory_.r32(from);
    far = memory_.r32(toward);
    core_.gte().execute(gte::kIntpl);
    to += 4;
    from += 4;
    toward += 4;
    v = unpack(near);
    core_.gte().writeData(gte::kIr1, v.x);
    core_.gte().writeData(gte::kIr2, v.y);
    core_.gte().writeData(gte::kIr3, v.z);
    const std::uint32_t packed = (core_.gte().readData(gte::kMac1) << 21) +
                                 (core_.gte().readData(gte::kMac2) << 10) +
                                 core_.gte().readData(gte::kMac3);
    core_.mem_w32(to - 4, packed);
  } while (end != from);
}

// Slot 1: colour words, each placed by the 8-bit skip in its own top byte.
void SectorAnimation::animateColourSlot(std::uint32_t slot, std::uint32_t index) {
  const std::uint32_t def = definition(slot, index);
  const std::uint32_t key = def + animation::kKeys + memory_.r8(def + animation::kKeyIndex) * 8;
  const std::uint32_t fraction = memory_.r8(key + 4);
  const std::uint32_t stride = memory_.r16(def + animation::kStride);
  const std::uint32_t data = def + memory_.r32(def + animation::kDataOffset);
  std::uint32_t from = data + memory_.r8(key + 5) * stride;
  const std::uint32_t end = from + stride;
  std::uint32_t to = sector_ + sector::kData + memory_.r8(sector_ + sector::kColourSkip) * 4;
  if (fraction == 0) {
    do {
      const std::uint32_t word = memory_.r32(from);
      from += 4;
      to += (word >> 22) & 0x3FCu;
      core_.mem_w32(to, word & 0xFFFFFFu);
    } while (from != end);
    return;
  }
  std::uint32_t toward = data + memory_.r8(key + 6) * stride;
  core_.gte().writeData(gte::kIr0, fraction << 4);
  std::uint32_t near = memory_.r32(from);
  std::uint32_t far = memory_.r32(toward);
  do {
    core_.gte().writeData(gte::kRgbc, near & 0xFFFFFFu);
    loadFarColour(core_, far);
    const std::uint32_t skip = (near >> 22) & 0x3FCu;
    core_.gte().execute(gte::kDpcs);
    near = memory_.r32(from + 4);
    far = memory_.r32(toward + 4);
    from += 4;
    toward += 4;
    to += skip;
    core_.mem_w32(to, core_.gte().readData(gte::kRgb2));
  } while (end != from);
}

// Slot 3: colour pairs placed into two arrays the sector's layout word locates.
void SectorAnimation::animatePairSlot(std::uint32_t slot, std::uint32_t index) {
  const std::uint32_t def = definition(slot, index);
  const std::uint32_t key = def + animation::kKeys + memory_.r8(def + animation::kKeyIndex) * 8;
  const std::uint32_t fraction = memory_.r8(key + 4);
  const std::uint32_t stride = memory_.r16(def + animation::kStride);
  const std::uint32_t data = def + memory_.r32(def + animation::kDataOffset);
  std::uint32_t from = data + memory_.r8(key + 5) * stride;
  const std::uint32_t end = from + stride;
  const std::uint32_t layout = memory_.r32(sector_ + sector::kLayout);
  std::uint32_t first =
      sector_ + sector::kData + ((layout >> 22) & 0x3FCu) + ((layout << 2) & 0x3FCu);
  std::uint32_t second = first + ((layout >> 6) & 0x3FCu);
  if (fraction == 0) {
    do {
      const std::uint32_t a = memory_.r32(from);
      const std::uint32_t b = memory_.r32(from + 4);
      from += 8;
      const std::uint32_t skip = (a >> 22) & 0x3FCu;
      first += skip;
      second += skip;
      core_.mem_w32(first, a & 0xFFFFFFu);
      core_.mem_w32(second, b);
    } while (from != end);
    return;
  }
  std::uint32_t toward = data + memory_.r8(key + 6) * stride;
  core_.gte().writeData(gte::kIr0, fraction << 4);
  std::uint32_t near = memory_.r32(from);
  std::uint32_t far = memory_.r32(toward);
  do {
    core_.gte().writeData(gte::kRgbc, near & 0xFFFFFFu);
    loadFarColour(core_, far);
    const std::uint32_t skip = (near >> 22) & 0x3FCu;
    core_.gte().execute(gte::kDpcs);
    near = memory_.r32(from + 4);
    far = memory_.r32(toward + 4);
    first += skip;
    core_.mem_w32(first, core_.gte().readData(gte::kRgb2));
    core_.gte().writeData(gte::kRgbc, near);
    loadFarColour(core_, far);
    from += 8;
    toward += 8;
    core_.gte().execute(gte::kDpcs);
    near = memory_.r32(from);
    far = memory_.r32(toward);
    second += skip;
    core_.mem_w32(second, core_.gte().readData(gte::kRgb2));
  } while (from != end);
}

} // namespace

void animateSector(TerrainFrame &frame,
                   const AnimationLayout &layout,
                   std::uint32_t sector,
                   std::uint32_t marks) {
  SectorAnimation(frame, layout, sector).run(marks);
}

} // namespace spyro::guest_terrain
