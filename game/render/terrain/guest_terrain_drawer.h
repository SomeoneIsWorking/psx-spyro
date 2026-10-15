// The native terrain drawer of this engine family: the entry and exit every image shares, around
// the passes each image runs.
#pragma once

#include "core.h"
#include "render/field/guest_render_globals.h"
#include "render/field/guest_working_memory.h"
#include "render/terrain/guest_camera_builder.h"
#include "render/terrain/guest_terrain_facts.h"
#include "render/terrain/guest_terrain_frame.h"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace spyro::guest_terrain {

// The guest ranges one field's traversal owns rather than reads as level data, the scratch block
// and ordering table among them, and the arena an in-between links its packets into. An in-between
// maps each empty; a real field keeps no words from them.
struct FieldWindows {
  std::uint32_t scratch = 0;
  std::uint32_t tableBase = 0;
  std::uint32_t arenaEnd = 0; // where the real field's packet pool ends
  guest_memory::PacketArena inBetweenArena;
  std::array<guest_memory::GuestRange, 5> owned{};
};

// The owned ranges' sizes. The flag window is the 64 KB a sector's flag-byte offset reaches from
// the scratch block; the table window is doubled because the far pass names bins past 512 (bin 573
// on SCUS_944.25).
inline constexpr std::uint32_t kFlagWindowBytes = 0x10000u;
inline constexpr std::uint32_t kFoggedColourBytes = 0x1000u;
inline constexpr std::uint32_t kOrderingWindowBytes = 0x2000u;

// One image's terrain drawer: the words it reads, the ranges a field owns, the camera builder an
// in-between rebuilds its camera with, and the passes it runs in retail order.
class TerrainImage {
public:
  constexpr TerrainImage() = default;
  constexpr virtual ~TerrainImage() = default;
  TerrainImage(const TerrainImage &) = delete;
  TerrainImage &operator=(const TerrainImage &) = delete;

  [[nodiscard]] virtual const Facts &facts() const = 0;
  [[nodiscard]] virtual const guest_render_globals::Globals &globals() const = 0;
  [[nodiscard]] virtual FieldWindows windows(Core &core) const = 0;
  [[nodiscard]] virtual guest_camera::Matrices camera(Core &core,
                                                      const guest_camera::Angles &angles) const = 0;
  // Everything between the shared entry and exit. A real field also makes the guest-only calls.
  virtual void drawPasses(TerrainFrame &frame) const = 0;
};

// What an in-between field draws with instead of the guest's own: its camera, the drawer's $a0
// the field was called with, its packet arena (0 = the guest's cursor) and that arena's budget.
struct InBetweenWindow {
  const InBetweenCamera *camera = nullptr;
  std::uint32_t argument = 0;
  std::uint32_t arenaBase = 0;
  std::int32_t packetBudget = 0;
};

class Drawer {
public:
  Drawer(Core &core,
         const TerrainImage &image,
         guest_memory::WorkingMemory &memory,
         FrameMode mode,
         const InBetweenWindow &inBetween = {});

  // Returns the frame the passes ran on so a caller that walks the ordering table afterwards walks
  // THIS field's table; a second `run()` would produce a second, empty record of the bins it
  // linked.
  const TerrainFrame &run();

private:
  Core &core_;
  const TerrainImage &image_;
  guest_memory::WorkingMemory &memory_;
  const FrameMode mode_;
  // Held, not returned from a local: the frame is what a caller walks the ordering table with.
  std::optional<TerrainFrame> frame_;
  // Null for a real field, which reads the guest's own camera; the constructor refuses otherwise.
  InBetweenWindow inBetween_;
};

// The override entry: it owns a `GuestMemory` for its own length and runs in
// `FrameMode::RealField`, so every guard inside the passes is open. Returns every guest word the
// field read, as first read.
std::vector<guest_memory::LevelWord> draw(Core &core, const TerrainImage &image);

} // namespace spyro::guest_terrain
