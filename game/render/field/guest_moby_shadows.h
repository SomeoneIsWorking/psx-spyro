// guest_moby_shadows.h — the moby shadow drawer of this engine family (SCUS_944.25 0x800499D4,
// SCUS_944.67 0x8002DDA8, the same instructions), which draws the close list the visibility walk
// and the deferred drawer wrote: under each record a square on its floor height, tilted by the two
// slope angles of its +0x1C word, projected and drawn as four semi-transparent textured triangles
// fanned from its centre, each binned by its depth less the record's bias byte.
//
// Native so its horizontal reject tests the presented window: retail drops a square whose corners
// all lie at or right of x 512, or whose last three lie left of x 0, so every shadow wholly in a
// widened margin. With no margin the tests compute retail's words.
#pragma once

#include "core.h"
#include "render/field/guest_render_globals.h"
#include "render/terrain/guest_terrain_facts.h"

#include <cstdint>

namespace spyro::guest_moby_shadows {

struct Facts {
  std::uint32_t entry = 0;
  const char *overrideName = "";
  // Two texture words, the centre corner's and the edge corners' less 0x1F00, then the close
  // list's end (the walk's v0 on return, guest_moby::Facts::exitV0).
  std::uint32_t texture = 0;
};

// One image's words the drawer reaches.
struct Image {
  const Facts &shadows;
  const guest_terrain::Facts &terrain;
  const guest_render_globals::Globals &globals;
};

// The drawer's body, from its prologue to its epilogue.
void draw(Core &core, const Image &image);

} // namespace spyro::guest_moby_shadows
