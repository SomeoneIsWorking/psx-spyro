#include "render/field/moby_record.h"

#include "render/field/record_blend.h"

namespace spyro::moby_record {

bool sameObject(const Bytes &from, const Bytes &to) {
  return record_blend::load<std::uint16_t>(from, kMesh) ==
         record_blend::load<std::uint16_t>(to, kMesh);
}

Bytes between(const Bytes &from, const Bytes &to, float t) {
  Bytes shown = to;
  for (const std::uint32_t at : {kX, kY, kZ}) {
    record_blend::wordBetween(shown, from, to, at, t);
  }
  for (std::uint32_t axis = 0; axis < 3; ++axis) {
    shown[kAngles + axis] = record_blend::angleBetween(from[kAngles + axis], to[kAngles + axis], t);
  }
  return shown;
}

} // namespace spyro::moby_record
