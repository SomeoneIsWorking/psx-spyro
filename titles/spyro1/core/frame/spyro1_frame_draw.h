#pragma once

#include "resumable_guest_call.h"

class Core;

namespace spyro1 {

class FieldScheduler;

// GamestateDraw 0x8001ED5C, run to its return. Its display tail waits on VSync(0) until two fields
// have passed since the last frame, then PutDispEnv, PutDrawEnv and DrawOTag; each wait is one
// unpresented field here, and the caller presents the frame after the return.
class FrameDraw {
public:
  void run(Core &core, FieldScheduler &fields);

private:
  psx::cpu::ResumableGuestCall call_;
};

} // namespace spyro1
