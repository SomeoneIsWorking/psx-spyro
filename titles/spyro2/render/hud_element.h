// hud_element.h — FUN_800520CC(image, x, y, size), the HUD's element drawer: one sprite (an image
// pointer in KSEG0) or one mirrored POLY_FT4 (any other image pointer) at (x, y), linked by
// FUN_8001B390 into the overlay pair. Its y corners lie `rows` below their whole rows, the part of
// the caller's slide offset its rounding dropped.
#pragma once

class Core;

namespace spyro2::hud_element {

// Runs FUN_800520CC on the Core's registers, as the guest leaves them: the packet in v0 (0 for a
// quad), the next free packet in v1.
void draw(Core &core, float rows);

} // namespace spyro2::hud_element
