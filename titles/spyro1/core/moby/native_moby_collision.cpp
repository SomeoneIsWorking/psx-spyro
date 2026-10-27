#include "spyro1/core/moby/native_moby_collision.h"

#include "spyro1/core/guest_globals.h"
#include "core/native_execution.h"
#include "game.h"

#include <lucent/log.h>

#include <cstdint>

namespace spyro1::native {
namespace {

// Shape table (`lui 0x8007 ; addiu 0x6378`) and the shared callee-saved spill area.
using spyro1::guest::kModels;
using spyro1::guest::kMobyCollisionChain;
using spyro1::guest::kRegisterSpillArea;

// The GTE register numbers this body touches, from external/spyro-1's own asm/c2regs.inc.
constexpr std::uint32_t kGteVxy0 = 0u;
constexpr std::uint32_t kGteVz0 = 1u;
constexpr std::uint32_t kGteIr0 = 8u;
constexpr std::uint32_t kGteIr1 = 9u;
constexpr std::uint32_t kGteIr2 = 10u;
constexpr std::uint32_t kGteIr3 = 11u;
constexpr std::uint32_t kGteMac1 = 25u;
constexpr std::uint32_t kGteMac2 = 26u;
constexpr std::uint32_t kGteMac3 = 27u;
constexpr std::uint32_t kGteLzcs = 30u;
constexpr std::uint32_t kGteLzcr = 31u;
constexpr std::uint32_t kGteRot11Rot12 = 0u;
constexpr std::uint32_t kGteRot13Rot21 = 1u;
constexpr std::uint32_t kGteRot22Rot23 = 2u;
constexpr std::uint32_t kGteRot31Rot32 = 3u;
constexpr std::uint32_t kGteRot33 = 4u;
constexpr std::uint32_t kGteTransX = 5u;
constexpr std::uint32_t kGteTransY = 6u;
constexpr std::uint32_t kGteTransZ = 7u;

// The three GTE commands the body issues, as the guest's own instruction words.
constexpr std::uint32_t kGteSqr = 0x4AA00428u;
constexpr std::uint32_t kGteRtps = 0x4A180001u;
constexpr std::uint32_t kGteGpf = 0x4B90003Du;

// g_MobyCollisionChain cell table (`lui 0x8007 ; addiu 0x5778`) and the scratch candidate list.
constexpr std::uint32_t kCandidateList = 0x1F800000u;
// Cell-edge bit and the bounds that turn a wrapping neighbour into -1.
constexpr std::uint32_t kCellEdge = 0x1000u;
constexpr std::uint32_t kCellColumnLimit = 0x20u;
constexpr std::uint32_t kCellRowStride = 0x20u;
constexpr std::uint32_t kCellRowLimit = 0x400u;

// Shape types reached by the computed jump at 0x8004E5B0; byte 1 is the slab case.
constexpr std::uint32_t kShapePointJump = 0x0000u;
constexpr std::uint32_t kShapePoint = 0x0400u;
constexpr std::uint32_t kShapeSlabJump = 0x0100u;
constexpr std::uint32_t kShapeSlab = 0x1600u;
constexpr std::uint32_t kShapeBoxJump = 0x0200u;
constexpr std::uint32_t kShapeBox = 0x2C00u;
constexpr std::uint32_t kShapeMobyJump = 0x0300u;
constexpr std::uint32_t kShapeMoby = 0x4F00u;

// Walk state: the position, $v1, and the two result registers the epilogue picks between.
struct Walker {
  Core *c = nullptr;
  std::uint32_t pos = 0u;       // $a0, and $gp for the resolved-position store
  std::uint32_t radius = 0u;    // $a1
  std::uint32_t damageOut = 0u; // $a2: hit flags accumulate here, 0 when the caller passed none
  std::uint32_t damageIn = 0u;  // $a3: ORed into the record of every shape that hits
  std::uint32_t ignore = 0u;    // the 5th argument, at entry sp+0x10: the moby to skip
  std::uint32_t flags = 0u;     // the 6th argument, at entry sp+0x14
  std::uint32_t x = 0u;         // s7/t8/t9: the position being resolved
  std::uint32_t y = 0u;
  std::uint32_t z = 0u;
  std::uint32_t v1 = 0u;
  std::uint32_t firstHit = 0u; // s2: the record of the shape that hit
  std::uint32_t resolved = 0u; // s1: the record the resolve loop settled on
};

// The record's position minus the position being resolved: what the GTE squares.
struct Offset {
  std::uint32_t x;
  std::uint32_t y;
  std::uint32_t z;
};

// Bounding-sphere separation, held in $a2/$a3/$t1 across the shape loop.
struct Separation {
  std::uint32_t mac1;
  std::uint32_t mac2;
  std::uint32_t squared;
};

// One shape's test result; a hit carries the resolve loop's start values and the next record.
struct ShapeOutcome {
  bool hit = false;
  std::uint32_t shape = 0u;
  std::uint32_t step = 0u;
  std::uint32_t target = 0u;
  std::uint32_t normalX = 0u;
  std::uint32_t normalY = 0u;
  std::uint32_t normalZ = 0u;
};

enum class Step : std::uint8_t {
  kNextShape,
  kNextRecord,
  kDone,
};

// The record being walked, the cursor just past the current shape word, and that word.
struct ShapeCursor {
  std::uint32_t record;
  std::uint32_t shape;
  std::uint32_t word;
};

bool isNegative(std::uint32_t value) {
  return (value & 0x80000000u) != 0u;
}

// Signed `mult`: hi/lo keep the signed 64-bit product.
std::uint32_t multiplyLow(Core *c, std::uint32_t a, std::uint32_t b) {
  const std::int64_t product = static_cast<std::int64_t>(static_cast<std::int32_t>(a)) *
                               static_cast<std::int64_t>(static_cast<std::int32_t>(b));
  c->lo = static_cast<std::uint32_t>(product);
  c->hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32);
  return c->lo;
}

// Point shape at 0x8004E5D8, 8-byte records.
ShapeOutcome testPointShape(
    Core *c, const Walker &w, const ShapeCursor &at, const Separation &sep, const Offset &off) {
  const std::uint32_t shape = at.shape;
  const std::uint32_t word = at.word;
  const std::uint32_t extent = (word >> 16) + w.radius;
  const std::uint32_t step = multiplyLow(c, extent, extent);
  ShapeOutcome outcome{};
  outcome.shape = shape + 4u; // the `bgez` delay slot, which runs on the miss arm as well
  outcome.step = step;
  outcome.target = sep.squared;
  outcome.normalX = off.x;
  outcome.normalY = off.y;
  outcome.normalZ = off.z;
  outcome.hit = isNegative(sep.squared - step);
  return outcome;
}

// Slab shape at 0x8004E668, 12-byte records.
ShapeOutcome testSlabShape(
    Core *c, const Walker &w, const ShapeCursor &at, const Separation &sep, const Offset &off) {
  const std::uint32_t shape = at.shape;
  const std::uint32_t word = at.word;
  const std::uint32_t packed = c->mem_r32(shape);
  const std::uint32_t extent = (word >> 16) + w.radius;
  const std::uint32_t step = multiplyLow(c, extent, extent);
  const std::uint32_t zExtent =
      (static_cast<std::uint32_t>(static_cast<std::int32_t>(packed) >> 16)) + off.z;
  const std::uint32_t target = multiplyLow(c, zExtent, zExtent) + sep.mac1 + sep.mac2;
  ShapeOutcome outcome{};
  outcome.shape = shape + 8u; // the `bgez` delay slot
  outcome.step = step;
  outcome.target = target;
  outcome.normalX = off.x;
  outcome.normalY = off.y;
  outcome.normalZ = zExtent;
  outcome.hit = isNegative(target - step);
  return outcome;
}

// Box shape at 0x8004E718, 20-byte records.
ShapeOutcome testBoxShape(
    Core *c, const Walker &w, const ShapeCursor &at, const Separation &sep, const Offset &off) {
  std::uint32_t shape = at.shape;
  const std::uint32_t word = at.word;
  const std::uint32_t packed = c->mem_r32(shape);
  shape += 0x10u; // the `blez` delay slot, which runs on the miss arm as well
  const std::uint32_t extent = (word >> 16) + w.radius;
  const std::uint32_t step = multiplyLow(c, extent, extent);
  ShapeOutcome outcome{};
  outcome.shape = shape;
  outcome.normalX = off.x;
  outcome.normalY = off.y;
  if (static_cast<std::int32_t>(step - sep.mac1 - sep.mac2) <= 0) {
    return outcome; // `blez`: the separation is already within the y slab
  }
  const std::uint32_t yExtent =
      (static_cast<std::uint32_t>(static_cast<std::int32_t>(packed) >> 16)) + off.z;
  const std::uint32_t xExtent =
      static_cast<std::uint32_t>(static_cast<std::int32_t>(packed << 16) >> 16) + off.z;
  if (!isNegative(yExtent | (0u - xExtent))) {
    outcome.hit = true;
    outcome.step = step;
    outcome.target = sep.mac1 + sep.mac2;
    outcome.normalZ = 0u;
    return outcome;
  }
  // 0x8004E760: the `bltz` delay slot loads 6 on both arms and the fall-through overwrites it with 4.
  const std::uint32_t slot = isNegative(yExtent) ? 6u : 4u;
  const std::uint32_t y = static_cast<std::uint32_t>(c->mem_r16s(slot + shape - 0x10u)) + off.z;
  const std::uint32_t target = multiplyLow(c, y, y) + sep.mac1 + sep.mac2;
  const std::uint32_t xHalf = c->mem_r32(shape - 8u) + w.radius;
  const std::uint32_t xStep = multiplyLow(c, xHalf, xHalf);
  outcome.normalZ = y;
  outcome.target = target;
  outcome.step = xStep;
  outcome.hit = isNegative(target - xStep);
  return outcome;
}

// Moby shape at 0x8004E830: RTPS one model vertex, 12-byte records.
ShapeOutcome testMobyShape(Core *c, const Walker &w, const ShapeCursor &at, const Offset &off) {
  const auto [record, shape, word] = at;
  c->gte().writeControl(kGteRot11Rot12, c->mem_r32(record + 0x20u));
  c->gte().writeControl(kGteRot13Rot21, c->mem_r32(record + 0x24u));
  c->gte().writeControl(kGteRot22Rot23, c->mem_r32(record + 0x28u));
  c->gte().writeControl(kGteRot31Rot32, c->mem_r32(record + 0x2Cu));
  c->gte().writeControl(kGteRot33, c->mem_r32(record + 0x30u));
  c->gte().writeControl(kGteTransX, 0u);
  c->gte().writeControl(kGteTransY, 0u);
  c->gte().writeControl(kGteTransZ, 0u);
  const std::uint32_t model = c->mem_r32(kModels + (c->mem_r16(record + 0x36u) * 4u));
  const std::uint32_t shapes = model + (c->mem_r8(record + 0x3Cu) * 4u);
  const std::uint32_t node = c->mem_r32(shapes + 0x38u);
  const std::uint32_t bounds = node + (c->mem_r8(record + 0x3Eu) * 8u);
  const std::uint32_t shift = c->mem_r8(node + 0x5u) & 31u;
  // Bit 31 selects a 21-bit index shifted up one, or the record's 16-bit index added.
  const std::uint32_t index = c->mem_r16(shape) * 4u;
  const std::uint32_t packed =
      c->mem_r32(((((c->mem_r32(bounds + 0x24u) << 11) >> 11) | 0x80000000u) + index));
  const std::uint32_t y = (static_cast<std::uint32_t>(static_cast<std::int32_t>(packed) >> 21))
                          << shift;
  const std::uint32_t z = static_cast<std::uint32_t>(static_cast<std::int32_t>(packed << 10) >> 21)
                          << shift;
  const std::uint32_t x = static_cast<std::uint32_t>(static_cast<std::int32_t>(packed << 20) >> 19)
                          << shift;
  c->gte().writeData(kGteVz0, y);
  c->gte().writeData(kGteVxy0, (x << 16) + z);
  c->gte().execute(kGteRtps);
  // RTPS leaves IR1..IR3 alone, so they still hold the starting separation.
  const std::uint32_t normalX = c->gte().readData(kGteIr3) + off.x;
  const std::uint32_t normalY = off.y - c->gte().readData(kGteIr1);
  const std::uint32_t normalZ = off.z - c->gte().readData(kGteIr2);
  c->gte().writeData(kGteIr1, normalX);
  c->gte().writeData(kGteIr2, normalY);
  c->gte().writeData(kGteIr3, normalZ);
  c->gte().execute(kGteSqr);
  const std::uint32_t extent = (word >> 16) + w.radius;
  const std::uint32_t step = multiplyLow(c, extent, extent);
  const std::uint32_t squared =
      c->gte().readData(kGteMac1) + c->gte().readData(kGteMac2) + c->gte().readData(kGteMac3);
  ShapeOutcome outcome{};
  outcome.shape = shape + 8u; // the `bgez` delay slot
  outcome.step = step;
  outcome.target = squared;
  outcome.normalX = normalX;
  outcome.normalY = normalY;
  outcome.normalZ = normalZ;
  outcome.hit = isNegative(squared - step);
  return outcome;
}

// Resolve loop at 0x8004E9D8.
void resolveOutOfShape(Walker &w, std::uint32_t record, const ShapeOutcome &outcome) {
  Core *const c = w.c;
  const std::uint32_t step = outcome.step;
  std::uint32_t target = outcome.target;
  std::uint32_t normalY = outcome.normalY;
  std::uint32_t normalZ = outcome.normalZ;
  for (;;) {
    w.resolved = record; // s1, rewritten on every pass
    c->gte().writeData(kGteLzcs, step);
    // The `bgtz` delay slot sets the exponent on both arms.
    const std::int32_t exponent = static_cast<std::int32_t>(c->gte().readData(kGteLzcr)) - 2;
    if (static_cast<std::int32_t>(target) <= 0) {
      target = 1u;
      normalZ = 0xFFFFFFFFu;
    }
    const std::uint32_t scaled = step << (static_cast<std::uint32_t>(exponent) & 31u);
    const std::int32_t overshoot = exponent - 16;
    std::uint32_t denominator;
    if (overshoot >= 0) {
      denominator = target << (static_cast<std::uint32_t>(overshoot) & 31u);
    } else {
      denominator = target >> (static_cast<std::uint32_t>(-overshoot) & 31u);
      if (static_cast<std::int32_t>(denominator) <= 0) {
        denominator = 1u; // the `j`'s delay slot
      }
    }
    psx::cpu::divide(c, {.numerator = scaled, .denominator = denominator});
    const std::uint32_t quotient = c->lo;
    c->gte().writeData(kGteLzcs, quotient);
    std::uint32_t reach = quotient;
    if (quotient != 0u) {
      // Arithmetic halving of LZCR: a negative quotient gives -1.
      const std::int32_t leading =
          static_cast<std::int32_t>(c->gte().readData(kGteLzcr) & 0xFFFFFFFEu);
      const std::int32_t halfShift = (0x1F - leading) >> 1;
      const std::int32_t below = leading - 0x18;
      const std::uint32_t tableOffset =
          below < 0 ? static_cast<std::uint32_t>(static_cast<std::int32_t>(quotient) >>
                                                 (0x18 - static_cast<std::uint32_t>(leading)))
                    : quotient << (static_cast<std::uint32_t>(below) & 31u);
      const std::uint32_t entry =
          c->mem_r16s(spyro1::guest::kMagnitudeTable + ((tableOffset - 0x40u) << 1));
      reach = (entry << (static_cast<std::uint32_t>(halfShift) & 31u)) >> 12;
    }
    target = reach + 1u;
    c->gte().writeData(kGteIr1, w.v1);
    c->gte().writeData(kGteIr2, normalY);
    c->gte().writeData(kGteIr3, normalZ);
    c->gte().writeData(kGteIr0, target);
    c->gte().execute(kGteGpf);
    const std::uint32_t mac1 = c->gte().readData(kGteMac1);
    const std::uint32_t mac2 = c->gte().readData(kGteMac2);
    const std::uint32_t mac3 = c->gte().readData(kGteMac3);
    const std::uint32_t dirX = static_cast<std::uint32_t>(static_cast<std::int32_t>(mac1) >> 8);
    const std::uint32_t dirY = static_cast<std::uint32_t>(static_cast<std::int32_t>(mac2) >> 8);
    const std::uint32_t dirZ = static_cast<std::uint32_t>(static_cast<std::int32_t>(mac3) >> 8);
    c->gte().writeData(kGteIr1, dirX);
    c->gte().writeData(kGteIr2, dirY);
    c->gte().writeData(kGteIr3, dirZ);
    c->gte().execute(kGteSqr);
    const std::uint32_t separation =
        c->gte().readData(kGteMac1) + c->gte().readData(kGteMac2) + c->gte().readData(kGteMac3);
    w.x = w.x + w.v1;
    w.y = w.y + normalY;
    w.z = w.z + normalZ;
    w.x = w.x - dirX;
    w.y = w.y - dirY;
    w.z = w.z - dirZ;
    if (!isNegative(separation - step)) {
      c->mem_w32(w.pos + 0u, w.x);
      c->mem_w32(w.pos + 4u, w.y);
      c->mem_w32(w.pos + 8u, w.z);
      return;
    }
    target = separation;
    w.v1 = dirX;
    normalY = dirY;
    normalZ = dirZ;
  }
}

// Shared hit tail (0x8004E5F8, 0x8004E6A4, 0x8004E7BC, 0x8004E95C).
Step shapeTail(Walker &w, const ShapeCursor &at, const ShapeOutcome &outcome) {
  const auto [record, shape, word] = at;
  Core *const c = w.c;
  if ((word & 4u) == 0u) {
    c->mem_w32(record + 0x18u, c->mem_r32(record + 0x18u) | w.damageIn);
  }
  if (w.damageOut != 0u) {
    c->mem_w32(w.damageOut, c->mem_r32(w.damageOut) | c->mem_r32(shape - 4u));
  }
  if ((word & 2u) != 0u) {
    return Step::kNextShape;
  }
  if ((w.flags & 1u) != 0u) {
    w.v1 = outcome.normalX; // the `j`'s delay slot, on the resolve arm only
    resolveOutOfShape(w, record, outcome);
    return Step::kDone;
  }
  if ((w.flags & 2u) != 0u) {
    return Step::kNextShape;
  }
  return Step::kDone;
}

// One record at 0x8004E52C; $v1 holds the x separation from the `sub` on.
Step walkRecord(Walker &w, std::uint32_t record) {
  Core *const c = w.c;
  Offset off{};
  off.x = w.v1 - w.x;
  w.v1 = off.x;
  off.y = c->mem_r32(record + 0x10u) - w.y;
  off.z = c->mem_r32(record + 0x14u) - w.z;
  c->gte().writeData(kGteIr1, off.x);
  c->gte().writeData(kGteIr2, off.y);
  c->gte().writeData(kGteIr3, off.z);
  std::uint32_t shape = c->mem_r32(record + 0x8u);
  c->gte().execute(kGteSqr);
  Separation sep{};
  sep.mac1 = c->gte().readData(kGteMac1);
  sep.mac2 = c->gte().readData(kGteMac2);
  sep.squared = sep.mac1 + sep.mac2 + c->gte().readData(kGteMac3);
  if (shape == 0u) {
    return Step::kNextRecord; // `beqz`: the record carries no shape data at all
  }
  const std::uint32_t reach = static_cast<std::uint32_t>(c->mem_r16(shape)) + w.radius;
  shape += 4u; // the `beqz`'s delay slot
  if (!isNegative(sep.squared - multiplyLow(c, reach, reach))) {
    return Step::kNextRecord; // `bgez`: outside the bounding sphere
  }
  for (std::uint32_t word = 0u;;) {
    // The `bnez` delay slot has already loaded the next word.
    if ((word & 1u) != 0u) {
      return Step::kNextRecord;
    }
    word = c->mem_r32(shape);
    shape += 4u; // the dispatch's delay slot
    const ShapeCursor at{record, shape, word};
    ShapeOutcome outcome{};
    switch (word & 0xFF00u) {
    case kShapePointJump:
    case kShapePoint:
      outcome = testPointShape(c, w, at, sep, off);
      break;
    case kShapeSlabJump:
    case kShapeSlab:
      outcome = testSlabShape(c, w, at, sep, off);
      break;
    case kShapeBoxJump:
    case kShapeBox:
      outcome = testBoxShape(c, w, at, sep, off);
      break;
    case kShapeMobyJump:
    case kShapeMoby:
      outcome = testMobyShape(c, w, at, off);
      break;
    default:
      return Step::kNextRecord; // an unmapped type byte ends the shape list
    }
    shape = outcome.shape;
    if (!outcome.hit) {
      continue;
    }
    w.firstHit = record;
    const Step tail = shapeTail(w, {record, shape, word}, outcome);
    if (tail == Step::kNextShape) {
      continue;
    }
    return tail;
  }
}

// Candidate list at 0x8004E494; the `beqz` delay slot makes the predecessor cell the default.
void buildCandidateList(Walker &w) {
  Core *const c = w.c;
  const std::uint32_t base = c->mem_r32(kMobyCollisionChain);
  const std::uint32_t column = w.x >> 13;
  const std::uint32_t row = (w.y >> 13) << 5;
  std::uint32_t list = kCandidateList;
  std::uint32_t nextColumn = column - 1u; // the `beqz`'s delay slot
  if ((w.x & kCellEdge) != 0u) {
    nextColumn = column + 1u;
    if (nextColumn == kCellColumnLimit) {
      nextColumn = 0xFFFFFFFFu;
    }
  }
  std::uint32_t nextRow = row - kCellRowStride; // the `beqz`'s delay slot
  if ((w.y & kCellEdge) != 0u) {
    nextRow = row + kCellRowStride;
    if (nextRow == kCellRowLimit) {
      nextRow = 0xFFFFFFFFu;
    }
  }
  c->mem_w32(list, base + ((column + row) << 2));
  list += 4u;
  if (!isNegative(nextColumn)) {
    c->mem_w32(list, base + ((nextColumn + row) << 2));
    list += 4u;
  }
  if (!isNegative(nextRow)) {
    c->mem_w32(list, base + ((column + nextRow) << 2));
    list += 4u;
  }
  if (!isNegative(nextColumn) && !isNegative(nextRow)) {
    c->mem_w32(list, base + ((nextColumn + nextRow) << 2));
    list += 4u;
  }
  c->mem_w32(list, 0u);
  w.v1 = column; // $v1, which an empty candidate list leaves as the return value's companion
}

// 0x8004E3C8: the spill to 0x80077DD8 is reproduced because the epilogue reloads it.
void mobyCollisionWalk(Core *c) {
  const std::uint32_t spilled[12] = {c->r[16],
                                     c->r[17],
                                     c->r[18],
                                     c->r[19],
                                     c->r[20],
                                     c->r[21],
                                     c->r[22],
                                     c->r[23],
                                     c->r[28],
                                     c->r[29],
                                     c->r[30],
                                     c->r[31]};
  for (std::uint32_t word = 0u; word < 12u; word++) {
    c->mem_w32(kRegisterSpillArea + (word * 4u), spilled[word]);
  }
  Walker w{};
  w.c = c;
  w.pos = c->r[4];
  w.radius = c->r[5];
  w.damageOut = c->r[6];
  w.damageIn = c->r[7];
  w.ignore = c->mem_r32(c->r[29] + 0x10u);
  w.flags = c->mem_r32(c->r[29] + 0x14u);
  w.x = c->mem_r32(w.pos + 0u);
  w.y = c->mem_r32(w.pos + 4u);
  w.z = c->mem_r32(w.pos + 8u);
  if (isNegative(w.x | w.y | w.z)) {
    // The sign test's delay slot clears v0 on both arms, and nothing has written v1 yet.
    c->r[2] = 0u;
    return;
  }
  buildCandidateList(w);
  bool done = false;
  std::uint32_t cell = kCandidateList;
  while (!done) {
    std::uint32_t record = c->mem_r32(cell);
    cell += 4u;
    if (record == 0u) {
      break; // the cell list is exhausted
    }
    record -= 4u; // the `beqz`'s delay slot
    for (;;) {
      record = c->mem_r32(record + 4u);
      // The `beqz` delay slot reads scratchpad word 0xC even on an exhausted chain.
      w.v1 = c->mem_r32(record + 0xCu);
      if (record == 0u) {
        break;
      }
      if (record == w.ignore) {
        continue; // the caller's moby, skipped after its y separation was read anyway
      }
      const Step step = walkRecord(w, record);
      if (step == Step::kDone) {
        done = true;
        break;
      }
    }
  }
  // 0x8004EB5C: $s2 is installed in the delay slot when the bit is clear; $s1 only when set.
  c->r[2] = (w.flags & 1u) == 0u ? w.firstHit : w.resolved;
  c->r[3] = w.v1;
}


// Moby fields; the two cleared flag bytes have no established name.
constexpr std::uint32_t kMobyCollisionGroup = 0x08u;
constexpr std::uint32_t kMobyCollisionRegion = 0x34u;
constexpr std::uint32_t kMobyFlagByteAt41 = 0x41u;
constexpr std::uint32_t kMobyFlagByteAt4B = 0x4bu;
constexpr std::uint32_t kNoCollisionRegion = 0xffffu;

// 0x800529CC: $at exits as 0xFFFFFFFF from the delay-slot halfword store.
void clearMobyCollisionState(Core *c) {
  const std::uint32_t moby = c->r[4];
  c->mem_w8(moby + kMobyFlagByteAt4B, 0);
  c->mem_w8(moby + kMobyFlagByteAt41, 0);
  c->mem_w32(moby + kMobyCollisionGroup, 0);
  c->r[1] = 0xffffffffu; // at — the immediate the delay slot's halfword store consumes
  c->mem_w16(moby + kMobyCollisionRegion, static_cast<std::uint16_t>(kNoCollisionRegion));
}

} // namespace

void registerMobyCollisionOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, 0x800529CCu, "clear_moby_collision_state", clearMobyCollisionState);
  psx::cpu::installNativeOverride(core, 0x8004E3C8u, "moby_collision_walk", mobyCollisionWalk);
}

} // namespace spyro1::native
