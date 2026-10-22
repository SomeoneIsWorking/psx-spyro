#include "spyro1/render/paired_actor.h"

#include "core.h"
#include "gte_registers.h"
#include "render/field/guest_render_globals.h"
#include "spyro1/core/effect/flame_block.h"
#include "spyro1/render/paired_actor_layout.h"

namespace spyro1::actors {

using namespace paired;

namespace {

struct Frame {
  std::uint32_t header = 0;
  std::uint32_t word = 0;
  std::uint32_t base = 0;
};

// The animation and frame record bytes at `slot` (2 * part, plus 1 for the partner).
Frame frameOf(const psx::present::EmitMemory &memory, std::uint32_t slot) {
  Frame frame;
  const std::uint32_t animation = memory.mem_r8(kSpyroRecord + spyro_record::kAnimations + slot);
  frame.header = memory.mem_r32(memory.mem_r32(kSpyroModel) + kAnimationTable + (animation << 2));
  const std::uint32_t index = memory.mem_r8(kSpyroRecord + spyro_record::kFrames + slot);
  frame.word = memory.mem_r32(frame.header + kFrameTable + (index << 2));
  frame.base = (frame.word << 11) >> 10;
  return frame;
}

// 80023B78..80023B98 and its part 1/2 forms: where a part's three streams start.
std::array<std::uint32_t, 3> streamsOf(const psx::present::EmitMemory &memory,
                                       std::uint32_t part,
                                       const Frame &frame) {
  const std::uint32_t first = frame.base + kFrameStreams;
  const std::uint32_t offsets = memory.mem_r32(frame.base + kFrameByteOffsets);
  std::uint32_t words = memory.mem_r32(frame.header + kAnimationWords);
  if (part == 0) {
    return {words, (offsets >> 20) + first, first};
  }
  words += static_cast<std::uint32_t>(memory.mem_r8(frame.header + kVertexEnds + part - 1)) << 2;
  const std::uint32_t halves = memory.mem_r32(frame.base + kFrameHalfOffsets);
  if (part == 1) {
    return {words, ((offsets >> 10) & 0x3FFu) + first, (halves >> 16) + first};
  }
  return {words, (offsets & 0x3FFu) + first, (halves & 0xFFFFu) + first};
}

std::array<std::uint32_t, 3> unpackOffset(std::uint32_t word) {
  return {sra(word, 21), sra(word << 11, 21), sra(word << 22, 21)};
}

} // namespace

PairedActor::PairedActor(Core &core, const psx::present::EmitMemory &memory)
    : core_(core), memory_(memory), bins_(memory) {}

bool PairedActor::draw() {
  stageParts();
  const Offsets offsets = blendOffsets();
  if (!placeCentre()) {
    return false;
  }
  orientParts(offsets);
  projectParts();
  if (waterLevel_ != 0) {
    measureHeights();
  }
  tintColours();
  beginPolygons();
  if (waterLevel_ == 0) {
    drawWhole();
  } else {
    drawCut();
  }
  return mergeBins();
}

void PairedActor::end() {
  core_.r[2] = v0_;
  core_.r[3] = v1_;
}

// 80023AFC..80023E08: each part's flags word (format bits, weight, vertex end) and streams, its
// partner's streams when it blends.
void PairedActor::stageParts() {
  model_ = memory_.mem_r32(kSpyroModel);
  depthShift_ = 0;
  partnerDepthShift_ = 0;
  for (std::uint32_t part = 0; part < kParts; ++part) {
    const std::uint32_t entry = kPartTable + part * kPartBytes;
    const std::uint32_t blend =
        static_cast<std::uint32_t>(memory_.mem_r8(kSpyroRecord + spyro_record::kBlends + part)) << 8;
    const Frame frame = frameOf(memory_, 2 * part);
    std::uint32_t flags =
        ((frame.word >> (9 + part)) & kPlainHalves) + blend +
        (static_cast<std::uint32_t>(memory_.mem_r8(frame.header + kVertexEnds + part)) << 16);
    if (part == 0) {
      blend_ = blend;
      header_ = frame.header;
      depthShift_ = memory_.mem_r8(frame.header + kDepthShift);
      offsetWords_ = {memory_.mem_r32(frame.base + kFrameOffsets),
                      memory_.mem_r32(frame.base + kFrameOffsets + 4)};
    }
    const std::array<std::uint32_t, 3> streams = streamsOf(memory_, part, frame);
    for (std::uint32_t i = 0; i < 3; ++i) {
      memory_.mem_w32(entry + part_entry::kStreams + 4 * i, streams[i]);
    }
    if (blend != 0) {
      const Frame partner = frameOf(memory_, 2 * part + 1);
      flags += (partner.word >> (8 + part)) & kPartnerHalves;
      if (part == 0) {
        partnerDepthShift_ = memory_.mem_r8(partner.header + kDepthShift);
        partnerOffsetWords_ = {memory_.mem_r32(partner.base + kFrameOffsets),
                               memory_.mem_r32(partner.base + kFrameOffsets + 4)};
      }
      const std::array<std::uint32_t, 3> partnerStreams = streamsOf(memory_, part, partner);
      for (std::uint32_t i = 0; i < 3; ++i) {
        memory_.mem_w32(entry + part_entry::kPartnerStreams + 4 * i, partnerStreams[i]);
      }
    }
    memory_.mem_w32(entry + part_entry::kFlags, flags);
  }
  memory_.mem_w32(kPartTable + kParts * kPartBytes, 0);
}

// 80023E08..80023EDC: parts 1 and 2's offsets from part 0, blended when part 0 is.
PairedActor::Offsets PairedActor::blendOffsets() {
  if (blend_ == 0) {
    return {unpackOffset(offsetWords_[0]), unpackOffset(offsetWords_[1])};
  }
  auto &gte = core_.gte();
  gte.writeData(gte::kIr0, blend_);
  const auto load = [&gte](std::uint32_t from, std::uint32_t to) {
    const std::array<std::uint32_t, 3> a = unpackOffset(from);
    const std::array<std::uint32_t, 3> b = unpackOffset(to);
    for (std::uint32_t i = 0; i < 3; ++i) {
      gte.writeData(gte::kIr1 + i, a[i]);
    }
    for (std::uint32_t i = 0; i < 3; ++i) {
      gte.writeControl(gte::kFarRed + i, b[i]);
    }
  };
  load(offsetWords_[0], partnerOffsetWords_[0]);
  gte.execute(gte::kIntpl);
  load(offsetWords_[1], partnerOffsetWords_[1]);
  Offsets offsets;
  for (std::uint32_t i = 0; i < 3; ++i) {
    offsets.first[i] = gte.readData(gte::kMac1 + i);
  }
  gte.execute(gte::kIntpl);
  for (std::uint32_t i = 0; i < 3; ++i) {
    offsets.second[i] = gte.readData(gte::kMac1 + i);
  }
  return offsets;
}

// 80023EE0..80023FD0: the centre into view space, culled, and the depth words parked in the back
// colour registers.
bool PairedActor::placeCentre() {
  auto &gte = core_.gte();
  const std::uint32_t camera = kRenderGlobals.cameraRotation;
  const std::uint32_t position = kRenderGlobals.cameraPosition;
  const std::uint32_t x = memory_.mem_r32(kSpyroRecord) - memory_.mem_r32(position);
  const std::uint32_t y = memory_.mem_r32(position + 4) - memory_.mem_r32(kSpyroRecord + 4);
  const std::uint32_t z = memory_.mem_r32(position + 8) - memory_.mem_r32(kSpyroRecord + 8);
  gte.loadRotation(spyro::guest_memory::rotationAt(memory_, camera));
  gte.loadTranslation({});
  gte.writeData(gte::kIr3, x);
  gte.writeData(gte::kIr1, y);
  gte.writeData(gte::kIr2, z);
  gte.execute(gte::kMvmvaRtIr);
  v1_ = gte.readData(gte::kMac3);
  v0_ = gte.readData(gte::kMac2);
  const std::uint32_t across = gte.readData(gte::kMac1);
  if (asSigned(v1_ - kFarthest) >= 0) {
    return false;
  }
  const std::uint32_t reach = v1_ + kSideMargin;
  if (asSigned(reach - across) <= 0 || asSigned(reach + across) <= 0 ||
      asSigned(reach - v0_) <= 0 || asSigned(reach + v0_) <= 0) {
    return false;
  }
  std::uint32_t bias = sra(v1_, 7) - memory_.mem_r8(kSpyroRecord + spyro_record::kDepthBias);
  if (asSigned(bias) < 0) {
    bias = 0;
  }
  if (asSigned(partnerDepthShift_ - depthShift_) < 0) {
    partnerDepthShift_ = depthShift_;
  }
  std::uint32_t base = v1_ - (kDepthSpan << (partnerDepthShift_ & 31u));
  if (asSigned(base) < 0) {
    base = 0;
  }
  gte.writeControl(gte::kBackRed, partnerDepthShift_);
  gte.writeControl(gte::kBackGreen, bias);
  gte.writeControl(gte::kBackBlue, base);
  centre_ = {across, v0_, v1_};
  return true;
}

// 80023FD4..800241DC: part 0 turned by its angles about the centre; parts 1 and 2 placed by their
// offsets under part 0's rotation and, when their angles are positive, turned on from it.
void PairedActor::orientParts(const Offsets &offsets) {
  auto &gte = core_.gte();
  for (std::uint32_t i = 0; i < 3; ++i) {
    memory_.mem_w32(kPartTable + kPartOffsets + 4 * i, 0);
    memory_.mem_w32(kPartTable + kPartOffsets + 0xC + 4 * i, offsets.first[i]);
    memory_.mem_w32(kPartTable + kPartOffsets + 0x18 + 4 * i, offsets.second[i]);
  }
  gte.loadTranslation(centre_);
  for (std::uint32_t i = 0; i < 3; ++i) {
    memory_.mem_w32(kPartTable + part_entry::kTranslation + 4 * i, centre_[i]);
  }
  rotation_ = gte.storeRotation();
  rotate(memory_.mem_r32(kSpyroRecord + spyro_record::kAngles));
  writeRotation(kPartTable + part_entry::kRotation);

  // 80024068: the offsets negated in y and z, through RTPS for each part's translation.
  const auto place = [&gte](const std::array<std::uint32_t, 3> &offset) {
    gte.writeData(gte::kVxy0, (0u - offset[1]) + ((0u - offset[2]) << 16));
    gte.writeData(gte::kVz0, offset[0]);
    gte.execute(gte::kRtps);
  };
  place(offsets.first);
  std::array<std::uint32_t, 3> translation{};
  for (std::uint32_t i = 0; i < 3; ++i) {
    translation[i] = gte.readData(gte::kIr1 + i);
  }
  place(offsets.second);
  for (std::uint32_t i = 0; i < 3; ++i) {
    memory_.mem_w32(kPartTable + kPartBytes + part_entry::kTranslation + 4 * i, translation[i]);
    memory_.mem_w32(kPartTable + 2 * kPartBytes + part_entry::kTranslation + 4 * i,
                gte.readData(gte::kIr1 + i));
  }

  const std::array<std::uint32_t, 5> body = rotation_;
  std::uint32_t angles = memory_.mem_r32(kSpyroRecord + spyro_record::kAngles + 4);
  if (asSigned(angles) > 0) {
    rotate(angles);
  }
  // 800240F8
  if (memory_.mem_r8(flame_block::kSelect) != 0) {
    writeRotation(flame_block::kRotation);
  }
  if (angles != 0) {
    writeRotation(kPartTable + kPartBytes + part_entry::kRotation);
    memory_.mem_w8(kPartTable + kPartBytes + part_entry::kOwnRotation, 0x80);
    rotation_ = body;
    loadRotation();
    writeRotation(kPartTable + 2 * kPartBytes + part_entry::kRotation);
    memory_.mem_w8(kPartTable + 2 * kPartBytes + part_entry::kOwnRotation, 0x80);
  }
  angles = memory_.mem_r32(kSpyroRecord + spyro_record::kAngles + 8);
  if (asSigned(angles) > 0) {
    rotate(angles);
    writeRotation(kPartTable + 2 * kPartBytes + part_entry::kRotation);
    memory_.mem_w8(kPartTable + 2 * kPartBytes + part_entry::kOwnRotation, 0x80);
    rotation_ = body;
    loadRotation();
  }
}

void PairedActor::writeRotation(std::uint32_t at) const {
  for (std::uint32_t i = 0; i < gte::kRotationWords; ++i) {
    memory_.mem_w32(at + 4 * i, rotation_[i]);
  }
}

void PairedActor::loadRotation() {
  core_.gte().loadRotation(rotation_);
}

// 800241E0..80024400: RT turned by the angle bytes of `angles`, bits 16..23 about y, then 8..15
// about x, then 0..7 about z, each column pair through MVMVA.
void PairedActor::rotate(std::uint32_t angles) {
  auto &gte = core_.gte();
  auto &rt = rotation_;
  const auto turn = [&gte](std::uint32_t xy, std::uint32_t z) {
    gte.writeData(gte::kVxy0, xy);
    gte.writeData(gte::kVz0, z);
    gte.execute(gte::kMvmvaRtV0);
    return std::array<std::uint32_t, 3>{
        gte.readData(gte::kIr1), gte.readData(gte::kIr2), gte.readData(gte::kIr3)};
  };
  const auto sine = [this](std::uint32_t index) {
    return static_cast<std::uint32_t>(memory_.mem_r16(kRenderGlobals.cameraSineTable + index));
  };
  const auto cosine = [this](std::uint32_t index) {
    return static_cast<std::uint32_t>(memory_.mem_r16(kRenderGlobals.cameraSineTable + spyro::guest_render_globals::kCosineOffset + index));
  };

  if (const std::uint32_t index = (angles >> 15) & 0x1FEu; index != 0) {
    const std::uint32_t c = cosine(index);
    const std::uint32_t s = sine(index);
    const std::array<std::uint32_t, 3> a = turn(c, s);
    const std::array<std::uint32_t, 3> b = turn((0u - s) & 0xFFFFu, c);
    rt[0] = (rt[0] & 0xFFFF0000u) + (a[0] & 0xFFFFu);
    rt[3] = (rt[3] & 0xFFFF0000u) + (a[2] & 0xFFFFu);
    rt[1] = (b[0] & 0xFFFFu) + (a[1] << 16);
    rt[2] = (rt[2] & 0xFFFFu) + (b[1] << 16);
    rt[4] = b[2] & 0xFFFFu;
    loadRotation();
  }
  if ((angles & 0xFF00u) != 0) {
    const std::uint32_t index = ((angles & 0xFF00u) >> 8) << 1;
    const std::uint32_t c = cosine(index);
    const std::uint32_t s = sine(index);
    const std::array<std::uint32_t, 3> a = turn(c << 16, s);
    const std::array<std::uint32_t, 3> b = turn(0u - (s << 16), c);
    rt[0] = (rt[0] & 0xFFFFu) + (a[0] << 16);
    rt[3] = (rt[3] & 0xFFFFu) + (a[2] << 16);
    rt[1] = (rt[1] & 0xFFFF0000u) + (b[0] & 0xFFFFu);
    rt[2] = (b[1] << 16) + (a[1] & 0xFFFFu);
    rt[4] = b[2] & 0xFFFFu;
    loadRotation();
  }
  if (const std::uint32_t index = (angles & 0xFFu) << 1; index != 0) {
    const std::uint32_t c = cosine(index);
    const std::uint32_t s = sine(index);
    const std::array<std::uint32_t, 3> a = turn((s << 16) + c, 0);
    const std::array<std::uint32_t, 3> b = turn(((0u - s) & 0xFFFFu) + (c << 16), 0);
    rt[1] = (rt[1] & 0xFFFFu) + (a[1] << 16);
    rt[0] = (b[0] << 16) + (a[0] & 0xFFFFu);
    rt[3] = (b[2] << 16) + (a[2] & 0xFFFFu);
    rt[2] = (rt[2] & 0xFFFF0000u) + (b[1] & 0xFFFFu);
    loadRotation();
  }
}

} // namespace spyro1::actors
