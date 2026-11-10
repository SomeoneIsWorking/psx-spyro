#include "spyro1/core/frame/texture_scroll.h"

#include "blend.h"
#include "core.h"
#include "guest_redraw.h"

#include <algorithm>
#include <cstddef>

namespace spyro1 {
namespace {

// A writer of bytes into a record held as little-endian guest words.
template <std::size_t N> auto bytesInto(std::array<std::uint32_t, N> &words) {
  return [&words](std::uint32_t offset, std::uint8_t value) {
    const unsigned shift = 8u * (offset & 3u);
    std::uint32_t &word = words[offset >> 2];
    word = (word & ~(0xFFu << shift)) | (std::uint32_t{value} << shift);
  };
}

template <std::size_t N>
void readWords(Core &core, std::uint32_t at, std::array<std::uint32_t, N> &words) {
  for (std::size_t i = 0; i < N; ++i) {
    words[i] = core.mem_r32(at + static_cast<std::uint32_t>(4 * i));
  }
}

} // namespace

void TextureScroll::sample(Core &core) {
  std::swap(from_, to_);
  to_.clear();
  const std::uint32_t count = core.mem_r32(scroll::kNodeList);
  const std::uint32_t list = core.mem_r32(scroll::kNodeList + 4u);
  const std::uint32_t narrowTable = core.mem_r32(scroll::kNarrowRecords);
  const std::uint32_t wideTable = core.mem_r32(scroll::kWideRecords);
  if (!core.isGuestStorage(list, count * 4u)) {
    return;
  }
  for (std::uint32_t i = 0; i < count; ++i) {
    Node node{.address = core.mem_r32(list + i * 4u)};
    if (!core.isGuestStorage(node.address, scroll::kNodePosition + 2u)) {
      continue;
    }
    node.index = core.mem_r16(node.address + scroll::kNodeIndex);
    node.position = core.mem_r16(node.address + scroll::kNodePosition);
    node.narrow = narrowTable + node.index * scroll::kNarrowRecordSize;
    node.wide = wideTable + node.index * scroll::kWideRecordSize;
    if (!core.isGuestStorage(node.narrow, scroll::kNarrowRecordSize) ||
        !core.isGuestStorage(node.wide, scroll::kWideRecordSize)) {
      continue;
    }
    readWords(core, node.narrow, node.narrowWords);
    readWords(core, node.wide, node.wideWords);
    to_.push_back(node);
  }
}

void TextureScroll::write(psx::present::GuestRedraw &redraw, float t) const {
  for (const Node &later : to_) {
    const auto earlier = std::find_if(from_.begin(), from_.end(), [&later](const Node &node) {
      return node.address == later.address && node.index == later.index && node.wide == later.wide;
    });
    if (earlier == from_.end() || earlier->position == later.position) {
      continue;
    }
    const std::uint32_t position =
        psx::present::lerpTurn(earlier->position, later.position, scroll::kPositionBits, t);
    NarrowWords narrow = later.narrowWords;
    WideWords wide = later.wideWords;
    scroll::bake(position, bytesInto(narrow), bytesInto(wide));
    for (std::size_t i = 0; i < narrow.size(); ++i) {
      redraw.writeWord(later.narrow + static_cast<std::uint32_t>(4 * i), narrow[i]);
    }
    for (std::size_t i = 0; i < wide.size(); ++i) {
      redraw.writeWord(later.wide + static_cast<std::uint32_t>(4 * i), wide[i]);
    }
  }
}

} // namespace spyro1
