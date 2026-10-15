// Spyro 1's sky 0x8004EBA8, drop shadow 0x80059A48, glows 0x800580F4 / 0x800584C4 and particles
// 0x800573C8 through the shipping overrides: each state producer composed between two fields.
#include "core.h"
#include "game.h"
#include "guest_call.h"
#include "record_wide_plan.h"
#include "spyro1/record_scene.h"
#include "spyro1/render/actor_facts.h"
#include "spyro1/render/drop_shadow_producer.h"
#include "spyro1/render/glow_producers.h"
#include "spyro1/render/particle_producer.h"
#include "spyro1/render/sky_producer.h"
#include "testutil.h"

#include <lucent/log.h>

#include <array>
#include <cstdint>
#include <exception>

namespace {

namespace actors = spyro1::actors;
namespace glows = spyro1::glows;
namespace particles = spyro1::particles;
namespace shadow = spyro1::shadow;
namespace sky = spyro1::sky;
namespace terrain = spyro::guest_terrain;
using spyro::test::composeBetween;
using spyro::test::DrawnFrame;
using spyro::test::primitives;
using spyro1::test::kFieldCamera;
using spyro1::test::movedCamera;

constexpr std::uint32_t kSector = 0x801C5000u;
constexpr std::uint32_t kSectorTable = 0x801C5100u;
constexpr std::uint32_t kShadowModel = 0x801C4000u;
constexpr std::uint32_t kShadowAnimation = 0x801C4100u;
// Below 2 MiB: a frame word holds its base halved.
constexpr std::uint32_t kShadowFrame = 0x001C4200u;
constexpr std::uint32_t kEmitterRim = 0x801C3000u;
constexpr std::uint32_t kEmitterPosition = 0x801C3100u;
constexpr std::uint32_t kParticleList = 0x801C2000u;
constexpr std::uint32_t kParticleFrames = 0x801C2100u;
constexpr std::uint32_t kParticleGroup = 3;

// Which of the two pools a field draws into, and how far its effect has moved.
struct FieldStep {
  std::uint32_t pool = 0;
  std::uint32_t step = 0;
};

// One field per check: `from` at rest, `midway` with the camera and the effect halfway, `to` at
// the end.
template <typename Draw>
spyro::test::ComposeCheck acrossThreeFields(spyro1::test::Field &field, Draw draw) {
  const DrawnFrame from = draw(kFieldCamera, FieldStep{.pool = 0, .step = 0});
  const DrawnFrame midway = draw(movedCamera(0x20), FieldStep{.pool = 1, .step = 1});
  const DrawnFrame to = draw(movedCamera(0x40), FieldStep{.pool = 0, .step = 2});
  return composeBetween(field.producers(), from, midway, to);
}

void checkComposed(const spyro::test::ComposeCheck &check) {
  CHECK(check.drawn);
  CHECK(check.asDrawn);
  CHECK(check.between);
  CHECK(check.midwayMoved);
}

class EffectScene {
public:
  Core &core() {
    return field_.core();
  }

  spyro1::test::Field &field() {
    return field_;
  }

  // One cyclorama sector of three vertices 0x1000 ahead and one gouraud triangle over them.
  void plantSky() {
    Core &guest = core();
    guest.mem_w32(sky::kCyclorama, 1);
    guest.mem_w32(sky::kCyclorama + 4, kSectorTable);
    guest.mem_w32(kSectorTable, kSector);
    guest.mem_w32(kSector + 0x00, 0x1000);
    guest.mem_w32(kSector + 0x04, 0x8001u); // radius -0x7FFF: always in front
    guest.mem_w32(kSector + 0x08, 0x0400u | (0x0200u << 16));
    guest.mem_w32(kSector + 0x0C, 3u | (0x1000u << 16));
    const std::uint32_t vertices = kSector + 0x18;
    guest.mem_w32(vertices + 0x0, 0);
    guest.mem_w32(vertices + 0x4, 0x7FFu << 10);
    guest.mem_w32(vertices + 0x8, 0x3FFu);
    const std::uint32_t colours = vertices + 0xC;
    guest.mem_w32(kSector + 0x10, (0xCu << 14) | 1u);
    for (std::uint32_t i = 0; i < 3; ++i) {
      guest.mem_w32(colours + 4 * i, 0x30204060u + i * 0x00102030u);
    }
    const std::uint32_t corners = (0u << 22) | (1u << 12) | (2u << 2);
    guest.mem_w32(colours + 0xC, corners);
    guest.mem_w32(colours + 0x10, corners);
  }

