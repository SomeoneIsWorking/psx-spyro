#include "render/field/effect_pools.h"

#include "blend.h"
#include "core.h"
#include "render/field/record_blend.h"

#include <span>

namespace spyro::effect_pools {
namespace {

using record_blend::load;

// Sprites, 0x8004A8D4: position words, colour, then kind, size, spin and depth bytes.
namespace sprite {
inline constexpr std::uint32_t kColour = 0x0C;
inline constexpr std::uint32_t kKind = 0x10; // 0: free
inline constexpr std::uint32_t kSpin = 0x12; // the draw steps it
} // namespace sprite

// Glows, 0x8004AC78: position words, then life, size, spin and kind bytes.
namespace glow {
inline constexpr std::uint32_t kLife = 0x0C; // 0: free; the draw counts it down
inline constexpr std::uint32_t kKind = 0x0F;
} // namespace glow

// Beams, 0x8004B194: start and end position words, colour, segment count.
namespace beam {
inline constexpr std::uint32_t kEnd = 0x0C;
inline constexpr std::uint32_t kColour = 0x18;
inline constexpr std::uint32_t kSegments = 0x1C; // 0: free
} // namespace beam

// Trails, 0x8004B72C (filled by 0x800502FC): owner, a ring of halfword points, head and count.
namespace trail {
inline constexpr std::uint32_t kOwner = 0x00; // 0: free
inline constexpr std::uint32_t kPoints = 0x04;
inline constexpr std::uint32_t kPointBytes = 6;
inline constexpr std::uint32_t kHead = 0x28;
inline constexpr std::uint32_t kCount = 0x29; // 2 to 6
} // namespace trail

// Particles, 0x80048128: texture, kind and age bytes, then per kind a point or a line.
namespace particle {
inline constexpr std::uint32_t kKind = 0x01; // low nibble; negative: skipped, 0xFF ends the pool
inline constexpr std::uint32_t kAge = 0x02;
inline constexpr std::uint32_t kPoint = 0x04;  // x, y, z halfwords
inline constexpr std::uint32_t kSecond = 0x0A; // kind 1: the line's other end
inline constexpr std::uint32_t kSize = 0x0A;   // other kinds: size byte, then a spin byte
inline constexpr std::uint32_t kSpin = 0x0B;   // kind 3: a second size
inline constexpr std::uint8_t kEnd = 0xFF;
inline constexpr std::uint32_t kLine = 1;
inline constexpr std::uint32_t kStretched = 3;
} // namespace particle

template <std::size_t Bytes, std::size_t Count>
void readPool(Core &core, std::uint32_t address, Pool<Bytes, Count> &pool) {
  core.readBytes(address, std::as_writable_bytes(std::span(pool)));
}

// The words of `shown` that differ from `to`'s, written at `address`.
template <std::size_t N>
void writeChanged(psx::present::GuestRedraw &run,
                  std::uint32_t address,
                  const std::array<std::uint8_t, N> &to,
                  const std::array<std::uint8_t, N> &shown) {
  for (std::uint32_t at = 0; at < N; at += 4) {
    if (load<std::uint32_t>(shown, at) != load<std::uint32_t>(to, at)) {
      run.writeWord(address + at, load<std::uint32_t>(shown, at));
    }
  }
}

// The three position words from `at`.
template <std::size_t N>
void positionBetween(std::array<std::uint8_t, N> &shown,
                     const std::array<std::uint8_t, N> &from,
                     const std::array<std::uint8_t, N> &to,
                     std::uint32_t at,
                     float t) {
  for (std::uint32_t axis = 0; axis < 3; ++axis) {
    record_blend::wordBetween(shown, from, to, at + 4 * axis, t);
  }
}

template <std::size_t N>
void pointBetween(std::array<std::uint8_t, N> &shown,
                  const std::array<std::uint8_t, N> &from,
                  const std::array<std::uint8_t, N> &to,
                  std::uint32_t at,
                  float t) {
  for (std::uint32_t axis = 0; axis < 3; ++axis) {
    record_blend::halfBetween(shown, from, to, at + 2 * axis, t);
  }
}

// Each slot that `same` pairs, moved by `move`, written over `to`'s.
template <std::size_t Bytes, std::size_t Count, typename Same, typename Move>
void poolBetween(psx::present::GuestRedraw &run,
                 std::uint32_t address,
                 const Pool<Bytes, Count> &from,
                 const Pool<Bytes, Count> &to,
                 Same same,
                 Move move) {
  for (std::size_t i = 0; i < Count; ++i) {
    if (!same(from[i], to[i])) {
      continue;
    }
    std::array<std::uint8_t, Bytes> shown = to[i];
    move(shown, from[i], to[i]);
    writeChanged(run, address + static_cast<std::uint32_t>(i * Bytes), to[i], shown);
  }
}

void meshesBetween(psx::present::GuestRedraw &run,
                   const Records &from,
                   const Records &to,
                   float t) {
  for (std::size_t i = 0; i < to.meshes.size(); ++i) {
    const auto list = load<std::uint32_t>(to.meshes[i], 0);
    const std::vector<std::uint32_t> &before = from.meshVertices[i];
    const std::vector<std::uint32_t> &after = to.meshVertices[i];
    if (list == 0 || list != load<std::uint32_t>(from.meshes[i], 0) ||
        before.size() != after.size()) {
      continue;
    }
    for (std::size_t w = 0; w < after.size(); ++w) {
      if (before[w] != after[w]) {
        run.writeWord(list + static_cast<std::uint32_t>(w) * 4,
                      psx::present::lerpWord(before[w], after[w], t));
      }
    }
  }
}

void particlesBetween(psx::present::GuestRedraw &run,
                      const Records &from,
                      const Records &to,
                      float t) {
  if (from.particlePool != to.particlePool) {
    return;
  }
  for (std::size_t i = 0; i < to.particles.size(); ++i) {
    const auto &before = from.particles[i];
    const auto &after = to.particles[i];
    if (after[particle::kKind] == particle::kEnd) {
      return;
    }
    if (before[0] != after[0] || before[particle::kKind] != after[particle::kKind] ||
        static_cast<std::int8_t>(after[particle::kKind]) < 0 ||
        after[particle::kAge] < before[particle::kAge]) {
      continue;
    }
    std::array<std::uint8_t, kParticleBytes> shown = after;
    pointBetween(shown, before, after, particle::kPoint, t);
    const std::uint32_t kind = after[particle::kKind] & 0xFu;
    if (kind == particle::kLine) {
      pointBetween(shown, before, after, particle::kSecond, t);
    } else if (kind != 0) {
      shown[particle::kSize] =
          record_blend::byteBetween(before[particle::kSize], after[particle::kSize], t);
      shown[particle::kSpin] =
          kind == particle::kStretched
              ? record_blend::byteBetween(before[particle::kSpin], after[particle::kSpin], t)
              : record_blend::angleBetween(before[particle::kSpin], after[particle::kSpin], t);
    }
    writeChanged(
        run, to.particlePool + static_cast<std::uint32_t>(i) * kParticleBytes, after, shown);
  }
}

} // namespace

void read(Core &core, const Facts &facts, Records &records) {
  readPool(core, facts.sprites, records.sprites);
  readPool(core, facts.glows, records.glows);
  readPool(core, facts.beams, records.beams);
  readPool(core, facts.trails, records.trails);
  readPool(core, facts.meshes, records.meshes);
  for (std::size_t i = 0; i < records.meshes.size(); ++i) {
    std::vector<std::uint32_t> &vertices = records.meshVertices[i];
    vertices.clear();
    for (std::uint32_t at = load<std::uint32_t>(records.meshes[i], 0);
         at != 0 && core.isGuestStorage(at, 12) && core.mem_r32(at) != 0xFFFFFFFFu;
         at += 12) {
      vertices.insert(vertices.end(),
                      {core.mem_r32(at), core.mem_r32(at + 4), core.mem_r32(at + 8)});
    }
  }
  records.particlePool = core.mem_r32(facts.particles);
  if (core.isGuestStorage(records.particlePool, sizeof(records.particles))) {
    readPool(core, records.particlePool, records.particles);
  } else {
    records.particles[0][particle::kKind] = particle::kEnd;
  }
}

void writeBetween(psx::present::GuestRedraw &run,
                  const Facts &facts,
                  const Records &from,
                  const Records &to,
                  float t) {
  poolBetween(
      run,
      facts.sprites,
      from.sprites,
      to.sprites,
      [](const auto &a, const auto &b) {
        return b[sprite::kKind] != 0 && a[sprite::kKind] == b[sprite::kKind] &&
               load<std::uint32_t>(a, sprite::kColour) == load<std::uint32_t>(b, sprite::kColour);
      },
      [t](auto &shown, const auto &a, const auto &b) {
        positionBetween(shown, a, b, 0, t);
        shown[sprite::kSpin] = record_blend::angleBetween(a[sprite::kSpin], b[sprite::kSpin], t);
      });
  poolBetween(
      run,
      facts.glows,
      from.glows,
      to.glows,
      [](const auto &a, const auto &b) {
        return b[glow::kLife] != 0 && b[glow::kLife] <= a[glow::kLife] &&
               a[glow::kKind] == b[glow::kKind];
      },
      [t](auto &shown, const auto &a, const auto &b) {
        positionBetween(shown, a, b, 0, t);
        shown[glow::kLife] = record_blend::byteBetween(a[glow::kLife], b[glow::kLife], t);
      });
  poolBetween(
      run,
      facts.beams,
      from.beams,
      to.beams,
      [](const auto &a, const auto &b) {
        return b[beam::kSegments] != 0 && a[beam::kSegments] == b[beam::kSegments] &&
               load<std::uint32_t>(a, beam::kColour) == load<std::uint32_t>(b, beam::kColour);
      },
      [t](auto &shown, const auto &a, const auto &b) {
        positionBetween(shown, a, b, 0, t);
        positionBetween(shown, a, b, beam::kEnd, t);
      });
  poolBetween(
      run,
      facts.trails,
      from.trails,
      to.trails,
      [](const auto &a, const auto &b) {
        const std::uint32_t count = b[trail::kCount];
        return load<std::uint32_t>(b, trail::kOwner) != 0 &&
               load<std::uint32_t>(a, trail::kOwner) == load<std::uint32_t>(b, trail::kOwner) &&
               a[trail::kCount] == count && count >= 2 && count <= 6;
      },
      [t](auto &shown, const auto &a, const auto &b) {
        // The point `age` fields old, in each ring.
        const std::uint32_t count = b[trail::kCount];
        const auto slot = [count](const auto &ring, std::uint32_t age) {
          return trail::kPoints + ((ring[trail::kHead] + count - age) % count) * trail::kPointBytes;
        };
        for (std::uint32_t age = 0; age < count; ++age) {
          const std::uint32_t before = slot(a, age);
          const std::uint32_t after = slot(b, age);
          for (std::uint32_t axis = 0; axis < 3; ++axis) {
            record_blend::store(shown,
                                after + 2 * axis,
                                static_cast<std::uint16_t>(
                                    psx::present::lerpInt(load<std::uint16_t>(a, before + 2 * axis),
                                                          load<std::uint16_t>(b, after + 2 * axis),
                                                          t)));
          }
        }
      });
  meshesBetween(run, from, to, t);
  particlesBetween(run, from, to, t);
}

} // namespace spyro::effect_pools
