// guest_moby_visibility.h — the moby visibility walk of this engine family, as a native routine.
//
// ONE ROUTINE, TWO IMAGES. SCUS_944.25 0x80043858..0x80044503 and SCUS_944.67
// 0x80030478..0x80031123 are the same routine: both are 811 instructions, both decompile to the
// same C with the addresses and one `gp` spill different. The per-image words it reaches are a
// per-title fact table below, so the traversal and the arithmetic are one implementation.
//
// WHAT IT WALKS. The level's mobys -- the 0x58-byte object records at the list pointer named in
// `Facts.firstRecordWord`. The terrain is the other half of the frame's draw (see
// guest_terrain_drawer.h).
//
// WHAT RETAIL DOES. Once per drawn frame (one caller, the object half of the frame's draw
// FUN_8004C534 at 0x8004C55C for SCUS_944.25 / FUN_8001EC5C at 0x8001EC74 for SCUS_944.67,
// immediately before the moby drawers) the walk visits every moby of the level and decides what
// those drawers will draw. It is hand-written assembly that saves every callee-saved register (and
// gp, sp, fp, ra) to the register save area, borrows them all, and parks list cursors in HI/LO and
// in unused GTE light-matrix registers. For each record it:
//
//   1. skips it on its flag byte, or ends the walk on 0xFF;
//   2. culls it on a world-space box around the camera (draw distance from the record);
//   3. defers it to a side buffer when its mesh is not resident or its reach byte is negative (zero
//   skips it);
//   4. rotates its centre into view space and culls its bounding sphere on far, near, horizontal
//      and vertical planes, classifying it as edge-touching (1) or fully inside (2);
//   5. composes its orientation onto the camera rotation, tests an optional second sphere, and
//      builds a 0x44-byte render-list entry: mesh pointers, shading, depth bias, the composed
//      matrix, the projected centre and the classification.
//
// WHY IT IS OWNED. Step 4's horizontal test is the 4:3 half-angle (guest_moby_frustum.h). At 16:9
// the canvas shows margins past the authored window and retail culls the mobys in them, so the
// routine applies the presentation plan's slope to every horizontal plane test; at 4:3 it is
// retail's. The drawers' edge tests stay 4:3, so a widened view lists a moby that reaches past that
// window and is clear of the eye as inside (2); its record keeps the frustum class.
//
// WHAT IT WRITES, all reproduced: the register save area, each visited record's classification byte
// (+0x4D), the render list (zero-terminated), the deferred-moby buffer (zero-terminated), the close
// list and its cursor, the far-shade watermark, and the GTE state it leaves for the drawer: VXY1/
// VZ1, the last rotation, TR, and the parked cursor registers. It returns with v0 = the guest word
// `Facts.exitV0` and v1 holding its last scratch value, as retail does.
//
// An override is a plain `void (*)(Core *)`, so a title's facts cannot be captured by it. The title
// binds them in a one-line thunk of its own and calls `walk` below.
#pragma once

#include "render/field/keyframe_blend.h"

#include "core.h"
#include "render/field/guest_render_globals.h"

#include <cstdint>
#include <optional>

