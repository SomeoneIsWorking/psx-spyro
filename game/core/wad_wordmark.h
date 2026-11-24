// A title's wordmark from WAD.WAD: one entry's block of VRAM rows, 512 halfwords wide, and the
// sprite the title screen draws from it.
#pragma once

#include "art_scene.h"
#include "vram_rect.h"

#include <cstdint>
#include <optional>

namespace psx::cd {
class Disc;
} // namespace psx::cd

namespace spyro {

struct WadWordmark {
  int entry = 0;                 // the WAD directory's entry
  std::uint32_t blockOffset = 0; // where the block starts in the entry
  psx::gpu::VramPoint origin;    // where the block lands in VRAM
  int rows = 0;
  // At the canvas origin: its texpage and CLUT name places inside the block.
  psx::present::ArtSprite sprite;
};

// nullopt when the disc's WAD does not hold the block.
std::optional<psx::present::ArtScene> wadWordmark(psx::cd::Disc &disc, const WadWordmark &wordmark);

} // namespace spyro
