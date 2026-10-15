#include "spyro2/core/runtime.h"

#include "core.h"
#include "core/boot_prefix_frame_driver.h"
#include "core/native_owners.h"
#include "crt0_boot.h"
#include "game.h"
#include "guest_code_module.h"
#include "spyro2/core/boot_facts.h"
#include "spyro2/core/logo_facts.h"
#include "spyro2/render/hud_anchor.h"
#include "spyro2/render/render_facts.h"

#include <lucent/log.h>
#include <memory>

namespace spyro2 {
namespace {

// MEASURED from SCUS_944.25 with external/psxport/tools/disasm.py over a RAM image built the
// PS-X EXE way (file[0x800] -> t_addr 0x80010000). Each address is quoted with the bytes that
// give it its meaning, so a reader can re-derive it instead of trusting the table.

// libetc VSync. 37 `jal` sites reach it in the resident text, and its own body is the double
// buffered field wait: 0x80058EDC reads the two counter words through 0x80066450/0x80066454,
// spins until they differ, and returns the delta for a negative argument.
constexpr std::uint32_t kVSync = 0x80058EDCu;

// The word SCUS_944.25's VSync returns for a negative argument: the guest's own vblank count, not a
// hardware register. The negative path (0x80058F34 `bgez $a0` not taken) falls to
//   80058F3C  lui $v0,0x8006 ; 80058F40  lw $v0,0x6618($v0)
// and that word is zeroed at 0x8005AC58 and incremented by the libetc vblank callback at
//   8005ACB0  addiu $v0,$v0,1 ; 8005ACB8  sw $v0,0x6618($at)
// The title's frame limiter (draw 0x80015900..0x80015990) compares it against its own saved last
// value, so answering from the 16-bit HBlank timer 0x1F801110 made `now - last < 2` hold for
// thousands of steps after the timer wrapped. See docs/issues/0160.
constexpr std::uint32_t kVSyncQueryCounter = 0x80066618u;

// libcd's low-level command worker. 0x80058858 indexes a per-command table at 0x800663C8 with the
// command byte, calls the controller send at 0x8005CB80, and restores the sync callback it
// cleared at 0x80066700 around the wait: CdCommand(com, parm, result).
constexpr std::uint32_t kCdCommand = 0x80058858u;

// libcd CdControlB(com, param, result) at 0x80058994: it sends the command through the controller
// leaf 0x8005CB80 and then waits in CD_sync (0x8005C900) on the guest's own interrupt-set status
// byte 0x800669DC to reach Complete. The game calls it for the XA Setmode 0xC8 at 0x800131B0. With
// CdRead and CdCommand native nothing ever raises that byte, so the retail body runs out its
// 0x3C0 vblank deadline ("CD timeout", 0x8005C9CC) before every such call. See docs/issues/0161.
constexpr std::uint32_t kCdControlB = 0x80058994u;

// libcd's synchronisation wait. 0x80058810 is called with a0=1 and polled until it returns 2
// (0x8001372C-0x80013738 and 0x80013778-0x80013784), which is CdSync(noblock) reporting CS_SELF.
constexpr std::uint32_t kCdSync = 0x80058810u;

// libcd's finite data read. 0x80058108 takes (dest, source, mode), derives the sector size from
// mode & 0x30 (0x80058198-0x800581B0: 0x200 / 0x249 / 0x246 bytes), and issues the read through
// the command owner: CdRead.
constexpr std::uint32_t kCdRead = 0x80058108u;

// libcd's initialisation, CdInit at 0x800582B8 (called by the CD bootstrap leaf 0x80011B3C), is NOT
// bound: the retail body runs through Lightrec and registers libcd's interrupt handler and its
// callback table itself, the same way Spyro 3's does. See docs/issues/0159.

// The guest word libcd's CdReadyCallback writes. 0x800582A4 is `*(0x800663B8) = a0` returning the
// previous value, and the CD bootstrap leaf 0x80011B68 calls it with the image's own per-sector
// callback 0x8001379C.
constexpr std::uint32_t kCdReadyCallbackPointer = 0x800663B8u;

// libgpu's DrawSync, and the two leaves of its measured timeout. The display bootstrap leaf
// 0x80011BBC calls 0x8005574C, 0x8005557C and 0x800556F0 in sequence after its VSync, and the
// old finite bootstrap named the DrawSync timeout globals 0x80066354/0x80066358 in this image.
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

constexpr psx::game::Crt0Image kCrt0{
    .bss = {0x80066ED8u, 0x8006D264u},
    .stackTopWordAddress = 0x80066D3Cu,
    .stackReserveWordAddress = 0x80066D38u,
    .heapBase = 0x8006D264u,
    .heapSizeStoreAddress = 0x8006509Cu,
    .heapBaseStoreAddress = 0x80065098u,
    .globalPointer = 0x80066D38u,
    .libcInitEntry = 0x8005ABD8u,
    .crt0Entry = 0x8005478Cu,
    .stackBias = {.declared = true, .bytes = -8},
};

// The loader's ready callback (0x8001379C) ends a read on `$a0 & 0xFF == 2` and otherwise chains
// the next one-sector read from inside the callback.
constexpr psx::cd::GuestInterruptLibcd::Completion kCdCompletion{.status = 2, .perStockRead = true};

} // namespace

Runtime::Runtime()
    : spyro::Runtime(spyro::Title::Spyro2), vsync_(kVSync, kVSyncQueryCounter),
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

void Runtime::startProgram(Core &core) {
  psx::game::Crt0Boot{kCrt0}.run(core);
  spyro::bootPrefixFrameDriver(core).initialize();
}

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

std::unique_ptr<psx::game::FrameDriver> Runtime::createFrameDriver(Game &game) {
  return std::make_unique<spyro::BootPrefixFrameDriver>(game, kBootPrefixFacts);
}

void Runtime::stockCdReadLanded(Core &core, const psx::cd::StockReadLanding &landing) {
  // The loader streams its code modules through the stock read and then calls into them.
  psx::code_module::publishStockReadLanding(core, landing);
}

std::optional<psx::gpu::PacketPoolExtent> Runtime::packetPool(Core &core) const {
  return kPacketPool.extent(core);
}

bool Runtime::interpolates() const {
  return true;
}

RenderCapabilities Runtime::renderCapabilities() const {
  return RenderCapabilities{
      .defaultPath = RenderPath::Record,
      .nativeRenderPath = false,
  };
}

bool Runtime::guestVramIsPicture(const Game &) const {
  // Every presented field is the guest's own drawing; producers only key its packets.
  return true;
}

bool Runtime::sealedFrameIsCut(Core &core) const {
  // A session's first record, sealed before its driver exists, is a cut.
  return core.game->frameDriver == nullptr || spyro::bootPrefixFrameDriver(core).cut().isCut();
}

const spyro::TitleLogoFacts &Runtime::logoFacts() const {
  return kLogoFacts;
}

} // namespace spyro2
