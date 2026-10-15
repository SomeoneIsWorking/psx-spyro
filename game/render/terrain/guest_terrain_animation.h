// guest_terrain_animation.h — the sector animation the classification pass of this engine family
// runs on each sector it lists (SCUS_944.25 80023E8C..80024314, SCUS_942.28 80025BB8..80026198).
//
// A sector's four mark bytes (+0x18) select one animation definition per word slot; a byte under
// 0x80 animates its slot once this field and is set to 0xFF. A definition holds keyframes of packed
// words: slots 0 and 2 interpolate packed vectors with INTPL, slot 1 colour words and slot 3 colour
// pairs with DPCS. Every slot writes the level's own sector data, so only a real field runs it.
#pragma once

#include <array>
#include <cstdint>

namespace spyro::guest_terrain {

struct TerrainFrame;

// How one image lays out its vector slots (0 and 2).
struct AnimationLayout {
  // The four definition-table words, one per slot.
  std::array<std::uint32_t, 4> tables{};
  // The sector flag each vector slot's key top byte sets or clears; 0 when the image has none.
  std::array<std::uint32_t, 2> vectorFlags{};
  // Whether a definition's byte +5 moves the slot's first word.
  bool definitionOffsetsTarget = false;
};

// Animate every slot of `sector` whose byte of `marks` (the sector's marks with this field's claim)
// is under 0x80.
void animateSector(TerrainFrame &frame,
                   const AnimationLayout &layout,
                   std::uint32_t sector,
                   std::uint32_t marks);

} // namespace spyro::guest_terrain
