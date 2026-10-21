// The guest's own camera builder, re-run over the guest's own tables.

#include "render/terrain/guest_camera_builder.h"

#include "blend.h"
#include "gte_registers.h"

#include <algorithm>

namespace spyro::guest_camera {
namespace {

// The GTE's rotation elements are 12-bit fractions, so MVMVA's shift is 12.
constexpr std::int32_t kShift = 12;
// FUN_80059d6C saturates its products because the GTE wrote them.
constexpr std::int32_t kMin = -0x8000;
constexpr std::int32_t kMax = 0x7FFF;

} // namespace

Rotation multiply(const Rotation &ir, const Rotation &matrix) {
  Rotation out{};
  for (std::int32_t column = 0; column < 3; ++column) {
    for (std::int32_t row = 0; row < 3; ++row) {
      const std::int64_t sum = static_cast<std::int64_t>(ir.row[row][0]) * matrix.row[0][column] +
                               static_cast<std::int64_t>(ir.row[row][1]) * matrix.row[1][column] +
                               static_cast<std::int64_t>(ir.row[row][2]) * matrix.row[2][column];
      out.row[row][column] =
          static_cast<std::int32_t>(std::clamp<std::int64_t>(sum >> kShift, kMin, kMax));
    }
  }
  return out;
}

namespace {

// The five control words, in the packing both the drawer and the classification pass read. The
// last holds one element, sign-extended as the builders store it.
std::array<std::uint32_t, psx::gte::kRotationWords> controlWords(const Rotation &matrix) {
  const auto element = [](std::int32_t value) {
    return static_cast<std::uint32_t>(static_cast<std::uint16_t>(static_cast<std::int16_t>(value)));
  };
  return {element(matrix.row[0][0]) | (element(matrix.row[0][1]) << 16),
          element(matrix.row[0][2]) | (element(matrix.row[1][0]) << 16),
          element(matrix.row[1][1]) | (element(matrix.row[1][2]) << 16),
          element(matrix.row[2][0]) | (element(matrix.row[2][1]) << 16),
          static_cast<std::uint32_t>(
              static_cast<std::int32_t>(static_cast<std::int16_t>(matrix.row[2][2])))};
}

struct SinCos {
  std::int32_t cos = 0;
  std::int32_t sin = 0;
};

// The order 0x8001c2f8 lays the rotations out: Ry into halfwords 0..9, Rz into 10..19, Rx
// into 20..29.

// Ry, on rows 1 and 2 with x fixed.
Rotation aroundY(SinCos t) {
  Rotation out{};
  out.row[0][0] = 0x1000;
  out.row[1][1] = t.cos;
  out.row[1][2] = -t.sin;
  out.row[2][1] = t.sin;
  out.row[2][2] = t.cos;
  return out;
}

// Rz, on rows 0 and 2 with y fixed.
Rotation aroundZ(SinCos t) {
  Rotation out{};
  out.row[0][0] = t.cos;
  out.row[0][2] = t.sin;
  out.row[1][1] = 0x1000;
  out.row[2][0] = -t.sin;
  out.row[2][2] = t.cos;
  return out;
}

// Rx, on rows 0 and 1 with z fixed.
Rotation aroundX(SinCos t) {
  Rotation out{};
  out.row[0][0] = t.cos;
  out.row[0][1] = t.sin;
  out.row[1][0] = -t.sin;
  out.row[1][1] = t.cos;
  out.row[2][2] = 0x1000;
  return out;
}

} // namespace

std::int32_t AngleTable::entry(std::uint32_t index) const {
  return core_.mem_r16s(base_ + 2 * index);
}

std::int32_t AngleTable::at(std::uint32_t angle) const {
  const std::uint32_t index = (angle & 0xFFFu) >> 4;
  const std::int32_t value = entry(index);
  const std::uint32_t fraction = angle & 0xFu;
  if (fraction == 0) {
    return value;
  }
  // The fraction scales the step to the next table entry and the whole thing shifts down by four;
  // truncating toward negative infinity is what the guest's `>>` does.
  return value + ((static_cast<std::int32_t>(fraction) * (entry(index + 1) - value)) >> 4);
}

Matrices Builder::build(const Angles &angles) const {
  const auto y = static_cast<std::uint32_t>(angles.aroundY);
  const auto z = static_cast<std::uint32_t>(angles.aroundZ);
  const auto x = static_cast<std::uint32_t>(angles.aroundX);
  Rotation view = compose(aroundX({.cos = cos(x), .sin = sin(x)}),
                          aroundY({.cos = cos(y), .sin = sin(y)}),
                          aroundZ({.cos = cos(z), .sin = sin(z)}));
  Matrices out{};
  out.view = controlWords(view);
  for (std::int32_t column = 0; column < 3; ++column) {
    view.row[1][column] = scaleVertical(view.row[1][column]);
  }
  out.projection = controlWords(view);
  return out;
}

Angles interpolate(const Angles &from, const Angles &to, float t) {
  // The short way: 4095 and 1 are one step apart in the space the tables cover.
  const auto angle = [t](std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(psx::present::lerpTurn(
        static_cast<std::uint32_t>(a), static_cast<std::uint32_t>(b), kTurnBits, t));
  };
  return Angles{angle(from.aroundX, to.aroundX), angle(from.aroundY, to.aroundY), angle(from.aroundZ, to.aroundZ)};
}

} // namespace spyro::guest_camera