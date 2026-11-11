#include "spyro1/render/inventory_band.h"

#include "core.h"
#include "guest_widescreen_projection.h"
#include "render/screen_overlays.h"

namespace spyro1::inventory_band {
namespace {

// The POLY_F4's words after its tag: colour with the command, then the corners left-top,
// right-top, left-bottom, right-bottom.
inline constexpr std::uint32_t kLeftTop = 0x08u;
inline constexpr std::uint32_t kRightTop = 0x0Cu;
inline constexpr std::uint32_t kRightBottom = 0x14u;

} // namespace

void linked(Core &core, std::uint32_t packet) {
  if (core.r[31] != kLinkReturn || !psx::present::widePresentation(core) ||
      (core.mem_r32(packet + kLeftTop) & 0xFFFFu) != 0) {
    return;
  }
  for (const std::uint32_t corner : {kRightTop, kRightBottom}) {
    core.mem_w32(packet + corner,
                 (core.mem_r32(packet + corner) & 0xFFFF0000u) |
                     spyro::screen_overlays::kScreenWidth);
  }
}

} // namespace spyro1::inventory_band
