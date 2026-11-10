// The native terrain drawer of this engine family: the entry and exit every image shares, around
// the passes each image runs.
#pragma once

#include "core.h"
#include "render/field/guest_render_globals.h"
#include "render/terrain/guest_terrain_facts.h"
#include "render/terrain/guest_terrain_frame.h"

#include <cstdint>
#include <optional>

namespace spyro::guest_terrain {

// One image's terrain drawer: the words it reads, its scratch block and the passes it runs in
// retail order.
class TerrainImage {
public:
  constexpr TerrainImage() = default;
  constexpr virtual ~TerrainImage() = default;
  TerrainImage(const TerrainImage &) = delete;
  TerrainImage &operator=(const TerrainImage &) = delete;

  [[nodiscard]] virtual const Facts &facts() const = 0;
  [[nodiscard]] virtual const guest_render_globals::Globals &globals() const = 0;
  // The scratch block base the passes build their sector lists in.
  [[nodiscard]] virtual std::uint32_t scratch(Core &core) const = 0;
  // Everything between the shared entry and exit.
  virtual void drawPasses(TerrainFrame &frame) const = 0;
};

class Drawer {
public:
  Drawer(Core &core, const TerrainImage &image, const psx::present::EmitMemory &memory)
      : core_(core), image_(image), memory_(memory) {}

  // Returns the frame the passes ran on so a caller that walks the ordering table afterwards walks
  // THIS field's table; a second `run()` would produce a second, empty record of the bins it
  // linked.
  const TerrainFrame &run();

private:
  Core &core_;
  const TerrainImage &image_;
  const psx::present::EmitMemory &memory_;
  // Held, not returned from a local: the frame is what a caller walks the ordering table with.
  std::optional<TerrainFrame> frame_;
};

// The override entry: one field over the guest.
void draw(Core &core, const TerrainImage &image);

} // namespace spyro::guest_terrain