namespace spyro::guest_moby {

// The walk's fields of the 0x58-byte record beyond spyro::moby_record's.
namespace record {
inline constexpr std::uint32_t kCloseFlag = 0x1C;   // negative: listed when near
inline constexpr std::uint32_t kKeyframes = 0x3C;   // bytes: slot A, slot B, frame A, frame B
inline constexpr std::uint32_t kKeyProgress = 0x40; // u8: frame A to B, see keyframe_blend.h
inline constexpr std::uint32_t kFlag = 0x48;        // i8: negative skips, -1 ends the walk
inline constexpr std::uint32_t kReach = 0x4C;       // u16: i8 reach in its low byte
// u8 written: 0, 1 edge, 2 inside (high byte of kReach).
inline constexpr std::uint32_t kClass = 0x4D;
inline constexpr std::uint32_t kDepthBias = 0x4E; // u16: bias byte, GPF scale byte
inline constexpr std::uint32_t kGroup = 0x52;     // u8 visibility group, >= 0xFE always visible
inline constexpr std::uint32_t kFlags = 0x54;     // bit 30 interpolated angles, bit 28 mirrored
inline constexpr std::int8_t kEnd = -1;           // kFlag of the record after the last
inline constexpr std::uint32_t kInterpolatedAngles = 1u << 30;
inline constexpr std::uint32_t kMirrored = 1u << 28;
} // namespace record

// The render list the walk writes for the drawer that runs next (SCUS_944.25 80044504,
// SCUS_944.67 80031124): 0x44-byte entries from kRenderListAboveScratch past the drawer scratch the
// walk parks in VXY1, ended by a zero class word.
inline constexpr std::uint32_t kDrawerScratchBelow = 0x3000; // below the frame's scratch block end
inline constexpr std::uint32_t kRenderListAboveScratch = 0x2000;
inline constexpr std::uint32_t kEntryStride = 0x44;
// The drawer's load of each face word, `lw $s0, 4($fp)` (SCUS_944.25 80044E2C, 80046008;
// SCUS_944.67 80031A4C, 80032C8C).
inline constexpr std::uint32_t kDrawerFaceLoad = 0x8FD00004u;
// Bits 0-2 of the face word, signed, << 2 << shift come off the SZ sum of four weights the drawer
// buckets by >> shift (SCUS_944.25 80044FB8..80044FD8, 800451C4..800451D8).
[[nodiscard]] constexpr std::int32_t drawerFaceSlots(std::uint32_t faceWord) {
  return (static_cast<std::int32_t>(faceWord << 29u) >> 29) * 4;
}
namespace entry {
inline constexpr std::uint32_t kClass = 0x00; // word: class byte, frame B share, depth bias
inline constexpr std::uint32_t kOverlay = 0x01;
inline constexpr std::uint32_t kDepthBias = 0x02;
inline constexpr std::uint32_t kMesh = 0x04;
inline constexpr std::uint32_t kOverlayMesh = 0x08;
inline constexpr std::uint32_t kTexture = 0x0C;        // frame A's vertex streams
inline constexpr std::uint32_t kOverlayTexture = 0x10; // frame B's
inline constexpr std::uint32_t kAnimation = 0x14;
inline constexpr std::uint32_t kDepthBin = 0x18; // u16: the bin its polygons sort back from
inline constexpr std::uint32_t kDetail = 0x1A;
inline constexpr std::uint32_t kDetailLimit = 0x1B;
inline constexpr std::uint32_t kCentre = 0x1C; // x, y, z words: the drawer's translation
inline constexpr std::uint32_t kRotation = 0x28;
inline constexpr std::uint32_t kDetailDepth = 0x3A;
inline constexpr std::uint32_t kFlags = 0x3C;
inline constexpr std::uint32_t kRecord = 0x40;
} // namespace entry

// u8 pair: the drawer counts with one or the other by the sign of its entry's first word.
inline constexpr std::uint32_t kMeshVertexCounts = 0x04;
// u8: the animation's frame count (SCUS_944.67 func_80034F80 reads it as m_NumFrames).
inline constexpr std::uint32_t kMeshFrameCount = 0x00;

struct Facts {
  // The walk's own entry, the address the native override is installed at.
  std::uint32_t entry = 0;
  // The name the framework's override differential and its logs identify this body by.
  const char *overrideName = "";

  std::uint32_t firstRecordWord = 0; // the level's moby array
  std::uint32_t drawerParameter = 0; // parked in VZ1 for the drawer that runs next
  std::uint32_t meshSlotTable = 0;   // per-mesh slot arrays
  std::uint32_t keyframeBlend = 0;   // frame B's share per progress byte
  std::uint32_t closeCursorHome = 0; // close-list cursor, written at the exit
  std::uint32_t exitV0 = 0;          // v0 on return
};

// Run one call of the walk against this image.
void walk(Core &core, const Facts &facts, const guest_render_globals::Globals &globals);

// The record's mesh in slots A and B of `keyframes` is resident and counts the same vertices: the
// drawer (SCUS_944.25 80044504) walks frame B's stream with slot A's count.
[[nodiscard]] bool
slotsShareLayout(Core &core, const Facts &facts, std::uint32_t meshIndex, std::uint32_t keyframes);

// Animation `animation` of mesh `meshIndex` loops from frame 0 over its frame count; nothing while
// the mesh is not resident.
[[nodiscard]] std::optional<keyframe_blend::Loop>
animationLoop(Core &core, const Facts &facts, std::uint32_t meshIndex, std::uint32_t animation);

} // namespace spyro::guest_moby
