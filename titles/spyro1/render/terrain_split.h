// terrain_split.h — what SCUS_942.28's two subdivision passes (coarse 80027688, fine
// 80028590) share beyond the engine family's grid (guest_terrain_split.h, guest_terrain_grid.h).
//
// A split polygon's texture is a 0xA8-byte record of the level's split-texture table, chosen by
// the low 7 bits of its links word: the crack UV pair at +0, then a pair per coarse piece from +8
// and per fine piece from +0x28. Its neighbours' flag bytes (1 drawn whole, 2 deferred coarse,
// 4 deferred fine) sit at signed 12-bit offsets from its own in the links and draw words.
#pragma once

#include "render/terrain/guest_terrain_cull.h"
#include "render/terrain/guest_terrain_polygon.h"
#include "render/terrain/guest_terrain_split.h"
#include "spyro1/render/terrain_passes.h"

#include <array>
#include <cstdint>

namespace spyro1::terrain {

using spyro::guest_terrain::GpuSpan;
using spyro::guest_terrain::SplitPolygon;
using spyro::guest_terrain::SplitSector;

// The draw word's sides and ordering-table bias, which the detail pass reads too.
inline constexpr std::uint32_t kFlipped = 2;
inline constexpr std::uint32_t kTwoSided = 4;
inline constexpr std::uint32_t kBinBiasMask = 0x38;
// The links word's semi-transparency bit and dropped triangle corner.
inline constexpr std::uint32_t kSemiTransparent = 0x80;
inline constexpr std::uint32_t kCornerShift = 8;

// 80028408: twice the engine's bytes per turn and page class.
inline constexpr spyro::guest_terrain::PieceUvLayout kPieceUvs{.turnShift = 7,
                                                               .pageClassMask = 0x78};

inline constexpr std::uint32_t kSplitTextureSize = 0xA8;
inline constexpr std::uint32_t kCoarsePairs = 0x08;
inline constexpr std::uint32_t kFinePairs = 0x28;

[[nodiscard]] inline spyro::guest_terrain::Sidedness sidedness(std::uint32_t draw) {
  return {.twoSided = (draw & kTwoSided) != 0, .flipped = (draw & kFlipped) != 0};
}

[[nodiscard]] inline std::uint32_t binBias(std::uint32_t draw) {
  return (draw & kBinBiasMask) >> 1;
}

// One polygon of a split list, with the record words both passes read.
struct SplitRecord {
  std::uint32_t address = 0;
  std::uint32_t indices = 0;
  std::uint32_t colours = 0;
  std::uint32_t links = 0;
  std::uint32_t draw = 0;
  std::uint32_t texture = 0; // its split-texture record
  bool overflow = false;
};

class SplitPass : public spyro::guest_terrain::SplitPolygonPass {
public:
  // 800276CC: the sector's origin from the camera parked in the back colour, its vertex and
  // colour arrays, and the base its polygons' flag bytes are counted from.
  SplitSector sector(std::uint32_t header) override;

protected:
  SplitPass(TerrainFrame &frame, const Facts &facts);
  SplitPass(const SplitPass &) = delete;
  SplitPass &operator=(const SplitPass &) = delete;
  ~SplitPass() = default;

  [[nodiscard]] SplitRecord record(const SplitPolygon &polygon) const;
  // The polygon's corner colours in grid order, each with its GP0 code.
  [[nodiscard]] std::array<std::uint32_t, 4> quadColours(const SplitSector &sector,
                                                         const SplitRecord &record) const;
  [[nodiscard]] std::array<std::uint32_t, 3> triangleColours(const SplitSector &sector,
                                                             const SplitRecord &record) const;
  // One byte per edge: the neighbour's flag byte masked with `bit`.
  [[nodiscard]] std::uint32_t quadEdges(const SplitRecord &record, std::uint32_t bit) const;
  [[nodiscard]] std::uint32_t triangleEdges(const SplitRecord &record, std::uint32_t bit) const;
  [[nodiscard]] spyro::guest_terrain::CrackPolygon crack(const SplitRecord &record,
                                                         std::uint32_t colourAdjust) const;

  // A texture pair and how far a GT4 piece's UVs reach across it.
  struct PieceTexture {
    std::uint32_t pair = 0;
    std::uint32_t span = 0;
  };
  // A GT4 piece's four UVs: the pair's corner and `span` toward the others, remapped by the
  // page's top bits (80027E98).
  void writePieceUvs(const PieceTexture &texture);
  // Where a pass's GT3 pieces find their sub-textures: a signed byte per (layout, dropped corner)
  // offsetting the record's pairs.
  struct TrianglePieces {
    std::uint32_t subTextures = 0;
    std::uint32_t selectMask = 0;
    std::uint32_t pairs = 0;
  };
  // A GT3 piece's three UVs (80028408).
  void writeTrianglePieceUvs(const SplitRecord &record,
                             std::uint32_t layout,
                             const TrianglePieces &pieces);

  TerrainFrame &frame_;
  const Facts &facts_;
  std::uint32_t textures_ = 0; // the split-texture table
  std::uint32_t flagBase_ = 0; // s6: a record's flag byte is scratch + (record - this) / 16
};

} // namespace spyro1::terrain
