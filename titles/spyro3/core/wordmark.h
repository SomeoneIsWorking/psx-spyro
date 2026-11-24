#pragma once

#include "core/wad_wordmark.h"

namespace spyro3 {

// WAD entry 6's block at +0x800 is VRAM (512, 0) onward; the title screen's POLY_FT4 reads uv (0,
// 1)..(190, 75) through the palette on the block's first row (spyro-3
// docs/notes/title-wordmark.md).
inline constexpr spyro::WadWordmark kWordmark{
    .entry = 6,
    .blockOffset = 0x800,
    .origin = {512, 0},
    .rows = 76,
    .sprite = {.v = 1, .width = 190, .height = 74, .texPage = 0x88, .clut = 512u / 16u}};

} // namespace spyro3
