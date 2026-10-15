// spyro1_actor_between.h — what Spyro 1's actor producers share to draw an in-between: a record's
// words and angle bytes moved toward the next field's, and the in-between camera mapped where the
// actor routines read it.
#pragma once

#include "guest_working_memory.h"
#include "state_producer.h"
#include "terrain_rebuild.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

class Core;

namespace spyro1::actors {

// A little-endian word of a saved record's bytes.
template <std::size_t N>
std::uint32_t loadWord(const std::array<std::uint8_t, N> &bytes, std::uint32_t at) {
  return bytes[at] | (std::uint32_t{bytes[at + 1]} << 8) | (std::uint32_t{bytes[at + 2]} << 16) |
         (std::uint32_t{bytes[at + 3]} << 24);
}

template <std::size_t N>
void storeWord(std::array<std::uint8_t, N> &bytes, std::uint32_t at, std::uint32_t word) {
  for (std::uint32_t i = 0; i < 4; ++i) {
    bytes[at + i] = static_cast<std::uint8_t>(word >> (8 * i));
  }
}

// A signed position word at `t` from `from` to `to`.
[[nodiscard]] std::uint32_t wordBetween(std::uint32_t from, std::uint32_t to, float t);
// An angle byte, 256 steps a turn, at `t` the short way round.
[[nodiscard]] std::uint8_t angleBetween(std::uint8_t from, std::uint8_t to, float t);

// A moby record's bytes.
using MobyBytes = std::array<std::uint8_t, 0x58>;
// `from` and `to`'s record at `t`: position and angles moved, everything else `to`'s.
[[nodiscard]] MobyBytes recordBetween(const MobyBytes &from, const MobyBytes &to, float t);

// Every packet an in-between linked into the depth bins at `table`, deepest bin first, each in its
// bin's slot, a draw-mode packet applied to the untextured ones after it in its bin; `who` names
// the producer when a packet is not a primitive.
void emitDepthBins(const spyro::guest_memory::WorkingMemory &memory,
                   const spyro::guest_memory::PacketArena &arena,
                   std::uint32_t table,
                   psx::present::PrimitiveSink &sink,
                   const char *who);

// What a pass that merges into the depth bins writes on the real field: the pool from the cursor,
// the table and the cursor.
[[nodiscard]] std::array<spyro::guest_memory::GuestRange, 3> binnedWorkingRanges(Core &core);

// A record's bytes from guest memory, and mapped into an in-between's host memory.
void readBytes(Core &core, std::uint32_t base, std::span<std::uint8_t> bytes);
void mapBytes(spyro::guest_memory::HostMemory &host,
              std::uint32_t base,
              std::span<const std::uint8_t> bytes);

// The camera block at 0x80076DD0: five rotation words, the position from +0x28.
void mapCamera(spyro::guest_memory::HostMemory &host,
               const spyro::guest_terrain::InBetweenCamera &camera);

// What a pass that merges into the depth bins draws an in-between over: the scratchpad, the work
// block and the arena empty, the camera, an empty table and the cursor at the arena's base. Returns
// the table.
std::uint32_t mapBinnedWorkspace(spyro::guest_memory::HostMemory &host,
                                 const spyro::guest_memory::PacketArena &arena,
                                 const spyro::guest_terrain::InBetweenCamera &camera);

} // namespace spyro1::actors
