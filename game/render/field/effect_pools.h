// effect_pools.h — the records the dispatcher's effects pass (flag 0x10) draws from, read once per
// field and moved between two fields for the replay. Each pool is a fixed array the
// level start clears; a slot is one effect while it stays live with the same kind, and a particle
// slot until the allocator hands it out again. Drawer addresses below are SCUS_944.25's.
#pragma once

#include "guest_redraw.h"
#include "motion_set.h"

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <vector>

class Core;

namespace spyro::effect_pools {

struct Facts {
  std::uint32_t sprites = 0;    // 0x8004A8D4's 16 records of 0x14
  std::uint32_t glows = 0;      // 0x8004AC78's 16 records of 0x10
  std::uint32_t beams = 0;      // 0x8004B194's 8 records of 0x30
  std::uint32_t trails = 0;     // 0x8004B72C's 32 records
  std::uint32_t trailBytes = 0; // one trail record
  std::uint32_t trailHead = 0;  // its head byte, then its count byte; the point ring ends here
  std::uint32_t meshes = 0;     // 0x8004BAA4's 8 entries of 0x14, each naming a vertex list
  std::uint32_t particles = 0;  // the word naming 0x80048128's pool of 256 records of 0x20
  std::uint32_t allocator = 0;  // stamps a0 as the kind of a slot it hands out, returned in v0
};

inline constexpr std::uint32_t kSpriteBytes = 0x14;
inline constexpr std::uint32_t kGlowBytes = 0x10;
inline constexpr std::uint32_t kBeamBytes = 0x30;
inline constexpr std::uint32_t kTrailMaxBytes = 0x6C; // the larger of the two images' records
inline constexpr std::uint32_t kMeshBytes = 0x14;
inline constexpr std::uint32_t kParticleBytes = 0x20;
inline constexpr std::size_t kParticles = 256;

template <std::size_t Bytes, std::size_t Count>
using Pool = std::array<std::array<std::uint8_t, Bytes>, Count>;

// One field's pools, as its draw read them.
struct Records {
  Pool<kSpriteBytes, 16> sprites{};
  Pool<kGlowBytes, 16> glows{};
  Pool<kBeamBytes, 8> beams{};
  Pool<kTrailMaxBytes, 32> trails{}; // each record's first `trailBytes`
  Pool<kMeshBytes, 8> meshes{};
  std::array<std::vector<std::uint32_t>, 8> meshVertices; // x, y, z words up to the -1
  std::uint32_t particlePool = 0;
  Pool<kParticleBytes, kParticles> particles{};
  std::bitset<kParticles> spawned; // slots the allocator handed out since the sample before
};

void read(Core &core, const Facts &facts, Records &records);

// `to`'s effects with each one that is the same effect in `from` moved `t` of the way from there;
// `to`'s live effects and how many of them had none.
psx::present::MotionCount writeBetween(psx::present::GuestRedraw &run,
                                       const Facts &facts,
                                       const Records &from,
                                       const Records &to,
                                       float t);

} // namespace spyro::effect_pools
