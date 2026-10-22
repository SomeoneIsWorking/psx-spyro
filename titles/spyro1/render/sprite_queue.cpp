#include "spyro1/render/sprite_queue.h"

#include "core.h"
#include "gte_registers.h"
#include "lucent/log.h"
#include "render/field/guest_moby_rotation.h"
#include "render/field/guest_packet.h"
#include "render/field/guest_view_slope.h"
#include "render/field/guest_word.h"
#include "render/field/moby_record.h"
#include "render/terrain/guest_depth_bins.h"
#include "spyro1/core/guest_globals.h"
#include "spyro1/render/terrain_facts.h"
#include "state_producer.h"

#include <cstdlib>

namespace spyro1::actors {
namespace {

namespace gte = psx::gte;
namespace guest_packet = spyro::guest_packet;
namespace frustum = spyro::guest_moby_frustum;
namespace rotation = spyro::guest_moby_rotation;
namespace moby_record = spyro::moby_record;
using terrain::kRenderGlobals;

using spyro::guest_word::asSigned;
using spyro::guest_word::sra;

// 80022C04..80022CC4: the sphere margins are constants here, not the mesh's radius.
constexpr frustum::SphereMargins kMargins{.horizontalLateral = 0x66,
                                          .horizontalDepth = 0x4D,
                                          .verticalLateral = 0x79,
                                          .verticalDepth = 0x28};
constexpr std::int32_t kNearPlane = 0x80;
// Closer than this a record with a negative +0x1C stages its drop shadow (80022C30).
constexpr std::int32_t kShadowDepth = 0x1100;
// 80022D64: a HUD record's rotation before its own angles.
constexpr psx::gte::RotationWords kScreenRotation = {0x1000u, 0u, 0xA00u, 0u, 0x1000u};
// 8002395C: the offsets every record but a HUD one draws with.
constexpr std::uint32_t kScreenOffsetX = 0x1000000u;
constexpr std::uint32_t kScreenOffsetY = 0x780000u;
// Closer than this a flat polygon's back face is drawn too (80023734).
constexpr std::int32_t kTwoSidedDepth = 0x800;

// 80023214..80023250: a channel past 0x1000 saturates, otherwise drops its fraction.
constexpr std::uint32_t saturate(std::uint32_t channel) {
  return asSigned(channel - 0x1000u) >= 0 ? 0xFFu : channel >> 4;
}

// 80023860..800238A4: the flat palette's clamp, which also floors a negative channel.
constexpr std::uint32_t clampFlat(std::uint32_t channel) {
  if (asSigned(channel - 0xFFFu) >= 0) {
    return 0xFFu;
  }
  return asSigned(channel) < 0 ? 0u : channel >> 4;
}

// The palette's colour word as the three back-colour registers take it.
struct BackColour {
  std::uint32_t red = 0;
  std::uint32_t green = 0;
  std::uint32_t blue = 0;
};

constexpr BackColour backColour(std::uint32_t colour) {
  return {(colour << 4) & 0xFF0u, (colour >> 4) & 0xFF0u, (colour >> 12) & 0xFF0u};
}

// 80023010..80023034: a packed 3-byte vertex into V0, each 9-bit field doubled. Returns retail's
// $v1, the z field shifted up.
std::uint32_t loadVertex(Core &core, std::uint32_t word) {
  const std::uint32_t x = sra(word << 24, 23);
  const std::uint32_t y = sra(word << 16, 23);
  const std::uint32_t z = sra(word << 8, 23) << 16;
  core.gte().writeData(gte::kVxy0, y + z);
  core.gte().writeData(gte::kVz0, x);
  return z;
}

} // namespace

QueueView presentedQueueView(const Core &core) {
  return QueueView{.slope = spyro::presentedSlope(core),
                   .bounds = spyro::presentedBounds(core, spyro1::terrain::kSharedFacts)};
}

SpriteQueue::SpriteQueue(Core &core,
                         const psx::present::EmitMemory &memory,
                         const QueueView &view,
                         psx::present::PrimitiveSink *places)
    : core_(core), memory_(memory), view_(view), places_(places), bins_(memory) {}

void SpriteQueue::begin() {
  shadows_ = memory_.mem_r32(kShadowListEnd);
  const std::uint32_t camera = kRenderGlobals.cameraRotation;
  for (std::uint32_t i = 0; i < 3; ++i) {
    memory_.mem_w32(kCameraCopy + i * 4, memory_.mem_r32(kRenderGlobals.cameraPosition + i * 4));
  }
  for (std::uint32_t i = 0; i < gte::kRotationWords; ++i) {
    memory_.mem_w32(kCameraCopy + 0xC + i * 4, memory_.mem_r32(camera + i * 4));
  }
  v0_ = memory_.mem_r32(kRenderGlobals.cameraPosition);
  const std::uint32_t x = memory_.mem_r32(kLightVector);
  const std::uint32_t y = memory_.mem_r32(kLightVector + 4);
  const std::uint32_t z = memory_.mem_r32(kLightVector + 8);
  core_.gte().writeControl(psx::gte::kLightColour + 0, (y << 16) + x);
  core_.gte().writeControl(psx::gte::kLightColour + 3, (y << 16) + x);
  core_.gte().writeControl(psx::gte::kLightColour + 1, (x << 16) + z);
  core_.gte().writeControl(psx::gte::kLightColour + 2, (z << 16) + y);
  core_.gte().writeControl(psx::gte::kLightColour + 4, z);
  v1_ = kLightVector;
}

std::optional<QueuedChain>
SpriteQueue::draw(std::uint32_t record, std::int32_t hudShift, std::uint32_t angleFractions) {
  angleFractions_ = angleFractions;
  const std::uint32_t cull = memory_.mem_r16(record + moby::kCullDistance);
  const std::uint32_t x = memory_.mem_r32(record + moby_record::kX);
  memory_.mem_w8(record + moby::kDrawn, 0);
  std::optional<Centre> centre;
  if ((cull & moby::kScreenSpace) != 0) {
    centre = placeOnScreen({.record = record, .cull = cull, .x = x}, hudShift);
  } else {
    centre = placeInWorld({.record = record, .cull = cull, .x = x});
  }
  if (!centre) {
    return std::nullopt;
  }
  orient(*centre, record);
  if (!projectVertices()) {
    return std::nullopt;
  }

  // 800230EC: a mesh whose vertices start past its header has a normal per vertex and is lit.
  const std::uint32_t vertices = memory_.mem_r32(mesh_ + queued_mesh::kVertices) & 0x7FFFFFFFu;
  vertexColours_ = memory_.mem_r32(mesh_ + queued_mesh::kColours);
  if (mesh_ + queued_mesh::kNormals != vertices) {
    lightVertices(record);
  }

  // 80023268: the polygons, into the local bins.
  paletteWord_ = memory_.mem_r32(record + moby::kSecondaryWord);
  const std::uint32_t polygonCount = memory_.mem_r8(mesh_ + queued_mesh::kPolygonCount);
  const std::uint32_t polygons = memory_.mem_r32(mesh_ + queued_mesh::kPolygons);
  v0_ = polygonCount << 3;
  core_.gte().writeControl(gte::kDqb, depthBin_);
  bins_.begin({.base = kLocalBins, .cursor = memory_.mem_r32(kRenderGlobals.primitiveCursor)});
  const std::uint32_t end = polygons + polygonCount * 8;
  std::uint32_t word = memory_.mem_r32(polygons);
  for (std::uint32_t polygon = polygons; polygon != end; polygon += 8) {
    const std::uint32_t colours = memory_.mem_r32(polygon + 4);
    drawPolygon({.corners = word, .colours = colours});
    word = memory_.mem_r32(polygon + 8);
  }

  // 80023958
  core_.gte().writeControl(gte::kOfx, kScreenOffsetX);
  core_.gte().writeControl(gte::kOfy, kScreenOffsetY);
  memory_.mem_w32(kRenderGlobals.primitiveCursor, bins_.cursor());
  v0_ = bins_.highest() - bins_.lowest();
  if (asSigned(v0_) < 0) {
    return std::nullopt;
  }
  return chainLocalBins();
}

// 80022B40..80022D18
std::optional<SpriteQueue::Centre> SpriteQueue::placeInWorld(const Placement &placement) {
  const auto [record, cull, worldX] = placement;
  const std::uint32_t cameraX = memory_.mem_r32(kCameraCopy);
  const std::uint32_t cameraY = memory_.mem_r32(kCameraCopy + 4);
  const std::uint32_t cameraZ = memory_.mem_r32(kCameraCopy + 8);
  const std::int32_t reach = asSigned(((cull & 0xFFu) << 8) + ((cull & 0x100u) << 1));
  v0_ = memory_.mem_r32(record + moby_record::kY);
  const std::int32_t dx = asSigned(worldX - cameraX) >> 2;
  if (dx + reach <= 0) {
    return std::nullopt;
  }
  v0_ = cameraY - v0_;
  if (dx - reach >= 0) {
    return std::nullopt;
  }
  v1_ = memory_.mem_r32(record + moby_record::kZ);
  v0_ = sra(v0_, 2);
  const std::int32_t dy = asSigned(v0_);
  if (dy + reach <= 0) {
    return std::nullopt;
  }
  v1_ = cameraZ - v1_;
  if (dy - reach >= 0) {
    return std::nullopt;
  }
  v1_ = sra(v1_, 2);
  const std::int32_t dz = asSigned(v1_);
  if (dz + reach <= 0 || dz - reach >= 0) {
    return std::nullopt;
  }

  rotation_ = spyro::guest_memory::rotationAt(memory_, kCameraCopy + 0xC);
  core_.gte().loadRotation(rotation_);
  core_.gte().writeData(gte::kIr3, static_cast<std::uint32_t>(dx));
  core_.gte().writeData(gte::kIr1, static_cast<std::uint32_t>(dy));
  core_.gte().writeData(gte::kIr2, static_cast<std::uint32_t>(dz));
  core_.gte().execute(gte::kMvmvaRtIr);
  const std::uint32_t meshIndex = memory_.mem_r16(record + moby_record::kMesh);
  v1_ = core_.gte().readData(gte::kMac3);
  v0_ = core_.gte().readData(gte::kMac2);
  const frustum::ViewPoint view{
      .x = asSigned(core_.gte().readData(gte::kMac1)), .y = asSigned(v0_), .z = asSigned(v1_)};
  if (view.z - reach >= 0 || frustum::behindEye(view, kNearPlane)) {
    return std::nullopt;
  }
  const std::uint32_t shadowFlag = memory_.mem_r32(record + moby::kShadowFlag);
  if (frustum::outsideHorizontal(view, kMargins, view_.slope)) {
    return std::nullopt;
  }
  mesh_ = memory_.mem_r32(spyro1::guest::kModels + meshIndex * 4);
  // 80022C38: the record is written to the cursor whenever the flag is set; only a near one
  // keeps it.
  if (asSigned(shadowFlag) < 0) {
    memory_.mem_w32(shadows_, record);
    if (view.z - kShadowDepth < 0) {
      shadows_ += 8;
      const auto shadowByte =
          static_cast<std::int8_t>(memory_.mem_r8(mesh_ + queued_mesh::kShadowByte));
      memory_.mem_w32(shadows_ - 4, static_cast<std::uint32_t>(std::int32_t{shadowByte}));
    }
  }
  if (frustum::outsideVertical(view, kMargins)) {
    return std::nullopt;
  }
  edge_ = frustum::insideFrustum(view, kMargins, view_.slope) ? 0u : kStraddlesView;
  angles_ = memory_.mem_r32(record + moby_record::kAngles);
  const std::uint32_t tint = memory_.mem_r8(record + moby::kTint);

  // 80022CCC: the centre doubled; a tinted record scales it toward the eye.
  Centre centre{.x = static_cast<std::uint32_t>(view.x) << 1,
                .y = v0_ << 1,
                .z = v1_ << 1,
                .seenAtRetail = !frustum::outsideHorizontal(view, kMargins, frustum::kRetailSlope)};
  v0_ = centre.y;
  v1_ = centre.z;
  depthBin_ = sra(centre.z, 6);
  if (tint != 0) {
    core_.gte().writeData(gte::kIr0, tint);
    core_.gte().writeData(gte::kIr1, centre.x);
    core_.gte().writeData(gte::kIr2, centre.y);
    core_.gte().writeData(gte::kIr3, centre.z);
    core_.gte().execute(gte::kGpf);
    v1_ = sra(core_.gte().readData(gte::kMac3), 5);
    v0_ = sra(core_.gte().readData(gte::kMac2), 5);
    centre.x = sra(core_.gte().readData(gte::kMac1), 5);
    centre.y = v0_;
    centre.z = v1_;
  }
  return centre;
}

// 80022D1C..80022D8C: a HUD record's x and y are its screen position, through the draw offset.
SpriteQueue::Centre SpriteQueue::placeOnScreen(const Placement &placement, std::int32_t shift) {
  const std::uint32_t record = placement.record;
  const std::uint32_t screenX = placement.x;
  const std::uint32_t screenY = memory_.mem_r32(record + moby_record::kY);
  v1_ = memory_.mem_r32(record + moby_record::kZ);
  const std::uint32_t column = screenX + static_cast<std::uint32_t>(shift);
  core_.gte().writeControl(gte::kOfx, column << 16);
  core_.gte().writeControl(gte::kOfy, screenY << 16);
  v0_ = 0;
  v1_ = sra(v1_, 1);
  depthBin_ = sra(v1_, 6);
  edge_ = 0;
  const std::uint32_t meshIndex = memory_.mem_r16(record + moby_record::kMesh);
  angles_ = memory_.mem_r32(record + moby_record::kAngles);
  mesh_ = memory_.mem_r32(spyro1::guest::kModels + meshIndex * 4);
  rotation_ = kScreenRotation;
  core_.gte().loadRotation(rotation_);
  return Centre{.x = 0, .y = 0, .z = v1_, .screenSpace = 1};
}

// 80022D90..80022FE8: the depth bin and bias, the translation, and the record's angles composed
// onto the rotation loaded.
void SpriteQueue::orient(const Centre &centre, std::uint32_t record) {
  core_.gte().writeControl(gte::kDqa, centre.screenSpace);
  if (centre.seenAtRetail) {
    memory_.mem_w8(record + moby::kDrawn, 1);
  }
  depthBin_ -= sra(angles_, 24);
  if (asSigned(depthBin_) < 0) {
    depthBin_ = 0;
  }
  depthBias_ = asSigned(centre.z - 0x100u) >= 0 ? (centre.z - 0x100u) << 2 : 0u;
  core_.gte().writeControl(gte::kTranslationX, centre.x);
  core_.gte().writeControl(gte::kTranslationY, centre.y);
  core_.gte().writeControl(gte::kTranslationZ, centre.z);
  (void)rotation::composeTabled(
      core_,
      kRenderGlobals.cameraSineTable,
      {.angles = angles_, .flags = angleFractions_},
      rotation_);
}

// 80022FEC..800230E8: every vertex projected; a straddling record keeps an outcode per vertex and
// is dropped when all share an edge.
bool SpriteQueue::projectVertices() {
  const std::uint32_t vertices = memory_.mem_r32(mesh_ + queued_mesh::kVertices);
  const auto count = static_cast<std::int8_t>(memory_.mem_r8(mesh_ + queued_mesh::kVertexCount));
  if (count <= 0) {
    lucent::error("spritequeue", "mesh 0x{:08X} declares {} vertices", mesh_, count);
    std::abort();
  }
  v1_ = loadVertex(core_, memory_.mem_r32(vertices));
  std::uint32_t next = vertices + 3;
  std::uint32_t shared = 0xFFFFFFFFu;
  for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(count); ++i) {
    core_.gte().execute(gte::kRtps);
    const std::uint32_t word = memory_.mem_r8(next) |
                               (std::uint32_t{memory_.mem_r8(next + 1)} << 8) |
                               (std::uint32_t{memory_.mem_r8(next + 2)} << 16) |
                               (std::uint32_t{memory_.mem_r8(next + 3)} << 24);
    next += 3;
    v1_ = loadVertex(core_, word);
    const std::uint32_t screen = core_.gte().readData(gte::kSxy2);
    if (places_ != nullptr) {
      vertexPlaces_[i] = core_.gte().projectedPlace(2);
    }
    v0_ = core_.gte().readData(gte::kSz3);
    const std::uint32_t slot = kVertexScreens + i * 4;
    memory_.mem_w32(slot + (kVertexDepths - kVertexScreens), v0_);
    if (edge_ == 0) {
      memory_.mem_w32(slot, screen);
      continue;
    }
    std::uint32_t code = screen << 5;
    const spyro::guest_terrain::ScreenBounds &bounds = view_.bounds;
    code |= bounds.aboveTop(screen) ? 1u : 0u;
    code |= bounds.belowBottom(screen) ? 2u : 0u;
    code |= bounds.atOrLeftOfLeft(screen) ? 4u : 0u;
    code |= bounds.atOrRightOfRight(screen) ? 8u : 0u;
    shared &= code;
    memory_.mem_w32(slot, code);
    v1_ = code;
  }
  return edge_ == 0 || (shared & 0xFu) == 0;
}

