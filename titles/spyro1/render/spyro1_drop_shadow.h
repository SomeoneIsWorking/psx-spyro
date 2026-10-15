// spyro1_drop_shadow.h — SCUS_942.28's DrawSpyroDropShadow 0x80059A48: a ring of sixteen points
// around the shadow's ground point, sized by Spyro's frame and lifted by the sampled ground
// heights, drawn as sixteen subtractive gouraud triangles each behind its own draw mode and linked
// into the depth bin of its rim. Runs over guest memory for the real field and over host memory
// for an in-between.
#pragma once

#include "guest_working_memory.h"
#include "spyro1_shadow_block.h"

#include <cstdint>
#include <optional>

class Core;

namespace spyro1::shadow {

// The ring's sine/cosine pairs, one word per point (80059DDC).
inline constexpr std::uint32_t kRing = 0x8006E268u;
// Where the ring is staged: the frame's radius bytes, the ring's screen words and depths.
inline constexpr std::uint32_t kStaging = 0x1F800000u;
inline constexpr std::uint32_t kStagingBytes = 0xA0u;

class DropShadow {
public:
  DropShadow(Core &core, spyro::guest_memory::WorkingMemory &memory);

  // The whole routine; false when the shadow is disabled and nothing was touched.
  bool draw();
  // The $v0 and $v1 retail returns.
  void end();

private:
  // 80059A84..80059BBC: the ground point projected and the camera turned by Spyro's heading.
  void placeCentre();
  // 80059BBC..80059D20: the frame's eight radius bytes, blended toward the partner frame.
  void stageRadii();
  // 80059D24..80059DD0: sixteen radii and heights, shrunk while airborne.
  void stageRing();
  // 80059DD4..80059E70: the ring projected.
  void projectRing();
  // 80059E74..80059F80: a draw mode and a triangle per segment, linked by its rim's depth.
  void drawSegments();

  Core &core_;
  spyro::guest_memory::WorkingMemory &memory_;
  std::uint32_t v0_ = 0;
  std::uint32_t v1_ = 0;
  std::uint32_t centreScreen_ = 0;
  std::uint32_t centreDepth_ = 0;
};

} // namespace spyro1::shadow
