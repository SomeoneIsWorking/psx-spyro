#include "spyro1_runtime.h"

#include "core.h"
#include "crt0_boot.h"
#include "game.h"
#include "native_audio_key_state.h"
#include "native_camera.h"
#include "native_collision_shade.h"
#include "native_cutscene.h"
#include "native_draw_setup.h"
#include "native_effect_state.h"
#include "native_environment_light.h"
#include "native_gamepad.h"
#include "native_glow_sparkle_pools.h"
#include "native_hud_collectables.h"
#include "native_level_globals_table.h"
#include "native_level_initialization.h"
#include "native_moby_allocator.h"
#include "native_moby_collision.h"
#include "native_moby_helpers.h"
#include "native_moby_lists.h"
#include "native_moby_transform.h"
#include "native_particle_alloc.h"
#include "native_pause_menu.h"
#include "native_pixel_fade.h"
#include "native_player_animation.h"
#include "native_player_physics.h"
#include "native_random_range.h"
#include "native_shaded_moby_queue.h"
#include "native_shared_models.h"
#include "native_sound_position.h"
#include "native_spu_callbacks.h"
#include "native_spu_common_attr.h"
#include "native_spu_key_status.h"
#include "native_spu_registers.h"
#include "native_spu_state.h"
#include "native_spu_transfer_mode.h"
#include "native_spu_voice_attributes.h"
#include "native_spu_voice_pitch.h"
#include "platform_hle.h"
#include "screen_overlays.h"
#include "spyro1_frame_driver.h"
#include "spyro1_logo_facts.h"
#include "spyro1_ordering_table.h"
#include "spyro_game.h"
#include "spyro_gate_debug.h"

#include <lucent/log.h>

