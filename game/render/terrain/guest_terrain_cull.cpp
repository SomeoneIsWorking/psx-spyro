#include "render/terrain/guest_terrain_cull.h"

#include "core.h"
#include "gte_registers.h"
#include "render/terrain/guest_terrain_frame.h"

namespace spyro::guest_terrain {
namespace {

namespace gte = psx::gte;

using spyro::guest_word::asSigned;

void loadTriangle(TerrainFrame &frame, const std::array<std::uint32_t, 3> &screens) {
  frame.core.gte().writeData(gte::kSxy0, screens[0]);
  frame.core.gte().writeData(gte::kSxy1, screens[1]);
  frame.core.gte().writeData(gte::kSxy2, screens[2]);
}

// NCLIP on SXY0..2, signed toward the camera for the polygon's winding.
std::int32_t winding(TerrainFrame &frame, Sidedness sides) {
  frame.core.gte().execute(gte::kNclip);
  std::uint32_t area = frame.core.gte().readData(gte::kMac0);
  if (sides.flipped) {
    area = 0u - area;
  }
  return asSigned(area);
}

} // namespace

bool facesCamera(TerrainFrame &frame,
                 const std::array<std::uint32_t, 4> &screens,
                 Sidedness sides,
                 SecondTriangle second) {
  loadTriangle(frame, {screens[0], screens[1], screens[2]});
  if (sides.twoSided) {
    frame.core.gte().execute(gte::kNclip);
    return true;
  }
  if (winding(frame, sides) >= 0) {
    return true;
  }
  if (second == SecondTriangle::Reversed) {
    loadTriangle(frame, {screens[3], screens[2], screens[1]});
    return winding(frame, sides) >= 0;
  }
  frame.core.gte().writeData(gte::kSxy0, screens[3]);
  return winding(frame, sides) <= 0;
}

bool triangleFacesCamera(TerrainFrame &frame,
                         const std::array<std::uint32_t, 3> &screens,
                         Sidedness sides) {
  loadTriangle(frame, screens);
  if (sides.twoSided) {
    frame.core.gte().execute(gte::kNclip);
    return true;
  }
  return winding(frame, sides) >= 0;
}

bool nearQuadFacesCamera(TerrainFrame &frame,
                         const std::array<std::uint32_t, 4> &screens,
                         Sidedness sides,
                         bool overflow) {
  loadTriangle(frame, {screens[0], screens[1], screens[2]});
  frame.core.gte().execute(gte::kNclip);
  if (overflow) {
    return true;
  }
  std::uint32_t area = frame.core.gte().readData(gte::kMac0);
  if (sides.flipped) {
    area = 0u - area;
  }
  if (asSigned(area) >= 0) {
    return true;
  }
  frame.core.gte().writeData(gte::kSxy0, screens[3]);
  frame.core.gte().execute(gte::kNclip);
  if (sides.twoSided) {
    return true;
  }
  const std::int32_t retried = asSigned(frame.core.gte().readData(gte::kMac0));
  return sides.flipped ? retried >= 0 : retried <= 0;
}

bool nearTriangleFacesCamera(TerrainFrame &frame,
                             const std::array<std::uint32_t, 3> &screens,
                             Sidedness sides,
                             bool overflow) {
  return triangleFacesCamera(frame, screens, overflow ? Sidedness{.twoSided = true} : sides);
}

} // namespace spyro::guest_terrain
