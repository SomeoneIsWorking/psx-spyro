// paired_actor_layout.h — the model, frame and part-entry layouts RasterizePairedActor
// 0x80023AC4 reads and writes, shared by its two halves (paired_actor*.cpp).
#pragma once

#include "gte_registers.h"
#include "render/field/guest_word.h"
#include "spyro1/render/actor_facts.h"

#include <cstdint>

namespace spyro1::actors::paired {

namespace gte = psx::gte;
using terrain::kRenderGlobals;

using spyro::guest_word::asSigned;

using spyro::guest_word::sra;

inline constexpr std::uint32_t kParts = 3;

// The model: animation headers from +0x38.
inline constexpr std::uint32_t kAnimationTable = 0x38;
// An animation header.
inline constexpr std::uint32_t kColourCount = 0x02; // halfword
inline constexpr std::uint32_t kVertexEnds = 0x08;  // a byte per part: its last vertex + 1
inline constexpr std::uint32_t kDepthShift = 0x0B;
inline constexpr std::uint32_t kAnimationWords = 0x10; // a base word per vertex
inline constexpr std::uint32_t kPolygonList = 0x14;    // byte length, then the polygons
inline constexpr std::uint32_t kColourList = 0x18;
inline constexpr std::uint32_t kFrameTable = 0x24;
// A frame: the part 1 and 2 offsets, then the byte and halfword streams.
inline constexpr std::uint32_t kFrameByteOffsets = 0x08;
inline constexpr std::uint32_t kFrameHalfOffsets = 0x0C;
inline constexpr std::uint32_t kFrameOffsets = 0x10;
inline constexpr std::uint32_t kFrameStreams = 0x18;

// A part's flags word: the vertex end << 16, these two bits, and the weight << 8 (low nibble used).
inline constexpr std::uint32_t kPlainHalves = 0x1000u; // the first vertex comes from the halfwords
inline constexpr std::uint32_t kPartnerHalves = 0x2000u; // the same for the partner frame

inline constexpr std::uint32_t kPartBytes = 0x44;
namespace part_entry {
inline constexpr std::uint32_t kFlags = 0x00;
inline constexpr std::uint32_t kOwnRotation = 0x03; // 0x80: load kRotation before decoding
inline constexpr std::uint32_t kTranslation = 0x04;
inline constexpr std::uint32_t kRotation = 0x10;
inline constexpr std::uint32_t kStreams = 0x24;        // words, bytes, halfwords
inline constexpr std::uint32_t kPartnerStreams = 0x34; // the same for the partner frame
} // namespace part_entry
// After the three parts: each part's offset from part 0, 12 bytes each.
inline constexpr std::uint32_t kPartOffsets = 0xCC;

// The vertex table's entries.
namespace vertex_entry {
inline constexpr std::uint32_t kDepth = 0x0;
inline constexpr std::uint32_t kVxy = 0x4; // the height after the cut's measure
inline constexpr std::uint32_t kVz = 0x8;
inline constexpr std::uint32_t kBytes = 0x10;
} // namespace vertex_entry

// 80023F5C..80023F84: past this depth, or outside 45 degrees plus this margin, nothing is drawn.
inline constexpr std::uint32_t kFarthest = 0x4000u;
inline constexpr std::uint32_t kSideMargin = 0x800u;
// 80023FA8: the depth one bin spans before the shift.
inline constexpr std::uint32_t kDepthSpan = 0x200u;

} // namespace spyro1::actors::paired