namespace spyro1 {
namespace {

constexpr psx::game::Crt0Image kCrt0{
    .bss = {0x80075640u, 0x8007AA38u},
    .stackTopWordAddress = 0x800755A8u,
    .stackReserveWordAddress = 0x800755A4u,
    .heapBase = 0x8007AA38u,
    .heapSizeStoreAddress = 0x800730C4u,
    .heapBaseStoreAddress = 0x800730C0u,
    .globalPointer = 0x80075264u,
    .libcInitEntry = 0x8005DB14u,
    .crt0Entry = 0x8005B8E0u,
    .stackBias = {.declared = true, .bytes = -8},
};

// VSync's negative-argument path reads D_800749E0 (asm/psyq.s:2646-2649), the vblank counter this
// title's field clock advances.
constexpr std::uint32_t kVSync = 0x8005DBC4u;
constexpr std::uint32_t kVSyncQueryCounter = 0x800749E0u;

constexpr psx::hle::StockLibgpu::Entries kLibgpu{.drawSync = 0x8005F764u,
                                                 .timeoutArm = 0x80062090u,
                                                 .timeoutCheck = 0x800620C4u,
                                                 .timeoutDeadlineWord = 0x80074B7Cu,
                                                 .timeoutFlagWord = 0x80074B80u};

// SetGeomOffset (ctc2 $a0,$24 at 0x80062620) and SetGeomScreen (ctc2 $a0,$26 at 0x80062638).
constexpr psx::hle::StockLibgte::Entries kLibgte{.setGeomOffset = 0x80062618u,
                                                 .setGeomScreen = 0x80062638u};

// From external/spyro-1's labels in asm/psyq.s. CdControlF takes (com, param) only: its a2 is the
// caller's leftover register, so it binds the entry that writes no result buffer.
constexpr psx::cd::StockLibcd::Entries kLibcd{.command = 0x80064CECu,
                                              .control = 0x80063C48u,
                                              .controlBlocking = 0x80063EACu,
                                              .controlFire = 0x80063D80u,
                                              .sync = 0x800647A0u};

// Written by libcd's command send: Setloc 0x80064DDC, Setmode 0x80064E08, command byte 0x80064EA4.
constexpr psx::cd::StockCommandWorkArea kCdWorkArea{.lastPositionAddress = 0x80074E50u,
                                                    .lastModeAddress = 0x80074E54u,
                                                    .lastCommandAddress = 0x80074E55u};

constexpr std::uint32_t kCdDataSync = 0x800655A0u;
constexpr std::uint32_t kCdInitHandshake = 0x800653B4u;

constexpr psx::input::StockPadBuffers::Layout kPadBuffers{.slot0Buffer = 0x800786A0u,
                                                          .slot1Buffer = 0x800786C2u};

} // namespace

Spyro1Runtime::Spyro1Runtime()
    : SpyroRuntime(spyro::SpyroTitle::Spyro1), vsync_(kVSync, kVSyncQueryCounter), libgpu_(kLibgpu),
      libgte_(kLibgte), libcd_(kLibcd, kCdWorkArea, 0), pad_(kPadBuffers) {}

void Spyro1Runtime::startProgram(Core &core) {
  psx::game::Crt0Boot{kCrt0}.run(core);
  frameDriver(core).initialize(core);
}

void Spyro1Runtime::registerOverrides(Game &game) {
  vsync_.bind(game);
  libgpu_.bind(game);
  libgte_.bind(game);
  libcd_.bind(game);
  game.platform_hle.bindStock(kCdDataSync, PlatformHle::completeImmediately);
  game.platform_hle.bindStock(kCdInitHandshake, PlatformHle::completeImmediately);
  spyro::registerCdQueue(game.core);
  spyro::registerNativeRand(game.core);
  spyro::registerNativeLeaves(game.core);
  spyro::registerNativeVec(game.core);
  spyro::registerNativeGte(game.core);
  spyro::registerNativeAngle(game.core);
  spyro::registerNativeUtil(game.core);
  native::registerAudioKeyStateOverrides(game.core);
  native::registerCameraOverrides(game.core);
  native::registerCollisionShadeOverrides(game.core);
  native::registerCutsceneOverrides(game.core);
  native::registerDrawSetupOverrides(game.core);
  native::registerEffectStateOverrides(game.core);
  native::registerEnvironmentLightOverrides(game.core);
  native::registerGamepadOverrides(game.core);
  native::registerGlowSparklePoolOverrides(game.core);
  native::registerHudCollectableOverrides(game.core);
  native::registerLevelGlobalsTableOverrides(game.core);
  native::registerMobyAllocatorOverrides(game.core);
  native::registerMobyCollisionOverrides(game.core);
  native::registerLevelInitializationOverrides(game.core);
  native::registerMobyHelperOverrides(game.core);
  native::registerMobyListOverrides(game.core);
  native::registerMobyTransformOverrides(game.core);
  native::registerParticleAllocOverrides(game.core);
  native::registerPauseMenuOverrides(game.core);
  native::registerPixelFadeOverrides(game.core);
  native::registerPlayerAnimationOverrides(game.core);
  native::registerPlayerPhysicsOverrides(game.core);
  native::registerRandomRangeOverrides(game.core);
  native::registerShadedMobyQueueOverrides(game.core);
  native::registerSharedModelOverrides(game.core);
  native::registerSoundPositionOverrides(game.core);
  native::registerSpuRegisterOverrides(game.core);
  native::registerSpuVoicePitchOverrides(game.core);
  native::registerSpuStateOverrides(game.core);
  native::registerSpuCallbackOverrides(game.core);
  native::registerSpuVoiceAttributeOverrides(game.core);
  native::registerSpuKeyStatusOverrides(game.core);
  native::registerSpuTransferModeOverrides(game.core);
  native::registerSpuCommonAttrOverrides(game.core);
  OrderingTable::registerOverrides(game.core);
  ScreenOverlays::registerOverrides(game.core);
  lucent::info("boot", "installed Spyro 1's verified image-scoped native overrides");
}

std::unique_ptr<psx::game::FrameDriver> Spyro1Runtime::createFrameDriver(Game &game) {
  return std::make_unique<Spyro1FrameDriver>(
      game, lucent::channel_on("stage-observe"), lucent::channel_on("handoff-store"));
}

void Spyro1Runtime::deliverPad(Core &core, const psx::input::PadDelivery &delivery) {
  pad_.deliver(core, delivery);
}

bool Spyro1Runtime::interpolates() const {
  return true;
}

RenderCapabilities Spyro1Runtime::renderCapabilities() const {
  return RenderCapabilities{
      .defaultPath = RenderPath::Record,
      .nativeRenderPath = false,
  };
}

bool Spyro1Runtime::guestVramIsPicture(const Game &) const {
  return true;
}

bool Spyro1Runtime::guestPictureIsNativeWidth(const Game &game) const {
  return game.frameDriver == nullptr || !frameDriver(game.core).bootComplete();
}

bool Spyro1Runtime::sealedFrameIsCut(Core &core) const {
  // A session's first record, sealed before its driver exists, is a cut.
  return core.game->frameDriver == nullptr || frameDriver(core).cut().isCut();
}

const spyro::TitleLogoFacts &Spyro1Runtime::logoFacts() const {
  return kLogoFacts;
}

bool Spyro1Runtime::titleCommand(Core &core,
                                 const psx::game::TitleCommandLine &input,
                                 std::FILE *reply) {
  return spyro::gate_debug::command(core, input, reply);
}

std::uint64_t Spyro1Runtime::inputPhase(Core &core) const {
  return inputPhase_.of(core);
}

} // namespace spyro1
