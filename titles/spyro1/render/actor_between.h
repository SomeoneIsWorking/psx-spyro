// actor_between.h — what Spyro 1's actor producers share: a saved record moved toward the next field's,
// the ranges a real field's routine owns, and one in-between run of a routine over host memory.
#pragma once

#include "blend.h"
#include "emit_memory.h"
#include "gp0_primitive_decode.h"
#include "gte_control.h"
#include "render/field/guest_working_memory.h"
#include "render/terrain/terrain_rebuild.h"
#include "state_producer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

class Core;

namespace spyro1::actors {

// A little-endian field of a saved record's bytes.
template <typename Value, std::size_t N> Value load(const std::array<std::uint8_t, N> &bytes, std::uint32_t at) {
  Value value;
  std::memcpy(&value, bytes.data() + at, sizeof(Value));
  return value;
}

template <typename Value, std::size_t N> void store(std::array<std::uint8_t, N> &bytes, std::uint32_t at, Value value) {
  std::memcpy(bytes.data() + at, &value, sizeof(Value));
}

// The signed word at `at` of two records, at `t`, into `shown`.
template <std::size_t N>
void wordBetween(std::array<std::uint8_t, N> &shown,
                 const std::array<std::uint8_t, N> &from,
                 const std::array<std::uint8_t, N> &to,
                 std::uint32_t at,
                 float t) {
  store(shown, at, psx::present::lerpWord(load<std::uint32_t>(from, at), load<std::uint32_t>(to, at), t));
}

// An angle byte, 256 steps a turn, at `t` the short way round.
[[nodiscard]] inline std::uint8_t angleBetween(std::uint8_t from, std::uint8_t to, float t) {
  return static_cast<std::uint8_t>(psx::present::lerpTurn(from, to, 8, t));
}

// A moby record's bytes.
using MobyBytes = std::array<std::uint8_t, 0x58>;
// `from` and `to`'s record at `t`: position and angles moved, everything else `to`'s.
[[nodiscard]] MobyBytes recordBetween(const MobyBytes &from, const MobyBytes &to, float t);

// What a pass that merges into the depth bins writes on the real field: the pool from the cursor, the
// table and the cursor, then `extra`.
[[nodiscard]] std::array<GuestSpan, 3> binnedWorkingRanges(Core &core);
template <typename... Extra>
[[nodiscard]] std::array<GuestSpan, 3 + sizeof...(Extra)> binnedWorkingRanges(Core &core, Extra... extra) {
  const std::array<GuestSpan, 3> binned = binnedWorkingRanges(core);
  return {binned[0], binned[1], binned[2], extra...};
}

// One run of a Spyro 1 render routine over host memory: the GTE handed back when it ends, the field's
// level words overlaid, the packet arena empty and the cursor and pool end around it.
class InBetween {
public:
  InBetween(Core &core, std::span<const psx::present::LevelWord> reads);
  InBetween(const InBetween &) = delete;
  InBetween &operator=(const InBetween &) = delete;

  [[nodiscard]] const psx::present::EmitMemory &memory() const {
    return memory_;
  }
  [[nodiscard]] psx::present::HostMemory &host() {
    return host_;
  }

  // `bytes` at `address` in place of the guest's.
  void provide(std::uint32_t address, std::span<const std::uint8_t> bytes);
  void provideWord(std::uint32_t address, std::uint32_t word);
  // The scratchpad vertex cache and the work block, empty.
  void mapWorkspace();
  // The camera block at 0x80076DD0: the projection rotation, the view rotation at +0x14, the position at +0x28.
  void mapCamera(const spyro::guest_terrain::InBetweenCamera &camera);
  // An empty ordering table at the base the field published.
  void mapTable();

  // Every packet linked into the table's bins, deepest bin first, under one draw mode.
  void emitBins(psx::present::PrimitiveSink &sink) const;
  // The chain from `first` into `slot`.
  void emitChain(std::uint32_t first,
                 psx::present::OtSlot slot,
                 psx::present::DrawModeTracker &drawMode,
                 psx::present::PrimitiveSink &sink) const;

private:
  psx::present::GteGuard gte_;
  psx::present::HostMemory host_;
  psx::present::EmitMemory memory_;
  spyro::guest_memory::PacketArena arena_;
  std::uint32_t table_ = 0;
};

} // namespace spyro1::actors
