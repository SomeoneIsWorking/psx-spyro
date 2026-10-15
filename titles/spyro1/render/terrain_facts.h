// terrain_facts.h — SCUS_942.28's terrain drawer (0x800258F0) as data: the guest words and
// tables its passes read, each named by the instruction that reaches it.
#pragma once

#include "render/field/guest_render_globals.h"
#include "render/terrain/guest_terrain_facts.h"

#include <cstdint>

namespace spyro1::terrain {

// The level's sector-table block the drawer reads through one base, 0x800785A8 (800259A8).
namespace lod {
inline constexpr std::uint32_t kSectors = 0x00;     // the sector table
inline constexpr std::uint32_t kSectorCount = 0x04; // 800259EC
inline constexpr std::uint32_t kGroupLists = 0x08;  // 800259D8: one 0xFF-ended byte list per group
inline constexpr std::uint32_t kDetailTextures = 0x18; // 800267B4: 16-byte records
inline constexpr std::uint32_t kSplitTextures = 0x1C;  // 800276A4: 0xA8-byte records
inline constexpr std::uint32_t kNearDepth = 0x24;      // 800259B0: the near/far handover
inline constexpr std::uint32_t kFarDepth = 0x28;       // 80026210: past this nothing is drawn
} // namespace lod

struct Facts {
  std::uint32_t lodTable = 0;
  // 0x100 bytes the sector loop clears and marks 0xFF per sector it draws (80025980).
  std::uint32_t sectorMarks = 0;
  // The fixed block the sector lists and polygon flag bytes live in (800259B4).
  std::uint32_t scratch = 0;
  // Non-zero skips the far pass (800261D8).
  std::uint32_t farPassSkip = 0;

  // The coarse split pass's tables.
  std::uint32_t coarseQuadCells = 0;          // 80027D7C
  std::uint32_t coarseQuadCracks = 0;         // 80027C44
  std::uint32_t pageRemaps = 0;               // 80027EA8, shared with the fine pass
  std::uint32_t coarseTriangleCracks = 0;     // 80028294
  std::uint32_t coarseTriangleCells = 0;      // 800283DC
  std::uint32_t coarseTriangleSubTexture = 0; // 800283E8
  std::uint32_t triangleUvs = 0;              // 800283F0, shared with the fine pass

  // The fine split pass's tables.
  std::uint32_t fineQuadCracks = 0;         // 80028EBC: three entries per edge
  std::uint32_t fineInnerPoints = 0;        // 80028D98
  std::uint32_t fineQuadPieces = 0;         // 80029204
  std::uint32_t fineTriangleCracks = 0;     // 80029ACC: 0x6C bytes per dropped corner
  std::uint32_t fineTrianglePieces = 0;     // 80029E18
  std::uint32_t fineTriangleSubTexture = 0; // 80029E24
};

inline constexpr Facts kFacts{
    .lodTable = 0x800785A8u,
    .sectorMarks = 0x800771C8u,
    .scratch = 0x8006FCF4u,
    .farPassSkip = 0x8007591Cu,
    .coarseQuadCells = 0x8006CF48u,
    .coarseQuadCracks = 0x8006CF98u,
    .pageRemaps = 0x8006D058u,
    .coarseTriangleCracks = 0x8006D138u,
    .coarseTriangleCells = 0x8006D0E8u,
    .coarseTriangleSubTexture = 0x8006D378u,
    .triangleUvs = 0x8006D3C8u,
    .fineQuadCracks = 0x8006CFC8u,
    .fineInnerPoints = 0x8006D0D8u,
    .fineQuadPieces = 0x8006CF58u,
    .fineTriangleCracks = 0x8006D1C8u,
    .fineTrianglePieces = 0x8006D0F8u,
    .fineTriangleSubTexture = 0x8006D388u,
};

// The words the drawer shares with the rest of the draw.
//
//   80077DD8  the register save area (800258F0)
//   800757B0  the next free primitive packet, read at 800261F0 and written at exit (8002A6B0)
//   80075820  the ordering table base (80026208)
//   80076DD0  the projection rotation, row 1 scaled (800261A8); 80076DE4 the classification one
//             (8002592C + 0x14); 80076DF8 the camera position (+0x28)
//   80076E1C  the three angle halfwords the camera builder 0x80033C50 reads
//   8006CBF8  its sine table (0x80016C58), 8006CC78 its cosine table (0x80016CB0)
inline constexpr spyro::guest_render_globals::Globals kRenderGlobals{
    .registerSaveArea = 0x80077DD8u,
    .primitiveCursor = 0x800757B0u,
    .orderingTable = 0x80075820u,
    .cameraRotation = 0x80076DD0u,
    .cameraPosition = 0x80076DF8u,
    .cameraAngles = 0x80076E1Cu,
    .cameraSineTable = 0x8006CBF8u,
    .cameraCosineTable = 0x8006CC78u,
};

// The packet pool's end, which GamestateDraw 0x8001ED5C sets kPacketPoolBytes past the cursor
// (8001EDC4).
inline constexpr std::uint32_t kPacketPoolEnd = 0x80075780u;
inline constexpr std::uint32_t kPacketPoolBytes = 0x1C000u;
// The depth bins it flattens (8001F138), 8 bytes each.
inline constexpr std::uint32_t kOrderingTableBytes = 0x800u * 8;

// The shared passes' view of the drawer.
inline constexpr spyro::guest_terrain::Facts kSharedFacts{
    .entry = 0x800258F0u,
    .overrideName = "spyro1-terrain-drawer",
    .nativeWidth = 512,
    // The outcode tests' `lui 1` and `lui 0x100`: rows 1 and 256.
    .vertical = {.top = 0x00010000u, .bottom = 0x01000000u},
    .classify =
        {
            .viewRotation = 0x80076DE4u,
            .animation =
                {
                    // 80025BC8: [0x80078560 + 0x14 + 8 * slot]
                    .tables = {0x80078574u, 0x8007857Cu, 0x80078584u, 0x8007858Cu},
                },
        },
    .resplit =
        {
            .queue = 0x2000u,              // 80028588
            .trianglePieces = 0x8006D5C8u, // 8002A510
            .quadPieces = 0x8006D5E4u,     // 8002A344
        },
};

} // namespace spyro1::terrain
