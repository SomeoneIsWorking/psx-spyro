// terrain_state_producer.h — the terrain drawer as a state producer: what one field saves, and the
// render that draws the terrain at any t between two saved fields.
//
// The field's packets are the guest's own. The state is only what the drawer needs to be re-run
// elsewhere: the camera, the sectors the guest's visibility call named and every guest word the
// drawer read, which a later field rewrites before this one is presented. An in-between runs the
// drawer over both fields' words (terrain_rebuild.h); a word neither saved comes from the guest,
// and only while no disc read has landed since the earlier field. The render hands each packet the
// rebuild linked to the sink in the bin it was linked into.
#pragma once

#include "core.h"
#include "render/field/guest_render_globals.h"
#include "render/terrain/guest_terrain_facts.h"
#include "render/terrain/terrain_rebuild.h"
#include "state_producer.h"

#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>

namespace spyro::guest_terrain {

// A saved field is this header followed by the guest words the field read.
struct FieldState {
  CameraState camera;
  std::uint32_t argument = 0;
  std::int32_t packetBudget = 0;
  std::uint32_t sectorCount = 0;
  // Session::discLandings when the field drew.
  std::uint64_t discLandings = 0;
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

  // Runs the drawer for the interval; refused when it read a word neither field saved after a disc
  // read landed, since that word may already be the next level's.
  [[nodiscard]] Composition composition(const psx::present::StateInterval &states) const override;
  // Emits the rebuild the composition just ran.
  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;

private:
  Core &core_;
  const TerrainImage &image_;
  // The composer renders right after composing, so the drawer runs once per in-between.
  mutable std::optional<Rebuild> rebuild_;
  mutable psx::present::StateInterval rebuiltFor_;
};

} // namespace spyro::guest_terrain
