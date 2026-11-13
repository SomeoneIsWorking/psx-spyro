// camera_input.h — Spyro 1 camera inputs, owned natively: the L2/R2 manual yaw (0x80035F58) and the
// target load from the mode's parameter record (0x80034198), each with the right-stick camera's
// share.
#pragma once

#include "core.h"

namespace spyro1::native {

void registerCameraInputOverrides(Core &core);

} // namespace spyro1::native
