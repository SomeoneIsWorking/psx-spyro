// sky.h — SCUS_942.28's cyclorama drawer 0x8004EBA8: the sky sectors in front of the camera
// projected, outcoded and drawn as flat or gouraud triangles chained into the deepest depth bin.
// Runs over guest memory for the real field and over host memory for an in-between.
#pragma once

#include "guest_memory_map.h"

#include "gte_registers.h"
#include "render/field/guest_working_memory.h"
#include "render/terrain/guest_terrain_screen.h"

#include <array>
#include <cstdint>
#include <optional>

class Core;

namespace spyro1::sky {

// g_Cyclorama: the sector count, the sector pointers and the per-group sector byte lists.
inline constexpr std::uint32_t kCyclorama = 0x80078A40u;
// Where the sectors in front of the camera are listed, zero-ended (8004EC20).
inline constexpr std::uint32_t kSectorList = 0x8006FCF4u;
inline constexpr std::uint32_t kSectorListBytes = 0x1C00u;
// The outcode word per vertex, `sxy << 5 | edges`.
inline constexpr std::uint32_t kOutcodes = psx::kScratchpadPhysicalBase;
inline constexpr std::uint32_t kOutcodesBytes = psx::kScratchpadBytes;
// The deepest of the 0x800 depth bins.
inline constexpr std::uint32_t kSkyBin = 0x7FFu;

// The view rotation sectors are culled with and the projection rotation vertices are drawn with.
struct Rotations {
  psx::gte::RotationWords view{};
  psx::gte::RotationWords projection{};
};

// The packets one draw linked, first to last.
struct SkyChain {
  std::uint32_t first = 0;
  std::uint32_t last = 0;
};

class Cyclorama {
public:
  Cyclorama(Core &core,
            const psx::present::EmitMemory &memory,
            const spyro::guest_terrain::ScreenBounds &bounds);

  // The whole routine for sector group `group` (negative: every sector). Nullopt when nothing was
  // linked.
  std::optional<SkyChain> draw(std::int32_t group, const Rotations &rotations);
  // The $v0 and $v1 retail returns.
  void end();

private:
  void listSectors(std::int32_t group);
  void keepIfInFront(std::uint32_t sector, std::uint32_t &cursor);
  // One sector's vertices outcoded; false when all lie past one edge.
  bool outcodeSector(std::uint32_t sector);
  // One sector's polygons into the pool; false when the pool ran out.
  bool drawPolygons(std::uint32_t polygons);
  std::optional<SkyChain> link();

  Core &core_;
  const psx::present::EmitMemory &memory_;
  spyro::guest_terrain::ScreenBounds bounds_;
  std::uint32_t v0_ = 0;
  std::uint32_t v1_ = 0;
  // The sector's vertex base: z, x and y (s4, s5, s6), and where its colours start.
  std::uint32_t baseZ_ = 0;
  std::uint32_t baseX_ = 0;
  std::uint32_t baseY_ = 0;
  std::uint32_t colours_ = 0;
  // The packet cursor (fp), the first packet (gp), the last linked (sp), its length tag (t9) and
  // the pool limit (t8).
  std::uint32_t cursor_ = 0;
  std::uint32_t first_ = 0;
  std::uint32_t last_ = 0;
  std::uint32_t tag_ = 0;
  std::uint32_t limit_ = 0;
};

} // namespace spyro1::sky