// 80023100..80023264: each vertex normal rotated and lit against the record's palette colour.
void SpriteQueue::lightVertices(std::uint32_t record) {
  // projectVertices refused a count that is not positive.
  const std::uint32_t count = memory_.mem_r8(mesh_ + queued_mesh::kVertexCount);
  const std::uint32_t palette = sra(memory_.mem_r32(record + moby::kSecondaryWord), 22);
  const std::uint32_t base = memory_.mem_r32(kPalette + palette);
  const std::uint32_t floor = base & 0x00FFFFFFu;
  const BackColour back = backColour(base);
  const std::uint32_t boost = base >> 28;
  std::uint32_t normal = mesh_ + queued_mesh::kNormals;
  auto component = [this](std::uint32_t address) {
    return static_cast<std::uint32_t>(
        std::int32_t{static_cast<std::int8_t>(memory_.mem_r8(address))});
  };
  std::uint32_t nx = component(normal);
  std::uint32_t ny = component(normal + 1);
  std::uint32_t nz = component(normal + 2);
  normal += 3;
  const std::uint32_t end = kColourTable + count * 4;
  for (std::uint32_t out = kColourTable; out != end; out += 4) {
    core_.gte().writeData(gte::kIr3, nx);
    core_.gte().writeData(gte::kIr1, ny);
    core_.gte().writeData(gte::kIr2, nz);
    core_.gte().execute(gte::kMvmvaRtIr);
    nx = component(normal);
    core_.gte().writeData(gte::kIr0, (base >> 23) & 0x1Eu);
    core_.gte().execute(gte::kGpf);
    core_.gte().writeData(gte::kRgbc, 0x00FFFFFFu);
    core_.gte().writeControl(gte::kBackRed, back.red);
    core_.gte().writeControl(gte::kBackGreen, back.green);
    core_.gte().writeControl(gte::kBackBlue, back.blue);
    core_.gte().execute(gte::kCc);
    ny = component(normal + 1);
    nz = component(normal + 2);
    std::uint32_t colour = core_.gte().readData(gte::kRgb2);
    normal += 3;
    if (asSigned(colour - floor) < 0) {
      colour = floor;
    } else if (boost != 0) {
      const std::uint32_t ir1 = core_.gte().readData(gte::kIr1);
      std::uint32_t lift = ir1 - (boost << 7) - core_.gte().readControl(gte::kBackRed);
      if (asSigned(lift) > 0) {
        lift <<= 1;
        colour = saturate(ir1 + lift) + (saturate(core_.gte().readData(gte::kIr2) + lift) << 8) +
                 (saturate(core_.gte().readData(gte::kIr3) + lift) << 16);
      }
    }
    memory_.mem_w32(out, colour);
  }
  v0_ = ny;
  v1_ = nz;
  vertexColours_ = kColourTable;
}

