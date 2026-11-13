#include "core/stick_camera.h"

#include "core.h"
#include "enhancement_gate.h"
#include "game.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>

namespace spyro {
namespace {

psx::config::Var<long> cv_right_stick_camera("right_stick_camera", 1, psx::config::kSaved);
const psx::config::Registration kRegistration{cv_right_stick_camera};

const std::array<const char *, 2> kOffOn{"Off", "On"};
const psx::game::TitleIntSetting kSetting{.id = "right_stick_camera",
                                          .label = "Right-stick camera",
                                          .unit = "",
                                          .var = &cv_right_stick_camera,
                                          .range = {.min = 0, .max = 1, .step = 1},
                                          .labels = kOffOn};

// A resting stick reads up to about a quarter of its travel on worn pads; SDL's own suggested dead
// zone.
constexpr int kDeadzone = 8000;
constexpr int kAxisMax = 32767;

double deflection(std::int16_t raw) {
  const int value = std::max(static_cast<int>(raw), -kAxisMax);
  const int magnitude = std::abs(value);
  if (magnitude <= kDeadzone) {
    return 0.0;
  }
  const double past =
      static_cast<double>(magnitude - kDeadzone) / static_cast<double>(kAxisMax - kDeadzone);
  return value < 0 ? -past : past;
}

} // namespace

StickDeflection rightStickCamera(const Core &core) {
  if (psx::config::enh_int(cv_right_stick_camera) == 0) {
    return {};
  }
  const psx::input::StickAxes stick = core.game->pad.rightStick();
  return {.x = deflection(stick.x), .y = -deflection(stick.y)};
}

std::int32_t scaledBy(double deflection, std::int32_t full) {
  return static_cast<std::int32_t>(std::trunc(deflection * static_cast<double>(full)));
}

const psx::game::TitleIntSetting &rightStickCameraSetting() {
  return kSetting;
}

} // namespace spyro
