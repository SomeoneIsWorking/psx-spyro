// record_scene.h — a booted Spyro 1 drawing one field through its shipping overrides:
// GamestateDraw's two packet pools and one table, the camera its builder writes, then the
// LinkOTPrimitives flatten and DrawOTag's walk into the device's record.
#pragma once

#include "booted_title.h"
#include "core.h"
#include "core/session.h"
#include "guest_call.h"
#include "spyro1/render/terrain_facts.h"
#include "spyro1/render/terrain_image.h"
#include "terrain_record_scene.h"

#include <cmath>
#include <cstdint>

namespace spyro1::test {

// The camera looking down +x at angles 0: depth is world x.
inline constexpr spyro::guest_terrain::CameraState kFieldCamera{{0x10000, 0x10000, 0x10000},
                                                                {0, 0, 0}};

// The camera `step` along z, turned half as many units.
inline spyro::guest_terrain::CameraState movedCamera(std::uint32_t step) {
  spyro::guest_terrain::CameraState camera = kFieldCamera;
  camera.angles[1] = static_cast<std::int32_t>(step / 2);
  camera.position[2] += step;
  return camera;
}

class Field {
public:
  static constexpr std::uint32_t kFlatten = 0x80016784u;
  static constexpr std::uint32_t kFlattenBins = 0x800;
  // GamestateDraw's two packet pools.
  static constexpr std::uint32_t kPools = 0x80140000u;

  Field() : booted_(spyro::Title::Spyro1) {
    core().gte().bind();
    constexpr std::uint32_t kJrRa = 0x03E00008u;
    core().mem_w32(kFlatten, kJrRa);
    core().mem_w32(kFlatten + 4, 0);
    plantSineTable();
  }

  Core &core() {
    return booted_.core();
  }

  [[nodiscard]] const psx::present::StateProducers &producers() {
    return spyro::session(core()).stateProducers;
  }

  void setCamera(const spyro::guest_terrain::CameraState &camera) {
    namespace s1 = spyro1::terrain;
    const spyro::guest_camera::Matrices matrices =
        s1::kTerrainImage.camera(core(), {camera.angles[0], camera.angles[1], camera.angles[2]});
    for (std::uint32_t i = 0; i < 5; ++i) {
      core().mem_w32(s1::kRenderGlobals.cameraRotation + 4 * i, matrices.projection[i]);
      core().mem_w32(s1::kSharedFacts.classify.viewRotation + 4 * i, matrices.view[i]);
    }
    for (std::uint32_t i = 0; i < 3; ++i) {
      core().mem_w16(s1::kRenderGlobals.cameraAngles + 2 * i,
                     static_cast<std::uint16_t>(camera.angles[i]));
      core().mem_w32(s1::kRenderGlobals.cameraPosition + 4 * i, camera.position[i]);
    }
  }

  // The field's start: the pool alternates with `field`, the table is emptied, the camera set.
  void beginField(const spyro::guest_terrain::CameraState &camera, std::uint32_t field) {
    namespace s1 = spyro1::terrain;
    const std::uint32_t cursor = kPools + (field & 1u) * s1::kPacketPoolBytes;
    core().mem_w32(s1::kRenderGlobals.primitiveCursor, cursor);
    core().mem_w32(s1::kPacketPoolEnd, cursor + s1::kPacketPoolBytes);
    core().mem_w32(s1::kRenderGlobals.orderingTable, spyro::test::kTable);
    for (std::uint32_t at = 0; at < s1::kOrderingTableBytes; at += 4) {
      core().mem_w32(spyro::test::kTable + at, 0);
    }
    core().frameStates.clear();
    setCamera(camera);
    spyro::test::setProjection(core());
  }

  // The flatten over every bin, then DrawOTag.
  spyro::test::DrawnFrame endField() {
    psx::cpu::callGuestNow(core(), "test", kFlatten, kFlattenBins);
    return spyro::test::submitField(
        core(),
        spyro::test::linkChain(core(), {.base = spyro::test::kTable, .bins = kFlattenBins}));
  }

private:
  // One sine table of 320 entries; the cosine table is the same table a quarter turn on.
  void plantSineTable() {
    constexpr std::uint32_t kSineEntries = 320;
    for (std::uint32_t i = 0; i < kSineEntries; ++i) {
      const double turn = 2.0 * 3.14159265358979323846 * static_cast<double>(i) / 256.0;
      core().mem_w16(spyro1::terrain::kRenderGlobals.cameraSineTable + 2 * i,
                     static_cast<std::uint16_t>(std::lround(std::sin(turn) * 4096.0)));
    }
  }

  spyro::test::BootedTitle booted_;
};

} // namespace spyro1::test
