#include "render/field/moby_record.h"

#include "render/field/record_blend.h"

namespace spyro::moby_record {

bool sameObject(const Bytes &from, const Bytes &to) {
  return record_blend::load<std::uint16_t>(from, kMesh) ==
         record_blend::load<std::uint16_t>(to, kMesh);
}

Between between(const Bytes &from, const Bytes &to, float t) {
  Between shown{.bytes = to};
  for (const std::uint32_t at : {kX, kY, kZ}) {
    record_blend::wordBetween(shown.bytes, from, to, at, t);
  }
  for (std::uint32_t axis = 0; axis < 3; ++axis) {
    const std::uint32_t angle = psx::present::lerpTurn(std::uint32_t{from[kAngles + axis]} << 4,
                                                       std::uint32_t{to[kAngles + axis]} << 4,
                                                       12,
                                                       t);
    shown.bytes[kAngles + axis] = static_cast<std::uint8_t>(angle >> 4);
    shown.angleFractions |= (angle & 0xFu) << (8 * axis);
  }
  return shown;
}

Bytes nearest(const Between &shown) {
  Bytes bytes = shown.bytes;
  for (std::uint32_t axis = 0; axis < 3; ++axis) {
    if (((shown.angleFractions >> (8 * axis)) & 0xFu) >= 8) {
      ++bytes[kAngles + axis];
    }
  }
  return bytes;
}

} // namespace spyro::moby_record
