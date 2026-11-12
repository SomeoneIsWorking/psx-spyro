// render_facts.h — SCUS_944.25's addresses for the render routines it shares with SCUS_944.67
// (game/render/), each read at the instruction its comment names, and the overrides that bind them.
#pragma once

#include "core/draw_motion.h"
#include "render/engine_render_overrides.h"
#include "spyro2/render/intro_card_facts.h"

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

// Spyro's drawer, 0x80041DBC. It projects his vertices with RTPS and keeps each SZ3 (800423CC),
// then sorts every face into its own bucket table by the sum of its four depths less the face's
// bias, 64 of that sum to a bucket (80042C48), and links the buckets into the ordering table
// (800437F4): a per-face sort, so his faces test each other. Its translation is his position less
// the camera's (80041E04), it bins him by that depth >> 7 less the signed byte +0x1E (80042168),
// and 0x8004C534 calls it first for the reflection while +0x2C is set.
inline constexpr spyro::spyro_drawer_depth::Facts kSpyroDrawer{
    .entry = 0x80041DBCu,
    .overrideName = "spyro2-spyro-depth",
    .spyro = 0x80069FF0u,
};

// What the draw body's replay moves (core/draw_motion.h).
//
//   80041E04  the Spyro drawer's base 0x80069FF0, its position words; 800421B0 hands FUN_8001B6C8
//             the angle bytes at +0xC and 8004228C the head's at +0x10; 80041E34 and 80041E30
//             read the body's keyframes at +0x14 and progress at +0x1C, 80041F40 and 80041F3C the
//             head's at +0x18 and +0x1D
//   800495E8  the shadow drawer reads its position at 0x800698D4 and angles at 0x800698E0
//   8001C68C  the level start clears the effects pass's pools and names the particle pool at
//             0x80066F18; each drawer walks its pool from the address below
// The conversation handler 0x80013BC4 (state 1 of 0x800681C8) places the box at
// [0x800633F4 + 2 * ((timer - 0x18) / 2)] while mode 0x80067134 is 0 and the timer 0x80067130 is
// 0x18..0x3B, and at [0x80063418 + 2 * (timer / 2)] in mode 2; 0x80016328 adds the field delta to
// the timer each update.
inline constexpr spyro::ConversationFacts kConversation{
    .gameState = 0x800681C8u,
    .state = 1u,
    .mode = 0x80067134u,
    .timer = 0x80067130u,
    .tables = {{
        {.mode = 0u, .table = 0x800633F4u, .firstTick = 0x18u, .entries = 0x12u},
        {.mode = 2u, .table = 0x80063418u, .firstTick = 0u, .entries = 0xCu},
    }},
};

// 0x80052498(slot, offset, y), the HUD counters' slide: states 1 and 4 read 0x80063430 and
// 0x80063448 at (slide + offset) / 2, clamped to 0..0x2F, others 0x28, negated below y 0x78. The
// six slots from 0x8006765C step their slide by the field delta in 0x8005199C (0x80051D18 clears
// them).
inline constexpr spyro::HudSlots kHudSlots{
    .firstSlot = 0x8006765Cu,
    .slotBytes = 8u,
    .slotCount = 6u,
    .stateOffset = 0u,
    .slideOffset = 2u,
};

inline constexpr spyro::HudSlideFacts kHudSlide{
    .slots = &kHudSlots,
    .function = 0x80052498u,
    .slidingIn = 1u,
    .slidingOut = 4u,
    .inTable = 0x80063430u,
    .outTable = 0x80063448u,
    .lastIndex = 0x2F,
    .restOffset = 0x28,
    .lowerHalfY = 0x78,
};

