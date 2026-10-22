// rescue_star.h — SCUS_942.28's 0x80058864: the star burst over a freed dragon (D_80076248, set
// when a dragon statue is touched, drawn by the dragon cutscene's draw 0x8001CFDC). Eight inner
// points and eight spikes at the block's scale around its centre, drawn as sixteen additive flat
// triangles appended to the overlay pair. Runs over guest memory for the real field and over host
// memory for an in-between.
#pragma once

#include "projected_vertex.h"
#include "guest_memory_map.h"

#include "render/field/guest_working_memory.h"

#include <cstdint>

class Core;

namespace spyro1::rescue_star {

inline constexpr std::uint32_t kBlock = 0x80076248u;
inline constexpr std::uint32_t kBytes = 0x2Cu;
inline constexpr std::uint32_t kShown = kBlock + 0x00u;    // nonzero while the star is drawn
inline constexpr std::uint32_t kPosition = kBlock + 0x04u; // world x, y, z words
// Five RT words: its angles under the projection.
inline constexpr std::uint32_t kRotation = kBlock + 0x10u;
inline constexpr std::uint32_t kScale = kBlock + 0x24u;
inline constexpr std::uint32_t kColour = kBlock + 0x2Bu; // grey level of every triangle
// Where the points are staged: nine inner (the last repeats the first), then eight spikes.
inline constexpr std::uint32_t kStaging = psx::kScratchpadPhysicalBase;
inline constexpr std::uint32_t kStagingBytes = 0x44u;
inline constexpr std::uint32_t kPoints = 8;

class RescueStar {
public:
  RescueStar(Core &core, const psx::present::EmitMemory &memory);

  // The whole routine; the first packet it appended to the overlay pair.
  std::uint32_t draw();
  // The $v0 and $v1 the retail routine leaves: both the end of the staged spikes.
  void end();

private:
  // 80058864..80058934: the centre into the translation through the camera, projected.
  void placeCentre();
  // 80058938..800589DC: the inner ring at scale / 4096 and the spikes at scale / 1024.
  void projectPoints();
  // 800589E0..80058AE4: an additive draw mode, then per point a spike and a fan triangle.
  std::uint32_t drawTriangles();

  Core &core_;
  const psx::present::EmitMemory &memory_;
  psx::gte::ProjectedVertex centreScreen_;
};

} // namespace spyro1::rescue_star
