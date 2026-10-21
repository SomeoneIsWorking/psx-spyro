#include "spyro1/render/flame.h"

#include "core.h"
#include "gte_registers.h"
#include "render/field/guest_render_globals.h"
#include "render/field/guest_word.h"
#include "render/terrain/guest_depth_bins.h"
#include "spyro1/core/effect/flame_block.h"
#include "spyro1/render/terrain_facts.h"
#include "state_producer.h"

namespace spyro1::flame {
namespace {

namespace gte = psx::gte;
using spyro::guest_word::asSigned;
using spyro::guest_word::signedHalf;
using spyro::guest_word::sra;
using terrain::kRenderGlobals;

constexpr std::uint32_t kNearStride = 0xC0u;
constexpr std::uint32_t kFarStride = 0x100u;
constexpr std::uint32_t kFarBase = 0x300u;
constexpr std::uint32_t kFarConfig = 0x80u;
constexpr std::uint32_t kEntryBytes = 0x10u;

constexpr std::uint32_t kTipTag = 0x06000000u;
constexpr std::uint32_t kTipBytes = 0x1Cu;
constexpr std::uint32_t kRibbonTag = 0x0C000000u;
constexpr std::uint32_t kRibbonBytes = 0x34u;
constexpr std::uint32_t kRibbonCommand = 0x3E000000u;
constexpr std::uint32_t kTipRing = 0x2Cu;
constexpr std::uint32_t kSuperRing = 0x40u;
constexpr std::uint32_t kNarrowRing = 8u;

std::uint32_t packXy(std::uint32_t x, std::uint32_t y) {
  return (x & 0xFFFFu) | (y << 16);
}

} // namespace

FlameTrails::FlameTrails(Core &core,
                         const psx::present::EmitMemory &memory,
                         psx::present::PrimitiveSink *places)
    : core_(core), memory_(memory), places_(places) {}

void FlameTrails::draw() {
  table_ = memory_.mem_r32(kRenderGlobals.orderingTable);
  cursor_ = memory_.mem_r32(kRenderGlobals.primitiveCursor);
  placeAnchor();
  superflame_ = memory_.mem_r32(flame_block::kSuper) != 0;
  for (std::uint32_t part = flame_block::kParts; part-- > 0;) {
    drawPart(part);
  }
  memory_.mem_w32(kRenderGlobals.primitiveCursor, cursor_);
}

void FlameTrails::end() {
  core_.r[2] = v0_;
  core_.r[3] = v1_;
}

void FlameTrails::placeAnchor() {
  auto &gte = core_.gte();
  const std::uint32_t position = kRenderGlobals.cameraPosition;
  const std::uint32_t x = memory_.mem_r32(flame_block::kPosition) - memory_.mem_r32(position);
  const std::uint32_t y =
      memory_.mem_r32(position + 4) - memory_.mem_r32(flame_block::kPosition + 4);
  const std::uint32_t z =
      memory_.mem_r32(position + 8) - memory_.mem_r32(flame_block::kPosition + 8);
  gte.loadRotation(spyro::guest_memory::rotationAt(memory_, kRenderGlobals.cameraRotation));
  gte.loadTranslation({});
  gte.writeData(gte::kIr3, x);
  gte.writeData(gte::kIr1, y);
  gte.writeData(gte::kIr2, z);
  gte.execute(gte::kMvmvaRtIr);
  gte.writeControl(gte::kTranslationZ, gte.readData(gte::kMac3));
  gte.writeControl(gte::kTranslationY, gte.readData(gte::kMac2));
  gte.writeControl(gte::kTranslationX, gte.readData(gte::kMac1));
  v0_ = gte.readData(gte::kMac2);
  v1_ = gte.readData(gte::kMac3);
  gte.loadRotation(spyro::guest_memory::rotationAt(memory_, flame_block::kRotation));
}

void FlameTrails::drawPart(std::uint32_t part) {
  const std::int32_t length = memory_.mem_r8s(flame_block::kLengths + part);
  const std::int32_t last = memory_.mem_r8s(flame_block::kLimits + part);
  if (length <= 0) {
    return;
  }
  std::uint32_t config = kAgeConfig + (static_cast<std::uint32_t>(length - 1) << 2);
  std::uint32_t stride = kNearStride;
  std::uint32_t base = part * kNearStride;
  if (part >= 4) {
    stride = kFarStride;
    base = (part - 4) * kFarStride + kFarBase;
    config += kFarConfig;
  }
  const std::uint32_t descriptor = memory_.mem_r32(config);
  const std::uint32_t path = kArcPaths + base;
  const std::uint32_t entry = path + (((descriptor >> 24) << 3) & 0x7F8u);
  const std::uint32_t limit = (static_cast<std::uint32_t>(last) << 3) + path;
  const bool tipped = asSigned(entry - limit) <= 0 && asSigned(path + stride - entry) > 0;
  if (tipped && !drawTip(entry)) {
    return;
  }
  drawRibbon({.entry = entry, .limit = limit, .descriptor = descriptor}, tipped);
}

bool FlameTrails::drawTip(std::uint32_t entry) {
  auto &gte = core_.gte();
  const std::uint32_t tip = memory_.mem_r32(entry);
  gte.writeData(gte::kVz0, signedHalf(tip));
  gte.writeData(gte::kVxy0,
                ((0u - sra(tip, 16)) & 0xFFFFu) | (0u - (memory_.mem_r32(entry + 4) << 16)));
  const std::uint32_t point = memory_.mem_r32(entry - kEntryBytes);
  const std::uint32_t angle = memory_.mem_r32(entry - kEntryBytes + 4);
  gte.execute(gte::kRtps);
  const std::uint32_t z = signedHalf(point);
  const std::uint32_t x = sra(point, 16);
  const std::uint32_t y = signedHalf(angle);
  const std::uint32_t ring = superflame_ ? kSuperRing : kTipRing;
  const std::uint32_t sine = kRenderGlobals.cameraSineTable + ((angle >> 15) & 0x1FEu);
  const std::uint32_t cosine = sra(ring * static_cast<std::uint32_t>(memory_.mem_r16s(
                                              sine + spyro::guest_render_globals::kCosineOffset)),
                                   11);
  const std::uint32_t sineStep = sra(ring * static_cast<std::uint32_t>(memory_.mem_r16s(sine)), 11);
  const std::uint32_t tipDepth = gte.readData(gte::kSz3);
  const std::uint32_t tipScreen = gte.readData(gte::kSxy2);
  const GtePlace tipPlace = gte.projectedPlace(2);
  const std::uint32_t frontX = cosine - x;
  const std::uint32_t frontY = sineStep - y;
  // The third corner lies across the angle, half as far for a superflame.
  std::uint32_t crossX = sineStep;
  std::uint32_t crossY = 0u - cosine;
  if (superflame_) {
    crossX = sra(crossX, 1);
    crossY = sra(crossY, 1);
  }
  gte.writeData(gte::kVz0, z);
  gte.writeData(gte::kVz1, z);
  gte.writeData(gte::kVz2, z);
  gte.writeData(gte::kVxy0, packXy(crossX - x, crossY - y));
  gte.writeData(gte::kVxy1, packXy(frontX, frontY));
  gte.writeData(gte::kVxy2, packXy(frontX - (cosine << 1), frontY - (sineStep << 1)));
  gte.execute(gte::kRtpt);
  const std::uint32_t colours = kCapColours + (superflame_ ? 0x10u : 0u);
  const std::array<std::uint32_t, 4> colour = {memory_.mem_r32(colours),
                                               memory_.mem_r32(colours + 4),
                                               memory_.mem_r32(colours + 8),
                                               memory_.mem_r32(colours + 0xC)};
  const std::uint32_t bin = (tipDepth >> 7) - 2;
  if (asSigned(bin) < 0) {
    v0_ = colour[1];
    v1_ = colour[2];
    return false;
  }
  const std::uint32_t slot = (bin << 3) + table_;
  const std::array<std::uint32_t, 3> ring3 = {
      gte.readData(gte::kSxy0), gte.readData(gte::kSxy1), gte.readData(gte::kSxy2)};
  const std::array<GtePlace, 3> places = {
      gte.projectedPlace(0), gte.projectedPlace(1), gte.projectedPlace(2)};
  tipTriangle(slot,
              {tipScreen, ring3[1], ring3[0]},
              {tipPlace, places[1], places[0]},
              {colour[0], colour[2], colour[1]});
  tipTriangle(slot,
              {tipScreen, ring3[0], ring3[2]},
              {tipPlace, places[0], places[2]},
              {colour[0], colour[1], colour[3]});
  tipTriangle(slot,
              {tipScreen, ring3[2], ring3[1]},
              {tipPlace, places[2], places[1]},
              {colour[0], colour[3], colour[2]});
  tipTriangle(slot, {ring3[0], ring3[1], ring3[2]}, places, {colour[1], colour[2], colour[3]});
  return true;
}

void FlameTrails::tipTriangle(std::uint32_t slot,
                              const std::array<std::uint32_t, 3> &screens,
                              const std::array<GtePlace, 3> &places,
                              const std::array<std::uint32_t, 3> &colours) {
  auto &gte = core_.gte();
  gte.writeData(gte::kSxy0, screens[0]);
  gte.writeData(gte::kSxy1, screens[1]);
  gte.writeData(gte::kSxy2, screens[2]);
  gte.execute(gte::kNclip);
  for (std::uint32_t i = 0; i < 3; ++i) {
    memory_.mem_w32(cursor_ + 4 + 8 * i, colours[i]);
    memory_.mem_w32(cursor_ + 8 + 8 * i, screens[i]);
  }
  memory_.mem_w32(cursor_, kTipTag);
  if (asSigned(gte.readData(gte::kMac0)) <= 0) {
    return;
  }
  if (places_ != nullptr) {
    places_->placeCorners(cursor_, places);
  }
  spyro::depth_bins::linkIntoBin(memory_, slot, cursor_, cursor_);
  cursor_ += kTipBytes;
}

void FlameTrails::drawRibbon(const PartWalk &walk, bool tipped) {
  auto &gte = core_.gte();
  std::uint32_t entry = walk.entry - kEntryBytes;
  std::uint32_t ramp = (walk.descriptor >> 8) & 0xFFu;
  std::uint32_t steps = (walk.descriptor >> 16) & 0xFFu;
  const std::uint32_t over = entry - walk.limit;
  if (asSigned(over) > 0) {
    ramp += over >> 3;
    steps -= over >> 3;
    entry = walk.limit;
  }
  const std::uint32_t dark = ramp * 5;
  const std::uint32_t grey = 0x80u - dark;
  const std::uint32_t greys = grey | (grey << 8) | (grey << 16);
  std::uint32_t colour = kRibbonCommand | greys;
  const std::uint32_t texture = flame_block::kTexture + (superflame_ ? 8u : 0u);
  std::uint32_t uv0 = memory_.mem_r32(texture) + (dark << 8);
  std::uint32_t uv1 = memory_.mem_r32(texture + 4) + (dark << 8);
  std::uint32_t vStep = 0xA00u;
  std::uint32_t darken = 0x0A0A0Au;
  std::uint32_t wide = tipped ? 1u : 0u;
  // The first step only primes the pair; its packet words are written and never linked.
  std::uint32_t previous[2] = {0, greys};
  GtePlace previousPlace[2]{};
  while (true) {
    const std::uint32_t point = memory_.mem_r32(entry);
    const std::uint32_t angle = memory_.mem_r32(entry + 4);
    entry -= kEntryBytes;
    const std::uint32_t z = signedHalf(point);
    const std::uint32_t x = sra(point, 16);
    const std::uint32_t sine = kRenderGlobals.cameraSineTable + ((angle >> 15) & 0x1FEu);
    std::uint32_t ring = kNarrowRing;
    if (asSigned(steps) > 0) {
      wide += previous[0];
      if (wide != 0) {
        ring = superflame_ ? kSuperRing : kTipRing;
      }
    }
    const std::uint32_t y = signedHalf(angle);
    const std::uint32_t cosine = sra(ring * static_cast<std::uint32_t>(memory_.mem_r16s(
                                                sine + spyro::guest_render_globals::kCosineOffset)),
                                     11);
    const std::uint32_t sineStep =
        sra(ring * static_cast<std::uint32_t>(memory_.mem_r16s(sine)), 11);
    const std::uint32_t frontX = cosine - x;
    const std::uint32_t frontY = sineStep - y;
    gte.writeData(gte::kVz0, z);
    gte.writeData(gte::kVz1, z);
    gte.writeData(gte::kVxy0, packXy(frontX, frontY));
    gte.writeData(gte::kVxy1, packXy(frontX - (cosine << 1), frontY - (sineStep << 1)));
    gte.execute(gte::kRtpt);
    memory_.mem_w32(cursor_ + 0x08, previous[0]);
    memory_.mem_w32(cursor_ + 0x14, previous[1]);
    memory_.mem_w32(cursor_ + 0x0C, uv0);
    memory_.mem_w32(cursor_ + 0x18, uv1);
    memory_.mem_w32(cursor_ + 0x04, colour);
    memory_.mem_w32(cursor_ + 0x10, colour);
    memory_.mem_w32(cursor_, kRibbonTag);
    const std::uint32_t screens[2] = {gte.readData(gte::kSxy0), gte.readData(gte::kSxy1)};
    const GtePlace places[2] = {gte.projectedPlace(0), gte.projectedPlace(1)};
    const std::uint32_t depth = gte.readData(gte::kSz1) + gte.readData(gte::kSz2);
    if (previous[0] != 0) {
      memory_.mem_w32(cursor_ + 0x20, screens[0]);
      memory_.mem_w32(cursor_ + 0x2C, screens[1]);
      uv0 += vStep;
      memory_.mem_w32(cursor_ + 0x24, uv0);
      uv1 += vStep;
      memory_.mem_w32(cursor_ + 0x30, uv1);
      colour -= darken;
      memory_.mem_w32(cursor_ + 0x1C, colour);
      memory_.mem_w32(cursor_ + 0x28, colour);
      const std::uint32_t bin = (depth >> 8) - 2;
      if (asSigned(bin) >= 0) {
        if (places_ != nullptr) {
          places_->placeCorners(cursor_,
                                {{previousPlace[0], previousPlace[1], places[0], places[1]}});
        }
        spyro::depth_bins::linkIntoBin(memory_, (bin << 3) + table_, cursor_, cursor_);
        cursor_ += kRibbonBytes;
      }
    }
    previous[0] = screens[0];
    previous[1] = screens[1];
    previousPlace[0] = places[0];
    previousPlace[1] = places[1];
    // 80059298: two steps an entry; an odd last step closes half an entry at half the rate.
    if (asSigned(steps) <= 0) {
      v0_ = previous[1];
      v1_ = uv0;
      return;
    }
    steps -= 1;
    if (asSigned(steps) > 0) {
      steps -= 1;
      continue;
    }
    steps -= 1;
    entry += kEntryBytes / 2;
    vStep = 0x500u;
    darken = 0x050505u;
  }
}

} // namespace spyro1::flame
