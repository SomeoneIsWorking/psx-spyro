// terrain_vegetation.h — added grass on the terrain faces a level's table marks (G006, plan
// more-vegetation): placed once per level from the near sectors, drawn host-native each terrain
// draw with the draw's camera, and inserted after the terrain of the same depth bin at the flatten
// (psx::gpu::HostPackets). Writes no guest memory.
#pragma once

#include "gp0_primitive_decode.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

class Core;

namespace spyro::guest_terrain {

struct TerrainFrame;

// One level's reviewed table: the texture records whose faces grow, and the path records nothing
// grows beside. Each title's table names the crop (tools/vegetation_listing.py) judged per record.
struct VegetationLevel {
  std::uint32_t level = 0;
  std::span<const std::uint8_t> grows;
  std::span<const std::uint8_t> paths;
};

// Where one image keeps what placement reads: the level word, the near sector table and the
// texture records, and how its sector vertices and polygons are packed.
struct VegetationFacts {
  std::uint32_t levelId = 0;
  std::uint32_t sectorTable = 0; // word holding the sector table; the count word follows it
  std::uint32_t textureTable = 0;
  std::uint32_t recordSize = 0;
  std::uint32_t fullBright = 0;  // the undarkened (uv, page) pair's offset in a record
  std::uint32_t heightShift = 0; // a vertex's height: (word << shift) & mask
  std::uint32_t heightMask = 0;
  bool textureByte = false; // the texture is the links byte (Spyro 1), not the draw word's low bits
  // A blade's middle height in vertex units: about a third of Spyro standing, as the title draws
  // him.
  std::uint32_t bladeHeight = 0;
  std::span<const VegetationLevel> levels;
};

class Vegetation {
public:
  // The terrain drawer drew `frame`: tufts near its camera built for the bins they sort into.
  void draw(TerrainFrame &frame, const VegetationFacts &facts);
  // The flatten returned `root`, the chain the walk draws: each built tuft goes after its bin's
  // last terrain packet.
  void link(Core &core, std::uint32_t root);

  // Faces kept, and faces dropped by each exclusion, at the last placement (overlay and log).
  struct Census {
    std::uint32_t faces = 0;
    std::uint32_t tufts = 0;
    std::uint32_t besidePath = 0;
    std::uint32_t translucent = 0;
  };

private:
  struct Tuft {
    std::int32_t x = 0; // vertex units, the sector packing's
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::uint32_t seed = 0;
    std::uint32_t colour = 0; // the face's first near colour
    std::uint32_t uv = 0;     // the face's full-bright pair: uv0 | clut << 16
    std::uint32_t page = 0;   // uv1 | tpage << 16
  };
  struct Built {
    std::uint32_t anchor = 0;
    psx::gpu::PacketBuffer<9> packet;
  };

  void place(Core &core, const VegetationFacts &facts, const VegetationLevel &level);

  std::uint32_t placedLevel_ = 0xFFFFFFFFu;
  std::uint32_t placedTable_ = 0;
  std::vector<Tuft> tufts_;
  std::vector<Built> built_;
  Census census_{};
  std::uint32_t site_ = 0;
};

} // namespace spyro::guest_terrain