  DrawnFrame drawSky(const terrain::CameraState &camera, std::uint32_t pool) {
    field_.beginField(camera, pool);
    psx::cpu::callGuestNow(core(),
                           "test",
                           sky::SkyProducer::kEntry,
                           0xFFFFFFFFu,
                           spyro1::terrain::kSharedFacts.classify.viewRotation,
                           spyro1::terrain::kRenderGlobals.cameraRotation);
    return field_.endField();
  }

  // Spyro's model at animation 0 frame 0, whose eight shadow radii are 0x40, standing level.
  void plantShadow() {
    Core &guest = core();
    for (std::uint32_t at = 0; at < actors::spyro_record::kBytes; at += 4) {
      guest.mem_w32(actors::kSpyroRecord + at, 0);
    }
    guest.mem_w32(actors::kSpyroModel, kShadowModel);
    guest.mem_w32(kShadowModel + 0x38, kShadowAnimation);
    guest.mem_w32(kShadowAnimation + 0x24, kShadowFrame >> 1);
    guest.mem_w32(0x80000000u | kShadowFrame, 0x40404040u);
    guest.mem_w32(0x80000000u | (kShadowFrame + 4), 0x40404040u);
    for (std::uint32_t at = 0; at < spyro1::shadow_block::kBytes; at += 4) {
      guest.mem_w32(spyro1::shadow_block::kBlock + at, 0);
    }
  }

  // The shadow 0x1000 ahead and `step` * 0x40 across, Spyro turned `step` * 8.
  DrawnFrame drawShadow(const terrain::CameraState &camera, FieldStep at) {
    Core &guest = core();
    guest.mem_w32(spyro1::shadow_block::kAnchor, camera.position[0] + 0x1000u);
    guest.mem_w32(spyro1::shadow_block::kAnchor + 4, camera.position[1] + at.step * 0x40u);
    guest.mem_w32(spyro1::shadow_block::kAnchor + 8, camera.position[2]);
    guest.mem_w8(actors::kSpyroRecord + actors::spyro_record::kAngles + 2,
                 static_cast<std::uint8_t>(at.step * 8));
    field_.beginField(camera, at.pool);
    psx::cpu::callGuestNow(guest, "test", shadow::DropShadowProducer::kEntry);
    return field_.endField();
  }

  // Emitter 0 a fan of four triangles of size 0x40; every other emitter empty.
  void plantEmitter() {
    Core &guest = core();
    for (std::uint32_t at = 0; at < glows::kEmitterCount * glows::kEmitterBytes; at += 4) {
      guest.mem_w32(glows::kEmitters + at, 0);
    }
    const std::array<std::array<std::int32_t, 2>, 5> rim = {
        {{0x1000, 0}, {0, 0x1000}, {-0x1000, 0}, {0, -0x1000}, {0x1000, 0}}};
    for (std::uint32_t i = 0; i < rim.size(); ++i) {
      guest.mem_w32(kEmitterRim + 8 * i, static_cast<std::uint32_t>(rim[i][0]));
      guest.mem_w32(kEmitterRim + 8 * i + 4, static_cast<std::uint32_t>(rim[i][1]));
    }
    guest.mem_w32(glows::kEmitters + glows::emitter::kCount,
                  static_cast<std::uint32_t>(rim.size()));
    guest.mem_w32(glows::kEmitters + glows::emitter::kRim, kEmitterRim);
    guest.mem_w32(glows::kEmitters + glows::emitter::kPosition, kEmitterPosition);
    guest.mem_w32(glows::kEmitters + glows::emitter::kColour, 0x00406080u);
    guest.mem_w32(glows::kEmitters + glows::emitter::kSize, 0x40);
  }

  DrawnFrame drawEmitter(const terrain::CameraState &camera, FieldStep at) {
    Core &guest = core();
    guest.mem_w32(kEmitterPosition, camera.position[0] + 0x1000u);
    guest.mem_w32(kEmitterPosition + 4, camera.position[1] + at.step * 0x40u);
    guest.mem_w32(kEmitterPosition + 8, camera.position[2]);
    field_.beginField(camera, at.pool);
    psx::cpu::callGuestNow(guest, "test", glows::GlowFanProducer::kEntry);
    return field_.endField();
  }

