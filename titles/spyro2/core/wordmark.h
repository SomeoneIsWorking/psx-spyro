#pragma once

#include "core/wad_wordmark.h"

namespace spyro2 {

// WAD entry 95's block at +0x40800 is VRAM (512, 256) onward; the plate is image 0 of the table at
// 0x80063F44 (spyro-2 docs/notes/title-wordmark.md).
inline constexpr spyro::WadWordmark kWordmark{
    .entry = 95,
    .blockOffset = 0x40800,
    .origin = {512, 256},
    .rows = 256,
    .sprite = {.width = 255, .height = 96, .texPage = 0x98, .clut = (511u << 6) | (512u / 16u)}};

} // namespace spyro2
