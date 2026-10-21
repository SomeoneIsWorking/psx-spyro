// actor_facts.h — SCUS_942.28's actor passes as data: the guest words, lists and record
// layouts the list builders, the sprite queue and the paired actor read and write.
#pragma once

#include "guest_memory_map.h"
#include "spyro1/core/guest_globals.h"
#include "spyro1/render/terrain_facts.h"

#include <cstdint>

namespace spyro1::actors {

// The register save area, packet cursor, ordering table and camera words are the terrain drawer's
// (terrain::kRenderGlobals); the mesh table is spyro1::guest::kModels.

// The 0x2A00-byte work block every pass shares with the terrain drawer's scratch: the actor
// pointer list, the local depth bins, the draw list, the secondary list, the sprite queue and the
// shadow list.
inline constexpr std::uint32_t kWorkBlock = terrain::kFacts.scratch;
inline constexpr std::uint32_t kLocalBins = kWorkBlock; // 0x120 (head, first) pairs
inline constexpr std::uint32_t kLocalBinsBytes = 0x900u;
inline constexpr std::uint32_t kActorList = kWorkBlock;               // zero-ended record pointers
inline constexpr std::uint32_t kDrawList = kWorkBlock + 0x1600u;      // 0x38-byte entries
inline constexpr std::uint32_t kSecondaryList = kWorkBlock + 0x2200u; // zero-ended record pointers
inline constexpr std::uint32_t kSpriteQueue = kWorkBlock + 0x2400u;   // zero-ended record pointers
inline constexpr std::uint32_t kShadowList = kWorkBlock + 0x2800u;    // (record, byte) pairs

// g_anEntitySpikeTexWords + 8: the shadow list's end, carried from pass to pass.
inline constexpr std::uint32_t kShadowListEnd = 0x80075F00u;
// The words the pass ends return in $v0 (8001F74C).
inline constexpr std::uint32_t kShadowListHolder = 0x80075EF8u;

// The rest of the work block: the sprite queue's lit vertex colours.
inline constexpr std::uint32_t kColourTable = kWorkBlock + 0x2A00u;
inline constexpr std::uint32_t kColourTableBytes = 0x200u; // 0x7F vertices at most
inline constexpr std::uint32_t kWorkBlockBytes = 0x2A00u + kColourTableBytes;

// The scratchpad the sprite queue projects into: a screen word per vertex, its depth 0x200 on,
// and the camera copied to 0x3E0 (position x, y, z, then the five rotation words).
inline constexpr std::uint32_t kVertexScreens = psx::kScratchpadPhysicalBase;
inline constexpr std::uint32_t kVertexDepths = kVertexScreens + 0x200u;
inline constexpr std::uint32_t kCameraCopy = kVertexScreens + 0x3E0u;

// g_anWorldLightDirVec: three words the sprite queue builds its light matrix from (80022AC8).
inline constexpr std::uint32_t kLightVector = guest::kSpecularLightDirection;
// A record's +0x4C word picks its base colour here (>> 22) and its flat-shaded pair (>> 21).
inline constexpr std::uint32_t kPalette = 0x8006E3D8u;
inline constexpr std::uint32_t kFlatPalette = 0x8006E44Cu;

// RasterizePairedActor 0x80023AC4 draws Spyro from his record and model, staging in the work block:
// a 16-byte entry per vertex (depth, VXY, VZ, height), three 0x44-byte part entries followed by
// each part's offset, and the tinted vertex colours.
inline constexpr std::uint32_t kSpyroRecord = guest::kSpyro;
// The model pointer (mesh table entry 0).
inline constexpr std::uint32_t kSpyroModel = guest::kModels;
inline constexpr std::uint32_t kHornAnchor = 0x800786C8u;   // g_anSpyroHornStrikeAnchorPos
inline constexpr std::uint32_t kVertexDeltas = 0x8006D614u; // 128 packed delta words
inline constexpr std::uint32_t kVertexTable = kWorkBlock + 0x900u;
inline constexpr std::uint32_t kPartTable = kWorkBlock + 0x1900u;
inline constexpr std::uint32_t kTintedColours = kWorkBlock + 0x1D00u;

namespace spyro_record {
inline constexpr std::uint32_t kPosition = 0x00;   // x, y, z
inline constexpr std::uint32_t kAngles = 0x0C;     // a word of three angle bytes per part
inline constexpr std::uint32_t kAnimations = 0x18; // per part: animation, partner animation
inline constexpr std::uint32_t kFrames = 0x1E;     // per part: frame, partner frame
inline constexpr std::uint32_t kBlends = 0x24;     // per part: weight toward the partner frame
inline constexpr std::uint32_t kDepthBias = 0x27;
inline constexpr std::uint32_t kTint = 0x28; // colour, amount in the top byte
inline constexpr std::uint32_t kWaterLevel =
    0x30; // g_nSpyroSwimWaterFloorZ: non-zero, polygons are cut there
inline constexpr std::uint32_t kOrientation =
    0x34; // g_anSpyroBodyMtx: five rotation words, for the cut
inline constexpr std::uint32_t kBytes = 0x48;
} // namespace spyro_record

// A moby record, 0x58 bytes.
// The position, mesh and angle bytes are spyro::moby_record's.
namespace moby {
inline constexpr std::uint32_t kShadowFlag = 0x1C; // negative: stage a drop shadow
inline constexpr std::uint32_t kFrames = 0x3C;     // frame bytes: 0, 1 and their vertex steps 2, 3
inline constexpr std::uint32_t kShadowFrame = 0x3E;
inline constexpr std::uint32_t kBlend = 0x40;         // non-zero blends frame 0 toward frame 1
inline constexpr std::uint32_t kDepthBias = 0x4B;     // low six bits
inline constexpr std::uint32_t kSecondaryWord = 0x4C; // the sprite queue's palette selector
inline constexpr std::uint32_t kCullDistance = 0x50;  // halfword: low byte <<8, bit 8 adds 0x200
inline constexpr std::uint32_t kScreenSpace = 0x80;   // in the cull halfword: a HUD record
inline constexpr std::uint32_t kDrawn = 0x51;
inline constexpr std::uint32_t kTint = 0x57;
} // namespace moby

// A mesh (spyro1::guest::kModels entry): its frame pointers from +0x38, each frame's
// radius byte at +7.
namespace mesh {
inline constexpr std::uint32_t kFrames = 0x38;
inline constexpr std::uint32_t kRadius = 0x07;
inline constexpr std::uint32_t kDepthShift = 0x0B;
inline constexpr std::uint32_t kVertexSteps = 0x24;
} // namespace mesh

// A sprite-queue mesh (spyro1::guest::kModels entry): one frame, no animation.
namespace queued_mesh {
inline constexpr std::uint32_t kVertexCount = 0x00; // signed byte
inline constexpr std::uint32_t kPolygonCount = 0x01;
inline constexpr std::uint32_t kShadowByte = 0x02;
inline constexpr std::uint32_t kVertices = 0x04; // packed 3-byte vertices; bit 31 clear here
inline constexpr std::uint32_t kColours = 0x08;  // vertex colours when unlit
inline constexpr std::uint32_t kPolygons = 0x0C; // two words each
inline constexpr std::uint32_t kNormals =
    0x10; // 3-byte vertex normals, when kVertices is past them
} // namespace queued_mesh

// A draw-list entry, 0x38 bytes, as the list builders write it and the emitters read it.
namespace entry {
inline constexpr std::uint32_t kFlags = 0x00; // edge class | depth shift | blend | z step | tint
inline constexpr std::uint32_t kFrame = 0x04;
inline constexpr std::uint32_t kSteps = 0x08;
inline constexpr std::uint32_t kBlendSteps = 0x0C;
inline constexpr std::uint32_t kView = 0x10; // x, y, z
inline constexpr std::uint32_t kRotation = 0x1C;
inline constexpr std::uint32_t kRotation33 = 0x2C;
inline constexpr std::uint32_t kDepthBias = 0x2E;
inline constexpr std::uint32_t kSecondaryWord = 0x30;
inline constexpr std::uint32_t kRecord = 0x34;
inline constexpr std::uint32_t kBytes = 0x38;
} // namespace entry

// The flag word's edge class.
inline constexpr std::uint32_t kInsideView = 0x40000000u;
inline constexpr std::uint32_t kStraddlesView = 0x80000000u;

} // namespace spyro1::actors
