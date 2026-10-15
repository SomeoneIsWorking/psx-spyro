// Spyro 1's moby emitters 0x8001F798 and 0x80020F34, sprite queue 0x80022A2C and paired actor
// 0x80023AC4 through the shipping overrides: their state producers composed between two fields,
// the emitters' margin at 16:9, and the HUD records and EmitTexturedQuad's rows anchored at 16:9.
#include "core.h"
#include "core/guest_globals.h"
#include "frame_composer.h"
#include "game.h"
#include "guest_call.h"
#include "native_dispatch.h"
#include "record_wide_plan.h"
#include "spyro1/core/frame/ordering_table.h"
#include "spyro1/core/hud/hud_anchor.h"
#include "spyro1/record_scene.h"
#include "spyro1/render/actor_between.h"
#include "spyro1/render/actor_emitter_layout.h"
#include "spyro1/render/actor_emitter_producer.h"
#include "spyro1/render/actor_facts.h"
#include "spyro1/render/paired_actor_producer.h"
#include "spyro1/render/sprite_queue_producer.h"
#include "spyro1/render/terrain_facts.h"
#include "testutil.h"

#include <lucent/log.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <vector>

namespace {

namespace actors = spyro1::actors;
namespace hud = spyro1::hud_anchor;
namespace terrain = spyro::guest_terrain;
using spyro::test::composeBetween;
using spyro::test::DrawnFrame;
using spyro::test::primitives;
using spyro1::test::kFieldCamera;
using spyro1::test::movedCamera;

constexpr std::uint32_t kMeshIndex = 5;
constexpr std::uint32_t kQueuedMesh = 0x801C0100u;
constexpr std::uint32_t kWorldRecord = 0x801C0000u;
constexpr std::uint32_t kOverlayPair = 0x80130800u;
constexpr std::uint32_t kSpyroModel = 0x801C1000u;
constexpr std::uint32_t kAnimation = 0x801C1100u;
// Below 2 MiB: a frame word holds its base halved.
constexpr std::uint32_t kFrameBase = 0x001C1200u;
constexpr std::uint32_t kAnimationWords = 0x801C1300u;
constexpr std::uint32_t kPolygonList = 0x801C1400u;
constexpr std::uint32_t kColourList = 0x801C1500u;
constexpr std::uint32_t kMobyMeshIndex = 6;
constexpr std::uint32_t kMobyRecord = 0x801C0200u;
constexpr std::uint32_t kMobyMesh = 0x801C2000u;
constexpr std::uint32_t kMobyFrame = 0x801C2100u;
constexpr std::uint32_t kMobyStream = 0x801C2200u;
constexpr std::uint32_t kMobyPolygons = 0x801C2300u;
constexpr std::uint32_t kMobyColours = 0x801C2400u;
// The secondary word's light byte 1: the strip reflection.
constexpr std::uint32_t kStripReflection = 0x01000000u;
constexpr std::uint16_t kQuadX = 0x100;
// A return address that is neither HUD row's.
constexpr std::uint32_t kOtherQuadReturn = 0x80019000u;

// A moby vertex word at forward 0: across in bits 11..21, up halved in bits 0..11, both under
// their field's sign bit.
std::uint32_t mobyVertex(std::uint32_t across, std::uint32_t up) {
  return (across << 11) | (up >> 1);
}

// A queued mesh's packed vertex: the 9-bit fields from bit 7 (screen x) and bit 15 (screen y).
std::uint32_t queuedVertex(std::int32_t across, std::int32_t down) {
  return ((static_cast<std::uint32_t>(across) & 0x1FFu) << 7) |
         ((static_cast<std::uint32_t>(down) & 0x1FFu) << 15);
}

// A paired actor delta word moving the vertex by (across, down) on screen.
std::uint32_t deltaWord(std::int32_t across, std::int32_t down) {
  return ((static_cast<std::uint32_t>(-across) & 0x7FFu) << 10) |
         (static_cast<std::uint32_t>(-down / 2) & 0x3FFu);
}

class ActorScene {
public:
  ActorScene() {
    Core &guest = core();
    guest.mem_w32(actors::kShadowListEnd, actors::kShadowList);
    guest.mem_w32(spyro1::OrderingTable::kOverlayPairPointer, kOverlayPair);
    plantQueuedMesh();
    plantSpyroModel();
  }

