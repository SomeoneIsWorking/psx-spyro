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
  const AnglesBetween angles = anglesBetween(record_blend::load<std::uint32_t>(from, kAngles),
                                             record_blend::load<std::uint32_t>(to, kAngles),
                                             t);
  for (std::uint32_t axis = 0; axis < 3; ++axis) {
    shown.bytes[kAngles + axis] = static_cast<std::uint8_t>(angles.bytes >> (8 * axis));
  }
  shown.angleFractions = angles.fractions;
  return shown;
}

AnglesBetween anglesBetween(std::uint32_t from, std::uint32_t to, float t) {
  AnglesBetween shown;
  for (std::uint32_t axis = 0; axis < 3; ++axis) {
    const std::uint32_t shift = 8 * axis;
    const std::uint32_t angle =
        psx::present::lerpTurn(((from >> shift) & 0xFFu) << 4, ((to >> shift) & 0xFFu) << 4, 12, t);
    shown.bytes |= (angle >> 4) << shift;
    shown.fractions |= (angle & 0xFu) << shift;
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
