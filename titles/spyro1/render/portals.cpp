#include "spyro1/render/portals.h"

#include "core.h"
#include "core/guest_magnitude.h"
#include "cpu_divide.h"
#include "lucent/log.h"
#include "render/field/guest_moby_rotation.h"
#include "render/field/guest_packet.h"
#include "render/field/guest_word.h"
#include "render/terrain/guest_depth_bins.h"
#include "render/terrain/guest_terrain_fog.h"
#include "render/terrain/guest_terrain_screen.h"
#include "spyro1/core/guest_globals.h"
#include "spyro1/render/sky.h"
#include "spyro1/render/terrain_facts.h"
#include "spyro1/render/terrain_image.h"
#include "state_producer.h"

#include <algorithm>
#include <cstdlib>

namespace spyro1::portals {
namespace {

namespace gte = psx::gte;
using spyro::guest_word::absolute;
using spyro::guest_word::asSigned;
using spyro::guest_word::asWord;
using spyro::guest_word::signedHalf;
using spyro::guest_word::sra;
using terrain::kRenderGlobals;

// D_8006CB8C: the two triangles that cover the screen, three packed points each.
constexpr std::uint32_t kScreenTriangles = 0x8006CB8Cu;
// The near sky's pieces, three points, three colours and their edge, stacked from kPieces
// (8004F920).
constexpr std::uint32_t kShadedPieceBytes = 0x1Cu;
constexpr std::uint32_t kCycloramaColour = sky::kCyclorama + 0x10u;

constexpr std::uint32_t kWhite = 0xFFFFFFu;
constexpr std::uint32_t kColourBits = 0xFFFFFFu;
constexpr std::uint32_t kFullAmount = 0x1000u;
// 80050FB4: depths keep this many bits before points are shifted down.
constexpr std::uint32_t kDepthBits = 0xDu;
// 80050F6C: a distance this many bits long is measured halved.
constexpr std::uint32_t kLongDistanceBits = 0xFu;
// 8005170C: nearer than this the outline must also face the camera.
constexpr std::int32_t kFacingDistance = 0x1000;
// 80050FDC: past this, with the far terrain skipped, the colour fades toward the sky's.
constexpr std::int32_t kSkyFadeStart = 0x4000;
constexpr std::uint32_t kSkyFadeEnd = 0x8000u;
// 80051414: an edge narrower than this in x and y on every side is no outline.
constexpr std::uint32_t kNarrowSpan = 3u;
// 80051630: bins past 0x100 move 0x40 deeper; the table has 0x800.
constexpr std::int32_t kNearBins = 0x100;
constexpr std::uint32_t kBinPush = 0x40u;
constexpr std::int32_t kLastBin = 0x7FF;
constexpr std::uint32_t kBinShift = 7u;
// The retail screen's height; its width is `screen_`.
constexpr std::int32_t kScreenHeight = 0xF0;
// 8005193C: the near outline's points step this far out from its middle.
constexpr std::uint32_t kNearPush = 2u;
// 8004FE4C..8004FE64: the crossing's fraction, 12 bits of IR0.
constexpr std::uint32_t kCrossingShift = 7u;
constexpr std::uint32_t kCrossingRound = 0x40u;
constexpr std::uint32_t kFractionShift = 5u;
constexpr std::uint32_t kFractionRound = 0x10u;
// 80050C24..80050C58: the near sky's spin, 2 a call, its pitch wrapped and held at 0x80.
constexpr std::uint32_t kSpinStep = 2u;
constexpr std::int32_t kPitchWrap = 0x800;
constexpr std::int32_t kPitchLimit = 0x80;
constexpr std::uint32_t kTurn = 0x1000u;
constexpr std::uint32_t kAngleMask = 0xFFFu;

// NCLIP's verdict per corner, 8004FFC0..8005000C.
constexpr std::uint32_t kFirstOut = 0x08u;
constexpr std::uint32_t kSecondOut = 0x10u;
constexpr std::uint32_t kThirdOut = 0x20u;

namespace edge {
constexpr std::uint32_t kFlag = 0x00;
constexpr std::uint32_t kFrom = 0x04;
constexpr std::uint32_t kTo = 0x08;
constexpr std::uint32_t kA = 0x0C;
constexpr std::uint32_t kB = 0x10;
constexpr std::uint32_t kC = 0x14;
} // namespace edge

std::uint32_t packed(std::uint32_t x, std::uint32_t y) {
  return (y << 16) | (x & 0xFFFFu);
}

} // namespace

Portals::Portals(Core &core,
                 const psx::present::EmitMemory &memory,
                 spyro::guest_terrain::ScreenBounds screen)
    : core_(core), memory_(memory), screen_(screen) {}

std::uint32_t
Portals::colourToward(std::uint32_t colour, std::uint32_t target, std::uint32_t amount) {
  auto &gte = core_.gte();
  gte.writeData(gte::kRgbc, colour);
  gte.writeData(gte::kIr0, amount);
  spyro::guest_terrain::loadFarColour(gte, target);
  gte.execute(gte::kDpcs);
  return gte.readData(gte::kRgb2);
}

std::uint32_t Portals::topBit(std::uint32_t value) {
  auto &gte = core_.gte();
  gte.writeData(gte::kLzcs, value);
  return value == 0 ? 0u : 31u - gte.readData(gte::kLzcr);
}

Portals::Point Portals::projected(std::uint32_t i) const {
  const std::uint32_t at = kProjected + i * portal::kPointBytes;
  return {asSigned(memory_.mem_r32(at)),
          asSigned(memory_.mem_r32(at + 4)),
          asSigned(memory_.mem_r32(at + 8))};
}

std::uint32_t
Portals::neighbour(std::uint32_t i, std::uint32_t count, std::uint32_t winding) const {
  if (asSigned(winding) > 0) {
    return i + 1 == count ? 0u : i + 1;
  }
  return i == 0 ? count - 1 : i - 1;
}

std::optional<std::array<std::uint32_t, 2>>
Portals::appendEdge(std::uint32_t i, std::uint32_t j, std::uint32_t &edges) {
  const Point from = projected(i);
  const Point to = projected(j);
  if ((from.x >= screen_.right() && to.x >= screen_.right()) ||
      (from.x <= screen_.left() && to.x <= screen_.left()) ||
      (from.y >= kScreenHeight && to.y >= kScreenHeight) || (from.y <= 0 && to.y <= 0)) {
    return std::nullopt;
  }
  const std::uint32_t at = kEdges + edges * kEdgeBytes;
  const std::uint32_t a = asWord(from.y) - asWord(to.y);
  const std::uint32_t b = asWord(to.x) - asWord(from.x);
  memory_.mem_w32(at + edge::kFlag, 1);
  memory_.storeVertex(at + edge::kFrom, {packed(asWord(from.x), asWord(from.y)), outline_.at(i)});
  memory_.storeVertex(at + edge::kTo, {packed(asWord(to.x), asWord(to.y)), outline_.at(j)});
  memory_.mem_w32(at + edge::kA, a);
  memory_.mem_w32(at + edge::kB, b);
  memory_.mem_w32(at + edge::kC, (0u - a) * asWord(from.x) - b * asWord(from.y));
  ++edges;
  return std::array<std::uint32_t, 2>{absolute(asWord(from.x) - asWord(to.x)),
                                      absolute(asWord(from.y) - asWord(to.y))};
}

// 80051654..800516F8: 2 when the outline's box overlaps the screen or covers it, else nothing at
// all.
std::uint32_t Portals::coverage(const Box &box, std::uint32_t edges, std::uint32_t onScreen) const {
  const std::int32_t bin = asSigned(memory_.mem_r32(kBin));
  const bool overlaps = box.minX < screen_.right() && box.minY < kScreenHeight &&
                        box.maxX > screen_.left() && box.maxY > 0 && bin > 0 && edges != 0;
  const bool covers = box.minX <= screen_.left() && box.minY <= 0 && box.maxX >= screen_.right() &&
                      box.maxY >= kScreenHeight && bin > 0;
  return overlaps || covers ? onScreen | 2u : 0u;
}

// 80051718..800517D8: the camera's forward axis against the way to the portal's middle.
bool Portals::facesCamera(const std::array<std::uint32_t, 3> &centre) const {
  std::array<std::uint32_t, 3> toCentre{};
  for (std::uint32_t axis = 0; axis < 3; ++axis) {
    toCentre[axis] = centre[axis] - memory_.mem_r32(kRenderGlobals.cameraPosition + axis * 4);
  }
  std::uint32_t angles = 0;
  for (std::uint32_t axis = 0; axis < 3; ++axis) {
    angles |= ((memory_.mem_r16(kRenderGlobals.cameraAngles + axis * 2) >> 4) & 0xFFu)
              << (axis * 8);
  }
  auto &gte = core_.gte();
  // RotVec8ToMatrix 0x80016D2C from the identity.
  gte::RotationWords rotation{kFullAmount, 0, kFullAmount, 0, kFullAmount};
  gte.loadRotation(rotation);
  gte.loadTranslation({});
  (void)spyro::guest_moby_rotation::composeTabled(
      core_, guest::kSinTable, {.angles = angles}, rotation);
  gte.writeData(gte::kIr3, kFullAmount);
  gte.writeData(gte::kIr1, 0);
  gte.writeData(gte::kIr2, 0);
  gte.execute(gte::kMvmvaRtIr);
  const std::uint32_t dot = gte.readData(gte::kMac3) * toCentre[0] +
                            (0u - gte.readData(gte::kMac1)) * toCentre[1] +
                            (0u - gte.readData(gte::kMac2)) * toCentre[2];
  return asSigned(dot) >= 0;
}

// 800517E4..80051858: the box clamped to the screen into D_8007AA00, the edge list ended.
void Portals::publishWindow(Box box, std::uint32_t edges) {
  box.minX = std::max(box.minX, screen_.left());
  box.minY = std::max(box.minY, 0);
  box.maxX = std::min(box.maxX, screen_.right());
  box.maxY = std::min(box.maxY, kScreenHeight);
  memory_.mem_w32(kWindow + 0x0, asWord(box.minY) << 16);
  memory_.mem_w32(kWindow + 0x4, asWord(box.maxY) << 16);
  memory_.mem_w32(kWindow + 0x8, asWord(box.maxX) << 16);
  memory_.mem_w32(kWindow + 0xC, asWord(box.minX) << 16);
  memory_.mem_w32(kEdges + edges * kEdgeBytes + edge::kFlag, 0);
}

// 80051354, 8005199C: x and y strictly inside the screen, as `x - 1 < width - 1` unsigned.
bool Portals::insideOpen(const Point &point) const {
  return point.x > screen_.left() && point.x < screen_.right() &&
         asWord(point.y) - 1u < kScreenHeight - 1u;
}

// D_8006CB8C's corners sit on retail's edges x = 0 and x = 512; they are moved onto the window's.
Portals::Corner Portals::spanned(Corner corner) const {
  const std::int32_t x =
      asSigned(signedHalf(corner.point.word())) <= 0 ? screen_.left() : screen_.right();
  corner.point = corner.point.steppedTo(packed(asWord(x), corner.point.word() >> 16));
  return corner;
}

std::optional<Approach> Portals::drawFar(std::uint32_t index) {
  const std::uint32_t at = memory_.mem_r32(guest::kPortals + index * 4);
  const std::uint32_t sector = memory_.mem_r32(at + portal::kSector);
  if (asSigned(sector) >= 0 && memory_.mem_r8(terrain::kFacts.sectorMarks + sector) == 0) {
    return std::nullopt;
  }
  Approach approach{.portal = at};
  const std::uint32_t sky = memory_.mem_r32(at + portal::kSky);
  const spyro::guest_camera::AngleTable cosine(core_, guest::kCosTable);
  const std::int32_t pulse = cosine.at((memory_.mem_r32(guest::kLevelTicks) << 4) + (index << 9));
  approach.colour =
      colourToward(memory_.mem_r32(sky + kSkyColour), kWhite, sra(absolute(asWord(pulse)), 1));

  std::array<std::uint32_t, 3> toPortal{};
  for (std::uint32_t axis = 0; axis < 3; ++axis) {
    toPortal[axis] = memory_.mem_r32(at + portal::kPoints + axis * 4) -
                     memory_.mem_r32(kRenderGlobals.cameraPosition + axis * 4);
  }
  const std::uint32_t bits =
      topBit(absolute(toPortal[0]) + absolute(toPortal[1]) + absolute(toPortal[2]));
  const std::uint32_t halved = sra(bits, 1);
  if (bits >= kLongDistanceBits) {
    for (std::uint32_t &component : toPortal) {
      component = sra(component, halved);
    }
  }
  auto &gte = core_.gte();
  gte.writeData(gte::kIr1, toPortal[0]);
  gte.writeData(gte::kIr2, toPortal[1]);
  gte.writeData(gte::kIr3, toPortal[2]);
  gte.execute(gte::kSqr);
  const std::uint32_t squares =
      gte.readData(gte::kMac1) + gte.readData(gte::kMac2) + gte.readData(gte::kMac3);
  approach.distance =
      spyro::guest_magnitude::magnitude(gte, memory_, guest::kMagnitudeTable, squares).length();
  if (bits >= kLongDistanceBits) {
    approach.distance <<= halved;
  }
  const std::int32_t distance = asSigned(approach.distance);
  const std::uint32_t shift =
      static_cast<std::uint32_t>(std::max(0, asSigned(topBit(approach.distance) - kDepthBits)));
  if (memory_.mem_r32(guest::kSkipLowPolyWorld) != 0 && distance > kSkyFadeStart) {
    const std::int32_t amount =
        std::clamp(asSigned(sra(kSkyFadeEnd - approach.distance, 2)), 0, 0x1000);
    approach.colour =
        colourToward(memory_.mem_r32(kCycloramaColour), approach.colour, asWord(amount));
  }

  // 8005102C..800510A0: each point projected, shifted down by `shift` and its depth back up.
  const std::uint32_t count = memory_.mem_r32(at + portal::kPointCount);
  if (asSigned(count) <= 0) {
    lucent::error("spyro1-portals", "portal 0x{:08X} has {} points", at, asSigned(count));
    std::abort();
  }
  gte.loadRotation(spyro::guest_memory::rotationAt(memory_, kRenderGlobals.cameraRotation));
  gte.loadTranslation({});
  const std::uint32_t cameraX = memory_.mem_r32(kRenderGlobals.cameraPosition);
  const std::uint32_t cameraY = memory_.mem_r32(kRenderGlobals.cameraPosition + 4);
  const std::uint32_t cameraZ = memory_.mem_r32(kRenderGlobals.cameraPosition + 8);
  for (std::uint32_t i = 0; i < count; ++i) {
    const std::uint32_t point = at + portal::kPoints + i * portal::kPointBytes;
    const std::uint32_t x = memory_.mem_r32(point);
    const std::uint32_t y = memory_.mem_r32(point + 4);
    const std::uint32_t z = memory_.mem_r32(point + 8);
    gte.writeData(gte::kVz0, sra(x - cameraX, shift));
    gte.writeData(gte::kVxy0, packed(sra(cameraY - y, shift), sra(cameraZ - z, shift)));
    gte.execute(gte::kRtps);
    const psx::gte::ProjectedVertex projected = gte.projected(2);
    const std::uint32_t screen = projected.word();
    outline_.at(i) = projected.place();
    const std::uint32_t out = kProjected + i * portal::kPointBytes;
    memory_.mem_w32(out, signedHalf(screen));
    memory_.mem_w32(out + 4, sra(screen, 16));
    memory_.mem_w32(out + 8, gte.readData(gte::kMac3) << shift);
    approach.centre[0] += x;
    approach.centre[1] += y;
    approach.centre[2] += z;
  }
  for (std::uint32_t &component : approach.centre) {
    component = psx::cpu::quotient({component, count});
  }

  // 80051168..80051214: which way round the outline runs, from the camera's side of its plane.
  std::uint32_t winding = 0;
  for (std::uint32_t axis = 0; axis < 3; ++axis) {
    const std::uint32_t offset =
        sra(memory_.mem_r32(kRenderGlobals.cameraPosition + axis * 4) -
                memory_.mem_r32(at + portal::kPoints + portal::kPointBytes + axis * 4),
            shift);
    winding -= offset * memory_.mem_r32(at + portal::kNormal + axis * 4);
  }
  approach.winding = winding;

  // 800511E4..80051558: the edges between neighbours, their box, their depth and their mean point.
  Box box{.minX = 0x13F8, .maxX = 0, .minY = kScreenHeight, .maxY = 0};
  std::uint32_t onScreen = 0;
  std::uint32_t edges = 0;
  std::array<std::uint32_t, 2> sum{};
  bool narrowX = true;
  bool narrowY = true;
  memory_.mem_w32(kBin, 0);
  memory_.mem_w32(kPieceQueue, kPieces);
  for (std::uint32_t i = 0; i < count; ++i) {
    const std::uint32_t j = neighbour(i, count, winding);
    const Point next = projected(j);
    sum[0] += asWord(next.x);
    sum[1] += asWord(next.y);
    memory_.mem_w32(kBin, memory_.mem_r32(kBin) + asWord(next.z));
    if (insideOpen(next) && next.z > 0) {
      onScreen = 1;
    }
    box.maxX = std::max(box.maxX, next.x);
    box.minX = std::min(box.minX, next.x);
    box.maxY = std::max(box.maxY, next.y);
    box.minY = std::min(box.minY, next.y);
    if (const auto spans = appendEdge(i, j, edges)) {
      narrowX = narrowX && (*spans)[0] < kNarrowSpan;
      narrowY = narrowY && (*spans)[1] < kNarrowSpan;
    }
  }
  if ((narrowX || narrowY) && edges != 0) {
    return std::nullopt;
  }
  approach.average = {psx::cpu::quotient({sum[0], count}), psx::cpu::quotient({sum[1], count})};
  std::int32_t bin = asSigned(sra(psx::cpu::quotient({memory_.mem_r32(kBin), count}), kBinShift));
  if (bin >= kNearBins) {
    bin += static_cast<std::int32_t>(kBinPush);
  }
  memory_.mem_w32(kBin, asWord(std::min(bin, kLastBin)));

  std::uint32_t shown = coverage(box, edges, onScreen);
  if (shown == 2u && distance < kFacingDistance && !facesCamera(approach.centre)) {
    shown = 0;
  }
  if (shown != 0) {
    publishWindow(box, edges);
    std::uint32_t colour = approach.colour;
    if (distance < kNearDistance) {
      if (distance > kFadeDistance) {
        const std::uint32_t step = approach.distance - kFadeDistance;
        memory_.mem_w32(kFadeStep, step);
        colour = colourToward(memory_.mem_r32(sky + kSkyColour), approach.colour, step);
        memory_.mem_w32(kFadeColour, colour);
      } else {
        colour = memory_.mem_r32(sky + kSkyColour);
      }
    }
    fill(colour);
  }
  return approach;
}

void Portals::drawNear(const Approach &approach, const sky::Rotations &rotations) {
  if (asSigned(approach.distance) >= kNearDistance) {
    return;
  }
  const auto near = outlineNear(approach);
  if (!near) {
    return;
  }
  const auto [skyAt, fading] = *near;
  drawSky(skyAt, fading, rotations);
}

void Portals::drawSky(std::uint32_t skyAt, bool fading, const sky::Rotations &rotations) {
  const spyro::guest_terrain::VerticalBounds vertical{.top = memory_.mem_r32(kWindow),
                                                      .bottom = memory_.mem_r32(kWindow + 4)};
  sky::Cyclorama cyclorama(core_,
                           memory_,
                           spyro::guest_terrain::ScreenBounds::edges(
                               {.left = asSigned(sra(memory_.mem_r32(kWindow + 0xC), 16)),
                                .right = asSigned(sra(memory_.mem_r32(kWindow + 0x8), 16))},
                               vertical));
  std::optional<sky::SkyFade> fade;
  if (fading) {
    fade = sky::SkyFade{.colour = memory_.mem_r32(kFadeColour), .step = memory_.mem_r32(kFadeStep)};
  }
  const std::uint32_t slot =
      memory_.mem_r32(kRenderGlobals.orderingTable) + (memory_.mem_r32(kBin) << 3);
  static_cast<void>(cyclorama.drawThrough(skyAt, rotations, fade, slot, *this));
}

std::optional<std::pair<std::uint32_t, bool>> Portals::outlineNear(const Approach &approach) {
  const std::uint32_t at = approach.portal;
  const std::uint32_t count = memory_.mem_r32(at + portal::kPointCount);
  Box box{.minX = screen_.right(), .maxX = screen_.left(), .minY = kScreenHeight, .maxY = 0};
  memory_.mem_w32(kPieceQueue, kPieces);
  for (std::uint32_t i = 0; i < count; ++i) {
    const std::uint32_t point = kProjected + i * portal::kPointBytes;
    for (std::uint32_t axis = 0; axis < 2; ++axis) {
      const std::int32_t value = asSigned(memory_.mem_r32(point + axis * 4));
      const std::int32_t middle = asSigned(approach.average[axis]);
      std::int32_t push = 0;
      if (middle < value) {
        push = static_cast<std::int32_t>(kNearPush);
      } else if (value < middle) {
        push = -static_cast<std::int32_t>(kNearPush);
      }
      if (push != 0) {
        memory_.mem_w32(point + axis * 4, asWord(value + push));
        (axis == 0 ? outline_.at(i).x : outline_.at(i).y) += static_cast<float>(push);
      }
    }
    const Point pushed = projected(i);
    box.maxX = std::max(box.maxX, pushed.x);
    box.minX = std::min(box.minX, pushed.x);
    box.maxY = std::max(box.maxY, pushed.y);
    box.minY = std::min(box.minY, pushed.y);
  }
  std::uint32_t onScreen = 0;
  std::uint32_t edges = 0;
  for (std::uint32_t i = 0; i < count; ++i) {
    const std::uint32_t j = neighbour(i, count, approach.winding);
    const Point next = projected(j);
    if (insideOpen(next) && next.z > 0) {
      onScreen = 1;
    }
    static_cast<void>(appendEdge(i, j, edges));
  }
  std::uint32_t shown = coverage(box, edges, onScreen);
  const std::int32_t distance = asSigned(approach.distance);
  if (shown == 2u && distance < kFacingDistance && !facesCamera(approach.centre)) {
    shown = 0;
  }
  if (shown == 0) {
    return std::nullopt;
  }
  publishWindow(box, edges);
  const std::uint32_t sky = memory_.mem_r32(at + portal::kSky);
  if (distance <= kFadeDistance) {
    return std::pair{sky, false};
  }
  const std::uint32_t step = approach.distance - kFadeDistance;
  memory_.mem_w32(kFadeStep, step);
  memory_.mem_w32(kFadeColour,
                  colourToward(memory_.mem_r32(sky + kSkyColour), approach.colour, step));
  return std::pair{sky, true};
}

void Portals::fill(std::uint32_t colour) {
  const std::uint32_t command = (colour & kColourBits) | spyro::guest_packet::kF3Code;
  std::uint32_t queued = kPieces;
  std::uint32_t cursor = memory_.mem_r32(kRenderGlobals.primitiveCursor);
  const std::uint32_t slot =
      memory_.mem_r32(kRenderGlobals.orderingTable) + (memory_.mem_r32(kBin) << 3);
  const auto screenCorner = [this](std::uint32_t at) {
    return Corner{.point = memory_.loadVertex(at)};
  };
  for (std::uint32_t triangle = 0; triangle < 2; ++triangle) {
    const std::uint32_t corners = kScreenTriangles + triangle * 12;
    clip({spanned(screenCorner(corners)),
          spanned(screenCorner(corners + 4)),
          spanned(screenCorner(corners + 8))},
         kEdges,
         queued,
         cursor,
         command,
         slot);
  }
  for (std::uint32_t piece = kPieces; piece != queued; piece += kPieceBytes) {
    clip({screenCorner(piece), screenCorner(piece + 4), screenCorner(piece + 8)},
         memory_.mem_r32(piece + 12),
         queued,
         cursor,
         command,
         slot);
  }
  memory_.mem_w32(kRenderGlobals.primitiveCursor, cursor);
}

// 0x8004FF90: the triangle cut by each edge in turn; a corner past an edge is moved onto it, and
// when one corner was cut off the quad left over queues its second half to go on from the next
// edge.
void Portals::clip(std::array<Corner, 3> p,
                   std::uint32_t at,
                   std::uint32_t &queued,
                   std::uint32_t &cursor,
                   std::uint32_t command,
                   std::uint32_t slot) {
  const auto queue = [&](const Corner &a, const Corner &b, const Corner &c, std::uint32_t next) {
    memory_.storeVertex(queued, a.point);
    memory_.storeVertex(queued + 4, b.point);
    memory_.storeVertex(queued + 8, c.point);
    memory_.mem_w32(queued + 12, next);
    queued += kPieceBytes;
  };
  // The fill's crossings carry no colour.
  const auto cross = [&](const Corner &inside, const Corner &outside) {
    return Corner{.point = crossing({.point = inside.point}, {.point = outside.point}, at).point};
  };
  for (;; at += kEdgeBytes) {
    if (memory_.mem_r32(at + edge::kFlag) == 0) {
      break;
    }
    const std::uint32_t next = at + kEdgeBytes;
    switch (outsideCorners({p[0].point.word(), p[1].point.word(), p[2].point.word()}, at)) {
    case 0:
      continue;
    case kFirstOut: {
      const Corner a = cross(p[1], p[0]);
      const Corner b = cross(p[2], p[0]);
      queue(a, p[2], b, next);
      p[0] = a;
      continue;
    }
    case kSecondOut: {
      const Corner a = cross(p[0], p[1]);
      const Corner b = cross(p[2], p[1]);
      queue(a, b, p[2], next);
      p[1] = a;
      continue;
    }
    case kFirstOut | kSecondOut: {
      const Corner a = cross(p[2], p[0]);
      p[1] = cross(p[2], p[1]);
      p[0] = a;
      continue;
    }
    case kThirdOut: {
      const Corner a = cross(p[0], p[2]);
      const Corner b = cross(p[1], p[2]);
      queue(a, p[1], b, next);
      p[2] = a;
      continue;
    }
    case kFirstOut | kThirdOut: {
      const Corner a = cross(p[1], p[0]);
      p[2] = cross(p[1], p[2]);
      p[0] = a;
      continue;
    }
    case kSecondOut | kThirdOut: {
      const Corner a = cross(p[0], p[1]);
      p[2] = cross(p[0], p[2]);
      p[1] = a;
      continue;
    }
    default:
      return;
    }
  }
  memory_.mem_w32(cursor, spyro::guest_packet::kF3Tag);
  memory_.mem_w32(cursor + 0x04, command);
  memory_.storeVertex(cursor + 0x08, p[0].point);
  memory_.storeVertex(cursor + 0x0C, p[1].point);
  memory_.storeVertex(cursor + 0x10, p[2].point);
  spyro::depth_bins::linkIntoBin(memory_, slot, cursor, cursor);
  cursor += spyro::guest_packet::kF3Bytes;
}

// 0x8004F7E8's cases: the corners cut off are moved onto the edge, and when one was, the quad left
// over is split from the new corner on the far side and queued to go on from the next edge.
void Portals::cutShaded(std::array<Corner, 3> q, std::uint32_t at, sky::Cyclorama &sky) {
  const auto queue = [this, &at](Corner a, Corner b, Corner c) {
    const std::uint32_t piece = memory_.mem_r32(kPieceQueue);
    memory_.storeVertex(piece + 0x00, a.point);
    memory_.storeVertex(piece + 0x04, b.point);
    memory_.storeVertex(piece + 0x08, c.point);
    memory_.mem_w32(piece + 0x0C, a.colour);
    memory_.mem_w32(piece + 0x10, b.colour);
    memory_.mem_w32(piece + 0x14, c.colour);
    memory_.mem_w32(piece + 0x18, at);
    memory_.mem_w32(kPieceQueue, piece + kShadedPieceBytes);
  };
  for (;; at += kEdgeBytes) {
    if (memory_.mem_r32(at + edge::kFlag) == 0) {
      break;
    }
    switch (outsideCorners({q[0].point.word(), q[1].point.word(), q[2].point.word()}, at)) {
    case 0:
      continue;
    case kFirstOut: {
      const Corner a = crossing(q[1], q[0], at);
      const Corner b = crossing(q[2], q[0], at);
      queue(b, a, q[2]);
      q[0] = a;
      continue;
    }
    case kSecondOut: {
      const Corner a = crossing(q[2], q[1], at);
      const Corner b = crossing(q[0], q[1], at);
      queue(b, a, q[0]);
      q[1] = a;
      continue;
    }
    case kFirstOut | kSecondOut:
      q[0] = crossing(q[2], q[0], at);
      q[1] = crossing(q[2], q[1], at);
      continue;
    case kThirdOut: {
      const Corner a = crossing(q[0], q[2], at);
      const Corner b = crossing(q[1], q[2], at);
      queue(b, a, q[1]);
      q[2] = a;
      continue;
    }
    case kFirstOut | kThirdOut:
      q[0] = crossing(q[1], q[0], at);
      q[2] = crossing(q[1], q[2], at);
      continue;
    case kSecondOut | kThirdOut:
      q[1] = crossing(q[0], q[1], at);
      q[2] = crossing(q[0], q[2], at);
      continue;
    default:
      return;
    }
  }
  const sky::SkyTriangle drawn{.points = {q[0].point, q[1].point, q[2].point},
                               .colours = {q[0].colour, q[1].colour, q[2].colour}};
  sky.emit(drawn, q[0].colour == q[1].colour && q[0].colour == q[2].colour);
}

void Portals::cut(const sky::SkyTriangle &triangle, sky::Cyclorama &sky) {
  const auto &p = triangle.points;
  const auto &c = triangle.colours;
  cutShaded({Corner{p[0], c[0]}, Corner{p[1], c[1]}, Corner{p[2], c[2]}}, kEdges, sky);
}

// 8004FC98..8004FD0C: the queued pieces last first, each from the edge after the one that split it.
void Portals::finishSector(sky::Cyclorama &sky) {
  for (std::uint32_t piece = memory_.mem_r32(kPieceQueue); piece != kPieces;
       piece = memory_.mem_r32(kPieceQueue)) {
    piece -= kShadedPieceBytes;
    memory_.mem_w32(kPieceQueue, piece);
    const auto corner = [this, piece](std::uint32_t n) {
      return Corner{memory_.loadVertex(piece + n * 4), memory_.mem_r32(piece + 0x0C + n * 4)};
    };
    cutShaded({corner(0), corner(1), corner(2)}, memory_.mem_r32(piece + 0x18) + kEdgeBytes, sky);
  }
}

std::uint32_t Portals::outsideCorners(const std::array<std::uint32_t, 3> &points,
                                      std::uint32_t at) {
  auto &gte = core_.gte();
  gte.writeData(gte::kSxy0, memory_.mem_r32(at + edge::kFrom));
  gte.writeData(gte::kSxy1, memory_.mem_r32(at + edge::kTo));
  std::uint32_t outside = 0;
  for (std::uint32_t corner = 0; corner < 3; ++corner) {
    gte.writeData(gte::kSxy2, points[corner]);
    gte.execute(gte::kNclip);
    if (asSigned(gte.readData(gte::kMac0)) <= 0) {
      outside |= kFirstOut << corner;
    }
  }
  return outside;
}

// The crossing's fraction is -((A x0 + B y0 + C) << 7 + 0x40) / (A dx + B dy), widened to 12 bits;
// GPL moves the inside point that far toward the outside one and DPCS its colour toward the
// outside's.
Portals::Corner Portals::crossing(Corner inside, Corner outside, std::uint32_t at) {
  auto &gte = core_.gte();
  gte.writeData(gte::kRgbc, inside.colour);
  spyro::guest_terrain::loadFarColour(gte, outside.colour);
  const std::uint32_t x0 = signedHalf(inside.point.word());
  const std::uint32_t y0 = sra(inside.point.word(), 16);
  const std::uint32_t dx = signedHalf(outside.point.word()) - x0;
  const std::uint32_t dy = sra(outside.point.word(), 16) - y0;
  const std::uint32_t a = memory_.mem_r32(at + edge::kA);
  const std::uint32_t b = memory_.mem_r32(at + edge::kB);
  const std::uint32_t c = memory_.mem_r32(at + edge::kC);
  gte.writeData(gte::kMac1, x0);
  gte.writeData(gte::kMac2, y0);
  gte.writeData(gte::kIr1, dx);
  gte.writeData(gte::kIr2, dy);
  const std::uint32_t along = a * dx + b * dy;
  if (along == 0) {
    // 8004FE98: retail returns with its scratch registers as they stand.
    return Corner{.point = {x0, inside.point.place()}, .colour = y0 | dy};
  }
  const std::uint32_t side = ((a * x0 + c + b * y0) << kCrossingShift) + kCrossingRound;
  gte.writeData(gte::kIr0,
                ((0u - psx::cpu::quotient({side, along})) << kFractionShift) + kFractionRound);
  gte.execute(gte::kGplSf);
  const std::uint32_t point =
      (gte.readData(gte::kMac2) << 16) | (gte.readData(gte::kMac1) & 0xFFFFu);
  gte.execute(gte::kDpcs);
  return Corner{.point = {point, crossingPlace(inside.point.place(), outside.point.place(), at)},
                .colour = spyro::guest_packet::kG3Code | gte.readData(gte::kRgb2)};
}

GtePlace Portals::crossingPlace(GtePlace inside, GtePlace outside, std::uint32_t at) const {
  const GtePlace from = memory_.loadVertex(at + edge::kFrom).place();
  const GtePlace to = memory_.loadVertex(at + edge::kTo).place();
  const float a = from.y - to.y;
  const float b = to.x - from.x;
  const float c = -a * from.x - b * from.y;
  const float along = a * (outside.x - inside.x) + b * (outside.y - inside.y);
  if (along == 0.0f) {
    return inside;
  }
  const float fraction = std::clamp(-(a * inside.x + b * inside.y + c) / along, 0.0f, 1.0f);
  return {inside.x + (outside.x - inside.x) * fraction,
          inside.y + (outside.y - inside.y) * fraction};
}

SkySpin advanceSkySpin(Core &core) {
  const std::uint32_t yaw = (core.mem_r32(kSkyYaw) + kSpinStep) & kAngleMask;
  core.mem_w32(kSkyYaw, yaw);
  std::uint32_t pitch = (core.mem_r32(kSkyPitch) + kSpinStep) & kAngleMask;
  if (asSigned(pitch) > kPitchWrap) {
    pitch -= kTurn;
  }
  if (asSigned(pitch) > kPitchLimit) {
    pitch = asWord(kPitchLimit);
  }
  core.mem_w32(kSkyPitch, pitch);
  return SkySpin{.yaw = yaw, .pitch = pitch};
}

// The camera builder's rotations with the pitch taken off the second angle and the yaw added to the
// third; the drawer's own copy of 0x80033C50.
sky::Rotations
nearSkyRotations(Core &core, const spyro::guest_camera::Angles &camera, SkySpin spin) {
  const spyro::guest_camera::Matrices matrices =
      terrain::kTerrainImage.camera(core,
                                    {.aroundX = camera.aroundX,
                                     .aroundY = asSigned(asWord(camera.aroundY) - spin.pitch),
                                     .aroundZ = asSigned(asWord(camera.aroundZ) + spin.yaw)});
  return sky::Rotations{.view = matrices.view, .projection = matrices.projection};
}

} // namespace spyro1::portals