// 800232A8..80023330: the polygon's four screen words, its face test, and its kind.
void SpriteQueue::drawPolygon(const PolygonWords &polygon) {
  const auto [word, colours] = polygon;
  v0_ = kVertexScreens + ((word >> 21) & 0x1FCu);
  v1_ = kVertexScreens + ((word >> 14) & 0x1FCu);
  const std::uint32_t a0 = kVertexScreens + ((word >> 7) & 0x1FCu);
  const std::uint32_t a1 = kVertexScreens + (word & 0x1FCu);
  screen_[0] = memory_.mem_r32(v0_);
  screen_[1] = memory_.mem_r32(v1_);
  screen_[2] = memory_.mem_r32(a0);
  screen_[3] = memory_.mem_r32(a1);
  if (places_ != nullptr) {
    cornerPlaces_ = {vertexPlaces_[(v0_ - kVertexScreens) / 4u],
                     vertexPlaces_[(v1_ - kVertexScreens) / 4u],
                     vertexPlaces_[(a0 - kVertexScreens) / 4u],
                     vertexPlaces_[(a1 - kVertexScreens) / 4u]};
  }
  if (asSigned(edge_) < 0) {
    if ((screen_[0] & screen_[1] & screen_[2] & screen_[3] & 0xFu) != 0) {
      return;
    }
    for (std::uint32_t &screen : screen_) {
      screen = sra(screen, 5);
    }
  }
  core_.gte().writeData(gte::kSxy0, screen_[0]);
  core_.gte().writeData(gte::kSxy1, screen_[1]);
  core_.gte().writeData(gte::kSxy2, screen_[2]);
  core_.gte().execute(gte::kNclip);
  if ((word & 1u) == 0) {
    drawGouraud({.colours = colours, .third = a0, .fourth = a1});
  } else if ((word & 2u) == 0) {
    drawLit({.colours = colours, .third = a0, .fourth = a1});
  } else {
    drawFlat({.colours = colours, .third = a0, .fourth = a1});
  }
}

