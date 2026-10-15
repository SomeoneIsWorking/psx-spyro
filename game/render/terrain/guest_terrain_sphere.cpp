#include "guest_terrain_sphere.h"

#include "gte_registers.h"
#include "guest_word.h"

namespace spyro::guest_terrain {
namespace {

namespace gte = psx::gte;

using spyro::guest_word::asSigned;

constexpr std::uint32_t asWord(std::int32_t value) {
  return static_cast<std::uint32_t>(value);
}

std::uint32_t magnitude(std::uint32_t value) {
  return asSigned(value) < 0 ? 0u - value : value;
}

} // namespace

SectorSphere testSectorSphere(Core &core,
                              const SphereWords &words,
                              const SphereCamera &camera,
                              const guest_moby_frustum::HorizontalSlope &slope) {
  const auto [centreXY, centreZ, radius] = words;
  core.gte().writeData(gte::kIr3, asWord(asSigned(centreXY >> 16) - camera.x));
  core.gte().writeData(gte::kIr1, asWord(camera.y - asSigned(centreXY & 0xFFFFu)));
  core.gte().writeData(gte::kIr2, asWord(camera.z - asSigned(centreZ >> 16)));
  core.gte().execute(gte::kMvmvaRtIr);
  const std::uint32_t r = radius;
  // r/cos and r*tan of the 4:3 half-angle horizontally, of the 17:32 slope vertically.
  const std::uint32_t horizontalLateral = (r >> 1) + (r >> 2) + (r >> 5);
  const std::uint32_t horizontalDepth = (r >> 1) + (r >> 4) + (r >> 5);
  const std::uint32_t verticalLateral = r - (r >> 3);
  const std::uint32_t verticalDepth = (r >> 1) - (r >> 4);
  const auto xWeight = static_cast<std::uint32_t>(slope.xWeight);
  const auto zWeight = static_cast<std::uint32_t>(slope.zWeight);
  SectorSphere sphere;
  // The image reads the saturated IR registers, not MAC.
  sphere.z = core.gte().readData(gte::kIr3);
  const std::uint32_t z = sphere.z;
  if (asSigned(z + r) <= 0) {
    return sphere;
  }
  const std::uint32_t x = magnitude(core.gte().readData(gte::kIr1));
  if (asSigned((x - horizontalLateral) * xWeight - (z + horizontalDepth) * zWeight) >= 0) {
    return sphere;
  }
  const std::uint32_t y = magnitude(core.gte().readData(gte::kIr2));
  if (asSigned(((y - verticalLateral) << 5) - (z + verticalDepth) * 17) >= 0) {
    return sphere;
  }
  sphere.visible = true;
  sphere.inside = asSigned(z - r) > 0 &&
                  asSigned(((x + horizontalLateral) << 2) - (z - horizontalDepth) * 3) < 0 &&
                  asSigned(((y + verticalLateral) << 5) - (z - verticalDepth) * 17) < 0;
  return sphere;
}

} // namespace spyro::guest_terrain
