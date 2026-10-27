#include "spyro1/render/actor_emitter.h"

#include "core.h"
#include "gte_registers.h"
#include "render/terrain/guest_terrain_fog.h"
#include "spyro1/render/actor_emitter_layout.h"

namespace spyro1::actors {

using namespace emitter;

ActorEmitter::ActorEmitter(Core &core,
                           const psx::present::EmitMemory &memory,
                           const spyro::guest_terrain::ScreenBounds &bounds)
    : core_(core), memory_(memory), bounds_(bounds), bins_(memory) {}

// 8001F7D0..8001F7F8
void ActorEmitter::begin(std::uint32_t v1) {
  v1_ = v1;
  cursor_ = memory_.mem_r32(kRenderGlobals.primitiveCursor);
  core_.gte().writeControl(gte::kZsf3, memory_.mem_r32(spyro1::guest::kSkipLowPolyWorld));
}

// 8001F7FC..800208A8
bool ActorEmitter::draw(std::uint32_t entry) {
  entry_ = entry;
  flags_ = memory_.mem_r32(entry + entry::kFlags);
  frame_ = memory_.mem_r32(entry + entry::kFrame);
  if (!place()) {
    return false;
  }
  patchNear();
  const std::uint32_t sign = core_.gte().readControl(gte::kZsf4) >> 31;
  const std::uint32_t count = memory_.mem_r8(frame_ + frame::kVertexCounts + sign);
  outside_ = 0xFFFFFFFFu;
  const std::uint32_t first = memory_.mem_r32(words_);
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
  memory_.mem_w32(kRenderGlobals.primitiveCursor, cursor_);
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
  const std::uint32_t viewZ = memory_.mem_r32(entry_ + entry::kView + 8);
  const std::uint32_t viewY = memory_.mem_r32(entry_ + entry::kView + 4);
  const std::uint32_t viewX = memory_.mem_r32(entry_ + entry::kView);
  std::uint32_t depthBin = sra(viewZ, 5) - zStep;
  if (asSigned(depthBin) < 0) {
    depthBin = 0;
  }
  const std::uint32_t positionShift = memory_.mem_r8(frame_ + frame::kPositionShift);
  if (asSigned(depthBin - 0x110u) >= 0) {
    depthBin += 0x20;
  }
  const std::uint32_t binShift = (flags_ >> 24) & 0x1Fu;
  std::uint32_t x = sra(viewX << 2, positionShift & 31u);
  std::uint32_t y = sra(viewY << 2, positionShift & 31u);
  std::uint32_t z = sra(viewZ << 2, positionShift & 31u);
  const std::uint32_t scale = flags_ & 0xFFu;
  halfShift_ = memory_.mem_r8(frame_ + frame::kHalfShift);
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
  gte.loadRotation(spyro::guest_memory::rotationAt(memory_, entry_ + entry::kRotation));
  gte.writeControl(gte::kZsf4, sra(memory_.mem_r32(entry_ + entry::kRotation33), 16));
  return true;
}

namespace {

// The near detail runs from `patch` (8001F910): each a header word (the target's offset from the
// near polygon list in its low half, the word count in bits 24..30, another run while positive) and
// then its words.
template <typename Visit>
void forEachNearRun(const psx::present::EmitMemory &memory, std::uint32_t patch, Visit visit) {
  std::uint32_t run = 0;
  do {
    run = memory.mem_r32(patch);
    const std::uint32_t words = (run >> 24) & 0x7Fu;
    visit(run & 0xFFFFu, words, patch + 4);
    patch += 4 + words * 4;
  } while (asSigned(run) > 0);
}

// The first run of the frame's near detail, or 0 when it has none.
std::uint32_t firstNearRun(const psx::present::EmitMemory &memory, std::uint32_t steps) {
  const std::uint32_t patch = (memory.mem_r32(steps + 4) & 0xFFFFu) << 2;
  return patch == 0 ? 0 : patch + steps;
}

} // namespace

std::uint32_t nearPatchSaveBytes(const psx::present::EmitMemory &memory, std::uint32_t steps) {
  std::uint32_t bytes = 4;
  if (const std::uint32_t patch = firstNearRun(memory, steps); patch != 0) {
    forEachNearRun(memory, patch, [&](std::uint32_t, std::uint32_t words, std::uint32_t) {
      bytes += 4 + words * 4;
    });
  }
  return bytes;
}

// 8001F910..8001F9D0: inside the near range the frame's patch list swaps detail words into its near
// polygon list, each run's old words kept at kPatchSaves; then the vertex streams.
void ActorEmitter::patchNear() {
  steps_ = memory_.mem_r32(entry_ + entry::kSteps);
  blendSteps_ = memory_.mem_r32(entry_ + entry::kBlendSteps);
  stepsWord_ = memory_.mem_r32(steps_ + 4);
  const std::uint32_t nearness = sra(memory_.mem_r32(entry_ + entry::kRotation33), 16);
  std::uint32_t saved = kPatchSaves;
  const std::uint32_t patch = firstNearRun(memory_, steps_);
  if (asSigned(nearness) > 0 && patch != 0) {
    const std::uint32_t polygons = memory_.mem_r32(frame_ + frame::kNearPolygons);
    forEachNearRun(
        memory_, patch, [&](std::uint32_t offset, std::uint32_t words, std::uint32_t source) {
          std::uint32_t target = offset + polygons;
          memory_.mem_w32(saved, (target << 8) + words);
          saved += 4;
          for (; words != 0; --words) {
            memory_.mem_w32(saved, memory_.mem_r32(target));
            saved += 4;
            memory_.mem_w32(target, memory_.mem_r32(source));
            target += 4;
            source += 4;
          }
        });
  }
  memory_.mem_w32(saved, 0);
  const std::uint32_t words = streamBase(memory_.mem_r32(steps_));
  halves_ = ((stepsWord_ >> 24) << 2) + words;
  words_ = words;
  if (blendSteps_ != 0) {
    const std::uint32_t partnerWord = memory_.mem_r32(blendSteps_ + 4);
    partnerWords_ = streamBase(memory_.mem_r32(blendSteps_));
    partnerHalves_ = ((partnerWord >> 24) << 2) + partnerWords_;
  }
}

// 80020860..800208A4: every swapped run's words put back.
void ActorEmitter::restorePatches() {
  std::uint32_t saved = kPatchSaves;
  for (;;) {
    const std::uint32_t run = memory_.mem_r32(saved);
    saved += 4;
    v1_ = run & 0xFFu;
    if (run == 0) {
      return;
    }
    std::uint32_t target = (run >> 8) | 0x80000000u;
    for (std::uint32_t words = v1_; words != 0; --words) {
      memory_.mem_w32(target, memory_.mem_r32(saved));
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
      polygon_ = memory_.mem_r32(frame_ + frame::kNearPolygons);
      colours_ = memory_.mem_r32(frame_ + frame::kNearColours);
      fogClut_ = 0;
      return;
    }
    const std::uint32_t weight = 0u - (inside << 2);
    gte.writeData(gte::kIr0, weight);
    fogClut_ = (weight >> 9) << 22;
    fadeToFar();
    polygon_ = memory_.mem_r32(frame_ + frame::kNearPolygons);
    colours_ = kFadedColours;
    return;
  }
  const std::uint32_t fogOn = gte.readControl(gte::kZsf3);
  polygon_ = memory_.mem_r32(frame_ + frame::kFarPolygons);
  colours_ = memory_.mem_r32(frame_ + frame::kFarColours);
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

// 8001FDF0..8001FE80: each near colour toward its far colour by IR0.
void ActorEmitter::fadeToFar() {
  auto &gte = core_.gte();
  std::uint32_t from = memory_.mem_r32(frame_ + frame::kNearColours);
  std::uint32_t to = memory_.mem_r32(frame_ + frame::kFarColours);
  std::uint32_t out = kFadedColours;
  std::uint32_t far = memory_.mem_r32(to);
  const std::uint32_t end =
      to + (std::uint32_t{memory_.mem_r16(frame_ + frame::kColourCount)} << 2);
  std::uint32_t near = memory_.mem_r32(from);
  do {
    gte.writeData(gte::kRgbc, near);
    spyro::guest_terrain::loadFarColour(gte, far);
    far = memory_.mem_r32(to + 4);
    from += 4;
    to += 4;
    gte.execute(gte::kDpcs);
    near = memory_.mem_r32(from);
    memory_.mem_w32(out, gte.readData(gte::kRgb2));
    out += 4;
  } while (to != end);
}

// 8001FED4..8001FF44: each far colour toward the fog colour by IR0.
void ActorEmitter::fadeToFog() {
  auto &gte = core_.gte();
  std::uint32_t from = memory_.mem_r32(frame_ + frame::kFarColours);
  std::uint32_t out = kFadedColours;
  spyro::guest_terrain::loadFarColour(gte, memory_.mem_r32(kFogColour));
  const std::uint32_t end =
      from + (std::uint32_t{memory_.mem_r16(frame_ + frame::kColourCount)} << 2);
  gte.writeData(gte::kRgbc, memory_.mem_r32(from));
  do {
    gte.execute(gte::kDpcs);
    const std::uint32_t next = memory_.mem_r32(from + 4);
    from += 4;
    const std::uint32_t colour = gte.readData(gte::kRgb2);
    gte.writeData(gte::kRgbc, next);
    memory_.mem_w32(out, colour);
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
  polygonEnd_ = memory_.mem_r32(polygon_);
  std::uint32_t offset = 0;
  if (asSigned(depthBase_) <= 0) {
    offset = ((0u - depthBase_) >> (binShift_ & 31u)) << 3;
    depthBase_ = 1;
  }
  binsBase_ = kLocalBins + offset;
  bins_.begin({.base = binsBase_, .cursor = cursor_});
  word_ = memory_.mem_r32(polygon_ + 4);
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
               .table = memory_.mem_r32(kRenderGlobals.orderingTable)});
  cursor_ = bins_.cursor();
  return true;
}

} // namespace spyro1::actors
