#include "spyro3/core/runtime.h"

#include "core.h"
#include "game.h"
#include "spyro3/core/boot_facts.h"
#include "spyro3/core/loader_checksum.h"
#include "spyro3/core/logo_facts.h"
#include "spyro3/core/warp.h"
#include "spyro3/render/hud_anchor.h"
#include "spyro3/render/hud_slide.h"
#include "spyro3/render/render_facts.h"

#include <lucent/log.h>
#include <memory>

namespace spyro3 {
namespace {

// libgte SetGeomOffset, called by the geometry init 0x8002A99C with (0x100, 0x78).
constexpr psx::hle::StockLibgte::Entries kLibgte{.setGeomOffset = 0x8005D35Cu};

constexpr psx::game::Crt0Image kCrt0{.entry = 0x80059444u};

// libetc VSync: a non-negative argument waits, a negative one returns the count.
constexpr std::uint32_t kVSync = 0x8005956Cu;

// libcd CdRead(dest, src, mode).
constexpr std::uint32_t kCdRead = 0x8005D96Cu;

// libcd CdSync, polled by the CD bootstrap leaf 0x80050440 until it reports CS_SELF.
constexpr std::uint32_t kCdSync = 0x8005E074u;

// libcd CdCommand(com, parm, result), reached from 0x8002A7D0 with $a0 = 0x0E.
constexpr std::uint32_t kCdCommand = 0x8005E0BCu;

// CdInit (0x8005DB1C) stays guest: it registers libcd's CD interrupt handler (docs/issues/0159).

// libcd's CdReadyCallback word, defaulted by CdInit at 0x8005DB6C.
constexpr std::uint32_t kCdReadyCallbackPointer = 0x8006B3D0u;

// The per-sector reader 0x80050504 ends a read on `$a0 & 0xFF == 2`. Its loads poll it, so the
// drive's rate is a loading-only wait; the loader's checksum that ran in that time is
// loader_checksum's.
constexpr psx::cd::GuestInterruptLibcd::Completion kCdCompletion{
    .status = 2, .perStockRead = true, .timing = psx::cd::ReadTiming::AtOnce};

// spyro-3 asm camera/func_80013918 polls L2/R2 (refused in gamestate 7 and with L1/R1 held),
// func_8001241C computes the errors that both the camera's error step and a level's own
// (0x80074308) take.
constexpr spyro::FollowCameraFacts kFollowCamera{.shoulderPoll = 0x80013918u,
                                                 .errorCompute = 0x8001241Cu,
                                                 .camera = 0x8006DFF8u,
                                                 .padHeld = 0x8006E538u,
                                                 .turnButtons = 0x3u,
                                                 .refusedHeld = 0xCu,
                                                 .spyroBusy = 0x80070568u,
                                                 .gamestate = 0x8006E344u,
                                                 .refusedGamestate = 7u};

} // namespace

Runtime::Runtime()
    : spyro::BootPrefixRuntime(kCrt0, kBootPrefixFacts),
      vsync_({.entry = kVSync, .queryCounter = kVSyncQueryCounter}),
      libcd_({.read = kCdRead, .command = kCdCommand, .sync = kCdSync},
             {},
             kCdReadyCallbackPointer,
             kCdCompletion),
      libgte_(kLibgte) {}

void Runtime::registerOverrides(Game &game) {
  vsync_.bind(game);
  libcd_.bind(game);
  // HUD edge widgets keep their distance from the frame edges.
  spyro3::hud_anchor::registerOverrides(game.core);
  spyro3::hud_slide::registerOverrides(game.core);
  spyro::registerFollowCamera(game.core, kFollowCamera);
  spyro::installSkyWave(game.core, kSkyWave);
  spyro::installEffectSpawns(game.core, kDrawMotion.effects);
  installLoaderChecksum(game.core);
  libgte_.bind(game);
  spyro::registerEngineRenderOverrides<kRenderFacts>(game.core);
  lucent::info(
      "boot",
      "installed Spyro 3's stock libraries and natives for the projection offset leaf, the "
      "moby visibility walk, the terrain drawer, the flatten and the HUD anchor");
}

std::unique_ptr<spyro::TitleState> Runtime::createTitleState() const {
  return std::make_unique<hud_anchor::DrawState>();
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

std::span<const psx::present::OutlineFont> Runtime::outlineFonts() const {
  return kOutlineFonts;
}

const spyro::TitleLogoFacts &Runtime::logoFacts() const {
  return kLogoFacts;
}

} // namespace spyro3
