// The moby emitters' vertex half: each vertex decoded from the frame's word and halfword streams,
// blended toward the partner frame when the entry has a weight, and projected into the scratchpad.
#include "spyro1/render/actor_emitter.h"

#include "core.h"
#include "render/field/guest_word.h"
#include "spyro1/render/actor_emitter_layout.h"

namespace spyro1::actors {

using namespace emitter;
using spyro::guest_word::signedHalf;

ActorEmitter::Projected ActorEmitter::readProjected() {
  auto &gte = core_.gte();
  return Projected{
      .screen = gte.readData(gte::kSxy2),
      .depth = gte.readData(gte::kSz3),
      .view = {gte.readData(gte::kMac1), gte.readData(gte::kMac2), gte.readData(gte::kMac3)},
      .place = gte.projectedPlace(2)};
}

// 8001FA00..8001FA18: up to 0x80 depths fit the scratchpad after the screen words.
std::uint32_t ActorEmitter::depthTable(std::uint32_t count) const {
  return asSigned(count - kScratchDepthVertices) > 0 ? kWorkDepths : kVertexDepths;
}

void ActorEmitter::keepDepth(const Projected &vertex) {
  depth_ += 4;
  memory_.mem_w32(depth_ - 4, vertex.depth);
}

// 8001FC70..8001FCA8: a straddling record keeps the screen word shifted up past four outcodes, and
// what every vertex shares.
bool ActorEmitter::keepScreen(const Projected &vertex) {
  const std::uint32_t screen = vertex.screen;
  if (places_ != nullptr) {
    vertexPlaces_[(screen_ - 4 - kVertexScreens) / 4u] = vertex.place;
  }
  if (asSigned(flags_) < 0) {
    std::uint32_t code = screen << 5;
    code |= bounds_.aboveTop(screen) ? 1u : 0u;
    code |= bounds_.belowBottom(screen) ? 2u : 0u;
    code |= bounds_.atOrLeftOfLeft(screen) ? 4u : 0u;
    code |= bounds_.atOrRightOfRight(screen) ? 8u : 0u;
    outside_ &= code;
    memory_.mem_w32(screen_ - 4, code);
  } else {
    memory_.mem_w32(screen_ - 4, screen);
  }
  return screen_ == screenEnd_;
}

// 8001F9F8..8001FDCC. Each iteration projects the previous vertex while decoding the next, so the
// streams are read one vertex ahead. False when every vertex is outside one edge.
bool ActorEmitter::project(std::uint32_t count, std::uint32_t first) {
  depth_ = depthTable(count);
  screen_ = kVertexScreens;
  screenEnd_ = kVertexScreens + (count << 2);
  words_ += 4;
  if (weight_ == 0) {
    projectPlain(first);
  } else {
    projectBlended(first);
  }
  return asSigned(flags_) >= 0 || (outside_ & 0x1Fu) == 0;
}

// 8001FCC8..8001FDCC
void ActorEmitter::projectPlain(std::uint32_t first) {
  auto &gte = core_.gte();
  Position position = decodeWord(first);
  std::uint32_t source = first;
  gte.writeData(gte::kVz0, position[0]);
  gte.writeData(gte::kVxy0, packedXy(position));
  for (;;) {
    gte.execute(gte::kRtps);
    if ((source & 1u) != 0) {
      source = signedHalf(memory_.mem_r16(halves_));
      halves_ += 2;
      addHalf(position, source, halfShift_);
    } else {
      source = memory_.mem_r32(words_);
      words_ += 4;
      position = decodeWord(source);
    }
    screen_ += 4;
    gte.writeData(gte::kVxy0, packedXy(position));
    gte.writeData(gte::kVz0, position[0]);
    const Projected vertex = readProjected();
    keepDepth(vertex);
    if (keepScreen(vertex)) {
      return;
    }
  }
}

// 8001FA44..8001FCC4: the position and the partner's, each from its own streams, through INTPL.
void ActorEmitter::projectBlended(std::uint32_t first) {
  auto &gte = core_.gte();
  const auto blend = [&](const Position &position, const Position &partner) {
    gte.writeData(gte::kIr0, weight_);
    gte.writeData(gte::kIr1, position[0]);
    gte.writeData(gte::kIr2, position[1]);
    gte.writeData(gte::kIr3, position[2]);
    gte.writeControl(gte::kFarRed, partner[0]);
    gte.writeControl(gte::kFarGreen, partner[1]);
    gte.writeControl(gte::kFarBlue, partner[2]);
  };
  const auto loadBlended = [&] {
    gte.writeData(gte::kVz0, gte.readData(gte::kMac1));
    gte.writeData(gte::kVxy0, gte.readData(gte::kMac2) + (gte.readData(gte::kMac3) << 16));
  };
  const auto next = [&](std::uint32_t source) {
    return (source & 1u) != 0 ? signedHalf(memory_.mem_r16(halves_)) : memory_.mem_r32(words_);
  };

  Position position = decodeWord(first);
  const std::uint32_t partnerFirst = memory_.mem_r32(partnerWords_);
  partnerWords_ += 4;
  Position partner = decodeWord(partnerFirst);
  blend(position, partner);
  std::uint32_t partnerSource = partnerFirst;
  gte.execute(gte::kIntpl);
  std::uint32_t source = first;
  std::uint32_t pending = next(source);
  loadBlended();
  for (;;) {
    gte.execute(gte::kRtps);
    const std::uint32_t partnerNext = (partnerSource & 1u) != 0
                                          ? signedHalf(memory_.mem_r16(partnerHalves_))
                                          : memory_.mem_r32(partnerWords_);
    if ((source & 1u) != 0) {
      addHalf(position, pending, halfShift_);
      halves_ += 2;
    } else {
      position = decodeWord(pending);
      words_ += 4;
    }
    source = pending;
    if ((partnerSource & 1u) != 0) {
      addHalf(partner, partnerNext, halfShift_);
    } else {
      partner = decodeWord(partnerNext);
    }
    blend(position, partner);
    const Projected vertex = readProjected();
    gte.execute(gte::kIntpl);
    pending = next(source);
    if ((partnerSource & 1u) != 0) {
      partnerHalves_ += 2;
    } else {
      partnerWords_ += 4;
    }
    partnerSource = partnerNext;
    screen_ += 4;
    loadBlended();
    keepDepth(vertex);
    if (keepScreen(vertex)) {
      return;
    }
  }
}

} // namespace spyro1::actors
