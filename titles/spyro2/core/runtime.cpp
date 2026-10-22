#include "spyro2/core/runtime.h"

#include "core.h"
#include "game.h"
#include "spyro2/core/boot_facts.h"
#include "spyro2/core/warp.h"
#include "spyro2/render/conversation_box.h"
#include "spyro2/render/hud_anchor.h"
#include "spyro2/render/render_facts.h"

#include <lucent/log.h>
#include <memory>

namespace spyro2 {
namespace {

// libetc VSync: a negative argument returns the field delta.
constexpr std::uint32_t kVSync = 0x80058EDCu;

// A negative VSync returns this guest vblank count (lw at 0x80058F40, ++ at 0x8005ACB0); docs/issues/0160.
constexpr std::uint32_t kVSyncQueryCounter = 0x80066618u;

// libcd CdCommand(com, parm, result).
constexpr std::uint32_t kCdCommand = 0x80058858u;

// CdControlB is native because its CD_sync wait never completes otherwise (docs/issues/0161).
constexpr std::uint32_t kCdControlB = 0x80058994u;

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
  installConversationBox(game.core);
  lucent::info(
      "boot",
      "installed Spyro 2's stock libraries and natives for the projection leaves, the moby "
      "visibility walk, the terrain drawer, the flatten, the HUD anchor and the conversation box");
}

std::optional<psx::gpu::PacketPoolExtent> Runtime::packetPool(Core &core) const {
  return kPacketPool.extent(core);
}

bool Runtime::titleCommand(Core &core, const psx::game::TitleCommandLine &input, std::FILE *reply) {
  return spyro::Runtime::titleCommand(core, input, reply) ||
         mobyTeleport_.command(core, input, reply);
}

std::unique_ptr<spyro::Warp> Runtime::createWarp() const {
  return std::make_unique<Warp>();
}

} // namespace spyro2
