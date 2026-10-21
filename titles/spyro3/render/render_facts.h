// render_facts.h — SCUS_944.67's addresses for the render routines it shares with SCUS_944.25
// (game/render/), each read at the instruction its comment names, and the overrides that bind them.
#pragma once

#include "render/engine_render_overrides.h"

#include <array>
#include <cstdint>

namespace spyro3 {

// The guest globals both routines share.
//
//   80071540  the register save area. The walk's prologue at 80030478..800304AC is word-for-word
//             the Spyro 2 prologue at 80043858..8004388C -- `sw $s0..$s7, $gp, $sp, $fp, $ra` at
//             0x20..0x2C -- with the base `lui $at,0x8007 ; addiu $at,$at,0x1540`. The terrain
//             drawer's prologue spills the same twelve words to the same base.
//   8006C668  the frame's scratch block end: the walk reads it at 800304B8 and derives every list
//             base from it; the drawer derives its sector-list base from it at 800223B8.
//   8006C664  next free primitive packet, written by the drawer's exit.
//   8006C634  the ordering table, whose 8-byte depth bins both routines link into.
//   8006C7D4  the far-shade watermark, lowered only by the walk.
//   8006DFF8  the camera rotation, five RT words: the walk's own `lui $t9,0x8007 ; addiu
//             $t9,$t9,-0x2008` at 80030514..80030518.
//   8006E020  the camera position x, y, z.
//   80072330  one visibility byte per terrain sector, written by the classification pass and read
//             by the walk for each moby's group byte.
//   8006E03C  three signed 16-bit camera angles, which FUN_8004F6C4 (called first by the per-field
//             body 0x8001E638) turns into the rotations at 8006E00C and 8006DFF8.
inline constexpr spyro::guest_render_globals::Globals kRenderGlobals{
    .registerSaveArea = 0x80071540u,
    .scratchBaseWord = 0x8006C668u,
    .primitiveCursor = 0x8006C664u,
    .orderingTable = 0x8006C634u,
    .orderingTableMark = 0x8006C7D4u,
    .overlayPair = 0x8006C638u, // 8002AAE0, beside the table word as in SCUS_944.25
    .cameraRotation = 0x8006DFF8u,
    .cameraPosition = 0x8006E020u,
    .cameraAngles = 0x8006E03Cu,
    // 800658A0: the sine table 0x8004E9E4 indexes; 0x8004EA2C reads its cosine a quarter turn on.
    .cameraSineTable = 0x800658A0u,
    .visibilityGroups = 0x80072330u,
};

// The terrain drawer, 0x80022378.
//
//   800223C8  jal 0x8002D0D8      the drawer's only call out of its own body
//   800223D8  sw  $v0, ...        its v0 lands in the visible-sector count word 0x8006D078
//   800223B8  the scratch-list base: [0x8006C668] - 0x3000
//
// Its caller is FUN_8001EC24 (0x8001EC24), which is the structural twin of Spyro 2's FUN_8004C4FC:
// each clears the same 0x1C00 scratch block and then calls its drawer, and neither has any other
// call.
inline constexpr spyro::guest_terrain::Facts kTerrainFacts{
    .entry = 0x80022378u,
    .overrideName = "spyro3-terrain-drawer",
    .visibilityCallSite = 0x800223C8u,
    .sectorVisibility = 0x8002D0D8u,
    .visibleSectorCount = 0x8006D078u,
    .scratchListsBelowEnd = 0x3000u,
    .fogColour = 0x8006D07Cu,     // RFC, GFC, BFC words
    .foggedColours = 0x8006E588u, // the faded copy of a sector's colours
    // The geometry init states SetGeomOffset(0x100, 0x78) for the 512-dot display mode, the same
    // pair Spyro 2 states, so the guest's authored view is 512 columns wide.
    .nativeWidth = 512,
    // The outcode tests' `lui 0xD` and `lui 0xE4`: rows 13 and 228.
    .vertical = {.top = 0x000D0000u, .bottom = 0x00E40000u},
    // The passes' own tables. Each Spyro 2 address is the one its use in game/render/
    // guest_terrain_*.cpp names, and each Spyro 3 address was read out of THIS image at the
    // offset-aligned counterpart instruction -- which is exact rather than inferred, because the
    // two drawers are instruction-identical, so instruction N of one is instruction N of the other.
    // The static data region moves by a constant +0x3CC8 between the two images, which every one of
    // these sites confirms and which is why no address below was derived by arithmetic alone.
    .classify =
        {
            .sectorTable = 0x8006D048u,  // 80023140
            .sectorCount = 0x8006D04Cu,  // 80023144
            .viewRotation = 0x8006E00Cu, // 800223A8: five RT words, camera-relative
            .animation =
                {
                    .tables = {0x8006E364u, 0x8006E36Cu, 0x8006E374u, 0x8006E37Cu},
                    .vectorFlags = {0x2000u, 0x4000u},
                    .definitionOffsetsTarget = true,
                },
        },
    .detail =
        {
            .textureTable = 0x8006D068u, // 80022D64
            .fogLevel = 0x8006E038u,     // 80022978: IR0 for the fog fade, 0 = none
        },
    .far =
        {
            .farDepth = 0x8006D074u, // 800272E0
        },
    .coarse =
        {
            .quadCells = 0x80065C28u,          // 80024BFC
            .quadCracks = 0x80065C78u,         // 80024AB4
            .pageRemaps = 0x80065D08u,         // 80024D28
            .triangleCracks = 0x80065DA8u,     // 8002514C
            .triangleCells = 0x80065D58u,      // 80025294
            .triangleSubTexture = 0x80065F58u, // 800252A0
            .triangleUvs = 0x80066028u,        // 800252A8
        },
    .fine =
        {
            .quadPieces = 0x80065C38u,     // 80025F20
            .quadCracks = 0x80065CA8u,     // 80025DCC
            .innerPoints = 0x80065D48u,    // 80025CD8
            .pageRemaps = 0x80065D08u,     // 80025FE8, the same table the coarse pass uses
            .trianglePieces = 0x80065D68u, // 80026A0C
            .triangleCracks = 0x80065E38u, // 800268B8
            .pieceTexture = 0x80065F68u,   // 80026A18
            .pieceShift = 0x80065FA8u,     // 80026A88
            .triangleUvs = 0x80066128u,    // 80026A20
        },
    .resplit =
        {
            .queue = 0x2400u,              // the coarse split list, already walked
            .trianglePieces = 0x80066228u, // 80027138
            .quadPieces = 0x80066244u,     // 80026F6C
        },
};

// The moby visibility walk, 0x80030478.
//
//   800304C0  the level's moby array, at 0x8006C550; the records are 0x58 bytes and the walk starts
//             at the LAST one (`addi $ra,$ra,-0x58` at 8003053C).
//   800304CC  the drawer parameter parked in VZ1, at 0x8006C5DC.
//   80030508  the per-mesh slot array table, at 0x8006EE2C.
//   80030B2C  frame B's share per keyframe progress byte, at 0x80066530.
//   80030CF0  v0 on return is 0x8006E388 (`lui $v0,0x8007 ; addiu $v0,$v0,-0x1c78`); 80030CF8
//             stores the close-list cursor at 0x8006E390, eight bytes past it.
//   800307BC  the sine table, at 0x800658A0; the cosine table is the same table a quarter turn on,
//             at 0x80065920, which is what this image states.
inline constexpr spyro::guest_moby::Facts kMobyFacts{
    .entry = 0x80030478u,
    .overrideName = "spyro3-moby-visibility",
    .firstRecordWord = 0x8006C550u,
    .drawerParameter = 0x8006C5DCu,
    .meshSlotTable = 0x8006EE2Cu,
    .keyframeBlend = 0x80066530u,
    .closeCursorHome = 0x8006E390u,
    .exitV0 = 0x8006E388u,
};

// The field's draw dispatcher, FUN_8001E460, FUN_800155A0's body over this image's drawers, called
// by each state handler (gameplay FUN_8001D274) with $a0 = 0x3D.
//
//   8001E494  a0 bit 4 calls FUN_8001EC24, the terrain drawer's caller
//   8001E6D4  FUN_8004F178 copies the camera position to 0x8006E02C before the handler runs
//   8003CDE8  the Spyro drawer FUN_8003CDA0's base 0x80070328, laid out as SCUS_944.25's 0x80069FF0
//   8001E5F0  the letterbox FUN_8001FF44, its only call site
inline constexpr spyro::field_redraw::Facts kFieldFacts{
    .entry = 0x8001E460u,
    .overrideName = "spyro3-field-dispatcher",
    .terrainFlag = 0x4u,
    .drawCameraPosition = 0x8006E02Cu,
    .spyroPosition = 0x80070328u,
    .spyroAngles = 0x80070334u,
    .spyroHeadAngles = 0x80070338u,
    .spyroKeyframes = 0x8007033Cu,
    .spyroKeyProgress = 0x80070344u,
    .effects =
        {
            .sprites = 0x8006E1F4u,   // 8002ECCC
            .glows = 0x8006DEF8u,     // 8002F0CC
            .beams = 0x80072430u,     // 8002F5EC
            .trails = 0x80070610u,    // 8002FB6C
            .trailBytes = 0x6Cu,      // 8002FB78
            .trailHead = 0x64u,       // 8002FB84: 16 points
            .meshes = 0x80070260u,    // 8001E4D8
            .particles = 0x8006C554u, // 8003AAE8
        },
    .lastDrawer = 0x8001FF44u,
    .lastDrawerName = "spyro3-field-letterbox",
};

// The flatten, FUN_8004E664: FUN_8001B2A8's body over this image's table (8006C634) and mark
// (8006C7D4), plus the chain's tail stored at 8006C6B0. The draw 0x8001E638 calls it once per frame
// (8001EB74, or 8001E994 / 8001EAC0 in states 4 and 0xD); FUN_80020168 calls it at 80020290.
inline constexpr std::uint32_t kFlatten = 0x8004E664u;

// States 4 and 0xD then link a second pass into the emptied bins (state 4 the moby walk 0x80030478
// and 0x80031124, state 0xD 0x8003CDA0), flatten it again and chain it after the first through
// 8006C6B0. Its packets are drawn after the whole first chain, not in their bins' place, so these
// two calls assign nothing.
inline constexpr std::array<std::uint32_t, 2> kSecondPassFlattenReturns = {0x8001EA40u,
                                                                           0x8001EB4Cu};

inline constexpr spyro::EngineRenderFacts kRenderFacts{
    .globals = kRenderGlobals,
    .terrain = kTerrainFacts,
    .moby = kMobyFacts,
    .field = kFieldFacts,
    .flatten = kFlatten,
    .flattenName = "spyro3-depth-bins",
    .unbinnedFlattens = kSecondPassFlattenReturns,
};

} // namespace spyro3