  Core &core() {
    return field_.core();
  }

  [[nodiscard]] const psx::present::StateProducers &producers() {
    return field_.producers();
  }

  // A world record 0x4000 in front of the camera and `across` to its side.
  void placeRecord(std::uint32_t record, const terrain::CameraState &camera, std::int32_t across) {
    Core &guest = core();
    for (std::uint32_t at = 0; at < hud::kRecordBytes; at += 4) {
      guest.mem_w32(record + at, 0);
    }
    guest.mem_w32(record + actors::moby::kX, camera.position[0] + 0x4000u);
    guest.mem_w32(record + actors::moby::kY,
                  camera.position[1] + static_cast<std::uint32_t>(across));
    guest.mem_w32(record + actors::moby::kZ, camera.position[2]);
    guest.mem_w16(record + actors::moby::kMesh, kMeshIndex);
    guest.mem_w16(record + actors::moby::kCullDistance, 0x20); // reach 0x2000
  }

  // A HUD record at screen (x, 0x60), depth 0x400.
  void placeHudRecord(std::uint32_t record, std::uint32_t x) {
    Core &guest = core();
    for (std::uint32_t at = 0; at < hud::kRecordBytes; at += 4) {
      guest.mem_w32(record + at, 0);
    }
    guest.mem_w32(record + actors::moby::kX, x);
    guest.mem_w32(record + actors::moby::kY, 0x60);
    guest.mem_w32(record + actors::moby::kZ, 0x400);
    guest.mem_w16(record + actors::moby::kMesh, kMeshIndex);
    guest.mem_w16(record + actors::moby::kCullDistance, actors::moby::kScreenSpace);
  }

  // GamestateDraw's queue pass over `records`.
  DrawnFrame drawQueue(const terrain::CameraState &camera,
                       std::uint32_t field,
                       const std::vector<std::uint32_t> &records) {
    field_.beginField(camera, field);
    for (std::uint32_t i = 0; i < records.size(); ++i) {
      core().mem_w32(actors::kSpriteQueue + 4 * i, records[i]);
    }
    core().mem_w32(actors::kSpriteQueue + 4 * static_cast<std::uint32_t>(records.size()), 0);
    psx::cpu::callGuestNow(core(), "test", actors::SpriteQueueProducer::kEntry);
    return field_.endField();
  }

  // One frame of three vertices, one two-sided textured triangle over them, lit and the moby's
  // secondary word the strip reflection when `lit`; `radius` the frame's bounding radius.
  void plantMoby(std::uint32_t radius, bool lit) {
    secondaryWord_ = lit ? kStripReflection : 0u;
    namespace frame = actors::emitter::frame;
    Core &guest = core();
    guest.mem_w32(spyro::guest::kModelSoundTables + 4 * kMobyMeshIndex, kMobyMesh);
    guest.mem_w32(kMobyMesh + actors::mesh::kFrames, kMobyFrame);
    for (std::uint32_t at = 0; at < 0x24; at += 4) {
      guest.mem_w32(kMobyFrame + at, 0);
    }
    guest.mem_w16(kMobyFrame + frame::kColourCount, 3);
    guest.mem_w8(kMobyFrame + frame::kPositionShift, 2);
    guest.mem_w8(kMobyFrame + actors::mesh::kRadius, static_cast<std::uint8_t>(radius >> 4));
    guest.mem_w8(kMobyFrame + frame::kVertexCounts, 3);
    guest.mem_w8(kMobyFrame + frame::kVertexCounts + 1, 3);
    for (const std::uint32_t polygons : {frame::kNearPolygons, frame::kFarPolygons}) {
      guest.mem_w32(kMobyFrame + polygons, kMobyPolygons);
      guest.mem_w32(kMobyFrame + polygons + 4, kMobyColours);
    }
    guest.mem_w32(kMobyFrame + actors::mesh::kVertexSteps, kMobyStream & 0x1FFFFFu);
    guest.mem_w32(kMobyFrame + actors::mesh::kVertexSteps + 4, 0);
    const std::array<std::uint32_t, 4> stream = {
        mobyVertex(0, 0), mobyVertex(0x40, 0), mobyVertex(0, 0x80), 0};
    for (std::uint32_t i = 0; i < stream.size(); ++i) {
      guest.mem_w32(kMobyStream + 4 * i, stream[i]);
    }
    constexpr std::uint32_t kTwoSided = 0x1u;
    constexpr std::uint32_t kTextured = 0x2u;
    constexpr std::uint32_t kLit = 0x4u;
    guest.mem_w32(kMobyPolygons, 0x14);
    guest.mem_w32(kMobyPolygons + 4,
                  (0u << 22) | (1u << 13) | (2u << 4) | kTwoSided | kTextured | (lit ? kLit : 0u));
    guest.mem_w32(kMobyPolygons + 8, (0u << 19) | (1u << 10) | (2u << 1));
    guest.mem_w32(kMobyPolygons + 0x0C, 0x7A200810u);
    guest.mem_w32(kMobyPolygons + 0x10, 0x00092010u);
    guest.mem_w32(kMobyPolygons + 0x14, 0x1010C0C0u);
    for (std::uint32_t i = 0; i < 3; ++i) {
      guest.mem_w32(kMobyColours + 4 * i, 0x00305070u + i * 0x00102030u);
    }
  }

