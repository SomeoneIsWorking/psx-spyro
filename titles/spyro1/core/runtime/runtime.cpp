#include "spyro1/core/runtime/runtime.h"

#include "core.h"
#include "crt0_boot.h"
#include "game.h"
#include "platform_hle.h"
#include "spyro1/core/audio/native_moby_sound.h"
#include "spyro1/core/audio/native_sound_position.h"
#include "spyro1/core/audio/spu_library.h"
#include "spyro1/core/camera/native_camera.h"
#include "spyro1/core/debug/teleport.h"
#include "spyro1/core/debug/warp.h"
#include "spyro1/core/effect/native_effect_state.h"
#include "spyro1/core/effect/native_particle_alloc.h"
#include "spyro1/core/execution/native_level_initialization.h"
#include "spyro1/core/execution/native_owners.h"
#include "spyro1/core/execution/native_random_range.h"
#include "spyro1/core/frame/frame_driver.h"
#include "spyro1/core/frame/logo_facts.h"
#include "spyro1/core/frame/native_draw_setup.h"
#include "spyro1/core/hud/native_hud_collectables.h"
#include "spyro1/core/hud/native_pause_menu.h"
#include "spyro1/core/hud/native_pixel_fade.h"
#include "spyro1/core/input/native_gamepad.h"
#include "spyro1/core/moby/native_collision_shade.h"
#include "spyro1/core/moby/native_environment_light.h"
#include "spyro1/core/moby/native_level_globals_table.h"
#include "spyro1/core/moby/native_moby_allocator.h"
#include "spyro1/core/moby/native_moby_animation.h"
#include "spyro1/core/moby/native_moby_collision.h"
#include "spyro1/core/moby/native_moby_lists.h"
#include "spyro1/core/moby/native_moby_transform.h"
#include "spyro1/core/moby/native_shaded_moby_queue.h"
#include "spyro1/core/moby/native_shared_models.h"
#include "spyro1/core/player/native_cutscene.h"
#include "spyro1/core/player/native_player_animation.h"
#include "spyro1/core/player/native_player_physics.h"
#include "spyro1/render/actor_emitter_producer.h"
#include "spyro1/render/actor_list.h"
#include "spyro1/render/drop_shadow_producer.h"
#include "spyro1/render/flame_producer.h"
#include "spyro1/render/pause_backdrop.h"
#include "spyro1/render/glow_pools.h"
#include "spyro1/render/glow_producers.h"
#include "spyro1/render/hud_anchor.h"
#include "spyro1/render/moby_shadow_producer.h"
#include "spyro1/render/ordering_table.h"
#include "spyro1/render/paired_actor_producer.h"
#include "spyro1/render/particle_producer.h"
#include "spyro1/render/portal_producer.h"
#include "spyro1/render/rescue_star_producer.h"
#include "spyro1/render/screen_overlays.h"
#include "spyro1/render/sky_producer.h"
#include "spyro1/render/sprite_queue_producer.h"
#include "spyro1/render/terrain_image.h"
#include "spyro1/render/title_sprites.h"

#include <lucent/log.h>

