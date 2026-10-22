// flame.h — SCUS_942.28's DrawSpyroHornStrikeTrails 0x80058D64: Spyro's flame breath as eight
// parts, each a tip of four gouraud triangles around the part's newest arc entry and a textured
// ribbon down its arc path, placed at the anchor by the view-space rotation Spyro's draw published.
// Runs over guest memory for the real field and over host memory for an in-between.
#pragma once

#include "projected_vertex.h"

#include "render/field/guest_working_memory.h"

#include <array>
#include <cstdint>

class Core;

namespace spyro1::flame {

// A part's descriptor per length: start entry << 24, ribbon steps << 16, grey ramp << 8 (+0x80 for
// parts 4..7).
inline constexpr std::uint32_t kAgeConfig = 0x8006D94Cu;
// The arc entries, 0x10 bytes each: parts 0..3 0xC0 bytes apart, parts 4..7 0x100 apart from
// +0x300.
inline constexpr std::uint32_t kArcPaths = 0x8006DAA8u;
// The tip's four colour words, +0x10 when superflame.
inline constexpr std::uint32_t kCapColours = 0x8006E1A8u;

class FlameTrails {
public:
  FlameTrails(Core &core, const psx::present::EmitMemory &memory);

  // The whole routine.
  void draw();
  // The $v0 and $v1 the retail routine leaves.
  void end();

private:
  // 80058DBC..80058E70: the anchor into the translation, the flame's rotation loaded.
  void placeAnchor();
  // 80058E8C..80059424: one part, its tip and ribbon.
  void drawPart(std::uint32_t part);
  // 80058F2C..80059208: four triangles from the tip to the ring of the entry before it; false when
  // the tip lies before the first bin, which drops the part.
  bool drawTip(std::uint32_t entry);
  // A tip triangle: written, and linked into `slot` when it faces the camera.
  void tipTriangle(std::uint32_t slot,
                   const std::array<psx::gte::ProjectedVertex, 3> &screens,
                   const std::array<std::uint32_t, 3> &colours);
  // A part's walk: its newest arc entry, the last entry it may reach and its age descriptor.
  struct PartWalk {
    std::uint32_t entry = 0;
    std::uint32_t limit = 0;
    std::uint32_t descriptor = 0;
  };
  // 8005920C..80059424: the ribbon from the walk's entry toward the path's start.
  void drawRibbon(const PartWalk &walk, bool tipped);

  Core &core_;
  const psx::present::EmitMemory &memory_;
  std::uint32_t table_ = 0;
  std::uint32_t cursor_ = 0;
  bool superflame_ = false;
  std::uint32_t v0_ = 0;
  std::uint32_t v1_ = 0;
};

} // namespace spyro1::flame
