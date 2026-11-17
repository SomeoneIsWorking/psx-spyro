#include "render/terrain/terrain_vegetation.h"

#include "core.h"
#include "enhancement_gate.h"
#include "gte_registers.h"
#include "host_packets.h"
#include "render/field/guest_moby_visibility.h"
#include "render/field/guest_render_globals.h"
#include "render/field/moby_record.h"
#include "render/terrain/guest_depth_bins.h"
#include "render/terrain/guest_terrain_frame.h"
#include "render/terrain/guest_terrain_polygon.h"

#include <lucent/log.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <unordered_set>

namespace spyro::guest_terrain {
namespace {

psx::config::Var<long> cv_more_vegetation("more_vegetation", 0, psx::config::kSaved);
// Tufts per million square vertex units of a grown face.
psx::config::Var<long> cv_vegetation_density("vegetation_density", 48, psx::config::kSaved);
// How far from the camera tufts are drawn, in vertex units; they shrink to nothing toward it.
psx::config::Var<long> cv_vegetation_distance("vegetation_distance", 0x1400, psx::config::kSaved);
const psx::config::Registration kRegistration{
    cv_more_vegetation, cv_vegetation_density, cv_vegetation_distance};
const std::array<const char *, 2> kOffOn{"Off", "On"};
const psx::game::TitleIntSetting kSetting{.id = "more_vegetation",
                                          .label = "More vegetation",
                                          .unit = "",
                                          .var = &cv_more_vegetation,
                                          .range = {.min = 0, .max = 1, .step = 1},
                                          .labels = kOffOn};

constexpr std::uint32_t kGt3Code = 0x34000000u;
// A blade's base half-width, height range and spread from its tuft's centre, in its image's blade
// heights (VegetationFacts::bladeHeight).
constexpr float kBladeHalfWidth = 0.06f;
constexpr float kBladeHeightMin = 0.6f;
constexpr float kBladeHeightSpan = 0.8f;
constexpr float kTuftSpread = 0.14f;
// A blade's tip lies this share of its height out from its base.
constexpr float kBladeLeanMin = 0.15f;
constexpr float kBladeLeanSpan = 0.35f;
// Nearer than this, a blade's projection outgrows its texture's detail.
constexpr float kNearDepth = 96.0f;
// The blades' draw site and model: KSEG2, where no guest function or render-list entry lies.
constexpr std::uint32_t kBladeSite = 0xC0000000u;

std::uint32_t mix(std::uint32_t value) {
  value += 0x9E3779B9u;
  value = (value ^ (value >> 16)) * 0x85EBCA6Bu;
  value = (value ^ (value >> 13)) * 0xC2B2AE35u;
  return value ^ (value >> 16);
}

float unit(std::uint32_t value) {
  return static_cast<float>(value >> 8) / static_cast<float>(1u << 24);
}

struct Vertex {
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t z = 0;
  bool operator==(const Vertex &) const = default;
};

struct VertexHash {
  std::size_t operator()(const Vertex &v) const {
    return mix(static_cast<std::uint32_t>(v.x) ^
               mix(static_cast<std::uint32_t>(v.y) ^ mix(static_cast<std::uint32_t>(v.z))));
  }
};

// One near sector as the detail pass reads it (guest_terrain_mesh.h): vertices in vertex units,
// its colour table and its polygon records.
struct Sector {
  std::vector<Vertex> vertices;
  std::uint32_t colours = 0;
  std::uint32_t records = 0;
  std::uint32_t end = 0;
  bool translucent = false;
};

Sector readSector(Core &core, const VegetationFacts &facts, std::uint32_t sector) {
  Sector out;
  out.translucent = (core.mem_r32(sector + 4) & 0x5000u) == 0x5000u;
  const std::uint32_t zx = core.mem_r32(sector + near_sector::kOriginZX);
  const std::uint32_t y = core.mem_r32(sector + near_sector::kOriginY);
  const std::uint32_t layout = core.mem_r32(sector + near_sector::kLayout);
  const std::uint32_t first = sector + near_sector::kData + ((layout >> 22) & 0x3FCu);
  const std::uint32_t count = layout & 0xFFu;
  out.vertices.reserve(count);
  for (std::uint32_t v = 0; v < count; ++v) {
    const std::uint32_t word = core.mem_r32(first + 4 * v);
    out.vertices.push_back(
        {.x = static_cast<std::int32_t>(((zx & 0xFFFFu) << 2) + ((word >> 8) & 0x1FFCu)),
         .y = static_cast<std::int32_t>((y >> 14) +
                                        ((word << facts.heightShift) & facts.heightMask)),
         .z = static_cast<std::int32_t>((zx >> 14) + ((word >> 19) & 0x1FFCu))});
  }
  out.colours = first + 4 * count;
  out.records = out.colours + 2 * ((layout >> 6) & 0x3FCu);
  out.end = out.records + ((layout >> 12) & 0xFF0u);
  return out;
}

std::uint32_t textureOf(Core &core, const VegetationFacts &facts, std::uint32_t record) {
  if (facts.textureByte) {
    const std::uint8_t byte = core.mem_r8(record + polygon::kLinks);
    return byte == 0xFFu ? 0xFFFFFFFFu : byte & 0x7Fu;
  }
  return core.mem_r32(record + polygon::kDraw) & kTextureRecordMask;
}

} // namespace

const psx::game::TitleIntSetting &moreVegetationSetting() {
  return kSetting;
}

std::array<Vegetation::Blade, Vegetation::kBladesPerTuft> Vegetation::bladesOf(std::uint32_t seed) {
  std::array<Blade, kBladesPerTuft> blades{};
  for (Blade &blade : blades) {
    seed = mix(seed);
    const float angle = unit(seed) * 6.2831853f;
    blade = {.cos = std::cos(angle),
             .sin = std::sin(angle),
             .lean = kBladeLeanMin + kBladeLeanSpan * unit(mix(seed)),
             .height = kBladeHeightMin + kBladeHeightSpan * unit(mix(seed + 1)),
             .column = (seed >> 3) % 28u};
  }
  return blades;
}

void Vegetation::place(Core &core, const VegetationFacts &facts, const VegetationLevel &level) {
  tufts_.clear();
  const std::uint32_t table = core.mem_r32(facts.sectorTable);
  const std::uint32_t count = core.mem_r32(facts.sectorTable + 4);
  const std::uint32_t textures = core.mem_r32(facts.textureTable);
  const auto grows = [&level](std::uint32_t texture) {
    return std::ranges::find(level.grows, texture) != level.grows.end();
  };
  const auto isPath = [&level](std::uint32_t texture) {
    return std::ranges::find(level.paths, texture) != level.paths.end();
  };
  std::unordered_set<Vertex, VertexHash> pathCorners;
  std::vector<Sector> sectors;
  sectors.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    const std::uint32_t address = core.mem_r32(table + 4 * i);
    sectors.push_back(address != 0 ? readSector(core, facts, address) : Sector{});
    const Sector &sector = sectors.back();
    for (std::uint32_t r = sector.records; r < sector.end; r += polygon::kSize) {
      if (!isPath(textureOf(core, facts, r))) {
        continue;
      }
      const PackedIndices ids(core.mem_r32(r + polygon::kIndices));
      for (const std::uint32_t id : {ids.first, ids.second, ids.third, ids.fourth}) {
        if (id / 4 < sector.vertices.size()) {
          pathCorners.insert(sector.vertices[id / 4]);
        }
      }
    }
  }
  const double density = static_cast<double>(psx::config::enh_int(cv_vegetation_density)) / 1.0e6;
  for (const Sector &sector : sectors) {
    for (std::uint32_t r = sector.records; r < sector.end; r += polygon::kSize) {
      const std::uint32_t texture = textureOf(core, facts, r);
      const bool path = isPath(texture);
      if (!path && !grows(texture)) {
        continue;
      }
      const PackedIndices ids(core.mem_r32(r + polygon::kIndices));
      const std::array<std::uint32_t, 4> corner{
          ids.first / 4, ids.second / 4, ids.third / 4, ids.fourth / 4};
      if (*std::ranges::max_element(corner) >= sector.vertices.size()) {
        continue;
      }
      Status status = Status::Grown;
      if (path) {
        status = Status::Path;
      } else if (sector.translucent) {
        status = Status::Translucent;
      } else if (std::ranges::any_of(corner, [&](std::uint32_t c) {
                   return pathCorners.contains(sector.vertices[c]);
                 })) {
        status = Status::BesidePath;
      }
      const std::uint32_t colourWord = core.mem_r32(r + polygon::kColours);
      const std::uint32_t colour =
          core.mem_r32(sector.colours + ((colourWord >> 22) & 0x3FCu)) & 0xFFFFFFu;
      const std::uint32_t pair = textures + texture * facts.recordSize + facts.fullBright;
      const std::uint32_t uv = core.mem_r32(pair);
      const std::uint32_t page = core.mem_r32(pair + 4);
      // A quad is (first, second, third) and (first, third, fourth); a triangle repeats its third.
      const bool quad = !ids.triangle();
      for (std::uint32_t half = 0; half < (quad ? 2u : 1u); ++half) {
        const Vertex &a = sector.vertices[corner[0]];
        const Vertex &b = sector.vertices[corner[1 + half]];
        const Vertex &c = sector.vertices[corner[2 + half]];
        const double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
        const double vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
        const double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        const double area = std::sqrt(nx * nx + ny * ny + nz * nz) / 2.0;
        const std::uint32_t key = mix(r ^ (half << 31));
        const double wanted = area * density;
        const auto n = static_cast<std::uint32_t>(wanted + unit(key));
        for (std::uint32_t k = 0; k < n; ++k) {
          const std::uint32_t seed = mix(key + k);
          float s = unit(seed);
          float t = unit(mix(seed));
          if (s + t > 1.0f) {
            s = 1.0f - s;
            t = 1.0f - t;
          }
          tufts_.push_back({.x = a.x + static_cast<std::int32_t>(std::lround(s * ux + t * vx)),
                            .y = a.y + static_cast<std::int32_t>(std::lround(s * uy + t * vy)),
                            .z = a.z + static_cast<std::int32_t>(std::lround(s * uz + t * vz)),
                            .blades = bladesOf(mix(seed ^ 0x5BD1E995u)),
                            .colour = colour,
                            .uv = uv,
                            .page = page,
                            .status = status});
        }
      }
    }
  }
  // The collision under each tuft, in world units: the packing's z, x and height are the world's
  // x, y and z, each a quarter of a world unit's scale (guest_render_globals::kNearViewShift).
  struct Object {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    double radius = 0.0;
  };
  std::vector<Object> objects;
  for (std::uint32_t rec = core.mem_r32(facts.objects); facts.objectRadius != nullptr;
       rec += moby_record::kBytes) {
    const auto flag = static_cast<std::int8_t>(core.mem_r8(rec + guest_moby::record::kFlag));
    if (flag == guest_moby::record::kEnd) {
      break;
    }
    const std::uint32_t radius = flag < 0 ? 0 : facts.objectRadius(core, rec);
    if (radius != 0) {
      objects.push_back(
          {.x = static_cast<std::int32_t>(core.mem_r32(rec + moby_record::kX)),
           .y = static_cast<std::int32_t>(core.mem_r32(rec + moby_record::kY)),
           .z = static_cast<std::int32_t>(core.mem_r32(rec + moby_record::kZ)),
           .radius = static_cast<double>(radius << guest_render_globals::kNearViewShift)});
    }
  }
  // How far a tuft's blades reach from its base point: the farthest tip, in world units.
  const float tallest = kBladeHeightMin + kBladeHeightSpan;
  const double reach = static_cast<double>(
      std::hypot(kTuftSpread + kBladeHalfWidth + (kBladeLeanMin + kBladeLeanSpan) * tallest,
                 tallest) *
      static_cast<float>(facts.bladeHeight << guest_render_globals::kNearViewShift));
  const VegetationGround ground(core, facts.ground);
  std::array<std::uint32_t, static_cast<std::size_t>(Status::Count)> census{};
  for (Tuft &tuft : tufts_) {
    constexpr std::uint32_t shift = guest_render_globals::kNearViewShift;
    const std::int32_t x = tuft.z << shift;
    const std::int32_t y = tuft.x << shift;
    const std::int32_t z = tuft.y << shift;
    if (tuft.status == Status::Grown && std::ranges::any_of(objects, [&](const Object &o) {
          return std::hypot(x - o.x, y - o.y, z - o.z) < o.radius + reach;
        })) {
      tuft.status = Status::Object;
    }
    if (tuft.status == Status::Grown) {
      switch (ground.at(x, y, z)) {
      case VegetationGround::Kind::Ground:
        break;
      case VegetationGround::Kind::Bare:
        tuft.status = Status::Bare;
        break;
      case VegetationGround::Kind::Water:
        tuft.status = Status::Water;
        break;
      case VegetationGround::Kind::Special:
        tuft.status = Status::Special;
        break;
      case VegetationGround::Kind::Steep:
        tuft.status = Status::Steep;
        break;
      }
    }
    ++census[static_cast<std::size_t>(tuft.status)];
  }
  const auto tally = [&census](Status status) {
    return census[static_cast<std::size_t>(status)];
  };
  lucent::info("vegetation",
               "level {}: {} tufts grown, {} of them on no collision; dropped {} on water, {} on a "
               "special surface, {} steep, {} at {} objects, {} beside a path, {} translucent; {} "
               "on paths; {} collision triangles",
               level.level,
               tally(Status::Grown) + tally(Status::Bare),
               tally(Status::Bare),
               tally(Status::Water),
               tally(Status::Special),
               tally(Status::Steep),
               tally(Status::Object),
               objects.size(),
               tally(Status::BesidePath),
               tally(Status::Translucent),
               tally(Status::Path),
               ground.triangles());
}

