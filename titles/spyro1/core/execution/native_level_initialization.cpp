#include "spyro1/core/execution/native_level_initialization.h"
#include "spyro1/core/guest_functions.h"
#include "spyro1/core/guest_globals.h"

#include "core/native_execution.h"
#include "guest_call.h"

#include <cstdint>
#include <string_view>

namespace spyro1::native {
namespace {

using spyro1::guest::kInitNewGameState;
using spyro1::guest::kLevelId;

constexpr std::uint32_t kActEnabled = 0x800757A4u;
constexpr std::uint32_t kCameraMode = 0x80075914u;
constexpr std::uint32_t kResetSavedState = 0x80012604u; // `jal` at 0x800127A8

constexpr std::uint32_t kArtisansLevelId = 0xAu;
constexpr std::uint32_t kActuatorsOn = 1u;
constexpr std::uint32_t kPassiveCameraMode = 0x52u;

constexpr std::string_view kOverrideName = "init_new_game_state";

// Level 10, actuators on, passive camera, then the saved-state reset with the caller's a0-a3; v0
// leaves holding the camera mode unless the reset returns one.
void initNewGameState(Core *c) {
  c->mem_w32(kLevelId, kArtisansLevelId);
  c->mem_w32(kActEnabled, kActuatorsOn);
  c->mem_w32(kCameraMode, kPassiveCameraMode);
  c->r[2] = kPassiveCameraMode;
  psx::cpu::callGuestNow(*c, kOverrideName, kResetSavedState, c->r[4], c->r[5], c->r[6], c->r[7]);
}

} // namespace

void registerLevelInitializationOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, kInitNewGameState, kOverrideName, initNewGameState);
}

} // namespace spyro1::native
