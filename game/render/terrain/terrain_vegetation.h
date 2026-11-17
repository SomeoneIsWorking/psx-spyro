// terrain_vegetation.h — added grass on the terrain faces a level's table marks (G006, plan
// more-vegetation): placed once per level from the near sectors, drawn host-native each terrain
// draw with the draw's camera, and inserted after the terrain of the same depth bin at the flatten
// (psx::gpu::HostPackets). Writes no guest memory.
#pragma once

#include "gp0_primitive_decode.h"
#include "render/terrain/vegetation_ground.h"
#include "title_settings.h"

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
  VegetationGroundFacts ground;
  // The level's objects: the word holding its first 0x58-byte record (flag byte +0x48 negative
  // skipped, -1 after the last; Spyro 1's actor mode byte reads the same), and each one's bounding
  // radius in view units as its walk culls by, 0 for none. Nothing grows inside it at the level's
  // start.
  std::uint32_t objects = 0;
  std::uint32_t (*objectRadius)(Core &core, std::uint32_t record) = nullptr;
  std::span<const VegetationLevel> levels;
};

// The settings row: Off / On, off by default.
const psx::game::TitleIntSetting &moreVegetationSetting();

class Vegetation {
public:
  // The terrain drawer drew `frame`: tufts near its camera built for the bins they sort into.
  void draw(TerrainFrame &frame, const VegetationFacts &facts);
  // The flatten returned `root`, the chain the walk draws: each built tuft goes after its bin's
  // last terrain packet.
  void link(Core &core, std::uint32_t root);

private:
  // Why a tuft grows or not; the `vegetationmap` debug channel draws every tuft tinted by it.
  enum class Status : std::uint8_t {
    Grown,
    Bare, // grown, on a face with no collision under it
    Path,
    BesidePath,
    Translucent,
    Water,
    Special,
    Steep,
    Object,
    Count
  };
  // The map's tint by status, a blade's modulation colour (0x80 the texel as is): grown green,
  // on no collision white, path yellow, beside a path orange, translucent cyan, water blue,
  // special magenta, steep red, at an object dark grey.
  static constexpr std::array<std::uint32_t, static_cast<std::size_t>(Status::Count)> kMapTints{
      0x10FF10u,
      0xFFFFFFu,
      0x00E0FFu,
      0x0080FFu,
      0xFFFF00u,
      0xFF4020u,
      0xFF00FFu,
      0x1010FFu,
      0x303030u};
  static constexpr std::size_t kBladesPerTuft = 4;
  // A blade's shape, fixed by its tuft's seed: its heading, lean and height share, and the texel
  // column it is cut from.
  struct Blade {
    float cos = 0.0f;
    float sin = 0.0f;
    float lean = 0.0f;
    float height = 0.0f;
    std::uint32_t column = 0;
  };
  struct Tuft {
    std::int32_t x = 0; // vertex units, the sector packing's
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::array<Blade, kBladesPerTuft> blades{};
    std::uint32_t colour = 0; // the face's first near colour
    std::uint32_t uv = 0;     // the face's full-bright pair: uv0 | clut << 16
    std::uint32_t page = 0;   // uv1 | tpage << 16
    Status status = Status::Grown;
  };
  // A tuft's blades from its seed.
  static std::array<Blade, kBladesPerTuft> bladesOf(std::uint32_t seed);
  struct Built {
    std::uint32_t anchor = 0;
    psx::gpu::PacketBuffer<9> packet;
  };

  void place(Core &core, const VegetationFacts &facts, const VegetationLevel &level);

  std::uint32_t placedLevel_ = 0xFFFFFFFFu;
  std::uint32_t placedTable_ = 0;
  std::vector<Tuft> tufts_;
  std::vector<Built> built_;
};

} // namespace spyro::guest_terrain