namespace spyro1 {
namespace {

constexpr psx::game::Crt0Image kCrt0{.entry = 0x8005B8E0u};

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

// InitActorMeshScratchRegions 0x8005B6F8 carves pool 0 [base0, base1) and pool 1 [base1, top); the
// bases are each frame env's +0x70 and the top its +0x78.
constexpr psx::gpu::GuestPacketPoolWindows kPacketPool{
    .representation = psx::gpu::GuestPacketPoolWindows::Representation::LiveBaseEndPointers,
    .basePointer = {0x80076F50u, 0x80076FD4u},
    .endPointer = {0x80076FD4u, 0x80076FDCu},
};

} // namespace

Runtime::Runtime()
    : vsync_({.entry = kVSync, .queryCounter = kVSyncQueryCounter}), libgpu_(kLibgpu),
      libgte_(kLibgte), libcd_(kLibcd, kCdWorkArea, 0), pad_(kPadBuffers) {}

void Runtime::startProgram(Core &core) {
  psx::game::Crt0Boot{kCrt0}.run(core);
  core.game->frameDriverAs<FrameDriver>().initialize(core);
}

void Runtime::registerOverrides(Game &game) {
  vsync_.bind(game);
  libgpu_.bind(game);
  libgte_.bind(game);
  libcd_.bind(game);
  game.platform_hle.bindStock(kCdDataSync, PlatformHle::completeImmediately);
  game.platform_hle.bindStock(kCdInitHandshake, PlatformHle::completeImmediately);
  spyro1::registerCdQueue(game.core);
  spyro1::registerNativeRand(game.core);
  spyro1::registerNativeLeaves(game.core);
  spyro1::registerNativeVec(game.core);
  spyro1::registerNativeGte(game.core);
  spyro1::registerNativeAngle(game.core);
  spyro1::registerNativeUtil(game.core);
  native::registerSpuLibraryOverrides(game.core);
  native::registerCameraOverrides(game.core);
  native::registerCollisionShadeOverrides(game.core);
  native::registerCutsceneOverrides(game.core);
  native::registerDrawSetupOverrides(game.core);
  native::registerEffectStateOverrides(game.core);
  native::registerEnvironmentLightOverrides(game.core);
  native::registerGamepadOverrides(game.core);
  glows::registerPoolOverrides(game.core);
  native::registerHudCollectableOverrides(game.core);
  native::registerLevelGlobalsTableOverrides(game.core);
  native::registerMobyAllocatorOverrides(game.core);
  native::registerMobyCollisionOverrides(game.core);
  native::registerLevelInitializationOverrides(game.core);
  native::registerMobyAnimationOverrides(game.core);
  native::registerMobySoundOverrides(game.core);
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
  OrderingTable::registerOverrides(game.core);
  registerScreenOverlayOverrides(game.core);
  registerTitleSpriteProducer(game.core);
  spyro1::terrain::registerOverrides(game.core);
  actors::registerListOverrides(game.core);
  actors::registerEmitterOverrides(game.core);
  actors::registerSpriteQueueOverrides(game.core);
  actors::registerPairedActorOverrides(game.core);
  sky::registerSkyOverrides(game.core);
  shadow::registerDropShadowOverrides(game.core);
  flame::registerFlameOverrides(game.core);
  rescue_star::registerRescueStarOverrides(game.core);
  shadows::registerMobyShadowOverrides(game.core);
  portals::registerPortalOverrides(game.core);
  glows::registerGlowOverrides(game.core);
  particles::registerParticleOverrides(game.core);
  pause_backdrop::registerOverrides(game.core);
  hud_anchor::registerOverrides(game.core);
  lucent::info("boot", "installed Spyro 1's verified image-scoped native overrides");
}

std::unique_ptr<psx::game::FrameDriver> Runtime::createFrameDriver(Game &game) {
  return std::make_unique<FrameDriver>(game);
}

void Runtime::deliverPad(Core &core, const psx::input::PadDelivery &delivery) {
  pad_.deliver(core, delivery);
}

std::optional<psx::gpu::PacketPoolExtent> Runtime::packetPool(Core &core) const {
  return kPacketPool.extent(core);
}

bool Runtime::guestPictureIsNativeWidth(const Game &game) const {
  return game.frameDriver == nullptr || !game.frameDriverAs<FrameDriver>().pastBootPrefix();
}

bool Runtime::sealedFrameIsCut(Core &core) const {
  // A session's first record, sealed before its driver exists, is a cut.
  return core.game->frameDriver == nullptr || core.game->frameDriverAs<FrameDriver>().cut().isCut();
}

const spyro::TitleLogoFacts &Runtime::logoFacts() const {
  return kLogoFacts;
}

std::uint64_t Runtime::inputPhase(Core &core) const {
  return inputPhase_.of(core);
}

std::unique_ptr<spyro::Warp> Runtime::createWarp() const {
  return std::make_unique<Warp>();
}

} // namespace spyro1