  // A moby 0x4000 in front of the camera and `across` to its side.
  void placeMoby(const terrain::CameraState &camera, std::int32_t across) {
    Core &guest = core();
    for (std::uint32_t at = 0; at < 0x58; at += 4) {
      guest.mem_w32(kMobyRecord + at, 0);
    }
    guest.mem_w32(kMobyRecord + actors::moby::kX, camera.position[0] + 0x4000u);
    guest.mem_w32(kMobyRecord + actors::moby::kY,
                  camera.position[1] + static_cast<std::uint32_t>(across));
    guest.mem_w32(kMobyRecord + actors::moby::kZ, camera.position[2]);
    guest.mem_w16(kMobyRecord + actors::moby::kMesh, kMobyMeshIndex);
    guest.mem_w32(kMobyRecord + actors::moby::kSecondaryWord, secondaryWord_);
    guest.mem_w16(kMobyRecord + actors::moby::kCullDistance, 0x40); // reach 0x4000
  }

  // GamestateDraw's list build, bin clear and emitter for the moby, through `pass`.
  DrawnFrame drawMoby(const terrain::CameraState &camera,
                      std::uint32_t field,
                      const actors::EmitterPass &pass) {
    field_.beginField(camera, field);
    Core &guest = core();
    guest.mem_w32(actors::kShadowListEnd, actors::kShadowList);
    guest.mem_w32(pass.list->input, kMobyRecord);
    guest.mem_w32(pass.list->input + 4, 0);
    psx::cpu::callGuestNow(guest, "test", pass.list->entry);
    for (std::uint32_t at = 0; at < actors::kLocalBinsBytes; at += 4) {
      guest.mem_w32(actors::kLocalBins + at, 0);
    }
    psx::cpu::callGuestNow(guest, "test", pass.entry);
    return field_.endField();
  }

  // Spyro 0x1000 in front of the camera, `across` to its side, part 0 turned across / 16 about z.
  void placeSpyro(const terrain::CameraState &camera, std::int32_t across) {
    const auto roll = static_cast<std::uint8_t>(across / 16);
    Core &guest = core();
    for (std::uint32_t at = 0; at < actors::spyro_record::kBytes; at += 4) {
      guest.mem_w32(actors::kSpyroRecord + at, 0);
    }
    guest.mem_w32(actors::kSpyroRecord + actors::spyro_record::kPosition,
                  camera.position[0] + 0x1000u);
    guest.mem_w32(actors::kSpyroRecord + actors::spyro_record::kPosition + 4,
                  camera.position[1] + static_cast<std::uint32_t>(across));
    guest.mem_w32(actors::kSpyroRecord + actors::spyro_record::kPosition + 8, camera.position[2]);
    guest.mem_w8(actors::kSpyroRecord + actors::spyro_record::kAngles, roll);
  }

  DrawnFrame drawSpyro(const terrain::CameraState &camera, std::uint32_t field) {
    field_.beginField(camera, field);
    psx::cpu::callGuestNow(core(), "test", actors::PairedActorProducer::kEntry);
    return field_.endField();
  }

