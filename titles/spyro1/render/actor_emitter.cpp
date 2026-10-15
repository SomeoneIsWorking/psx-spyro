#include "spyro1/render/actor_emitter.h"

#include "core.h"
#include "gte_registers.h"
#include "spyro1/render/actor_emitter_layout.h"

namespace spyro1::actors {

using namespace emitter;

ActorEmitter::ActorEmitter(Core &core,
                           spyro::guest_memory::WorkingMemory &memory,
                           const spyro::guest_terrain::ScreenBounds &bounds)
    : core_(core), memory_(memory), bounds_(bounds), bins_(memory) {}

// 8001F7D0..8001F7F8
void ActorEmitter::begin(std::uint32_t v1) {
  v1_ = v1;
  cursor_ = memory_.r32(kRenderGlobals.primitiveCursor);
  core_.gte().writeControl(gte::kZsf3, memory_.r32(kFogSwitch));
}

// 8001F7FC..800208A8
bool ActorEmitter::draw(std::uint32_t entry) {
  entry_ = entry;
  flags_ = memory_.r32(entry + entry::kFlags);
  frame_ = memory_.r32(entry + entry::kFrame);
  if (!place()) {
    return false;
  }
  patchNear();
  const std::uint32_t sign = core_.gte().readControl(gte::kZsf4) >> 31;
  const std::uint32_t count = memory_.r8(frame_ + frame::kVertexCounts + sign);
  outside_ = 0xFFFFFFFFu;
  const std::uint32_t first = memory_.r32(words_);
  bool linked = false;
  if (count != 0 && project(count, first)) {
    fadeColours();
    drawPolygons();
    linked = mergeBins();
  }
  restorePatches();
  return linked;
}

// 800208AC
void ActorEmitter::end() {
  memory_.w32(kRenderGlobals.primitiveCursor, cursor_);
  core_.r[2] = cursor_;
  core_.r[3] = v1_;
}

// 8001F808..8001F908: the entry's centre into TR (scaled when the scale byte is set), its rotation
// into RT, and the depth words the later passes read back from the GTE: RBK the bin shift (the
// position shift in its second byte), GBK the table depth bin, BBK the near bin depth, ZSF4 the
// signed distance inside the near detail range.
bool ActorEmitter::place() {
  auto &gte = core_.gte();
  weight_ = (flags_ & 0xFF00u) >> 2;
  const std::uint32_t zStep = sra(flags_ << 8, 24);
  const std::uint32_t viewZ = memory_.r32(entry_ + entry::kView + 8);
  const std::uint32_t viewY = memory_.r32(entry_ + entry::kView + 4);
  const std::uint32_t viewX = memory_.r32(entry_ + entry::kView);
  std::uint32_t depthBin = sra(viewZ, 5) - zStep;
  if (asSigned(depthBin) < 0) {
    depthBin = 0;
  }
  const std::uint32_t positionShift = memory_.r8(frame_ + frame::kPositionShift);
  if (asSigned(depthBin - 0x110u) >= 0) {
    depthBin += 0x20;
  }
  const std::uint32_t binShift = (flags_ >> 24) & 0x1Fu;
  std::uint32_t x = sra(viewX << 2, positionShift & 31u);
  std::uint32_t y = sra(viewY << 2, positionShift & 31u);
  std::uint32_t z = sra(viewZ << 2, positionShift & 31u);
  const std::uint32_t scale = flags_ & 0xFFu;
  halfShift_ = memory_.r8(frame_ + frame::kHalfShift);
  if (scale != 0) {
    gte.writeData(gte::kIr0, scale);
    gte.writeData(gte::kIr1, x);
    gte.writeData(gte::kIr2, y);
    gte.writeData(gte::kIr3, z);
    gte.execute(gte::kGpf);
    z = sra(gte.readData(gte::kMac3), 5);
    y = sra(gte.readData(gte::kMac2), 5);
    if (asSigned(z - kFarthestScaled) > 0) {
      v1_ = z;
      return false;
    }
    x = sra(gte.readData(gte::kMac1), 5);
  }
  const std::uint32_t nearDepth = z - (0x200u << binShift);
  halfShift_ += 1;
  gte.writeControl(gte::kTranslationX, x);
  gte.writeControl(gte::kTranslationY, y);
  gte.writeControl(gte::kTranslationZ, z);
  gte.writeControl(gte::kBackRed, binShift + (positionShift << 8));
  gte.writeControl(gte::kBackGreen, depthBin);
  gte.writeControl(gte::kBackBlue, nearDepth);
  for (std::uint32_t i = 0; i < gte::kRotationWords; ++i) {
    gte.writeControl(gte::kRotation + i, memory_.r32(entry_ + entry::kRotation + 4 * i));
  }
  gte.writeControl(gte::kZsf4, sra(memory_.r32(entry_ + entry::kRotation33), 16));
  return true;
}

// 8001F910..8001F9D0: inside the near range the frame's patch list swaps detail words into its near
// polygon list, each run's old words kept at kPatchSaves; then the vertex streams.
void ActorEmitter::patchNear() {
  steps_ = memory_.r32(entry_ + entry::kSteps);
  blendSteps_ = memory_.r32(entry_ + entry::kBlendSteps);
  stepsWord_ = memory_.r32(steps_ + 4);
  const std::uint32_t nearness = sra(memory_.r32(entry_ + entry::kRotation33), 16);
  std::uint32_t saved = kPatchSaves;
  std::uint32_t patch = (stepsWord_ & 0xFFFFu) << 2;
  if (asSigned(nearness) > 0 && patch != 0) {
    patch += steps_;
    const std::uint32_t polygons = memory_.r32(frame_ + frame::kNearPolygons);
    std::uint32_t run = 0;
    do {
      run = memory_.r32(patch);
      patch += 4;
      std::uint32_t target = (run & 0xFFFFu) + polygons;
      std::uint32_t words = (run >> 24) & 0x7Fu;
      memory_.w32(saved, (target << 8) + words);
      saved += 4;
      for (; words != 0; --words) {
        memory_.w32(saved, memory_.r32(target));
        target += 4;
        saved += 4;
        const std::uint32_t word = memory_.r32(patch);
        patch += 4;
        memory_.w32(target - 4, word);
      }
    } while (asSigned(run) > 0);
  }
  memory_.w32(saved, 0);
  const std::uint32_t words = streamBase(memory_.r32(steps_));
  halves_ = ((stepsWord_ >> 24) << 2) + words;
  words_ = words;
  if (blendSteps_ != 0) {
    const std::uint32_t partnerWord = memory_.r32(blendSteps_ + 4);
    partnerWords_ = streamBase(memory_.r32(blendSteps_));
    partnerHalves_ = ((partnerWord >> 24) << 2) + partnerWords_;
  }
}

// 80020860..800208A4: every swapped run's words put back.
void ActorEmitter::restorePatches() {
  std::uint32_t saved = kPatchSaves;
  for (;;) {
    const std::uint32_t run = memory_.r32(saved);
    saved += 4;
    v1_ = run & 0xFFu;
    if (run == 0) {
      return;
    }
    std::uint32_t target = (run >> 8) | 0x80000000u;
    for (std::uint32_t words = v1_; words != 0; --words) {
      memory_.w32(target, memory_.r32(saved));
      saved += 4;
      target += 4;
    }
  }
}

// 8001FDD0..8001FF60: inside the near range the near colours fade toward the far ones as the record
// nears the range's edge; outside it, with the fog switch on, the far colours fade toward the fog
// colour with distance. A faded set is written to kFadedColours.
void ActorEmitter::fadeColours() {
  auto &gte = core_.gte();
  const std::uint32_t nearness = gte.readControl(gte::kZsf4);
  const std::uint32_t inside = nearness - 0x400u;
  if (asSigned(nearness) > 0) {
    if (asSigned(inside) >= 0) {
      polygon_ = memory_.r32(frame_ + frame::kNearPolygons);
      colours_ = memory_.r32(frame_ + frame::kNearColours);
      fogClut_ = 0;
      return;
    }
    const std::uint32_t weight = 0u - (inside << 2);
    gte.writeData(gte::kIr0, weight);
    fogClut_ = (weight >> 9) << 22;
    fadeToFar();
    polygon_ = memory_.r32(frame_ + frame::kNearPolygons);
    colours_ = kFadedColours;
    return;
  }
  const std::uint32_t fogOn = gte.readControl(gte::kZsf3);
  polygon_ = memory_.r32(frame_ + frame::kFarPolygons);
  colours_ = memory_.r32(frame_ + frame::kFarColours);
  fogClut_ = 0;
  if (asSigned(fogOn) <= 0) {
    return;
  }
  std::uint32_t distance = inside + 0xC00u;
  if (asSigned(distance) >= 0) {
    return;
  }
  const std::uint32_t full = distance + 0x800u;
  distance = 0u - distance;
  if (asSigned(full) <= 0) {
    distance = 0x7FFu;
  }
  fogClut_ = distance << 1;
  gte.writeData(gte::kIr0, fogClut_);
  fadeToFog();
  colours_ = kFadedColours;
}

namespace {

void writeFarColour(psx::cpu::Gte &gte, std::uint32_t colour) {
  gte.writeControl(gte::kFarRed, (colour << 4) & 0xFF0u);
  gte.writeControl(gte::kFarGreen, (colour >> 4) & 0xFF0u);
  gte.writeControl(gte::kFarBlue, (colour >> 12) & 0xFF0u);
}

} // namespace

// 8001FDF0..8001FE80: each near colour toward its far colour by IR0.
void ActorEmitter::fadeToFar() {
  auto &gte = core_.gte();
  std::uint32_t from = memory_.r32(frame_ + frame::kNearColours);
  std::uint32_t to = memory_.r32(frame_ + frame::kFarColours);
  std::uint32_t out = kFadedColours;
  std::uint32_t far = memory_.r32(to);
  const std::uint32_t end = to + (std::uint32_t{memory_.r16(frame_ + frame::kColourCount)} << 2);
  std::uint32_t near = memory_.r32(from);
  do {
    gte.writeData(gte::kRgbc, near);
    writeFarColour(gte, far);
    far = memory_.r32(to + 4);
    from += 4;
    to += 4;
    gte.execute(gte::kDpcs);
    near = memory_.r32(from);
    memory_.w32(out, gte.readData(gte::kRgb2));
    out += 4;
  } while (to != end);
}

// 8001FED4..8001FF44: each far colour toward the fog colour by IR0.
void ActorEmitter::fadeToFog() {
  auto &gte = core_.gte();
  std::uint32_t from = memory_.r32(frame_ + frame::kFarColours);
  std::uint32_t out = kFadedColours;
  writeFarColour(gte, memory_.r32(kFogColour));
  const std::uint32_t end = from + (std::uint32_t{memory_.r16(frame_ + frame::kColourCount)} << 2);
  gte.writeData(gte::kRgbc, memory_.r32(from));
  do {
    gte.execute(gte::kDpcs);
    const std::uint32_t next = memory_.r32(from + 4);
    from += 4;
    const std::uint32_t colour = gte.readData(gte::kRgb2);
    gte.writeData(gte::kRgbc, next);
    memory_.w32(out, colour);
    out += 4;
  } while (from != end);
}

// 8001FF64..8001FFF4: the bins start far enough back that the record's nearest depth lands in bin
// 0; the polygon list's byte length, then its polygons.
void ActorEmitter::beginPolygons(std::uint32_t depths) {
  auto &gte = core_.gte();
  depths_ = depths;
  const std::uint32_t edge = (flags_ >> 31) << 31;
  depthBase_ = gte.readControl(gte::kBackBlue) << 2;
  binShift_ = (gte.readControl(gte::kBackRed) & 0xFFu) + 4 + edge;
  polygonEnd_ = memory_.r32(polygon_);
  std::uint32_t offset = 0;
  if (asSigned(depthBase_) <= 0) {
    offset = ((0u - depthBase_) >> (binShift_ & 31u)) << 3;
    depthBase_ = 1;
  }
  binsBase_ = kLocalBins + offset;
  bins_.begin({.base = binsBase_, .cursor = cursor_});
  word_ = memory_.r32(polygon_ + 4);
  polygon_ += 4;
  polygonEnd_ += polygon_;
}

// 8002074C..8002085C
bool ActorEmitter::mergeBins() {
  if (bins_.empty()) {
    return false;
  }
  auto &gte = core_.gte();
  const std::uint32_t depthBin = gte.readControl(gte::kBackGreen);
  const std::uint32_t shifts = gte.readControl(gte::kBackRed);
  bins_.merge({.shift = (shifts & 0xFFu) + (shifts >> 8),
               .depthBin = depthBin,
               .table = memory_.r32(kRenderGlobals.orderingTable)});
  cursor_ = bins_.cursor();
  return true;
}

} // namespace spyro1::actors
