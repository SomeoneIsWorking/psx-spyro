// moby_record.h — the 0x58-byte moby record as all three images lay out the fields an in-between
// moves: the world position, the mesh and the three angle bytes; and one record moved between two
// fields.
#pragma once

#include <array>
#include <cstdint>

namespace spyro::moby_record {

inline constexpr std::uint32_t kBytes = 0x58;
inline constexpr std::uint32_t kX = 0x0C;
inline constexpr std::uint32_t kY = 0x10;
inline constexpr std::uint32_t kZ = 0x14;
inline constexpr std::uint32_t kMesh = 0x36;   // u16 index into the level's mesh table
inline constexpr std::uint32_t kAngles = 0x44; // angle bytes 0, 1, 2; byte 3 is not an angle

using Bytes = std::array<std::uint8_t, kBytes>;

// One object a field apart: the slot still holds the same mesh.
[[nodiscard]] bool sameObject(const Bytes &from, const Bytes &to);

// `to` with its position and angles `t` of the way from `from`'s, the angles the short way round.
[[nodiscard]] Bytes between(const Bytes &from, const Bytes &to, float t);

} // namespace spyro::moby_record