void Vegetation::draw(TerrainFrame &frame, const VegetationFacts &facts) {
  built_.clear();
  Core &core = frame.core;
  if (facts.levels.empty() || psx::config::enh_int(cv_more_vegetation) == 0) {
    return;
  }
  const std::uint32_t levelId = core.mem_r32(facts.levelId);
  const auto level = std::ranges::find(facts.levels, levelId, &VegetationLevel::level);
  if (level == facts.levels.end()) {
    return;
  }
  const std::uint32_t table = core.mem_r32(facts.sectorTable);
  if (levelId != placedLevel_ || table != placedTable_) {
    place(core, facts, *level);
    placedLevel_ = levelId;
    placedTable_ = table;
  }
  // One model: a blade tests against the terrain and objects before it and paints over earlier
  // blades in order, so a bin's blades draw as one run whatever they overlap.
  core.depthTest.mark(kBladeSite, psx::present::DepthOrder::Model);
  const float reach = static_cast<float>(psx::config::enh_int(cv_vegetation_distance));
  const auto size = static_cast<float>(facts.bladeHeight);
  const psx::gte::RotationWords rw = frame.rotation();
  const auto element = [&rw](std::uint32_t index) {
    const std::uint32_t word = rw[index / 2];
    return static_cast<float>(
        static_cast<std::int16_t>(index % 2 == 0 ? word & 0xFFFFu : word >> 16));
  };
  // R11 R12 R13 / R21 R22 R23 / R31 R32 R33, 4.12 fixed point.
  std::array<float, 9> rot{};
  for (std::uint32_t i = 0; i < 9; ++i) {
    rot[i] = element(i) / 4096.0f;
  }
  const float h = static_cast<float>(core.gte().readControl(psx::gte::kH) & 0xFFFFu);
  const float ofx =
      static_cast<float>(static_cast<std::int32_t>(core.gte().readControl(psx::gte::kOfx))) /
      65536.0f;
  const float ofy =
      static_cast<float>(static_cast<std::int32_t>(core.gte().readControl(psx::gte::kOfy))) /
      65536.0f;
  const std::array<float, 3> camera{
      static_cast<float>(static_cast<std::int32_t>(frame.positionWord(0)) >>
                         guest_render_globals::kNearViewShift),
      static_cast<float>(static_cast<std::int32_t>(frame.positionWord(1)) >>
                         guest_render_globals::kNearViewShift),
      static_cast<float>(static_cast<std::int32_t>(frame.positionWord(2)) >>
                         guest_render_globals::kNearViewShift)};
  // The detail pass's view vector of a vertex (guest_terrain_mesh.cpp SectorProjector): the
  // packing's x and height pair with the camera's second and third words, its z with the first.
  const auto project = [&](float x, float y, float z) -> std::optional<psx::gte::ProjectedVertex> {
    const float vx = camera[1] - x;
    const float vy = camera[2] - y;
    const float vz = z - camera[0];
    const float rx = rot[0] * vx + rot[1] * vy + rot[2] * vz;
    const float ry = rot[3] * vx + rot[4] * vy + rot[5] * vz;
    const float rz = rot[6] * vx + rot[7] * vy + rot[8] * vz;
    if (rz < kNearDepth) {
      return std::nullopt;
    }
    const float sx = ofx + h * rx / rz;
    const float sy = ofy + h * ry / rz;
    if (std::abs(sx) > 1000.0f || std::abs(sy) > 1000.0f) {
      return std::nullopt;
    }
    return psx::gte::ProjectedVertex::nearest(
        GtePlace{.x = sx, .y = sy, .z = rz, .back = 0.0f, .model = kBladeSite});
  };
  const std::uint32_t capacity =
      static_cast<std::uint32_t>(psx::gpu::HostPackets::kPacketCapacity / 2);
  std::uint32_t inReach = 0;
  const bool map = lucent::channel_on("vegetationmap");
  for (const Tuft &tuft : tufts_) {
    const bool grows = tuft.status == Status::Grown || tuft.status == Status::Bare;
    if (!grows && !map) {
      continue;
    }
    const float dx = static_cast<float>(tuft.x) - camera[1];
    const float dy = static_cast<float>(tuft.y) - camera[2];
    const float dz = static_cast<float>(tuft.z) - camera[0];
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (distance >= reach) {
      continue;
    }
    ++inReach;
    // Full height to half the reach, then down to nothing at it, so the edge never pops.
    const float grown = std::min(1.0f, 2.0f * (1.0f - distance / reach));
    for (const Blade &blade : tuft.blades) {
      if (built_.size() == capacity) {
        break;
      }
      const float lean = blade.lean;
      const float height = grown * size * blade.height;
      const float ca = blade.cos;
      const float sa = blade.sin;
      const float bx = static_cast<float>(tuft.x) + kTuftSpread * size * ca;
      const float bz = static_cast<float>(tuft.z) + kTuftSpread * size * sa;
      const float by = static_cast<float>(tuft.y);
      const float half = kBladeHalfWidth * size;
      const auto left = project(bx - half * sa, by, bz + half * ca);
      const auto right = project(bx + half * sa, by, bz - half * ca);
      const auto tip = project(bx + lean * height * ca, by + height, bz + lean * height * sa);
      if (!left || !right || !tip) {
        continue;
      }
      const std::uint32_t bin = std::min(
          polygonBin(4 * static_cast<std::uint32_t>(left->place().z), 0), depth_bins::kBins - 1);
      std::uint32_t anchor = 0;
      for (std::uint32_t b = bin; b < depth_bins::kBins && anchor == 0; ++b) {
        anchor = frame.memory.mem_r32(frame.markSlot(b));
      }
      if (anchor == 0) {
        continue;
      }
      const std::uint32_t column = blade.column;
      const std::uint32_t base = tuft.uv & 0xFFFFu;
      const std::uint32_t clut = tuft.uv & 0xFFFF0000u;
      const std::uint32_t tpage = tuft.page & 0xFFFF0000u;
      const std::uint32_t colour =
          map ? kMapTints[static_cast<std::size_t>(tuft.status)] : tuft.colour;
      const std::uint32_t tipColour =
          map ? colour
              : std::min<std::uint32_t>(0xFF, (colour & 0xFF) * 5 / 4) |
                    std::min<std::uint32_t>(0xFF, ((colour >> 8) & 0xFF) * 5 / 4) << 8 |
                    std::min<std::uint32_t>(0xFF, ((colour >> 16) & 0xFF) * 5 / 4) << 16;
      Built &built = built_.emplace_back();
      built.anchor = anchor;
      psx::gpu::PacketBuffer<9> &packet = built.packet;
      packet.store(0, colour | kGt3Code);
      packet.storeVertex(1, *left);
      packet.store(2, clut | (base + 0x1F00u + column));
      packet.store(3, colour);
      packet.storeVertex(4, *right);
      packet.store(5, tpage | (base + 0x1F00u + column + 3));
      packet.store(6, tipColour);
      packet.storeVertex(7, *tip);
      packet.store(8, base + 0x0800u + column + 1);
    }
  }
  lucent::debug("vegetation",
                "camera ({}, {}, {}): {} tufts in reach, {} blades built",
                camera[0],
                camera[1],
                camera[2],
                inReach,
                built_.size());
}

void Vegetation::link(Core &core, std::uint32_t root) {
  if (built_.empty()) {
    return;
  }
  // A table flattened before and never walked keeps no stale tufts.
  core.hostPackets.discard(root);
  for (const Built &built : built_) {
    core.hostPackets.insertAfter(core, root, built.anchor, built.packet.view(), kBladeSite);
  }
  built_.clear();
}

} // namespace spyro::guest_terrain