// 80023338..80023530: POLY_G4 or POLY_G3 from the vertex colours; a quad tries its other half.
void SpriteQueue::drawGouraud(const PolygonTail &tail) {
  const auto [colours, a0, a1] = tail;
  auto colour = [this](std::uint32_t index) {
    return memory_.mem_r32(vertexColours_ + (index & 0x1FCu));
  };
  const std::uint32_t t9 = bins_.cursor();
  if (a0 != a1) {
    const std::int32_t face = asSigned(core_.gte().readData(gte::kMac0));
    core_.gte().writeData(gte::kSxy0, screen_[3]);
    if (face <= 0) {
      core_.gte().execute(gte::kNclip);
      if (asSigned(core_.gte().readData(gte::kMac0)) >= 0) {
        return;
      }
    }
    const std::uint32_t c0 = colour(colours >> 21) + guest_packet::kG4Code;
    const std::uint32_t c1 = colour(colours >> 14);
    const std::uint32_t c2 = colour(colours >> 7);
    const std::uint32_t c3 = colour(colours);
    v0_ = memory_.mem_r32(v0_ + 0x200u);
    v1_ = memory_.mem_r32(v1_ + 0x200u);
    const std::uint32_t d2 = memory_.mem_r32(a0 + 0x200u);
    const std::uint32_t d3 = memory_.mem_r32(a1 + 0x200u);
    memory_.mem_w32(t9 + 0x08, screen_[0]);
    memory_.mem_w32(t9 + 0x10, screen_[1]);
    memory_.mem_w32(t9 + 0x18, screen_[2]);
    memory_.mem_w32(t9 + 0x20, screen_[3]);
    const std::uint32_t depth = v0_ + v1_ + d2 + d3 - depthBias_;
    v0_ = sra(depth, 5);
    if (asSigned(depth) <= 0) {
      return;
    }
    v0_ += colours & 3u;
    memory_.mem_w32(t9 + 0x04, c0);
    memory_.mem_w32(t9 + 0x0C, c1);
    memory_.mem_w32(t9 + 0x14, c2);
    memory_.mem_w32(t9 + 0x1C, c3);
    memory_.mem_w32(t9, guest_packet::kG4Tag);
    declarePlaces(t9, 4);
    v1_ = bins_.link(kLocalBins + (v0_ << 3));
    v0_ = kLocalBins + (v0_ << 3);
    return;
  }
  if (asSigned(core_.gte().readData(gte::kMac0)) <= 0) {
    return;
  }
  const std::uint32_t c0 = colour(colours >> 21);
  const std::uint32_t c1 = colour(colours >> 14);
  const std::uint32_t c2 = colour(colours >> 7);
  v0_ = memory_.mem_r32(v0_ + 0x200u);
  v1_ = memory_.mem_r32(v1_ + 0x200u);
  const std::uint32_t d2 = memory_.mem_r32(a0 + 0x200u);
  memory_.mem_w32(t9 + 0x08, screen_[0]);
  memory_.mem_w32(t9 + 0x10, screen_[1]);
  memory_.mem_w32(t9 + 0x18, screen_[2]);
  v0_ = v0_ + v1_ + d2 + d2 - depthBias_;
  memory_.mem_w32(t9 + 0x04, c0 + guest_packet::kG3Code);
  if (asSigned(v0_) <= 0) {
    return;
  }
  memory_.mem_w32(t9 + 0x0C, c1);
  memory_.mem_w32(t9 + 0x14, c2);
  v0_ = sra(v0_, 5) + (colours & 3u);
  memory_.mem_w32(t9, guest_packet::kG3Tag);
  declarePlaces(t9, 3);
  v1_ = bins_.link(kLocalBins + (v0_ << 3));
  v0_ = kLocalBins + (v0_ << 3);
}