// 0x8004C66C, the sky-object drawer: per object, index 0xE8($sp) (8004CBC8), it stores
// (sine 0x80061C58[(tick 0x8006700C * 4 + index * 0x40) & 0xFF] >> 4) + 0xE00 into both halves of
// 0x80068C08 (8004CBCC..8004CC30), then calls 0x800296D0 (8004D07C), which loads it into the GTE.
inline constexpr spyro::SkyWaveFacts kSkyWave{
    .consumer = 0x800296D0u,
    .objectSlot = 0xE8u,
    .tick = 0x8006700Cu,
    .sine = 0x80061C58u,
    .wave = 0x80068C08u,
    .step = 4u,
    .objectStep = 0x40u,
    .base = 0xE00,
};

//   8002F08C  the update anchors Spyro's flame breath 0x8006AA1C at his mouth while byte
//             0x8006AAB4 is set, lit by 8002FC20 only while it is clear; 0x80048F04 draws it
//   800155A0  the dispatcher draws the subtractive fade 0x80066F68 and the additive 0x80067150
//             (words) while non-zero; 8001FA58 ramps the subtractive fade both ways
// The credits overlay's letters: its update 0x8006D96C moves them, its draw 0x8006EE18 copies them
// into the moby array; game state 10 is the credits'.
inline constexpr spyro::CreditLetterFacts kCreditLetters{
    .pool = 0x80066FB8u, .gameState = 0x800681C8u, .credits = 10u};

inline constexpr spyro::DrawMotionFacts kDrawMotion{
    .cameraAngles = kRenderGlobals.cameraAngles,
    .cameraPosition = kRenderGlobals.cameraPosition,
    .spyro = 0x80069FF0u,
    .shadow = 0x800698D4u,
    .flame = 0x8006AA1Cu,
    .moby = &kMobyFacts,
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
            .allocator = 0x80041C50u,
        },
    .fades = {{{"subtractive fade", 0x80066F68u}, {"additive fade", 0x80067150u}}},
    .fadeCount = 2,
    .conversation = &kConversation,
    .hudSlide = &kHudSlots,
    .skyWave = &kSkyWave,
    .creditLetters = &kCreditLetters,
};

// The flatten, FUN_8001B2A8, called by the draw at 800159A4 and by three other draws (800146AC,
// 80014A54, 8004D938), each once per table.
inline constexpr std::uint32_t kFlatten = 0x8001B2A8u;

// The game state's step word: the level load's and the loading screen's sub-state.
inline constexpr std::uint32_t kGameSubState = 0x800698F0u;

// The letterbox FUN_8004D604, the dispatcher's last call (800156D4, its only site): steps
// 0x8006701C toward 12 while 0x80067100 is set and toward 0 after, bars down from row 0xC and up
// from 0xE4.
inline constexpr spyro::screen_overlays::LetterboxFacts kCinematicLetterbox{
    .entry = 0x8004D604u,
    .overrideName = "spyro2-field-letterbox",
    .height = 0x8006701Cu,
    .primitiveCursor = kRenderGlobals.primitiveCursor,
    .top = 0xC,
    .bottom = 0xE4,
    .rowPerDraw = true,
};

// The sky drawer, 0x80029170: its `jal` to the visibility leaf at 0x800291A8, and the sky's sector
// count and table at 0x800681B8.
inline constexpr spyro::guest_sky::Facts kSkyFacts{
    .entry = 0x80029170u,
    .overrideName = "spyro2-sky-drawer",
    .visibilityCallSite = 0x800291A8u,
    .sectors = 0x800681B8u,
};

inline constexpr spyro::EngineRenderFacts kRenderFacts{
    .globals = kRenderGlobals,
    .terrain = kTerrainFacts,
    .moby = kMobyFacts,
    // jal at 8004C564; its translation is the entry's centre, loaded at 800445A8.
    .mobyDrawer = {.entry = 0x80044504u,
                   .overrideName = "spyro2-moby-drawer",
                   .fieldPassReturn = 0x8004C56Cu,
                   .introCard = kIntroCard},
    .spyroDrawer = kSpyroDrawer,
    .sky = kSkyFacts,
    .flatten = kFlatten,
    .flattenName = "spyro2-depth-bins",
    .letterbox = kCinematicLetterbox,
};

} // namespace spyro2
