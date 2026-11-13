#include "spyro2/core/runtime.h"

#include "core.h"
#include "game.h"
#include "spyro2/core/boot_facts.h"
#include "spyro2/core/logo_facts.h"
#include "spyro2/core/warp.h"
#include "spyro2/render/hud_anchor.h"
#include "spyro2/render/render_facts.h"

#include <lucent/log.h>
#include <memory>

namespace spyro2 {
namespace {

// libetc VSync: a negative argument returns the field delta.
constexpr std::uint32_t kVSync = 0x80058EDCu;

// libcd CdCommand(com, parm, result).
constexpr std::uint32_t kCdCommand = 0x80058858u;

// libcd CdControlF(com, param): every CD_cw call passes a2 = 0 (0x80058A1C, 0x80058A44, 0x80058A68)
// and the last sends async (a3 = 1 at 0x80058A70). Native because CD_cw's wait at 0x8005C900 polls
// a status byte only libcd's interrupt raises (docs/issues/0161).
constexpr std::uint32_t kCdControlF = 0x80058994u;

// libcd CdSync, polled until it reports CS_SELF.
constexpr std::uint32_t kCdSync = 0x80058810u;

// libcd CdRead(dest, source, mode).
constexpr std::uint32_t kCdRead = 0x80058108u;

// CdInit 0x800582B8 stays retail: it registers libcd's interrupt handler itself (docs/issues/0159).

// libcd's CdReadyCallback word (0x800582A4 stores it).
constexpr std::uint32_t kCdReadyCallbackPointer = 0x800663B8u;

// libgpu DrawSync's timeout: its deadline and poll-count words and the arm and check leaves.
constexpr std::uint32_t kGpuTimeoutDeadline = 0x80066354u;
constexpr std::uint32_t kGpuTimeoutPollCount = 0x80066358u;
constexpr std::uint32_t kGpuTimeoutArm = 0x80057B20u;
constexpr std::uint32_t kGpuTimeoutCheck = 0x80057AF4u;

// Both parity pools sit below the OT header, between 0x80069998 and 0x8006B2A4.
constexpr psx::gpu::GuestPacketPoolWindows kPacketPool{
    .representation = psx::gpu::GuestPacketPoolWindows::Representation::LiveBaseEndPointers,
    .basePointer = {0x80069998u, 0u},
    .endPointer = {0x8006B2A4u, 0u},
};

// libgte SetGeomScreen 0x80057AE8 and SetGeomOffset 0x80057AF8, called from 0x80011D24.
constexpr psx::hle::StockLibgte::Entries kLibgte{.setGeomOffset = 0x80057AF8u,
                                                 .setGeomScreen = 0x80057AE8u};

constexpr psx::game::Crt0Image kCrt0{.entry = 0x8005478Cu};

// The ready callback 0x8001379C ends a read on `$a0 & 0xFF == 2`.
// Its loads poll that callback, so a completion at the drive's rate is a loading-only wait.
constexpr psx::cd::GuestInterruptLibcd::Completion kCdCompletion{
    .status = 2, .perStockRead = true, .timing = psx::cd::ReadTiming::AtOnce};

// spyro-2 src/camera.c: func_8001F020 polls the shoulders, func_8001DB58 computes the errors that
// both the camera's error step and a level's own (D_80067040) take; D_800683A0 the held pad word.
constexpr spyro::FollowCameraFacts kFollowCamera{.shoulderPoll = 0x8001F020u,
                                                 .errorCompute = 0x8001DB58u,
                                                 .camera = 0x80067E84u,
                                                 .padHeld = 0x800683A0u,
                                                 .turnButtons = 0xFu,
                                                 .refusedHeld = 0u,
                                                 .spyroBusy = 0x8006A230u,
                                                 .gamestate = 0u,
                                                 .refusedGamestate = 0u};

} // namespace

Runtime::Runtime()
    : spyro::BootPrefixRuntime(kCrt0, kBootPrefixFacts),
      vsync_({.entry = kVSync, .queryCounter = kVSyncQueryCounter}),
      libgpu_({.timeoutArm = kGpuTimeoutArm,
               .timeoutCheck = kGpuTimeoutCheck,
               .timeoutDeadlineWord = kGpuTimeoutDeadline,
               .timeoutFlagWord = kGpuTimeoutPollCount}),
      libcd_({.read = kCdRead, .command = kCdCommand, .controlFire = kCdControlF, .sync = kCdSync},
             {},
             kCdReadyCallbackPointer,
             kCdCompletion),
      libgte_(kLibgte) {}

void Runtime::registerOverrides(Game &game) {
  vsync_.bind(game);
  libgpu_.bind(game);
  libcd_.bind(game);
  // libgte's two projection leaves, carrying the title's widening decision.
  libgte_.bind(game);
  spyro::registerEngineRenderOverrides<kRenderFacts>(game.core);
  hud_anchor::registerOverrides(game.core);
  spyro::registerFollowCamera(game.core, kFollowCamera);
  spyro::installHudSlide<kHudSlide>(game.core);
  spyro::installSkyWave(game.core, kSkyWave);
  spyro::installEffectSpawns(game.core, kDrawMotion.effects);
  lucent::info(
      "boot",
      "installed Spyro 2's stock libraries and natives for the projection leaves, the moby "
      "visibility walk, the terrain drawer, the flatten, the HUD anchor, the HUD slide and the sky "
      "wave");
}

std::optional<psx::gpu::PacketPoolExtent> Runtime::packetPool(Core &core) const {
  return kPacketPool.extent(core);
}

const spyro::FollowCameraFacts &Runtime::followCamera() const {
  return kFollowCamera;
}

const spyro::DrawMotionFacts &Runtime::drawMotion() const {
  return kDrawMotion;
}

std::unique_ptr<spyro::Warp> Runtime::createWarp() const {
  return std::make_unique<Warp>();
}

const psx::present::OutlineFont *Runtime::outlineFont() const {
  return &kTextFont;
}

const spyro::TitleLogoFacts &Runtime::logoFacts() const {
  return kLogoFacts;
}

} // namespace spyro2
