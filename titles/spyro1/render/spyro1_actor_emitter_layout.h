// spyro1_actor_emitter_layout.h — the words, frame layout and vertex encodings the moby emitters
// 0x8001F798 and 0x80020F34 read, shared by their halves (spyro1_actor_emitter*.cpp).
#pragma once

#include "gte_registers.h"
#include "guest_word.h"
#include "spyro1_actor_facts.h"

#include <array>
#include <cstdint>

namespace spyro1::actors::emitter {

namespace gte = psx::gte;
using spyro::guest_word::asSigned;
using spyro::guest_word::sra;
using terrain::kRenderGlobals;

// 8001F7F0: non-zero turns distance fog on; parked in ZSF3.
inline constexpr std::uint32_t kFogSwitch = 0x8007591Cu;
// 8001FEEC: the fog colour.
inline constexpr std::uint32_t kFogColour = 0x80078A50u;
// 8001F8A0: a scaled record farther than this is not drawn.
inline constexpr std::uint32_t kFarthestScaled = 0xF618u;
// The faded colour set, and the near detail runs' old words.
inline constexpr std::uint32_t kFadedColours = kWorkBlock + 0x1100u;
inline constexpr std::uint32_t kPatchSaves = kWorkBlock + 0x2A00u;
// Over 0x80 vertices the depths go to the work block instead of the scratchpad.
inline constexpr std::uint32_t kScratchDepthVertices = 0x80u;
inline constexpr std::uint32_t kWorkDepths = kWorkBlock + 0x900u;

// The packet commands the emitters write, in the colour word's top byte.
inline constexpr std::uint32_t kGouraudTriangle = 0x30000000u;
inline constexpr std::uint32_t kGouraudTexturedTriangle = 0x34000000u;
inline constexpr std::uint32_t kGouraudQuad = 0x38000000u;
inline constexpr std::uint32_t kGouraudTexturedQuad = 0x3C000000u;
inline constexpr std::uint32_t kTexturedSprite = 0x2C000000u;

// A frame, as the draw entry's kFrame points at it.
namespace frame {
inline constexpr std::uint32_t kColourCount = 0x02; // halfword
inline constexpr std::uint32_t kPositionShift = 0x05;
inline constexpr std::uint32_t kHalfShift = 0x06;
inline constexpr std::uint32_t kVertexCounts = 0x08; // near, then far
inline constexpr std::uint32_t kNearPolygons = 0x14;
inline constexpr std::uint32_t kNearColours = 0x18;
inline constexpr std::uint32_t kFarPolygons = 0x1C;
inline constexpr std::uint32_t kFarColours = 0x20;
} // namespace frame

// A steps record's first word: the vertex word stream, as a main-RAM offset.
constexpr std::uint32_t streamBase(std::uint32_t word) {
  return (word << 11) >> 11;
}

using Position = std::array<std::uint32_t, 3>;

// A vertex word: x from bit 21, y from bit 10, z the low twelve bits doubled.
constexpr Position decodeWord(std::uint32_t word) {
  return {sra(word, 21), sra(word << 10, 21), sra(word << 20, 19)};
}

// A sign-extended halfword delta: three 5-bit fields from bits 11, 6 and 1, scaled by `shift`.
constexpr void addHalf(Position &position, std::uint32_t half, std::uint32_t shift) {
  shift &= 31u;
  position[0] += sra(half, 11) << shift;
  position[1] -= sra(half << 21, 27) << shift;
  position[2] -= sra(half << 26, 27) << shift;
}

constexpr std::uint32_t signedHalf(std::uint16_t half) {
  return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(half)));
}

// The GTE's VXY0 word for a decoded position.
constexpr std::uint32_t packedXy(const Position &position) {
  return position[1] + (position[2] << 16);
}

} // namespace spyro1::actors::emitter