// 80023534..8002371C: POLY_F4 or POLY_F3 lit from the face normal against the palette colour.
void SpriteQueue::drawLit(const PolygonTail &tail) {
  const auto [colours, a0, a1] = tail;
  const std::uint32_t palette = kPalette + sra(paletteWord_, 22);
  const bool opaque = sra(paletteWord_, 22) != 0;
  if (asSigned(core_.gte().readData(gte::kMac0)) <= 0) {
    core_.gte().writeData(gte::kSxy0, screen_[3]);
    if (a0 == a1) {
      return;
    }
    core_.gte().execute(gte::kNclip);
    if (asSigned(core_.gte().readData(gte::kMac0)) >= 0) {
      return;
    }
  }
  const std::uint32_t base = memory_.mem_r32(palette);
  const std::uint32_t t9 = bins_.cursor();
  core_.gte().writeData(gte::kIr3, sra(colours, 24));
  core_.gte().writeData(gte::kIr1, sra(colours << 8, 24));
  core_.gte().writeData(gte::kIr2, sra(colours << 16, 24));
  memory_.mem_w32(t9 + 0x08, screen_[0]);
  memory_.mem_w32(t9 + 0x0C, screen_[1]);
  core_.gte().execute(gte::kMvmvaRtIr);
  memory_.mem_w32(t9 + 0x10, screen_[2]);
  memory_.mem_w32(t9 + 0x14, screen_[3]);
  const BackColour back = backColour(base);
  core_.gte().writeData(gte::kIr0, (base >> 23) & 0x1Eu);
  core_.gte().execute(gte::kGpf);
  core_.gte().writeControl(gte::kBackRed, back.red);
  core_.gte().writeControl(gte::kBackGreen, back.green);
  core_.gte().writeControl(gte::kBackBlue, back.blue);
  core_.gte().writeData(gte::kRgbc, 0x00FFFFFFu);
  const std::uint32_t d0 = memory_.mem_r32(v0_ + 0x200u);
  const std::uint32_t d1 = memory_.mem_r32(v1_ + 0x200u);
  core_.gte().execute(gte::kCc);
  const std::uint32_t d2 = memory_.mem_r32(a0 + 0x200u);
  const std::uint32_t d3 = memory_.mem_r32(a1 + 0x200u);
  const std::uint32_t depth = d0 + d1 + d2 + d3 - depthBias_;
  if (asSigned(depth) <= 0) {
    return;
  }
  if (a0 != a1) {
    v0_ = guest_packet::kF4Tag;
    v1_ = guest_packet::kF4Code;
  } else {
    v0_ = guest_packet::kF3Tag;
    v1_ = guest_packet::kF3Code;
  }
  if (!opaque) {
    v1_ += guest_packet::kSemiCode;
    edge_ |= 1u;
  }
  std::uint32_t colour = core_.gte().readData(gte::kRgb2);
  memory_.mem_w32(t9, v0_);
  if (const std::uint32_t boost = base >> 28; boost != 0) {
    const std::uint32_t ir1 = core_.gte().readData(gte::kIr1);
    std::uint32_t lift = ir1 - (boost << 7) - core_.gte().readControl(gte::kBackRed);
    if (asSigned(lift) > 0) {
      lift <<= 1;
      colour = saturate(ir1 + lift) + (saturate(core_.gte().readData(gte::kIr2) + lift) << 8) +
               (saturate(core_.gte().readData(gte::kIr3) + lift) << 16);
    }
  }
  memory_.mem_w32(t9 + 0x04, colour | v1_);
  declarePlaces(t9, a0 != a1 ? 4 : 3);
  (void)bins_.link(kLocalBins + (sra(depth, 5) << 3));
}

