// loading_picture.h — the S2/S3 loading screen's picture as a state producer: a row of 128-wide
// page sprites at the brightness a counter fades in, linked into one list, then the screen's text
// in the overlay pair, all from one game state's draw.
//
// The override keys what the draw links to the counter and saves the brightness and the packets
// themselves; the render walks the pages and then the text again, the pages at the brightness
// between two saves.
#pragma once

#include "emit_memory.h"
#include "frame_record.h"
#include "render/field/guest_render_globals.h"
#include "state_producer.h"

#include <cstdint>

class Core;

namespace spyro::loading_picture {

struct Facts {
  std::uint32_t entry = 0; // the state's draw; 0 when the image has none
  const char *overrideName = "";
  std::uint32_t gate = 0;    // the draw shows the picture while this word is 0
  std::uint32_t counter = 0; // and this one, its fade, is not
  std::uint32_t pages = 0;   // each a DR_MODE and a SPRT, linked first
  // The list the pages are linked into; the text after them goes to the overlay pair.
  psx::present::OtSlot pageSlot;
};

// The override body: the draw, keyed and saved when it shows the picture.
void draw(Core &core, const Facts &facts, const guest_render_globals::Globals &globals);

class LoadingPicture final : public psx::present::StateProducer {
public:
  LoadingPicture(Core &core, const Facts &facts) : core_(core), facts_(facts) {}

  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;

private:
  Core &core_;
  const Facts &facts_;
  mutable psx::present::HostMemory region_;
};

} // namespace spyro::loading_picture