  // Sparkle 0 alive and unfading at size 0x10; every other sparkle spent.
  void plantSparkle() {
    Core &guest = core();
    for (std::uint32_t at = 0; at < glows::kSparkleCount * glows::kSparkleBytes; at += 4) {
      guest.mem_w32(glows::kSparkles + at, 0);
    }
    guest.mem_w32(glows::kSparkles + glows::sparkle::kCommand, 0x00C0C0C0u);
    guest.mem_w32(glows::kSparkles + glows::sparkle::kSizeDepth, 0xFF10u);
  }

  // The sparkle 0x1000 ahead and `step` * 0x40 across, turned `step` * 8; no time passes.
  DrawnFrame drawSparkle(const terrain::CameraState &camera, FieldStep at) {
    Core &guest = core();
    guest.mem_w32(glows::kSparkles, camera.position[0] + 0x1000u);
    guest.mem_w32(glows::kSparkles + 4, camera.position[1] + at.step * 0x40u);
    guest.mem_w32(glows::kSparkles + 8, camera.position[2]);
    guest.mem_w32(glows::kSparkles + glows::sparkle::kFade, 0x4040u | (at.step * 8u << 16));
    field_.beginField(camera, at.pool);
    psx::cpu::callGuestNow(guest, "test", glows::SparkleProducer::kEntry, 0u);
    return field_.endField();
  }

  // One textured quad particle and the end record; its texture group's frame 0.
  void plantParticle() {
    Core &guest = core();
    guest.mem_w32(particles::kListBase, kParticleList);
    guest.mem_w32(particles::kTextureGroups + 4 * kParticleGroup, kParticleFrames);
    guest.mem_w32(kParticleFrames + 4, 0x7A001020u);
    guest.mem_w32(kParticleFrames + 8, 0x00093020u);
    for (std::uint32_t at = 0; at < 2 * particles::kRecordBytes; at += 4) {
      guest.mem_w32(kParticleList + at, 0);
    }
    guest.mem_w16(kParticleList + particles::kRecordBytes, 0xFF00u);
  }