  // EmitTexturedQuad's override over a 0x10-square rect at (kQuadX, 0x50), entered as the `jal`
  // returning to `returnAddress` enters it. The quad's packet, once the override has returned.
  std::optional<std::uint32_t> emitQuad(std::uint32_t returnAddress) {
    field_.beginField(kFieldCamera, 0);
    Core &guest = core();
    const std::uint32_t packet = guest.mem_r32(spyro1::terrain::kRenderGlobals.primitiveCursor);
    guest.mem_w16(kQuadRect, kQuadX);
    guest.mem_w16(kQuadRect + 2, 0x50);
    guest.mem_w16(kQuadRect + 4, 0x10);
    guest.mem_w16(kQuadRect + 6, 0x10);
    guest.mem_w32(kQuadUv, 0x7A200810u);
    guest.mem_w32(kQuadUv + 4, 0x00092010u);
    plantQuadBody();
    guest.r[4] = kQuadRect;
    guest.r[5] = kQuadUv;
    guest.r[6] = 0;
    guest.r[29] = kQuadStack;
    guest.r[31] = returnAddress;
    if (!psx::cpu::dispatchGuestHostService(guest, hud::kTexturedQuad).returned()) {
      return std::nullopt;
    }
    return packet;
  }

private:
  // The synthetic image has no text, so EmitTexturedQuad's original is planted: the rect's x and
  // x + w into the four x halfwords of the packet at the cursor, then the cursor past it.
  void plantQuadBody() {
    const std::uint32_t cursor = spyro1::terrain::kRenderGlobals.primitiveCursor;
    const std::uint32_t high = (cursor + 0x8000u) >> 16;
    const std::uint32_t low = cursor & 0xFFFFu;
    const std::array<std::uint32_t, 13> body = {
        0x3C080000u | high, // lui t0, high
        0x8D080000u | low,  // lw t0, low(t0)
        0x94820000u,        // lhu v0, 0(a0)
        0x94830004u,        // lhu v1, 4(a0)
        0xA5020008u,        // sh v0, 8(t0)
        0xA5020018u,        // sh v0, 0x18(t0)
        0x00431021u,        // addu v0, v0, v1
        0xA5020010u,        // sh v0, 0x10(t0)
        0xA5020020u,        // sh v0, 0x20(t0)
        0x25080028u,        // addiu t0, t0, 0x28
        0x3C010000u | high, // lui at, high
        0x03E00008u,        // jr ra
        0xAC280000u | low,  // sw t0, low(at)
    };
    for (std::uint32_t i = 0; i < body.size(); ++i) {
      core().mem_w32(hud::kTexturedQuad + 4 * i, body[i]);
    }
  }

  static constexpr std::uint32_t kQuadRect = 0x801C3000u;
  static constexpr std::uint32_t kQuadUv = 0x801C3010u;
  static constexpr std::uint32_t kQuadStack = 0x801C3800u;

  // Four unlit vertices in a square, one gouraud quad over them.
  void plantQueuedMesh() {
    Core &guest = core();
    guest.mem_w32(spyro::guest::kModelSoundTables + 4 * kMeshIndex, kQueuedMesh);
    guest.mem_w8(kQueuedMesh + actors::queued_mesh::kVertexCount, 4);
    guest.mem_w8(kQueuedMesh + actors::queued_mesh::kPolygonCount, 1);
    guest.mem_w32(kQueuedMesh + actors::queued_mesh::kVertices,
                  kQueuedMesh + actors::queued_mesh::kNormals);
    guest.mem_w32(kQueuedMesh + actors::queued_mesh::kColours, kQueuedMesh + 0x40);
    guest.mem_w32(kQueuedMesh + actors::queued_mesh::kPolygons, kQueuedMesh + 0x60);
    const std::array<std::uint32_t, 4> corners = {queuedVertex(-0x80, -0x80),
                                                  queuedVertex(0x80, -0x80),
                                                  queuedVertex(-0x80, 0x80),
                                                  queuedVertex(0x80, 0x80)};
    for (std::uint32_t i = 0; i < corners.size(); ++i) {
      for (std::uint32_t b = 0; b < 3; ++b) {
        guest.mem_w8(kQueuedMesh + actors::queued_mesh::kNormals + 3 * i + b,
                     static_cast<std::uint8_t>(corners[i] >> (8 * b)));
      }
      guest.mem_w32(kQueuedMesh + 0x40 + 4 * i, 0x00406080u + i * 0x00101010u);
    }
    const std::uint32_t indices = (0u << 23) | (1u << 16) | (2u << 9) | (3u << 2);
    guest.mem_w32(kQueuedMesh + 0x60, indices);
    guest.mem_w32(kQueuedMesh + 0x64, indices);
  }

