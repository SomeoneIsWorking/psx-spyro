#pragma once

#include "core/wad_wordmark.h"

namespace spyro1 {

// WAD entry 1 is VRAM (512, 256) onward; the wordmark is sprite 0 of the title overlay's table at
// 0x8006FACC (open-spyro docs/notes/title-wordmark.md).
inline constexpr spyro::WadWordmark kWordmark{
    .entry = 1,
    .blockOffset = 0,
    .origin = {512, 256},
    .rows = 256,
    .sprite = {.width = 255, .height = 128, .texPage = 0x98, .clut = (511u << 6) | (512u / 16u)}};

} // namespace spyro1
