// vegetation_ground.h — what the world collision says about the ground under a point, for the
// added grass's placement (G006, plan more-vegetation step 3): water, a special surface, or
// steeper than Spyro can stand on. Read once per level from guest RAM; writes nothing.
#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

class Core;

namespace spyro::guest_terrain {

// Where one image keeps its world collision. The header's shared fields: +0x00 the triangle count,
// +0x04 how many triangles have a surface byte, +0x18 those bytes; its packed triangles are at
// `triangles` (Spyro 1 open-spyro world_collision.h; Spyro 2 0x8001C214, Spyro 3 0x8004F5DC).
struct VegetationGroundFacts {
  std::uint32_t header = 0;    // word holding the collision header
  std::uint32_t triangles = 0; // header field holding the packed triangle table
  // Water: a bit of a triangle's third word (Spyro 2 0x8003DB1C, Spyro 3 0x80018FF0), or the
  // level's death plane, the height at and below which the ground is water (Spyro 1 0x8004A088).
  std::uint32_t waterBit = 0;
  std::uint32_t deathPlanes = 0; // the death plane table, by the word at `levelIndex`
  std::uint32_t levelIndex = 0;
};

class VegetationGround {
public:
  enum class Kind : std::uint8_t { Ground, Water, Special, Steep, Bare };

  VegetationGround(Core &core, const VegetationGroundFacts &facts);

  // The collision triangle under world point (x, y) nearest height z: Bare where none covers it.
  [[nodiscard]] Kind at(std::int32_t x, std::int32_t y, std::int32_t z) const;
  [[nodiscard]] std::uint32_t triangles() const {
    return static_cast<std::uint32_t>(triangles_.size());
  }

private:
  struct Triangle {
    std::int32_t x[3]{};
    std::int32_t y[3]{};
    std::int32_t z[3]{};
    Kind kind = Kind::Ground;
  };

  static std::uint64_t cell(std::int32_t x, std::int32_t y);

  std::vector<Triangle> triangles_;
  std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> cells_;
  std::int32_t deathPlane_ = 0;
  bool deathPlaneKnown_ = false;
};

} // namespace spyro::guest_terrain