  // Animation 0 of three parts, four vertices each in a square; one one-sided textured quad over
  // part 0's.
  void plantSpyroModel() {
    Core &guest = core();
    guest.mem_w32(actors::kSpyroModel, kSpyroModel);
    guest.mem_w32(kSpyroModel + 0x38, kAnimation);
    guest.mem_w16(kAnimation + 0x02, 4);
    guest.mem_w8(kAnimation + 0x08, 4);
    guest.mem_w8(kAnimation + 0x09, 8);
    guest.mem_w8(kAnimation + 0x0A, 12);
    guest.mem_w32(kAnimation + 0x10, kAnimationWords);
    guest.mem_w32(kAnimation + 0x14, kPolygonList);
    guest.mem_w32(kAnimation + 0x18, kColourList);
    guest.mem_w32(kAnimation + 0x24, kFrameBase >> 1);
    // Each part's four byte codes start 4 bytes after the last's; every code is zero.
    guest.mem_w32(0x80000000u | (kFrameBase + 0x08), (4u << 10) | 8u);
    const std::array<std::uint32_t, 4> steps = {deltaWord(-0x100, -0x100),
                                                deltaWord(0x200, 0),
                                                deltaWord(-0x200, 0x200),
                                                deltaWord(0x200, 0)};
    for (std::uint32_t i = 0; i < 12; ++i) {
      guest.mem_w32(kAnimationWords + 4 * i, steps[i % 4]);
    }
    // Corners 0 (top left), 2 (bottom left), 1 (top right), then 3: retail's facing quad order.
    guest.mem_w32(kPolygonList, 0x18);
    guest.mem_w32(kPolygonList + 0x04, 0x80000000u | (0u << 22) | (2u << 13) | (1u << 4));
    guest.mem_w32(kPolygonList + 0x08, (0u << 19) | (1u << 10) | (2u << 1));
    guest.mem_w32(kPolygonList + 0x0C, (3u << 11) | (3u << 2));
    guest.mem_w32(kPolygonList + 0x10, 0x7A200810u);
    guest.mem_w32(kPolygonList + 0x14, 0x00092010u);
    guest.mem_w32(kPolygonList + 0x18, 0x1010C0C0u);
    for (std::uint32_t i = 0; i < 4; ++i) {
      guest.mem_w32(kColourList + 4 * i, 0x00804020u + i * 0x00081008u);
    }
  }

