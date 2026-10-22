// guest_terrain_mesh.h — how the near terrain passes of this engine family's drawers (SCUS_944.25's
// detail pass 800245D0 and translucent pass 80025440, SCUS_942.28's detail pass 80026788) project
// one sector's vertices before they draw its polygons.
//
// A listed sector word is the sector's address with two classification bits in its low bits: bit 0
// when its bounding sphere touches a screen edge, bit 1 when it reaches within 0x100 of the camera.
// Its header locates a packed vertex array, and each vertex is rotated and projected with RTPS
// (camera RT, TR left at zero) relative to the sector origin minus the camera position the drawer
// parked in three GTE control registers.
//
// Every vertex leaves two things in the scratchpad: a word at kScratchpad + 4 * i and its depth SZ
// as a halfword at kVertexDepths + 2 * i. What the word holds depends on the sector's class:
//
//   inside   the projected SXY itself; the polygons are drawn without clip tests;
//   edge     SXY << 5 with four outcode bits (1 above, 2 below, 0xC left or right of the window);
//   close    as edge, but left and right are separate bits (4, 8), +0x10 when the projection
//            overflowed (where the pass reports it), and a vertex nearer than 0x100 whose |MAC1|,
//            |MAC2| are under 0x100 is projected again at 16x scale so its SXY does not collapse.
//
// A sector whose vertices are all outside one edge is skipped whole, as is (where the pass tests
// it) a close sector whose depths sum to zero.
#pragma once

#include "projected_vertex.h"

#include <cstdint>

namespace spyro::guest_terrain {

struct TerrainFrame;

// The sector header fields the near passes read.
namespace near_sector {
inline constexpr std::uint32_t kOriginZX = 0x08; // VZ origin << 14 in the high bits, VX / 4 low
inline constexpr std::uint32_t kOriginY = 0x0C;  // VY origin << 14 high; low: flag-byte offset
inline constexpr std::uint32_t kFog = 0x10;      // negative: the sector is never fogged
inline constexpr std::uint32_t kLayout = 0x14;   // vertex skip, colour count, polygon count
inline constexpr std::uint32_t kData = 0x1C;
} // namespace near_sector

inline constexpr std::uint32_t kSectorClassMask = 3;
inline constexpr std::uint32_t kEdgeSector = 1;
inline constexpr std::uint32_t kCloseSector = 2;

// The outcode bit a close vertex carries when its projection overflowed.
inline constexpr std::uint32_t kOverflowOutcode = 0x10;

struct ProjectedSector {
  bool drawn = false;          // false: the whole sector is off screen
  bool clipped = false;        // the scratchpad words are outcodes, not SXY
  bool overflowed = false;     // a close vertex's projection set the GTE error flag
  std::uint32_t vertexEnd = 0; // s0: two words past the last vertex word
};

// One near pass's sector projection. The loops are this family's; a pass says how its image packs
// a vertex and which close-vertex rules it keeps.
class SectorProjector {
public:
  SectorProjector(const SectorProjector &) = delete;
  SectorProjector &operator=(const SectorProjector &) = delete;

  // Project the sector at `sector` (class bits already removed) into the scratchpad.
  // `entry` is the sector's list word: its address with its class in the low bits.
  ProjectedSector project(std::uint32_t entry);

protected:
  explicit SectorProjector(TerrainFrame &frame);
  virtual ~SectorProjector() = default;

private:
  // The vertex word's height field, before it is subtracted from the origin.
  [[nodiscard]] virtual std::uint32_t height(std::uint32_t word) const = 0;
  // The first of the three GTE control registers the camera position (>> 2) is parked in.
  [[nodiscard]] virtual std::uint32_t cameraRegister() const = 0;
  // Whether a close vertex nearer than 0x100 at `depth` may be projected again at 16x scale.
  [[nodiscard]] virtual bool refines(std::uint32_t depth) const = 0;
  // Whether a close vertex reports a projection overflow in its outcode.
  [[nodiscard]] virtual bool reportsOverflow() const = 0;
  // Whether a close vertex's depth counts toward the sector's behind-the-camera sum, and whether
  // the pass tests that sum at all.
  [[nodiscard]] virtual bool sumsDepth(bool deep) const = 0;
  [[nodiscard]] virtual bool testsBehind() const = 0;

  struct UnpackedVertex {
    std::uint32_t z;
    std::uint32_t xy;
  };
  // The scratchpad word for one vertex (its SXY or its outcode) at the vertex's place, and its depth.
  struct Projected {
    psx::gte::ProjectedVertex vertex;
    std::uint32_t depth = 0;
  };

  [[nodiscard]] UnpackedVertex unpack(std::uint32_t word) const;
  void load(const UnpackedVertex &vertex);
  void projectInside();
  void projectEdge();
  void projectClose();
  std::uint32_t edgeOutcode(std::uint32_t sxy);
  void advance();
  void store(const Projected &projected);

  TerrainFrame &frame_;
  std::uint32_t originZ_ = 0;
  std::uint32_t originX_ = 0;
  std::uint32_t originY_ = 0;
  std::uint32_t cursor_ = 0; // t7: the next vertex word to read
  std::uint32_t end_ = 0;    // s0
  std::uint32_t next_ = 0;   // at: the vertex word read one ahead
  std::uint32_t slot_ = 0;
  std::uint32_t depthSlot_ = 0;
  std::uint32_t common_ = 0xFFFFFFFFu; // t0: the outcode bits every vertex shares
  bool overflow_ = false;
};

// SCUS_944.25's two near passes: the detail pass (800245D0) packs height (w << 3) & 0x1FFC and
// reports overflow; the translucent pass (80025440) packs w & 0x3FF and sums only shallow depths.
enum class NearPass : std::uint8_t {
  Detail,
  Translucent,
};

ProjectedSector projectSector(TerrainFrame &frame, std::uint32_t entry, NearPass pass);

} // namespace spyro::guest_terrain