// 80023720..80023948: POLY_F4 or POLY_F3 from the flat palette's pair, two-sided up close.
void SpriteQueue::drawFlat(const PolygonTail &tail) {
  const auto [colours, a0, a1] = tail;
  const std::uint32_t pair = kFlatPalette + sra(paletteWord_, 21);
  std::uint32_t face = core_.gte().readData(gte::kMac0);
  std::uint32_t opaque = 0;
  if (asSigned(core_.gte().readControl(gte::kTranslationZ) - kTwoSidedDepth) < 0) {
    opaque = (face >> 31) << 25;
  } else {
    opaque = guest_packet::kSemiCode;
    if (asSigned(face) <= 0) {
      core_.gte().writeData(gte::kSxy0, screen_[3]);
      if (a0 == a1) {
        return;
      }
      core_.gte().execute(gte::kNclip);
      face = core_.gte().readData(gte::kMac0);
      if (asSigned(face) >= 0) {
        return;
      }
    }
  }
  const std::uint32_t t9 = bins_.cursor();
  memory_.mem_w32(t9 + 0x08, screen_[0]);
  memory_.mem_w32(t9 + 0x0C, screen_[1]);
  memory_.mem_w32(t9 + 0x10, screen_[2]);
  memory_.mem_w32(t9 + 0x14, screen_[3]);
  core_.gte().writeData(gte::kIr3, sra(colours, 24));
  core_.gte().writeData(gte::kIr1, sra(colours << 8, 24));
  core_.gte().writeData(gte::kIr2, sra(colours << 16, 24));
  v0_ = memory_.mem_r32(v0_ + 0x200u);
  v1_ = memory_.mem_r32(v1_ + 0x200u);
  core_.gte().execute(gte::kMvmvaRtIr);
  std::uint32_t lit = memory_.mem_r32(pair);
  const std::uint32_t shade = memory_.mem_r32(pair + 4);
  v0_ = v0_ + v1_ - depthBias_;
  core_.gte().execute(psx::gte::kMvmvaLcmIr);
  edge_ |= 2u;
  const BackColour shadeChannels = backColour(shade);
  std::uint32_t intensity = core_.gte().readData(gte::kMac1);
  core_.gte().writeData(gte::kIr1, shadeChannels.red);
  core_.gte().writeData(gte::kIr2, shadeChannels.green);
  core_.gte().writeData(gte::kIr3, shadeChannels.blue);
  intensity = sra(intensity, 8);
  if (asSigned(face) < 0) {
    intensity = sra(intensity, 2);
    lit >>= 1;
    v0_ += 0x200u;
  }
  core_.gte().writeData(gte::kIr0, intensity);
  const BackColour litChannels = backColour(lit);
  core_.gte().execute(gte::kGpf);
  std::uint32_t red = litChannels.red + sra(core_.gte().readData(gte::kMac1), 12);
  std::uint32_t green = litChannels.green + sra(core_.gte().readData(gte::kMac2), 12);
  std::uint32_t blue = litChannels.blue + sra(core_.gte().readData(gte::kMac3), 12);
  if (const std::uint32_t over = intensity - 0x5C0u; asSigned(over) > 0) {
    red += over << 3;
    green += over << 3;
    blue += over << 3;
  }
  red = clampFlat(red);
  green = clampFlat(green);
  blue = clampFlat(blue);
  std::uint32_t tag = guest_packet::kF3Tag;
  std::uint32_t command = guest_packet::kF3Code + guest_packet::kSemiCode;
  if (a0 != a1) {
    tag = guest_packet::kF4Tag;
    command = guest_packet::kF4Code + guest_packet::kSemiCode;
  }
  const std::uint32_t d2 = memory_.mem_r32(a0 + 0x200u);
  const std::uint32_t d3 = memory_.mem_r32(a1 + 0x200u);
  command = command - opaque + red + (green << 8) + (blue << 16);
  memory_.mem_w32(t9, tag);
  memory_.mem_w32(t9 + 0x04, command);
  const std::uint32_t depth = v0_ + d2 + d3;
  v0_ = sra(depth, 5);
  if (asSigned(depth) <= 0) {
    return;
  }
  declarePlaces(t9, a0 != a1 ? 4 : 3);
  v1_ = bins_.link(kLocalBins + (v0_ << 3));
  v0_ = kLocalBins + (v0_ << 3);
}

