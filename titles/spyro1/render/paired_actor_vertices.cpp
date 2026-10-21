// RasterizePairedActor's vertex half: the decode and projection, the cut's heights and the tint.
#include "spyro1/render/paired_actor.h"

#include "core.h"
#include "render/field/guest_word.h"
#include "spyro1/render/paired_actor_layout.h"

namespace spyro1::actors {

using spyro::guest_word::signedHalf;

using namespace paired;

namespace {

using Position = std::array<std::uint32_t, 3>;

// A packed delta word: x from bit 21 added, y from bit 10 and z from bit 0 (doubled) taken away.
void addWord(Position &position, std::uint32_t word) {
  position[0] += sra(word, 21);
  position[1] -= sra(word << 11, 21);
  position[2] -= sra(word << 22, 21);
}

// A positive halfword delta: three 7-bit fields from bits 7, 2 and 0 (times 8).
void addHalf(Position &position, std::uint32_t half) {
  position[0] += sra(half << 18, 25);
  position[1] -= sra(half << 23, 25);
  position[2] -= sra(half << 28, 25);
}

// A negative halfword and the halfword after it: an absolute position.
Position absolute(std::uint32_t half, std::uint32_t low) {
  return {sra(half << 18, 21), 0u - sra((low << 12) | (half << 28), 21), 0u - sra(low << 22, 21)};
}

} // namespace

// 80024404..80024A50. An iteration decodes vertex k into V0 while RTPS projects vertex k - 1, so a
// part's first iteration projects the previous V0 into its first slot, steps back one slot, and the
// part's last vertex is projected after its loop.
void PairedActor::projectParts() {
  auto &gte = core_.gte();
  std::uint32_t part = kPartTable;
  std::uint32_t flags = memory_.mem_r32(part);
  screen_ = kVertexScreens;
  vertex_ = kVertexTable;
  waterLevel_ = memory_.mem_r32(kSpyroRecord + spyro_record::kWaterLevel);
  for (;;) {
    gte.loadTranslation({memory_.mem_r32(part + part_entry::kTranslation),
                         memory_.mem_r32(part + part_entry::kTranslation + 4),
                         memory_.mem_r32(part + part_entry::kTranslation + 8)});
    if (asSigned(flags) < 0) {
      gte.loadRotation(spyro::guest_memory::rotationAt(memory_, part + part_entry::kRotation));
    }
    last_ = kVertexScreens + ((flags >> 14) & 0x3FFu) - 4;
    end_ = screen_ + 4;
    format_ = flags & kPlainHalves;
    std::uint32_t words = memory_.mem_r32(part + part_entry::kStreams);
    v0_ = memory_.mem_r32(part + part_entry::kStreams + 4);
    v1_ = memory_.mem_r32(part + part_entry::kStreams + 8);
    position_ = {};
    weight_ = flags & 0xFFFu;
    partnerFormat_ = flags & kPartnerHalves;
    if (weight_ != 0) {
      for (std::uint32_t i = 0; i < 3; ++i) {
        partnerStreams_[i] = memory_.mem_r32(part + part_entry::kPartnerStreams + 4 * i);
      }
      partner_ = {};
      projectBlended(words);
    } else {
      projectPlain(words);
    }

    // 80024A14
    gte.execute(gte::kRtps);
    vertex_ += vertex_entry::kBytes;
    screen_ += 4;
    part += kPartBytes;
    flags = memory_.mem_r32(part);
    memory_.mem_w32(screen_ - 4, gte.readData(gte::kSxy2));
    keepPlace();
    v0_ = gte.readData(gte::kSz3);
    memory_.mem_w32(vertex_ - vertex_entry::kBytes, v0_);
    if (waterLevel_ != 0) {
      v0_ = keepInput();
    }
    if (flags == 0) {
      return;
    }
  }
}

// 80024560..8002457C: at the part's end the first time, step back one slot and run on to the last.
bool PairedActor::stepOn() {
  if (screen_ != end_) {
    return true;
  }
  if (last_ == end_) {
    return false;
  }
  end_ = last_;
  vertex_ -= vertex_entry::kBytes;
  screen_ -= 4;
  return true;
}

// 80024514: the projected vertex's input, for the cut's heights.
std::uint32_t PairedActor::keepInput() {
  auto &gte = core_.gte();
  const std::uint32_t z = gte.readData(gte::kVz0);
  memory_.mem_w32(vertex_ - vertex_entry::kBytes + vertex_entry::kVxy, gte.readData(gte::kVxy0));
  memory_.mem_w32(vertex_ - vertex_entry::kBytes + vertex_entry::kVz, z);
  return z;
}

void PairedActor::loadVertex() {
  core_.gte().writeData(gte::kVxy0, (position_[2] << 16) + position_[1]);
  core_.gte().writeData(gte::kVz0, position_[0]);
}

void PairedActor::keepProjection() {
  auto &gte = core_.gte();
  memory_.mem_w32(vertex_ - vertex_entry::kBytes, gte.readData(gte::kSz3));
  memory_.mem_w32(screen_ - 4, gte.readData(gte::kSxy2));
  keepPlace();
}

void PairedActor::keepPlace() {
  if (places_ != nullptr) {
    vertexPlaces_[(screen_ - 4 - kVertexScreens) / 4u] = core_.gte().projectedPlace(2);
  }
}

// 800244D8..800246B0
void PairedActor::projectPlain(std::uint32_t &words) {
  do {
    words += 4;
    core_.gte().execute(gte::kRtps);
    if (format_ == 0) {
      const std::uint32_t code = memory_.mem_r8(v0_);
      format_ = code & 1u;
      const std::uint32_t delta =
          memory_.mem_r32(kVertexDeltas + ((code << 1) & 0x1FCu)) + memory_.mem_r32(words - 4);
      v0_ += 1;
      vertex_ += vertex_entry::kBytes;
      screen_ += 4;
      if (waterLevel_ != 0) {
        keepInput();
      }
      addWord(position_, delta);
    } else {
      const std::uint32_t half = signedHalf(memory_.mem_r16(v1_));
      v1_ += 2;
      vertex_ += vertex_entry::kBytes;
      screen_ += 4;
      format_ = half & 0x4000u;
      if (asSigned(half) >= 0) {
        addHalf(position_, half);
        addWord(position_, memory_.mem_r32(words - 4));
      } else {
        const std::uint32_t low = memory_.mem_r16(v1_);
        v1_ += 2;
        position_ = absolute(half, low);
      }
      if (waterLevel_ != 0) {
        keepInput();
      }
    }
    loadVertex();
    keepProjection();
  } while (stepOn());
}

// 800246B4..80024A10: the frame's vertex and the partner's, decoded side by side and blended by
// INTPL toward the partner.
void PairedActor::projectBlended(std::uint32_t &words) {
  auto &gte = core_.gte();
  do {
    words += 4;
    gte.execute(gte::kRtps);
    std::uint32_t partnerWord = 0;
    if (format_ == 0) {
      const std::uint32_t code = memory_.mem_r8(v0_);
      format_ = code & 1u;
      const std::uint32_t delta =
          memory_.mem_r32(kVertexDeltas + ((code << 1) & 0x1FCu)) + memory_.mem_r32(words - 4);
      v0_ += 1;
      vertex_ += vertex_entry::kBytes;
      screen_ += 4;
      addWord(position_, delta);
    } else {
      const std::uint32_t half = signedHalf(memory_.mem_r16(v1_));
      v1_ += 2;
      vertex_ += vertex_entry::kBytes;
      screen_ += 4;
      format_ = half & 0x4000u;
      if (asSigned(half) >= 0) {
        addHalf(position_, half);
        addWord(position_, memory_.mem_r32(words - 4));
      } else {
        const std::uint32_t low = memory_.mem_r16(v1_);
        v1_ += 2;
        position_ = absolute(half, low);
      }
    }
    partnerWord = memory_.mem_r32(partnerStreams_[0]);
    partnerStreams_[0] += 4;

    if (partnerFormat_ == 0) {
      const std::uint32_t code = memory_.mem_r8(partnerStreams_[1]);
      partnerFormat_ = code & 1u;
      partnerStreams_[1] += 1;
      addWord(partner_, memory_.mem_r32(kVertexDeltas + ((code << 1) & 0x1FCu)) + partnerWord);
    } else {
      const std::uint32_t half = signedHalf(memory_.mem_r16(partnerStreams_[2]));
      partnerFormat_ = half & 0x4000u;
      addWord(partner_, partnerWord);
      if (asSigned(half) >= 0) {
        addHalf(partner_, half);
        partnerStreams_[2] += 2;
      } else {
        const std::uint32_t low = memory_.mem_r16(partnerStreams_[2] + 2);
        partnerStreams_[2] += 4;
        partner_ = absolute(half, low);
      }
    }

    gte.writeData(gte::kIr0, weight_);
    for (std::uint32_t i = 0; i < 3; ++i) {
      gte.writeData(gte::kIr1 + i, position_[i]);
      gte.writeControl(gte::kFarRed + i, partner_[i]);
    }
    keepProjection();
    gte.execute(gte::kIntpl);
    if (waterLevel_ != 0) {
      keepInput();
    }
    const std::uint32_t x = gte.readData(gte::kMac1);
    const std::uint32_t y = gte.readData(gte::kMac2);
    const std::uint32_t z = gte.readData(gte::kMac3);
    gte.writeData(gte::kVz0, x);
    gte.writeData(gte::kVxy0, y + (z << 16));
  } while (stepOn());
}

// 80024A5C..80024B5C: each vertex's height above the water, (record z - water level) less its y in
// the body matrix about its part's offset, over the vertex's VXY.
void PairedActor::measureHeights() {
  auto &gte = core_.gte();
  gte.loadRotation(spyro::guest_memory::rotationAt(memory_, kSpyroRecord + spyro_record::kOrientation));
  const std::uint32_t level = memory_.mem_r32(kSpyroRecord + spyro_record::kPosition + 8) - waterLevel_;
  std::uint32_t entry = kVertexTable;
  for (std::uint32_t part = 0; part < kParts; ++part) {
    const std::uint32_t end =
        kVertexTable + (static_cast<std::uint32_t>(memory_.mem_r8(header_ + kVertexEnds + part)) << 4);
    const std::uint32_t offset = kPartTable + kPartOffsets + part * 12;
    gte.writeData(gte::kIr1, 0u - memory_.mem_r32(offset + 4));
    gte.writeData(gte::kIr2, 0u - memory_.mem_r32(offset + 8));
    gte.writeData(gte::kIr3, memory_.mem_r32(offset));
    gte.execute(gte::kMvmvaRtIr);
    gte.loadTranslation({gte.readData(gte::kMac1), gte.readData(gte::kMac2), gte.readData(gte::kMac3)});
    gte.writeData(gte::kVxy0, memory_.mem_r32(entry + vertex_entry::kVxy));
    gte.writeData(gte::kVz0, memory_.mem_r32(entry + vertex_entry::kVz));
    entry += vertex_entry::kBytes;
    bool more = true;
    while (more) {
      gte.execute(psx::gte::kMvmvaRtV0Tr);
      v0_ = memory_.mem_r32(entry + vertex_entry::kVz);
      gte.writeData(gte::kVxy0, memory_.mem_r32(entry + vertex_entry::kVxy));
      gte.writeData(gte::kVz0, v0_);
      v1_ = level - gte.readData(gte::kMac2);
      memory_.mem_w32(entry - vertex_entry::kBytes + vertex_entry::kVxy, v1_);
      more = entry != end;
      entry += vertex_entry::kBytes;
    }
    entry -= vertex_entry::kBytes;
  }
}

// 80024B60..80024C2C: with a tint amount in the record's top byte, the colours blended toward the
// tint into the work block; the polygon passes read whichever list results.
void PairedActor::tintColours() {
  auto &gte = core_.gte();
  const std::uint32_t tint = memory_.mem_r32(kSpyroRecord + spyro_record::kTint);
  std::uint32_t at = memory_.mem_r32(header_ + kColourList);
  colours_ = at;
  const std::uint32_t amount = tint >> 24;
  if (amount == 0) {
    return;
  }
  const std::uint32_t count = memory_.mem_r16(header_ + kColourCount);
  v1_ = kTintedColours;
  colours_ = v1_;
  gte.writeData(gte::kIr0, amount << 4);
  gte.writeControl(gte::kFarRed, (tint << 4) & 0xFF0u);
  gte.writeControl(gte::kFarGreen, (tint >> 4) & 0xFF0u);
  gte.writeControl(gte::kFarBlue, (tint >> 12) & 0xFF0u);
  const auto load = [&gte, this](std::uint32_t address) {
    const std::uint32_t colour = memory_.mem_r32(address);
    gte.writeData(gte::kIr1, (colour << 4) & 0xFF0u);
    gte.writeData(gte::kIr2, (colour >> 4) & 0xFF0u);
    gte.writeData(gte::kIr3, (colour >> 12) & 0xFF0u);
  };
  load(at);
  at += 4;
  v0_ = (count << 2) + at;
  bool more = true;
  while (more) {
    gte.execute(gte::kIntpl);
    load(at);
    at += 4;
    memory_.mem_w32(v1_, gte.readData(gte::kRgb2));
    more = at != v0_;
    v1_ += 4;
  }
}

} // namespace spyro1::actors
