// drawn_records.h — the mobys a draw builds itself in its buffer's packet pool each draw (menu items,
// dialogue and title letters), whose spins and slides come from a count the draw reads whole
// (the menus' D_800758B8, the balloonist's D_800777E8 +0x04): what the last real draw queued for
// RasterizeSpritePrimQueue, by slot, and a replay's record moved there from it.
#pragma once

#include "render/field/moby_record.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

class Core;

namespace spyro1::actors {

class DrawnRecords {
public:
  // The draw queued `record`, `slot` bytes below the arena end it was taken from (the arena moves
  // with the draw buffer; the slot is its place in the draw's build order). Outside a replay it
  // is saved as the real draw's; inside one it is written with its position and angles the
  // replay's `t` of the way from the last real draw's (the same mesh in the same slot) and the
  // angles' sixteenths are returned.
  std::optional<std::uint32_t> queued(Core &core, std::uint32_t record, std::uint32_t slot);

private:
  using Saved = std::pair<std::uint32_t, spyro::moby_record::Bytes>;
  std::uint32_t frame_ = 0;
  // The latest real draw's; a frame's replays run before its real draw.
  std::vector<Saved> drawn_;
};

} // namespace spyro1::actors
