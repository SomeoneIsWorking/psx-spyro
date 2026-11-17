#include "render/terrain/vegetation_ground.h"

#include "core.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace spyro::guest_terrain {
namespace {

constexpr std::uint32_t kTriangleCount = 0x00;
constexpr std::uint32_t kSurfaceCount = 0x04;
constexpr std::uint32_t kSurfaces = 0x18;
constexpr std::uint32_t kTriangleBytes = 12;
// A triangle's coordinates are 16 world units a step (UnpackWorldCollisionTri).
constexpr std::int32_t kCoordinateShift = 4;
// The surface byte's low bits index the special surfaces; this value is none (TRIGGER_REGION_NONE).
constexpr std::uint8_t kSurfaceMask = 0x3F;
// At or above this 8-bit slope Spyro slides (open-spyro SLOPE_STEEP; Spyro 2 0x80035E44).
constexpr double kSteepSlope = 0x17;
// The lookup grid's cell, world units: a triangle's corners lie within 255 steps of its first.
constexpr std::int32_t kCellShift = 12;

std::int32_t signedField(std::uint32_t word, std::uint32_t low, std::uint32_t bits) {
  return static_cast<std::int32_t>(word << (32 - low - bits)) >> (32 - bits);
}

} // namespace

std::uint64_t VegetationGround::cell(std::int32_t x, std::int32_t y) {
  return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x >> kCellShift)) << 32) |
         static_cast<std::uint32_t>(y >> kCellShift);
}

VegetationGround::VegetationGround(Core &core, const VegetationGroundFacts &facts) {
  const std::uint32_t header = core.mem_r32(facts.header);
  if (header == 0) {
    return;
  }
  if (facts.deathPlanes != 0) {
    deathPlane_ = static_cast<std::int32_t>(
        core.mem_r32(facts.deathPlanes + 4 * core.mem_r32(facts.levelIndex)));
    deathPlaneKnown_ = true;
  }
  const std::uint32_t count = core.mem_r32(header + kTriangleCount);
  const std::uint32_t surfaceCount = core.mem_r32(header + kSurfaceCount);
  const std::uint32_t surfaces = core.mem_r32(header + kSurfaces);
  const std::uint32_t table = core.mem_r32(header + facts.triangles);
  triangles_.resize(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    Triangle &t = triangles_[i];
    const std::uint32_t at = table + kTriangleBytes * i;
    const std::uint32_t wx = core.mem_r32(at);
    const std::uint32_t wy = core.mem_r32(at + 4);
    const std::uint32_t wz = core.mem_r32(at + 8);
    // x and y: the first corner's 14 bits, then two signed 9-bit steps; z: two unsigned bytes.
    for (const auto &[word, out] : {std::pair{wx, t.x}, std::pair{wy, t.y}}) {
      const auto base = static_cast<std::int32_t>(word & 0x3FFFu);
      out[0] = base << kCoordinateShift;
      out[1] = (base + signedField(word, 14, 9)) << kCoordinateShift;
      out[2] = (base + signedField(word, 23, 9)) << kCoordinateShift;
    }
    const auto zBase = static_cast<std::int32_t>(wz & 0x3FFFu);
    t.z[0] = zBase << kCoordinateShift;
    t.z[1] = (zBase + static_cast<std::int32_t>((wz >> 16) & 0xFFu)) << kCoordinateShift;
    t.z[2] = (zBase + static_cast<std::int32_t>(wz >> 24)) << kCoordinateShift;

    const double ux = t.x[1] - t.x[0], uy = t.y[1] - t.y[0], uz = t.z[1] - t.z[0];
    const double vx = t.x[2] - t.x[0], vy = t.y[2] - t.y[0], vz = t.z[2] - t.z[0];
    const double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
    const double slope = std::atan2(std::hypot(nx, ny), std::abs(nz)) * 256.0 / (2.0 * M_PI);
    if (facts.waterBit != 0 && (wz & facts.waterBit) != 0) {
      t.kind = Kind::Water;
    } else if (i < surfaceCount && (core.mem_r8(surfaces + i) & kSurfaceMask) != kSurfaceMask) {
      t.kind = Kind::Special;
    } else if (slope >= kSteepSlope) {
      t.kind = Kind::Steep;
    }
    const auto [x0, x1] = std::minmax({t.x[0], t.x[1], t.x[2]});
    const auto [y0, y1] = std::minmax({t.y[0], t.y[1], t.y[2]});
    for (std::int32_t cx = x0 >> kCellShift; cx <= x1 >> kCellShift; ++cx) {
      for (std::int32_t cy = y0 >> kCellShift; cy <= y1 >> kCellShift; ++cy) {
        cells_[cell(cx << kCellShift, cy << kCellShift)].push_back(i);
      }
    }
  }
}

VegetationGround::Kind VegetationGround::at(std::int32_t x, std::int32_t y, std::int32_t z) const {
  const auto found = cells_.find(cell(x, y));
  if (found == cells_.end()) {
    return Kind::Bare;
  }
  const Triangle *nearest = nullptr;
  double nearestHeight = 0.0;
  double nearestGap = std::numeric_limits<double>::infinity();
  for (const std::uint32_t index : found->second) {
    const Triangle &t = triangles_[index];
    const double ax = t.x[1] - t.x[0], ay = t.y[1] - t.y[0];
    const double bx = t.x[2] - t.x[0], by = t.y[2] - t.y[0];
    const double px = x - t.x[0], py = y - t.y[0];
    const double area = ax * by - ay * bx;
    if (area == 0.0) {
      continue;
    }
    const double s = (px * by - py * bx) / area;
    const double r = (ax * py - ay * px) / area;
    if (s < 0.0 || r < 0.0 || s + r > 1.0) {
      continue;
    }
    const double height = t.z[0] + s * (t.z[1] - t.z[0]) + r * (t.z[2] - t.z[0]);
    const double gap = std::abs(height - z);
    if (gap < nearestGap) {
      nearestGap = gap;
      nearestHeight = height;
      nearest = &t;
    }
  }
  if (nearest == nullptr) {
    return Kind::Bare;
  }
  if (nearest->kind == Kind::Ground && deathPlaneKnown_ && nearestHeight <= deathPlane_) {
    return Kind::Water;
  }
  return nearest->kind;
}

} // namespace spyro::guest_terrain