  spyro1::test::Field field_;
  std::uint32_t secondaryWord_ = 0;
};

std::vector<std::int32_t> columnsOf(const DrawnFrame &frame) {
  std::vector<std::int32_t> xs;
  for (const psx::present::DrawPrimitive &primitive : primitives(frame.shown)) {
    for (int i = 0; i < primitive.vertexCount; ++i) {
      xs.push_back(primitive.vertices[i].x);
    }
  }
  return xs;
}

// Records past the gem icon are left-edge text until another icon; icons take their row's class.
void test_the_queue_names_each_hud_record() {
  hud::QueueAnchors anchors;
  const auto icon = [](std::uint32_t index) {
    return hud::kIconRecords + index * hud::kRecordBytes;
  };
  CHECK(anchors.next(0x80100000u) == spyro::ui_anchor::Anchor::Centred);
  CHECK(anchors.next(icon(0)) == spyro::ui_anchor::Anchor::LeftEdge);
  CHECK(anchors.next(icon(hud::kGemIcon)) == spyro::ui_anchor::Anchor::LeftEdge);
  CHECK(anchors.next(0x80100000u) == spyro::ui_anchor::Anchor::LeftEdge);
  CHECK(anchors.next(icon(6)) == spyro::ui_anchor::Anchor::Centred);
  CHECK(anchors.next(0x80100000u) == spyro::ui_anchor::Anchor::Centred);
  CHECK(anchors.next(icon(10)) == spyro::ui_anchor::Anchor::RightEdge);
  CHECK(hud::quadAnchor(hud::kOrbQuadReturn) == spyro::ui_anchor::Anchor::RightEdge);
  CHECK(hud::quadAnchor(hud::kEggQuadReturn) == spyro::ui_anchor::Anchor::LeftEdge);
  CHECK(!hud::quadAnchor(0x80019000u));
}

// Angles move the short way round; position words move as signed values.
void test_the_records_move_between_fields() {
  std::array<std::uint8_t, actors::spyro_record::kBytes> from{};
  std::array<std::uint8_t, actors::spyro_record::kBytes> to{};
  from[actors::spyro_record::kAngles] = 0xF8;
  to[actors::spyro_record::kAngles] = 0x08;
  actors::storeWord(from, actors::spyro_record::kPosition, 0xFFFFFF00u);
  actors::storeWord(to, actors::spyro_record::kPosition, 0x100u);
  to[actors::spyro_record::kTint] = 0x55;
  const auto spyro = actors::spyroBetween(from, to, 0.5f);
  CHECK_EQ(spyro[actors::spyro_record::kAngles], 0);
  CHECK_EQ(actors::loadWord(spyro, actors::spyro_record::kPosition), 0u);
  CHECK_EQ(spyro[actors::spyro_record::kTint], 0x55);

  std::array<std::uint8_t, 0x58> record{};
  std::array<std::uint8_t, 0x58> next{};
  actors::storeWord(next, actors::moby::kX, 0x200u);
  CHECK_EQ(actors::loadWord(actors::recordBetween(record, next, 0.5f), actors::moby::kX), 0x100u);
  next[actors::moby::kMesh] = 1;
  CHECK_EQ(actors::loadWord(actors::recordBetween(record, next, 0.5f), actors::moby::kX), 0x200u);
}

// A queued record is a producer through the runtime: t = 1 is the field as drawn and t = 0.5 the
// field drawn with the record and camera halfway.
void test_the_composer_draws_a_queued_record_between_two_fields() {
  ActorScene scene;
  Core &core = scene.core();
  CHECK(core.game->runtime.stateProducer(core, actors::SpriteQueueProducer::kEntry) != nullptr);
  const terrain::CameraState half = movedCamera(0x20);
  const terrain::CameraState after = movedCamera(0x40);
  scene.placeRecord(kWorldRecord, kFieldCamera, 0);
  const DrawnFrame from = scene.drawQueue(kFieldCamera, 0, {kWorldRecord});
  scene.placeRecord(kWorldRecord, half, 0x100);
  const DrawnFrame midway = scene.drawQueue(half, 1, {kWorldRecord});
  scene.placeRecord(kWorldRecord, after, 0x200);
  const DrawnFrame to = scene.drawQueue(after, 0, {kWorldRecord});
  const spyro::test::ComposeCheck check = composeBetween(scene.producers(), from, midway, to);
  CHECK(check.drawn);
  CHECK(check.asDrawn);
  CHECK(check.between);
  CHECK(check.midwayMoved);
}

// Spyro is a producer through the runtime: t = 1 is the field as drawn and t = 0.5 the field drawn
// with him, his turn and the camera halfway.
void test_the_composer_draws_spyro_between_two_fields() {
  ActorScene scene;
  Core &core = scene.core();
  CHECK(core.game->runtime.stateProducer(core, actors::PairedActorProducer::kEntry) != nullptr);
  const terrain::CameraState half = movedCamera(0x20);
  const terrain::CameraState after = movedCamera(0x40);
  scene.placeSpyro(kFieldCamera, 0);
  const DrawnFrame from = scene.drawSpyro(kFieldCamera, 0);
  scene.placeSpyro(half, 0x40);
  const DrawnFrame midway = scene.drawSpyro(half, 1);
  scene.placeSpyro(after, 0x80);
  const DrawnFrame to = scene.drawSpyro(after, 0);
  const spyro::test::ComposeCheck check = composeBetween(scene.producers(), from, midway, to);
  CHECK(check.drawn);
  CHECK(check.asDrawn);
  CHECK(check.between);
  CHECK(check.midwayMoved);
}

// A moby is a producer through each emitter: t = 1 is the field as drawn and t = 0.5 the field
// drawn with the moby and camera halfway.
void composeMobyBetweenTwoFields(const actors::EmitterPass &pass) {
  ActorScene scene;
  Core &core = scene.core();
  CHECK(core.game->runtime.stateProducer(core, pass.entry) != nullptr);
  scene.plantMoby(0x100, pass.lit);
  const terrain::CameraState half = movedCamera(0x20);
  const terrain::CameraState after = movedCamera(0x40);
  scene.placeMoby(kFieldCamera, 0);
  const DrawnFrame from = scene.drawMoby(kFieldCamera, 0, pass);
  scene.placeMoby(half, 0x100);
  const DrawnFrame midway = scene.drawMoby(half, 1, pass);
  scene.placeMoby(after, 0x200);
  const DrawnFrame to = scene.drawMoby(after, 0, pass);
  const spyro::test::ComposeCheck check = composeBetween(scene.producers(), from, midway, to);
  CHECK(check.drawn);
  CHECK(check.asDrawn);
  CHECK(check.between);
  CHECK(check.midwayMoved);
}

void test_the_composer_draws_a_moby_between_two_fields() {
  composeMobyBetweenTwoFields(actors::kRegularEmitter);
}

void test_the_composer_draws_a_reflective_moby_between_two_fields() {
  composeMobyBetweenTwoFields(actors::kLitEmitter);
}

// A moby straddling the 16:9 edge whose triangle lies past the 4:3 edge: nothing at 4:3, the
// triangle at 16:9, so the emitter clips against the presented edge.
void test_a_moby_past_the_narrow_edge_is_drawn_wide() {
  for (const actors::EmitterPass *pass : {&actors::kRegularEmitter, &actors::kLitEmitter}) {
    ActorScene scene;
    scene.plantMoby(0x100, false);
    scene.placeMoby(kFieldCamera, 0x3A00);
    const DrawnFrame narrow = scene.drawMoby(kFieldCamera, 0, *pass);
    scene.core().game->guestDisplay.latch(spyro::test::recordWidePlan());
    const DrawnFrame wide = scene.drawMoby(kFieldCamera, 1, *pass);
    CHECK(primitives(narrow.shown).empty());
    CHECK_EQ(primitives(wide.shown).size(), 1u);
  }
}

// Level code reads +0x51, so an actor drawn only into the 16:9 margin keeps retail's clear mark;
// one inside the 4:3 view is still marked.
void test_the_margin_leaves_the_drawn_mark_as_retail() {
  ActorScene scene;
  Core &core = scene.core();
  scene.plantMoby(0x100, false);
  scene.core().game->guestDisplay.latch(spyro::test::recordWidePlan());
  scene.placeMoby(kFieldCamera, 0x3A00);
  CHECK_EQ(primitives(scene.drawMoby(kFieldCamera, 0, actors::kRegularEmitter).shown).size(), 1u);
  CHECK_EQ(core.mem_r8(kMobyRecord + actors::moby::kDrawn), 0);
  scene.placeMoby(kFieldCamera, 0);
  (void)scene.drawMoby(kFieldCamera, 1, actors::kRegularEmitter);
  CHECK_EQ(core.mem_r8(kMobyRecord + actors::moby::kDrawn), 1);
}

// The queue's sphere cull leaves +0x51 as retail's too.
void test_the_margin_leaves_the_queued_mark_as_retail() {
  ActorScene scene;
  Core &core = scene.core();
  scene.core().game->guestDisplay.latch(spyro::test::recordWidePlan());
  scene.placeRecord(kWorldRecord, kFieldCamera, 0x3A00);
  CHECK(!primitives(scene.drawQueue(kFieldCamera, 0, {kWorldRecord}).shown).empty());
  CHECK_EQ(core.mem_r8(kWorldRecord + actors::moby::kDrawn), 0);
  scene.placeRecord(kWorldRecord, kFieldCamera, 0);
  (void)scene.drawQueue(kFieldCamera, 1, {kWorldRecord});
  CHECK_EQ(core.mem_r8(kWorldRecord + actors::moby::kDrawn), 1);
}

// At 16:9 the gem icon moves left of its 4:3 column, the life icon right, a dragon digit stays.
void test_the_hud_icons_take_the_wide_edges() {
  ActorScene scene;
  const auto icon = [](std::uint32_t index) {
    return hud::kIconRecords + index * hud::kRecordBytes;
  };
  struct Case {
    std::uint32_t index;
    std::int32_t sign;
  };
  const std::array<Case, 3> cases = {Case{hud::kGemIcon, -1}, Case{6, 0}, Case{10, 1}};
  std::array<std::vector<std::int32_t>, 3> narrow;
  for (std::size_t i = 0; i < cases.size(); ++i) {
    scene.placeHudRecord(icon(cases[i].index), 0x100);
    narrow[i] = columnsOf(scene.drawQueue(kFieldCamera, 0, {icon(cases[i].index)}));
  }
  scene.core().game->guestDisplay.latch(spyro::test::recordWidePlan());
  for (std::size_t i = 0; i < cases.size(); ++i) {
    const std::vector<std::int32_t> wide =
        columnsOf(scene.drawQueue(kFieldCamera, 1, {icon(cases[i].index)}));
    CHECK(!narrow[i].empty());
    CHECK_EQ(narrow[i].size(), wide.size());
    for (std::size_t v = 0; v < narrow[i].size() && v < wide.size(); ++v) {
      const std::int32_t moved = wide[v] - narrow[i][v];
      CHECK_EQ(moved > 0 ? 1 : (moved < 0 ? -1 : 0), cases[i].sign);
    }
  }
}

// EmitTexturedQuad's override moves the life orbs right and the egg row left by their anchor's
// correction at 16:9, and leaves a quad from any other caller where it was drawn.
void test_the_hud_quads_take_the_wide_edges() {
  ActorScene scene;
  Core &core = scene.core();
  core.game->guestDisplay.latch(spyro::test::recordWidePlan());
  struct Case {
    std::uint32_t returnAddress;
    spyro::ui_anchor::Anchor anchor;
  };
  for (const Case &c : {Case{hud::kOrbQuadReturn, spyro::ui_anchor::Anchor::RightEdge},
                        Case{hud::kEggQuadReturn, spyro::ui_anchor::Anchor::LeftEdge}}) {
    const std::int32_t shift =
        hud::columns(core, {.name = "spyro1-hud-quad", .index = c.returnAddress}, c.anchor);
    CHECK(c.anchor == spyro::ui_anchor::Anchor::RightEdge ? shift > 0 : shift < 0);
    const std::optional<std::uint32_t> emitted = scene.emitQuad(c.returnAddress);
    CHECK(emitted.has_value());
    const std::uint32_t packet = *emitted;
    for (const std::uint32_t at : {0x08u, 0x18u}) {
      CHECK_EQ(static_cast<std::int16_t>(core.mem_r16(packet + at)), kQuadX + shift);
    }
    for (const std::uint32_t at : {0x10u, 0x20u}) {
      CHECK_EQ(static_cast<std::int16_t>(core.mem_r16(packet + at)), kQuadX + 0x10 + shift);
    }
  }
  const std::optional<std::uint32_t> packet = scene.emitQuad(kOtherQuadReturn);
  CHECK(packet.has_value());
  CHECK_EQ(core.mem_r16(*packet + 0x08), kQuadX);
  CHECK_EQ(core.mem_r16(*packet + 0x10), kQuadX + 0x10);
}

} // namespace

int main() {
  try {
    RUN(the_queue_names_each_hud_record);
    RUN(the_records_move_between_fields);
    RUN(the_composer_draws_a_queued_record_between_two_fields);
    RUN(the_composer_draws_spyro_between_two_fields);
    RUN(the_composer_draws_a_moby_between_two_fields);
    RUN(the_composer_draws_a_reflective_moby_between_two_fields);
    RUN(a_moby_past_the_narrow_edge_is_drawn_wide);
    RUN(the_margin_leaves_the_drawn_mark_as_retail);
    RUN(the_margin_leaves_the_queued_mark_as_retail);
    RUN(the_hud_icons_take_the_wide_edges);
    RUN(the_hud_quads_take_the_wide_edges);
    return psx::test::summary();
  } catch (const std::exception &error) {
    lucent::error("test_spyro1_actors", "FAIL: {}", error.what());
    return 1;
  }
}
