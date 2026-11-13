// stick_camera.h — the right-stick camera (plan 0252): the stick's deflection, read through the pad
// owner, for each title to feed into its own camera inputs. Never written into the guest's pad
// words.
#pragma once

#include "title_settings.h"

#include <cstdint>

class Core;

namespace spyro {

// The right stick past its rest deadzone, each axis in [-1, 1]: +x pushed right, +y pushed up.
struct StickDeflection {
  double x = 0.0;
  double y = 0.0;
};

// Centred while the setting is off, in a comparison run, during a pad replay, or while the host
// holds the input.
StickDeflection rightStickCamera(const Core &core);

// `full` scaled by a deflection, truncated toward zero as the guest's own integer scaling does.
std::int32_t scaledBy(double deflection, std::int32_t full);

// The settings row: Off / On, on by default.
const psx::game::TitleIntSetting &rightStickCameraSetting();

} // namespace spyro
