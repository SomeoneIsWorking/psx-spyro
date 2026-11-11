// drawn_records.h — the mobys a state draw builds itself each draw (the menus' g_HudMobys, whose
// spins and slides come from the draw's own count D_800758B8, read whole): what the last real
// draw queued for RasterizeSpritePrimQueue, by record address, and a replay's record moved there
// from it.
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
  // The draw queued `record`. Outside a replay it is saved as the real draw's; inside one it is
  // written with its position and angles the replay's `t` of the way from the last real draw's
  // (the same mesh at the same address) and the angles' sixteenths are returned.
  std::optional<std::uint32_t> queued(Core &core, std::uint32_t record);

private:
  using Saved = std::pair<std::uint32_t, spyro::moby_record::Bytes>;
  std::uint32_t frame_ = 0;
  // The latest real draw's; a frame's replays run before its real draw.
  std::vector<Saved> drawn_;
};

} // namespace spyro1::actors
