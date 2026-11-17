#include "spyro1/render/flyby_card.h"

#include "core.h"
#include "spyro1/core/guest_globals.h"

namespace spyro1::flyby_card {

psx::present::PictureBeyondBuffer pictureBeyondBuffer(Core &core) {
  if (core.mem_r32(guest::kGamestate) != guest::kGamestateTitleScreen ||
      core.mem_r32(guest::kTitlescreenMode) != guest::kTitleModeDemo) {
    return {};
  }
  psx::present::PictureBeyondBuffer beyond;
  beyond.picture = psx::present::HorizontalMargins{0, 0};
  return beyond;
}

} // namespace spyro1::flyby_card
