// terrain_state_producer.h — the terrain drawer as a state producer: what one field saves, and the
// render that draws the terrain at any t between two saved fields.
//
// The field's packets are the guest's own. The state is only what the drawer needs to be re-run
// elsewhere: the camera, the sectors the guest's visibility call named and every guest word the
// drawer read, which a later field rewrites before this one is presented. The render runs the
// drawer over host memory (terrain_rebuild.h) and hands each packet it linked to the sink in the
// bin it was linked into.
#pragma once

#include "core.h"
#include "guest_render_globals.h"
#include "guest_terrain_facts.h"
#include "state_producer.h"
#include "terrain_rebuild.h"

#include <cstdint>
#include <span>
#include <type_traits>

namespace spyro::guest_terrain {

// A saved field is this header followed by the guest words the field read.
struct FieldState {
  CameraState camera;
  std::uint32_t argument = 0;
  std::int32_t packetBudget = 0;
  std::uint32_t sectorCount = 0;
  std::uint8_t visible[kScratchpadBytes] = {};
};
static_assert(std::is_trivially_copyable_v<FieldState>);

// The header of a saved field. Aborts when `state` is not one.
[[nodiscard]] FieldState fieldHeader(std::span<const std::byte> state);

// The real drawer's field, saved for the scope it draws in. Aborts when the level declares more
// sectors than the state holds.
void drawField(Core &core, const TerrainImage &image);

class TerrainStateProducer final : public psx::present::StateProducer {
public:
  TerrainStateProducer(Core &core, const TerrainImage &image) : core_(core), image_(image) {}

  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;

private:
  Core &core_;
  const TerrainImage &image_;
};

} // namespace spyro::guest_terrain
