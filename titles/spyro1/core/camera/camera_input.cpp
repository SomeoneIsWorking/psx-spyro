#include "spyro1/core/camera/camera_input.h"

#include "core/native_execution.h"
#include "core/stick_camera.h"
#include "spyro1/core/guest_globals.h"
#include "spyro1/core/guest_gp.h"

#include <algorithm>
#include <cstdint>

namespace spyro1::native {
namespace {

using spyro1::guest::kGp;
using spyro1::guest::kPlayerControlFlags;

// open-spyro names (config/symbol_addrs.txt, include/camera.h).
constexpr std::uint32_t kManualYawInput = kGp + 0x6C0u; // g_nCameraManualYawInput 0x80075924
constexpr std::uint32_t kCameraCurrentMode = 0x80076E28u;
constexpr std::uint32_t kCameraTargetYaw =
    0x80076E30u; // then pitch, distance, roll, pitch bias, yaw bias
constexpr std::uint32_t kCameraTargetPitch = kCameraTargetYaw + 4u;
constexpr std::uint32_t kCameraTargetPitchBias = kCameraTargetYaw + 0x10u;
constexpr std::uint32_t kCameraStuckFrames = 0x80076E98u;
constexpr std::uint32_t kCameraParamsReferenceYaw = 0x80076EA4u;
constexpr std::uint32_t kCameraTargetParams = 0x80076EA8u;
constexpr std::uint32_t kPadHeld = 0x80077380u;
constexpr std::uint32_t kLockOnRequest = 0x1000u; // SPYRO_RQ_LOCK_ON
constexpr std::uint32_t kPadR2 = 0x2u;
constexpr std::uint32_t kPadL2 = 0x1u;
constexpr std::uint32_t kManualYawRight = 0xFFFFFC00u;
constexpr std::uint32_t kManualYawLeft = 0x400u;
constexpr std::int32_t kManualYawFull = 0x400;
constexpr std::uint32_t kAngleMask = 0xFFFu;
constexpr std::uint32_t kTargetLoadHi = 0x80070000u; // the body's last `lui $at`

// The modes UpdateCameraSpringForMode (0x80035FB4) refuses the manual yaw in: CAM_HELD with free
// look, scripted target, level transition, portal path and scripted vantage, and the charge.
constexpr std::uint32_t kNoManualModes[] = {
    0x80000009u, 0x80000007u, 0x80000012u, 0x8000000Bu, 0x8000000Eu, 3u};

// The free look's own stick limits (CAM_LOOK_MAX_UP / MAX_DOWN / PITCH_SPLIT): past the split a
// downward look tilts the view through the pitch bias instead of lowering the camera.
constexpr std::int32_t kLookMaxUp = 0x500;
constexpr std::int32_t kLookMaxDown = 0x3D0;
constexpr std::int32_t kLookPitchSplit = 0x140;
// g_anCameraParamFallBehind's pitch, straight down from overhead: the highest any follow preset
// sits.
constexpr std::int32_t kOverheadPitch = 0x400;

bool followsSpyro(Core *c) {
  const std::uint32_t mode = c->mem_r32(kCameraCurrentMode);
  return std::find(std::begin(kNoManualModes), std::end(kNoManualModes), mode) ==
         std::end(kNoManualModes);
}

// UpdateCameraManualYawInput's own refusals, which the stick shares with L2/R2.
bool manualInputRefused(Core *c) {
  return (c->mem_r32(kPlayerControlFlags) & kLockOnRequest) != 0u ||
         c->mem_r32(kCameraStuckFrames) != 0u;
}

// ── 0x80035F58 — the L2/R2 manual yaw, cleared first, so both early exits leave it at zero. Every
// branch
//     has an `addiu` delay slot, so v0 never carries out what the branch tested: a refusal hands
//     back the word its `lw` loaded, R2 hands back -1024, and the L2 slot runs on both arms, so
//     "neither held" hands back 1024 having stored nothing. v1 holds the pad word only where the
//     button tests are reached. With neither held, the right stick's x drives the same word, full
//     deflection matching a shoulder.
void cameraManualYawInput(Core *c) {
  c->mem_w32(kManualYawInput, 0u);
  const std::uint32_t controlFlags = c->mem_r32(kPlayerControlFlags);
  c->r[2] = controlFlags & kLockOnRequest;
  if (c->r[2] != 0u) {
    return;
  }
  const std::uint32_t stuckFrames = c->mem_r32(kCameraStuckFrames);
  c->r[2] = stuckFrames;
  if (stuckFrames != 0u) {
    return;
  }
  const std::uint32_t held = c->mem_r32(kPadHeld);
  c->r[3] = held;
  if ((held & kPadR2) != 0u) {
    c->r[2] = kManualYawRight;
    c->mem_w32(kManualYawInput, kManualYawRight);
    return;
  }
  const bool leftHeld = (held & kPadL2) != 0u;
  c->r[2] = kManualYawLeft;
  if (leftHeld) {
    c->mem_w32(kManualYawInput, kManualYawLeft);
    return;
  }
  // R2 turns the camera with -0x400, so a stick pushed right does the same.
  const std::int32_t stickYaw = spyro::scaledBy(-spyro::rightStickCamera(*c).x, kManualYawFull);
  if (stickYaw != 0) {
    c->mem_w32(kManualYawInput, static_cast<std::uint32_t>(stickYaw));
  }
}

// The stick's y on the freshly loaded target pitch, the way the free look puts its own stick there,
// but not inverted: a raised pitch lifts the camera over Spyro, so a stick pushed up lowers it to
// look up.
void applyStickPitch(Core *c) {
  if (!followsSpyro(c) || manualInputRefused(c)) {
    return;
  }
  const double lift = -spyro::rightStickCamera(*c).y;
  const std::int32_t look = spyro::scaledBy(lift, lift > 0.0 ? kLookMaxUp : kLookMaxDown);
  if (look == 0) {
    return;
  }
  std::int32_t pitch = static_cast<std::int32_t>(c->mem_r32(kCameraTargetPitch));
  if (look < -kLookPitchSplit) {
    pitch -= kLookPitchSplit;
    const std::int32_t bias = static_cast<std::int32_t>(c->mem_r32(kCameraTargetPitchBias));
    c->mem_w32(kCameraTargetPitchBias, static_cast<std::uint32_t>(bias + kLookPitchSplit + look));
  } else {
    pitch = std::min(pitch + look, std::max(pitch, kOverheadPitch));
  }
  c->mem_w32(kCameraTargetPitch, static_cast<std::uint32_t>(pitch));
}

// ── 0x80034198 — the camera target from the mode's parameter record: yaw relative to the reference
// yaw,
//     wrapped to 12 bits, then the other five words verbatim. Exits with a0 the record, v1 the
//     reference yaw, v0 the last word copied and at the last store's `lui`.
void loadCameraTargetAngles(Core *c) {
  const std::uint32_t params = c->mem_r32(kCameraTargetParams);
  const std::uint32_t referenceYaw = c->mem_r32(kCameraParamsReferenceYaw);
  c->mem_w32(kCameraTargetYaw, (c->mem_r32(params) - referenceYaw) & kAngleMask);
  std::uint32_t word = 0u;
  for (std::uint32_t index = 1; index < 6u; ++index) {
    word = c->mem_r32(params + index * 4u);
    c->mem_w32(kCameraTargetYaw + index * 4u, word);
  }
  c->r[1] = kTargetLoadHi;
  c->r[2] = word;
  c->r[3] = referenceYaw;
  c->r[4] = params;
  applyStickPitch(c);
}

} // namespace

void registerCameraInputOverrides(Core &core) {
  psx::cpu::installNativeOverride(
      core, 0x80035F58u, "camera_shoulder_rotation_input", cameraManualYawInput);
  psx::cpu::installNativeOverride(
      core, 0x80034198u, "camera_target_from_mode", loadCameraTargetAngles);
}

} // namespace spyro1::native
