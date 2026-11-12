// guest_moby_deferred.h — the deferred-moby drawer of this engine family (SCUS_944.25 0x80046FD8,
// SCUS_944.67 0x80033C5C, the same instructions), which draws the buffer the visibility walk hands
// on: a moby whose mesh is not resident at its camera-relative centre from its far mesh, and one
// whose reach byte is negative at a screen offset. Each mesh's polygons are binned into local
// buckets, then merged into the ordering table.
//
// Native so its horizontal tests read the presented window. Retail culls each centred entry's
// sphere on the 4:3 half-angle, outcodes the vertices of one that may cross an edge, and drops a
// polygon (or the whole mesh) whose vertices all lie past one edge. The guest's call keeps retail's
// selection, so the class byte (+0x4D), the close list and the guest pool hold retail's answer, and
// tests the inside flag and the outcodes on the presented window, so a retail-drawn moby keeps its
// polygons in the margins. The moby drawer's margin pass (guest_moby_drawer.h) calls it again over
// the margin walk's buffer for only the entries the presented half-angle adds. With no margin the
// guest's call computes retail's words.
#pragma once

#include "core.h"
#include "render/field/guest_moby_visibility.h"
#include "render/field/guest_render_globals.h"
#include "render/terrain/guest_terrain_facts.h"

#include <cstdint>

namespace spyro::guest_moby_deferred {

struct Facts {
  std::uint32_t entry = 0;
  const char *overrideName = "";
  // Four colour words per palette, the record's +0x54 word its index (8004771C).
  std::uint32_t paletteTable = 0;
  // The packed far colour a screen-placed mesh fades to (8004805C).
  std::uint32_t fogColour = 0;
};

// One image's words the drawer reaches.
struct Image {
  const Facts &deferred;
  const guest_moby::Facts &walk;
  const guest_terrain::Facts &terrain;
  const guest_render_globals::Globals &globals;
};

// Which entries a call draws.
enum class Selection : std::uint8_t {
  // Retail's: every screen-placed entry and the centred ones inside the 4:3 half-angle.
  Retail,
  // The centred entries past the 4:3 half-angle and inside the presented one.
  Margin,
};

// The drawer's body, from its prologue to its epilogue.
void draw(Core &core, const Image &image, Selection selection);

} // namespace spyro::guest_moby_deferred
