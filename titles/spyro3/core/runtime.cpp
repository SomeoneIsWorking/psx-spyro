#include "spyro3/core/runtime.h"

#include "core.h"
#include "game.h"
#include "spyro3/core/boot_facts.h"
#include "spyro3/core/warp.h"
#include "spyro3/render/hud_anchor.h"
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

// Negative VSync reads the libetc vblank count at 0x8006B480 (docs/issues/0160).
constexpr std::uint32_t kVSyncQueryCounter = 0x8006B480u;

// libcd CdRead(dest, src, mode).
constexpr std::uint32_t kCdRead = 0x8005D96Cu;

// libcd CdSync, polled by the CD bootstrap leaf 0x80050440 until it reports CS_SELF.
constexpr std::uint32_t kCdSync = 0x8005E074u;

// libcd CdCommand(com, parm, result), reached from 0x8002A7D0 with $a0 = 0x0E.
constexpr std::uint32_t kCdCommand = 0x8005E0BCu;

// CdInit (0x8005DB1C) stays guest: it registers libcd's CD interrupt handler (docs/issues/0159).

// libcd's CdReadyCallback word, defaulted by CdInit at 0x8005DB6C.
constexpr std::uint32_t kCdReadyCallbackPointer = 0x8006B3D0u;

// The per-sector reader 0x80050504 ends a read on `$a0 & 0xFF == 2`.
constexpr psx::cd::GuestInterruptLibcd::Completion kCdCompletion{.status = 2, .perStockRead = true};

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
  libgte_.bind(game);
  spyro::registerEngineRenderOverrides<kRenderFacts>(game.core);
  lucent::info(
      "boot",
      "installed Spyro 3's stock libraries and natives for the projection offset leaf, the "
      "moby visibility walk, the terrain drawer, the flatten and the HUD anchor");
}

void Runtime::codeLanded(Core &core,
                         const psx::cd::StockReadLanding &landing,
                         const psx::code_module::Digest &content) {
  spyro::engine_render::screenDrawsLanded<kRenderFacts>(core, landing, content.identity);
}

std::unique_ptr<spyro::TitleState> Runtime::createTitleState() const {
  return std::make_unique<hud_anchor::DrawState>();
}

std::unique_ptr<spyro::Warp> Runtime::createWarp() const {
  return std::make_unique<Warp>();
}

} // namespace spyro3
