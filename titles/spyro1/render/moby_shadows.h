// moby_shadows.h — SCUS_942.28's moby shadows 0x80059F8C: each (record, radius) pair the list
// builders staged is a disc of four semi-transparent textured triangles around the moby's ground
// point, tilted by the record's two ground slopes and linked into the depth bin of its rim. Runs
// over guest memory for the real field and over host memory for an in-between.
#pragma once

#include "guest_memory_map.h"

#include "render/field/guest_working_memory.h"
#include "render/terrain/guest_terrain_screen.h"

#include <cstdint>

class Core;

namespace spyro1::shadows {

// The camera, the two texture words, the cursor and the table, then one disc's points and depths.
inline constexpr std::uint32_t kStaging = psx::kScratchpadPhysicalBase;
inline constexpr std::uint32_t kStagingBytes = 0x58u;

// m_ShadowDistance: the ground height in the low halfword, two slope angles in bits 16..21
// and 22..27.
inline constexpr std::uint32_t kGround = 0x1Cu;
// The moby's depth step, signed.
inline constexpr std::uint32_t kDepthStep = 0x47u;

class MobyShadows {
public:
  MobyShadows(Core &core,
              const psx::present::EmitMemory &memory,
              const spyro::guest_terrain::ScreenBounds &bounds);

  // 80059F8C..8005A02C: the camera, texture words, cursor and table staged.
  void begin();
  // 8005A02C..8005A454: one record's disc of `radius`.
  void draw(std::uint32_t record, std::uint32_t radius);
  // 8005A458: the cursor handed back.
  void end();

private:
  Core &core_;
  const psx::present::EmitMemory &memory_;
  spyro::guest_terrain::ScreenBounds bounds_;
};

} // namespace spyro1::shadows
