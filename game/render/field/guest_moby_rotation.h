// guest_moby_rotation.h — a moby's three angles composed onto the GTE rotation (SCUS_944.25 0x80043858,
// SCUS_944.67 0x80030478, SCUS_942.28 8001F50C..8001F70C). Plain angles read the tables with `lhu`; bit 30 of
// the flags interpolates with `lh`, so each source hands the composition the exact register words.
#pragma once

#include "gte_registers.h"

#include <cstdint>
#include <optional>

class Core;

namespace spyro::guest_moby_rotation {


// A moby's two rotation words: three 8-bit angles, and the 4-bit fractions that refine them.
struct MobyAngles {
  std::uint32_t angles = 0;
  std::uint32_t flags = 0;
};

// Retail's guest register words for one angle.
struct AngleWords {
  std::uint32_t sine = 0;
  std::uint32_t cosine = 0;
};

// Whether the byte-0 axis loads its result into CR0..CR4. SCUS_942.28's actor list builders only
// store it (8001F70C).
enum class Byte0Result : std::uint8_t { Loaded, Stored };

// What the byte-1 axis leaves in RT12. SCUS_942.28's moby shadows (8005A160) add the byte-0
// angle's table offset where the product belongs.
enum class Byte1Rt12 : std::uint8_t { Product, Byte0Offset };

// Rotate `rotation` by `angles.angles`, reading the tables as `lhu` does. Retail's records carry no
// fractions here; an in-between's `angles.flags` steps each entry toward the next. Loads the result
// into CR0..CR4 after each angle, as retail does. `rotation` must hold what CR0..CR4
// hold on entry. Returns what retail leaves in $v1 when an angle applied: the last product's IR3,
// shifted up 16 when that was the byte-0 axis.
std::optional<std::uint32_t> composeTabled(Core &core,
                                           std::uint32_t sineTable,
                                           const MobyAngles &angles,
                                           psx::gte::RotationWords &rotation,
                                           Byte0Result byte0 = Byte0Result::Loaded,
                                           Byte1Rt12 rt12 = Byte1Rt12::Product);

// The same, with each angle refined by the matching 4-bit fraction of `angles.flags` (bits 16, 8,
// 0).
void composeInterpolated(Core &core,
                         std::uint32_t sineTable,
                         const MobyAngles &angles,
                         psx::gte::RotationWords &rotation);

// Bit 28 of the flags mirrors the composed matrix's second column before it is stored. Retail
// negates RT12, RT22 and RT32 in the stored copy only; CR0..CR4 keep the unmirrored matrix.
void mirror(psx::gte::RotationWords &rotation);

} // namespace spyro::guest_moby_rotation
