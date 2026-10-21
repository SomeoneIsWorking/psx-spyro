// render_facts.h — SCUS_944.25's addresses for the render routines it shares with SCUS_944.67
// (game/render/), each read at the instruction its comment names, and the overrides that bind them.
#pragma once

#include "render/engine_render_overrides.h"

#include <array>
#include <cstdint>

namespace spyro2 {

// The guest globals both routines share, each named by the instruction that reaches it.
//
//   8006A9EC  the register save area: the walk's prologue (80043858..8004388C) and the terrain
//             drawer's (80023BB4..80023BD8) both spill s0..s7, gp, sp, fp, ra here, twelve words
//             at four bytes apart, and both reload the same words on exit.
//   80067034  the frame's scratch block end. The walk reads it at 80043894 and derives every list
//             base from it; the drawer derives its sector-list base from it at 80023BF4.
//   80067030  next free primitive packet, written by the drawer's exit (80029118).
//   80066FFC  the ordering table, whose 8-byte depth bins both routines link into.
//   80067168  the far-shade watermark, lowered only by the walk (80044030..80044050).
//   80067000  the overlay pair, the 8 bytes below the table (80011E80); FUN_8001B390 appends to it
//             and the flatten links it after bin 0.
//   80067E84  the camera rotation, five RT words, read at 800438F8.
//   80067EAC  the camera position x, y, z, read at 800438FC.
//   8006B300  one visibility byte per terrain sector, written by the classification pass (800244F0)
//             and read by the walk for each moby's group byte.
//   80067EC8  three signed 16-bit camera angles, which FUN_8001C2F8 (called first by the per-field
//             body FUN_800156FC) turns into the rotations at 80067E98 and 80067E84.
inline constexpr spyro::guest_render_globals::Globals kRenderGlobals{
    .registerSaveArea = 0x8006A9ECu,
    .scratchBaseWord = 0x80067034u,
    .primitiveCursor = 0x80067030u,
    .orderingTable = 0x80066FFCu,
    .orderingTableMark = 0x80067168u,
    .overlayPair = 0x80067000u,
    .cameraRotation = 0x80067E84u,
    .cameraPosition = 0x80067EACu,
    .cameraAngles = 0x80067EC8u,
    // 80061BD8: the sine table FUN_8001B61C indexes; FUN_8001B664 reads its cosine a quarter turn
    // on.
    .cameraSineTable = 0x80061BD8u,
    .visibilityGroups = 0x8006B300u,
};

// The terrain drawer, 0x80023BB4.
//
//   80023C04  jal 0x80048D18      the drawer's only call out of its own body
//   80023C14  sw  $v0, ...        its v0 lands in the visible-sector count word 0x80067404
//   80023BF4  the scratch-list base: [0x80067034] - 0x3000
//
// The passes' own tables (the classification pass's sector table and count, the texture table, the
// fog level and colour words, and the split passes' cell/crack/UV tables) are named at their use in
// game/render/guest_terrain_*.cpp with the instruction that reaches them; those tables live in the
// same static data region in both images of this family and their addresses are quoted there per
// image.
inline constexpr spyro::guest_terrain::Facts kTerrainFacts{
    .entry = 0x80023BB4u,
    .overrideName = "spyro2-terrain-drawer",
    .visibilityCallSite = 0x80023C04u,
    .sectorVisibility = 0x80048D18u,
    .visibleSectorCount = 0x80067404u,
    .scratchListsBelowEnd = 0x3000u,
    .fogColour = 0x80067408u,     // RFC, GFC, BFC words
    .foggedColours = 0x800683F0u, // the faded copy of a sector's colours
    // The display bootstrap's own geometry init states SetGeomOffset(0x100, 0x78) for the 512-dot
    // display mode, so the guest's authored view is 512 columns wide.
    .nativeWidth = 512,
    // The outcode tests' `lui 0xD` and `lui 0xE4`: rows 13 and 228.
    .vertical = {.top = 0x000D0000u, .bottom = 0x00E40000u},
    // The passes' own tables, each named at its use in game/render/guest_terrain_*.cpp with the
    // instruction here that reaches it.
    .classify =
        {
            .sectorTable = 0x800673D4u,  // 8002497C
            .sectorCount = 0x800673D8u,  // 80024980
            .viewRotation = 0x80067E98u, // 80023C20: five RT words, camera-relative
            .animation =
                {
                    .tables = {0x800681E8u, 0x800681F0u, 0x800681F8u, 0x80068200u},
                    .vectorFlags = {0x2000u, 0x4000u},
                    .definitionOffsetsTarget = true,
                },
        },
    .detail =
        {
            .textureTable = 0x800673F4u, // 800245A0
            .fogLevel = 0x80067EC4u,     // 80024550: IR0 for the fog fade, 0 = none
        },
    .far =
        {
            .farDepth = 0x80067400u, // 80028B1C
        },
    .coarse =
        {
            .quadCells = 0x80061F60u,          // 80026438
            .quadCracks = 0x80061FB0u,         // 800262F0
            .pageRemaps = 0x80062040u,         // 80026564
            .triangleCracks = 0x800620E0u,     // 80026988
            .triangleCells = 0x80062090u,      // 80026AD0
            .triangleSubTexture = 0x80062290u, // 80026ADC
            .triangleUvs = 0x80062360u,        // 80026AE4
        },
    .fine =
        {
            .quadPieces = 0x80061F70u,     // 8002775C
            .quadCracks = 0x80061FE0u,     // 80027608
            .innerPoints = 0x80062080u,    // 80027514
            .pageRemaps = 0x80062040u,     // 80027824, the same table the coarse pass uses
            .trianglePieces = 0x800620A0u, // 80028248
            .triangleCracks = 0x80062170u, // 800280F4
            .pieceTexture = 0x800622A0u,   // 80028254
            .pieceShift = 0x800622E0u,     // 800282C4
            .triangleUvs = 0x80062460u,    // 8002825C
        },
    .resplit =
        {
            .queue = 0x2400u,              // the coarse split list, already walked
            .trianglePieces = 0x80062560u, // 80028974
            .quadPieces = 0x8006257Cu,     // 800287A8
        },
};

// The moby visibility walk, 0x80043858.
//
//   800438A4  the level's moby array, at 0x80066F14; the records are 0x58 bytes and the walk starts
//             at the LAST one.
//   800438B0  the drawer parameter parked in VZ1, at 0x80066FA4.
//   800438E4  the per-mesh slot array table, at 0x80068C94.
//   80043F0C  frame B's share per keyframe progress byte, at 0x80062868.
//   800440D0  v0 on return is 0x8006820C; 800440D8 stores the close-list cursor at 0x80068214.
//   80043B9C  the sine table, at 0x80061BD8; the cosine table is the same table a quarter turn on.
inline constexpr spyro::guest_moby::Facts kMobyFacts{
    .entry = 0x80043858u,
    .overrideName = "spyro2-moby-visibility",
    .firstRecordWord = 0x80066F14u,
    .drawerParameter = 0x80066FA4u,
    .meshSlotTable = 0x80068C94u,
    .keyframeBlend = 0x80062868u,
    .closeCursorHome = 0x80068214u,
    .exitV0 = 0x8006820Cu,
};

// The field's draw dispatcher, FUN_800155A0, called by the per-field body FUN_800156FC through
// FUN_80013A14 with $a0 = 0x3D in gameplay.
//
//   800155D4  a0 bit 4 calls FUN_8004C4FC, the terrain drawer's caller
//   80015788  FUN_8001BDB0 copies the camera position to 0x80067EB8 before the dispatcher runs
//   80041E04  the Spyro drawer's base 0x80069FF0, its position words; 800421B0 hands FUN_8001B6C8
//             the angle bytes at +0xC and 8004228C the head's at +0x10; 80041E34 and 80041E30
//             read the body's keyframes at +0x14 and progress at +0x1C, 80041F40 and 80041F3C the
//             head's at +0x18 and +0x1D
//   8001C68C  the level start clears the effects pass's pools and names the particle pool at
//             0x80066F18; each drawer walks its pool from the address below
// The title screen and its menus, game state 11: FUN_80015560 calls the title overlay's draw
// (0x8007BFC4), which calls the dispatcher with 0xD, then links its logo, text and menu sprites.
inline constexpr std::array<spyro::field_redraw::ScreenDraw, 1> kScreenDraws{{
    {.entry = 0x80015560u, .overrideName = "spyro2-title-screen-draw"},
}};

inline constexpr spyro::field_redraw::Facts kFieldFacts{
    .entry = 0x800155A0u,
    .overrideName = "spyro2-field-dispatcher",
    .terrainFlag = 0x4u,
    .drawCameraPosition = 0x80067EB8u,
    .spyroPosition = 0x80069FF0u,
    .spyroAngles = 0x80069FFCu,
    .spyroHeadAngles = 0x8006A000u,
    .spyroKeyframes = 0x8006A004u,
    .spyroKeyProgress = 0x8006A00Cu,
    .effects =
        {
            .sprites = 0x80068078u,   // 8004A8F8
            .glows = 0x80067D00u,     // 8004ACAC
            .beams = 0x8006B400u,     // 8004B1CC
            .trails = 0x8006A2B4u,    // 8004B730
            .trailBytes = 0x30u,      // 8004B73C
            .trailHead = 0x28u,       // 8004B748: 6 points
            .meshes = 0x80069F30u,    // 80015618
            .particles = 0x80066F18u, // 800481DC
        },
    .lastDrawer = 0x8004D604u,           // the letterbox, called at 800156D4 and nowhere else
    .lastDrawerName = "spyro2-field-letterbox",
    .screenDraws = kScreenDraws,
};

// The flatten, FUN_8001B2A8, called by the draw at 800159A4 and by three other draws (800146AC,
// 80014A54, 8004D938), each once per table.
inline constexpr std::uint32_t kFlatten = 0x8001B2A8u;

// The loading screen, game state 5: FUN_800151F0 shows the storybook while 0x800698F0 is 0 and its
// fade 0x800698EC is not, three pages at brightness fade << 4 up to 0x80, then "Auto Saving".
inline constexpr spyro::loading_picture::Facts kLoadingPicture{
    .entry = 0x800151F0u,
    .overrideName = "spyro2-loading-book",
    .gate = 0x800698F0u,
    .counter = 0x800698ECu,
    .pages = 3,
    .pageSlot = {spyro::depth_bins::kOverlayTable, 0},
};

inline constexpr spyro::EngineRenderFacts kRenderFacts{
    .globals = kRenderGlobals,
    .terrain = kTerrainFacts,
    .moby = kMobyFacts,
    .field = kFieldFacts,
    .flatten = kFlatten,
    .flattenName = "spyro2-depth-bins",
    .loadingPicture = kLoadingPicture,
};

} // namespace spyro2
