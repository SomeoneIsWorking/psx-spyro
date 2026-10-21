// guest_render_globals.h — the guest globals this engine family's render routines share, as one
// image's facts.
#pragma once

#include "core.h"

#include <array>
#include <cstdint>

namespace spyro::guest_render_globals {

struct Globals {
  std::uint32_t registerSaveArea = 0;  // s0..s7, gp, sp, fp, ra
  std::uint32_t scratchBaseWord = 0;   // the frame's scratch block end
  std::uint32_t primitiveCursor = 0;   // next free primitive packet
  std::uint32_t orderingTable = 0;     // 8-byte depth bins: last packet linked, first
  std::uint32_t orderingTableMark = 0; // deepest bin a far prim reached
  // The screen overlays' (head, first) pair, flattened after every bin.
  std::uint32_t overlayPair = 0;
  std::uint32_t cameraRotation = 0; // five RT words
  std::uint32_t cameraPosition = 0; // x, y, z words
  // The three signed 16-bit angles the camera builder writes the rotation from (0x80067EC8 on
  // SCUS_944.25).
  std::uint32_t cameraAngles = 0;
  // 16-bit sine entries; cosine reads the same table kCosineOffset on.
  std::uint32_t cameraSineTable = 0;
  std::uint32_t visibilityGroups = 0; // one byte per terrain sector
};

// A quarter turn of 256 halfword sine entries.
inline constexpr std::uint32_t kCosineOffset = 0x80u;

// The registers both routines' prologues spill, in save-area order.
inline constexpr std::array<std::uint32_t, 12> kSavedRegisters = {
    16, 17, 18, 19, 20, 21, 22, 23, 28, 29, 30, 31};

// Retail's prologue; the native bodies never change these, so the epilogue needs no counterpart.
inline void spillBorrowedRegisters(Core &core, const Globals &globals) {
  for (std::size_t i = 0; i < kSavedRegisters.size(); ++i) {
    core.mem_w32(globals.registerSaveArea + static_cast<std::uint32_t>(4 * i),
                 core.r[kSavedRegisters[i]]);
  }
}

} // namespace spyro::guest_render_globals
