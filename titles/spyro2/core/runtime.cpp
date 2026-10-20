#include "spyro2/core/runtime.h"

#include "core.h"
#include "game.h"
#include "spyro2/core/boot_facts.h"
#include "spyro2/core/warp.h"
#include "spyro2/render/hud_anchor.h"
#include "spyro2/render/render_facts.h"

#include <lucent/log.h>
#include <memory>

namespace spyro2 {
namespace {

// libetc VSync. 37 `jal` sites reach it in the resident text, and its own body is the double
// buffered field wait: 0x80058EDC reads the two counter words through 0x80066450/0x80066454,
// spins until they differ, and returns the delta for a negative argument.
constexpr std::uint32_t kVSync = 0x80058EDCu;

// A negative VSync returns this guest vblank count (lw at 0x80058F40, ++ at 0x8005ACB0); docs/issues/0160.
constexpr std::uint32_t kVSyncQueryCounter = 0x80066618u;

// libcd's low-level command worker. 0x80058858 indexes a per-command table at 0x800663C8 with the
// command byte, calls the controller send at 0x8005CB80, and restores the sync callback it
// cleared at 0x80066700 around the wait: CdCommand(com, parm, result).
constexpr std::uint32_t kCdCommand = 0x80058858u;

// CdControlB(com, param, result): its CD_sync wait on 0x800669DC never completes once CdRead and
// CdCommand are native, so it is native too (docs/issues/0161).
constexpr std::uint32_t kCdControlB = 0x80058994u;

// libcd's synchronisation wait. 0x80058810 is called with a0=1 and polled until it returns 2
// (0x8001372C-0x80013738 and 0x80013778-0x80013784), which is CdSync(noblock) reporting CS_SELF.
constexpr std::uint32_t kCdSync = 0x80058810u;

// libcd's finite data read. 0x80058108 takes (dest, source, mode), derives the sector size from
// mode & 0x30 (0x80058198-0x800581B0: 0x200 / 0x249 / 0x246 bytes), and issues the read through
// the command owner: CdRead.
constexpr std::uint32_t kCdRead = 0x80058108u;

// CdInit 0x800582B8 stays retail: it registers libcd's interrupt handler itself (docs/issues/0159).

// The guest word libcd's CdReadyCallback writes. 0x800582A4 is `*(0x800663B8) = a0` returning the
// previous value, and the CD bootstrap leaf 0x80011B68 calls it with the image's own per-sector
// callback 0x8001379C.
constexpr std::uint32_t kCdReadyCallbackPointer = 0x800663B8u;

// libgpu DrawSync's timeout: its deadline and poll-count words and the arm and check leaves.
constexpr std::uint32_t kGpuTimeoutDeadline = 0x80066354u;
constexpr std::uint32_t kGpuTimeoutPollCount = 0x80066358u;
constexpr std::uint32_t kGpuTimeoutArm = 0x80057B20u;
constexpr std::uint32_t kGpuTimeoutCheck = 0x80057AF4u;

// Both parity pools sit below the OT header: the lowest base is 0x80069998 and the top 0x8006B2A4
// (FUN_80011DD4, FUN_8004D158; a state that shrinks the pools keeps both inside that window).
constexpr psx::gpu::GuestPacketPoolWindows kPacketPool{
    .representation = psx::gpu::GuestPacketPoolWindows::Representation::LiveBaseEndPointers,
    .basePointer = {0x80069998u, 0u},
    .endPointer = {0x8006B2A4u, 0u},
};

// libgte's projection leaves, reached by `jal` from the display bootstrap's geometry init 80011D24:
//   80057AE8  48c4d000  mtc2 $a0, $26          ; SetGeomScreen(h)
//   80057AF8  sll $a0,$a0,16 ; sll $a1,$a1,16 ; mtc2 $a0,$24 ; mtc2 $a1,$25   ; SetGeomOffset(x, y)
constexpr psx::hle::StockLibgte::Entries kLibgte{.setGeomOffset = 0x80057AF8u,
                                                 .setGeomScreen = 0x80057AE8u};

constexpr psx::game::Crt0Image kCrt0{.entry = 0x8005478Cu};

// The loader's ready callback (0x8001379C) ends a read on `$a0 & 0xFF == 2` and otherwise chains
// the next one-sector read from inside the callback.
constexpr psx::cd::GuestInterruptLibcd::Completion kCdCompletion{.status = 2, .perStockRead = true};

} // namespace

Runtime::Runtime()
    : spyro::BootPrefixRuntime(kCrt0, kBootPrefixFacts),
      vsync_({.entry = kVSync, .queryCounter = kVSyncQueryCounter}),
      libgpu_({.timeoutArm = kGpuTimeoutArm,
               .timeoutCheck = kGpuTimeoutCheck,
               .timeoutDeadlineWord = kGpuTimeoutDeadline,
               .timeoutFlagWord = kGpuTimeoutPollCount}),
      libcd_(
          {.read = kCdRead, .command = kCdCommand, .controlBlocking = kCdControlB, .sync = kCdSync},
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
  lucent::info(
      "boot",
      "installed Spyro 2's stock libraries and natives for the projection leaves, the moby "
      "visibility walk, the terrain drawer, the flatten and the HUD anchor");
}

std::optional<psx::gpu::PacketPoolExtent> Runtime::packetPool(Core &core) const {
  return kPacketPool.extent(core);
}

std::unique_ptr<spyro::Warp> Runtime::createWarp() const {
  return std::make_unique<Warp>();
}

} // namespace spyro2
