#include "spyro1/render/local_bins.h"

#include "render/field/guest_working_memory.h"
#include "render/field/guest_word.h"
#include "render/terrain/guest_depth_bins.h"
#include "spyro1/render/actor_facts.h"

namespace spyro1::actors {
namespace {

using spyro::guest_word::asSigned;

} // namespace

void LocalBins::begin(const Start &start) {
  cursor_ = start.cursor;
  lowest_ = start.base + kLocalBinsBytes;
  highest_ = start.base;
}

std::uint32_t LocalBins::link(std::uint32_t slot) {
  const std::uint32_t packet = take();
  const std::uint32_t head = spyro::depth_bins::linkIntoBin(memory_, slot, packet, packet);
  if (head != 0) {
    return head;
  }
  if (asSigned(lowest_ - slot) > 0) {
    lowest_ = slot;
  }
  const std::uint32_t above = highest_ - slot;
  if (asSigned(above) < 0) {
    highest_ = slot;
  }
  return above;
}

std::uint32_t LocalBins::take() {
  const std::uint32_t packet = cursor_;
  cursor_ += ((memory_.mem_r32(packet) >> 24) + 1) * 4;
  return packet;
}

bool LocalBins::empty() const {
  return asSigned(highest_ - lowest_) < 0;
}

// 800257A0..800258AC (8002074C..8002085C).
LocalBins::Merged LocalBins::merge(const Target &target) {
  const std::uint32_t shift = target.shift & 31u;
  const std::uint32_t table = target.table;
  std::uint32_t at = (target.depthBin << 3) + ((0x20u << shift) - 8);
  std::uint32_t span = kLocalBins + 0x800u - highest_;
  const bool below = asSigned(span) < 0;
  span <<= shift;
  if (below) {
    span = 0;
  }
  at -= (span >> 8) << 3;
  std::uint32_t a0 = highest_;
  if (asSigned(at) < 0) {
    at = 0;
  }
  const Merged merged{.table = table, .stride = 0x100u >> shift};
  const std::uint32_t lowest = lowest_ - 8;
  at += table;
  for (;;) {
    std::uint32_t tail = memory_.mem_r32(at);
    std::uint32_t stop = a0 - merged.stride;
    if (asSigned(lowest - stop) > 0) {
      stop = lowest;
    }
    bool merging = true;
    if (tail == 0) {
      // The first non-empty local bin becomes the table bin's first packet.
      merging = false;
      while (a0 != stop) {
        const std::uint32_t first = memory_.mem_r32(a0 + 4);
        a0 -= 8;
        if (first != 0) {
          tail = memory_.mem_r32(a0 + 8);
          memory_.mem_w32(at + 4, first);
          memory_.mem_w32(a0 + 0xC, 0);
          memory_.mem_w32(a0 + 8, 0);
          merging = true;
          break;
        }
      }
    }
    while (merging && a0 != stop) {
      const std::uint32_t first = memory_.mem_r32(a0 + 4);
      a0 -= 8;
      if (first == 0) {
        continue;
      }
      const std::uint32_t last = memory_.mem_r32(a0 + 8);
      spyro::guest_memory::linkAfter(memory_, tail, first);
      tail = last;
      memory_.mem_w32(a0 + 8, 0);
      memory_.mem_w32(a0 + 0xC, 0);
    }
    memory_.mem_w32(at, tail);
    if (stop == lowest) {
      break;
    }
    if (at != table) {
      at -= 8;
    }
  }
  return merged;
}

} // namespace spyro1::actors