  // The quad 0x100 / 4 ahead and `step` * 0x10 / 4 across, turned `step` * 8.
  DrawnFrame drawParticle(const terrain::CameraState &camera, FieldStep at) {
    Core &guest = core();
    const auto quarter = [](std::uint32_t word) {
      return static_cast<std::uint16_t>(word >> 2);
    };
    guest.mem_w16(kParticleList + particles::record::kKind, (6u << 8) | kParticleGroup);
    guest.mem_w16(kParticleList + 4,
                  static_cast<std::uint16_t>(quarter(camera.position[0]) + 0x100u));
    guest.mem_w16(kParticleList + 6,
                  static_cast<std::uint16_t>(quarter(camera.position[1]) + at.step * 0x10u));
    guest.mem_w16(kParticleList + 8, quarter(camera.position[2]));
    guest.mem_w8(kParticleList + 0xA, 0x20);
    guest.mem_w8(kParticleList + particles::record::kAngle, static_cast<std::uint8_t>(at.step * 8));
    guest.mem_w32(kParticleList + 0xC, 0x2C808080u);
    field_.beginField(camera, at.pool);
    psx::cpu::callGuestNow(guest, "test", particles::ParticleProducer::kEntry);
    return field_.endField();
  }

private:
  spyro1::test::Field field_;
};

// Points move as halfwords the short way, a line's second point too; the angle only for a turned
// kind; a new kind is `to`.
void test_a_particle_moves_between_fields() {
  particles::RecordBytes from{};
  particles::RecordBytes to{};
  from[4] = 0xF0;
  from[5] = 0xFF;
  to[4] = 0x10;
  from[0xE] = 0x20;
  from[particles::record::kAngle] = 0xF0;
  to[particles::record::kAngle] = 0x10;
  const auto between = [&](std::uint8_t kind) {
    from[1] = kind;
    to[1] = kind;
    return particles::particleBetween(from, to, 0.5f);
  };
  const particles::RecordBytes line = between(1);
  CHECK_EQ(line[4], 0);
  CHECK_EQ(line[5], 0);
  CHECK_EQ(line[0xE], 0x10);
  const particles::RecordBytes dot = between(0);
  CHECK_EQ(dot[0xE], 0);
  CHECK_EQ(dot[particles::record::kAngle], 0x10);
  CHECK_EQ(between(6)[particles::record::kAngle], 0);
  to[1] = 0;
  CHECK_EQ(particles::particleBetween(from, to, 0.5f)[4], 0x10);
}

void test_the_composer_draws_the_sky_between_two_fields() {
  EffectScene scene;
  CHECK(scene.core().game->runtime.stateProducer(scene.core(), sky::SkyProducer::kEntry) !=
        nullptr);
  scene.plantSky();
  checkComposed(
      acrossThreeFields(scene.field(), [&](const terrain::CameraState &camera, FieldStep at) {
        return scene.drawSky(camera, at.pool);
      }));
}

void test_the_composer_draws_the_drop_shadow_between_two_fields() {
  EffectScene scene;
  scene.plantShadow();
  checkComposed(
      acrossThreeFields(scene.field(), [&](const terrain::CameraState &camera, FieldStep at) {
        return scene.drawShadow(camera, at);
      }));
}

void test_the_composer_draws_a_glow_between_two_fields() {
  EffectScene scene;
  scene.plantEmitter();
  checkComposed(
      acrossThreeFields(scene.field(), [&](const terrain::CameraState &camera, FieldStep at) {
        return scene.drawEmitter(camera, at);
      }));
}

void test_the_composer_draws_a_sparkle_between_two_fields() {
  EffectScene scene;
  scene.plantSparkle();
  checkComposed(
      acrossThreeFields(scene.field(), [&](const terrain::CameraState &camera, FieldStep at) {
        return scene.drawSparkle(camera, at);
      }));
}

void test_the_composer_draws_a_particle_between_two_fields() {
  EffectScene scene;
  scene.plantParticle();
  checkComposed(
      acrossThreeFields(scene.field(), [&](const terrain::CameraState &camera, FieldStep at) {
        return scene.drawParticle(camera, at);
      }));
}

// At 16:9 a particle or sparkle drawn in a margin leaves its seen or fade byte as retail's 4:3
// window wrote it.
void test_the_margins_leave_guest_state_as_retail_wrote_it() {
  EffectScene narrow;
  EffectScene wide;
  wide.core().game->guestDisplay.latch(spyro::test::recordWidePlan());
  narrow.plantParticle();
  wide.plantParticle();
  narrow.plantSparkle();
  wide.plantSparkle();
  std::uint32_t marginParticles = 0;
  std::uint32_t marginSparkles = 0;
  for (std::uint32_t step = 0; step < 0x80; ++step) {
    const FieldStep at{.pool = 0, .step = step};
    const bool particleNarrow = !primitives(narrow.drawParticle(kFieldCamera, at).shown).empty();
    const bool particleWide = !primitives(wide.drawParticle(kFieldCamera, at).shown).empty();
    CHECK_EQ(wide.core().mem_r8(kParticleList + particles::record::kSeen),
             narrow.core().mem_r8(kParticleList + particles::record::kSeen));
    marginParticles += particleWide && !particleNarrow ? 1u : 0u;
    const bool sparkleNarrow = !primitives(narrow.drawSparkle(kFieldCamera, at).shown).empty();
    const bool sparkleWide = !primitives(wide.drawSparkle(kFieldCamera, at).shown).empty();
    CHECK_EQ(wide.core().mem_r8(glows::kSparkles + glows::sparkle::kFade),
             narrow.core().mem_r8(glows::kSparkles + glows::sparkle::kFade));
    marginSparkles += sparkleWide && !sparkleNarrow ? 1u : 0u;
  }
  CHECK(marginParticles > 0);
  CHECK(marginSparkles > 0);
}

} // namespace

int main() {
  try {
    RUN(a_particle_moves_between_fields);
    RUN(the_composer_draws_the_sky_between_two_fields);
    RUN(the_composer_draws_the_drop_shadow_between_two_fields);
    RUN(the_composer_draws_a_glow_between_two_fields);
    RUN(the_composer_draws_a_sparkle_between_two_fields);
    RUN(the_composer_draws_a_particle_between_two_fields);
    RUN(the_margins_leave_guest_state_as_retail_wrote_it);
    return psx::test::summary();
  } catch (const std::exception &error) {
    lucent::error("test_spyro1_effects", "FAIL: {}", error.what());
    return 1;
  }
}
