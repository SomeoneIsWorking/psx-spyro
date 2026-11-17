// The flyby card (GS_TitleScreen in TSM_Demo, drawn by 0x8001E6B8): an authored 4:3 composition
// whose update (GamestateCutsceneTransition) parks Spyro just outside the 4:3 frame between its
// passes.
#pragma once

#include "horizontal_margins.h"

class Core;

namespace spyro1::flyby_card {

// No picture beside the buffer while the card shows, so a widened canvas pillarboxes it.
psx::present::PictureBeyondBuffer pictureBeyondBuffer(Core &core);

} // namespace spyro1::flyby_card
