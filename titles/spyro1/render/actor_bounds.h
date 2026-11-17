// actor_bounds.h — an actor record's bounding sphere, as the list builder or the sprite queue
// culls it (actor_list.cpp, sprite_queue.cpp).
#pragma once

#include <cstdint>

class Core;

namespace spyro1::actors {

// The bounding sphere radius record `record` is culled by, view units: its model's first frame's
// for the list builder, the sprite queue's fixed one for a queued record; 0 for none or a HUD one.
[[nodiscard]] std::uint32_t boundingRadius(Core &core, std::uint32_t record);

} // namespace spyro1::actors