void SpriteQueue::declarePlaces(std::uint32_t packet, std::size_t count) const {
  if (places_ != nullptr) {
    places_->placeCorners(packet, std::span(cornerPlaces_).first(count));
  }
}

// 8002397C..800239B4: the bins the record reached, deepest first, into one chain; each emptied.
std::optional<QueuedChain> SpriteQueue::chainLocalBins() {
  std::uint32_t at = bins_.highest();
  QueuedChain chain{.first = memory_.mem_r32(at + 4),
                    .last = memory_.mem_r32(at),
                    .depthBin = depthBin_,
                    .overlay = false,
                    .drawMode = 0};
  v0_ = chain.first;
  memory_.mem_w32(at, 0);
  memory_.mem_w32(at + 4, 0);
  for (;;) {
    if (bins_.lowest() == at) {
      v1_ = memory_.mem_r32(at - 4);
      break;
    }
    v1_ = memory_.mem_r32(at - 4);
    at -= 8;
    if (v1_ == 0) {
      continue;
    }
    spyro::guest_memory::linkAfter(memory_, chain.last, v1_);
    chain.last = memory_.mem_r32(at);
    memory_.mem_w32(at, 0);
    memory_.mem_w32(at + 4, 0);
    if (bins_.lowest() == at) {
      break;
    }
  }
  chain.overlay = depthBin_ == 0 && core_.gte().readControl(gte::kDqa) != 0;
  chain.drawMode = edge_ & 0xFu;
  return chain;
}

void SpriteQueue::link(const QueuedChain &chain) {
  v1_ = chain.overlay ? memory_.mem_r32(terrain::kRenderGlobals.overlayPair)
                      : memory_.mem_r32(kRenderGlobals.orderingTable) + (chain.depthBin << 3);
  if (chain.drawMode != 0) {
    const std::uint32_t packet = bins_.cursor();
    memory_.mem_w32(packet, 0x02000000u);
    memory_.mem_w32(packet + 4, chain.modeCommand());
    memory_.mem_w32(packet + 8, 0);
    bins_.take();
    memory_.mem_w32(kRenderGlobals.primitiveCursor, bins_.cursor());
    spyro::depth_bins::linkIntoBin(memory_, v1_, packet, packet);
  }
  if (spyro::depth_bins::linkIntoBin(memory_, v1_, chain.first, chain.last) != 0) {
    v0_ = chain.first >> 16;
  }
}

void SpriteQueue::end() {
  memory_.mem_w32(kShadowListEnd, shadows_);
  core_.r[2] = v0_;
  core_.r[3] = v1_;
}

} // namespace spyro1::actors
