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

// The PS-X EXE header, from the identity manifest and confirmed by the crt0 that consumes it:
//   0x10 entry 0x80059444   0x18 t_addr 0x80010000   0x1C t_size 0x0005C800
// The crt0 clears exactly the declared bss span and then derives `$sp` from the declared stack top:
//   80059444  lui $v0,0x8007 ; addiu $v0,$v0,-0x3b0c   -> 0x8006C4F4, the bss low
//   8005944C  lui $v1,0x8007 ; addiu $v1,$v1,0x42d0    -> 0x800742D0, the bss high (the loop bound)
//   8005946C  lw  $v0,-0x3c1c($v0)                      -> reads 0x8006C3E4, stackTopWordAddress
//   80059474  addi $v0,$v0,-8 ; or $sp,$v0,$t0          -> the -8 bias
// libgte's SetGeomOffset, called by the geometry init 0x8002A99C with (0x100, 0x78):
//   8005D35C  sll $a0,$a0,16 ; sll $a1,$a1,16 ; mtc2 $a0,$24 ; mtc2 $a1,$25
// The image's one CR26 write (0x8005955C) takes an argument computed at its call site and is left
// to the guest.
constexpr psx::hle::StockLibgte::Entries kLibgte{.setGeomOffset = 0x8005D35Cu};

constexpr psx::game::Crt0Image kCrt0{.entry = 0x80059444u};

// libetc VSync. 36 `jal` sites reach it in the resident text, and its own body is the double
// buffered field wait:
//   8005956C  lui $v0,0x8007 ; 80059570 lw $v0,-0x60d8($v0)   the mode-0 counter POINTER
//   80059574  lui $a1,0x8007 ; 80059578 lw $a1,-0x60d4($a1)   the mode-1 counter POINTER
//   8005958C  lw  $s0,($v0)                                     the mode-0 count on entry
//   80059590  lw  $v0,($a1) ; 800595A8 bne $v1,$v0,...          poll the mode-1 count until it
//   moves 800595C0  subu $v0,$v0,$v1                                  minus the count at 0x80069F30
//   800595C4  bgez $a0,0x800595dc ; 800595E0 beq $a0,1 ; 800595E8 blez $a0
// so a nonnegative argument waits and only a negative one returns a count.
constexpr std::uint32_t kVSync = 0x8005956Cu;

// The word a negative VSync returns: the guest's own vblank count maintained by the libetc vblank
// callback, not a hardware register. VSync's negative path (0x800595C4 `bgez $a0` not taken) is
//   800595CC  lui $v0,0x8007 ; 800595D0  lw $v0,-0x4b80($v0)     -> 0x8006B480
// The earlier reading of 0x80069F2C/0x1F801110 described the blocking-wait pointer pair, not this
// query. See docs/issues/0160.
constexpr std::uint32_t kVSyncQueryCounter = 0x8006B480u;

// libcd's finite data read. Structural twin of the Spyro 2 leaf, located here by its own bytes:
//   8005D96C  addiu $sp,$sp,-0x28 ; sw $s4,0x20($sp) ; move $s4,$a0     (dest)
//   8005D978  move $s3,$a1 (src) ; 8005D9A4 move $s2,$a2 (mode)
//   8005D984  lui $s0,0x8007 ; addiu $s0,$s0,-0x4c4c   the stock libcd work area at 0x8006B3B4
//   8005D9A8  jal  0x8005956C   with $a0 = -1 : the VSync query the read's timeout is built on
//   8005D9FC  andi $v1,$a2,0x30                       the sector-size selector, mode & 0x30
// CdRead(dest, src, mode).
constexpr std::uint32_t kCdRead = 0x8005D96Cu;

// libcd's synchronisation wait, the leaf this image's own loader polls. The loader is the CD
// bootstrap leaf 0x80050440 and its poll is unambiguous:
//   80050490  addiu $a0,$zero,1 ; 80050498 move $a1,$zero
//   80050494  jal  0x8005E074
//   8005049C  bne $v0,$s0,0x80050494   with $s0 = 2 (set at 0x8005048C)
// i.e. CdSync(1) polled until it reports CS_SELF. 0x8005E074 is the thin entry
// (`addiu $sp,$sp,-0x18 ; sw $ra ; jal 0x8006143C ; lw $ra ; jr $ra`) over the wait body.
constexpr std::uint32_t kCdSync = 0x8005E074u;

// libcd's low-level command worker, CdCommand(com, parm, result):
//   8005E0BC  addiu $sp,$sp,-0x38 ; move $s1,$a1 ; move $s2,$a2 ; move $s4,$a0
//   8005E0E4  andi $s3,$s4,0xff                            the command byte
//   8005E0E8  lui $v1,0x8007 ; addiu $v1,$v1,-0x4c20        the per-command table at 0x8006B3E0
//   8005E0F4  lw  $s5,-0x4a98($s5)                          the sync callback at 0x8006B568
// The CD bootstrap leaf reaches it directly at 0x8002A7D0 with $a0 = 0x0E.
constexpr std::uint32_t kCdCommand = 0x8005E0BCu;

// libcd's initialisation, CdInit at 0x8005DB1C (called by the CD bootstrap leaf 0x8002A7B4 at
// 0x8002A7C0), is NOT bound: the retail body runs through Lightrec. It registers libcd's
// interrupt handler (CD_init 0x80061C8C -> InterruptCallback(2, 0x800621CC)), which is what the
// libapi dispatcher 0x8005C6B4 needs to acknowledge the CD interrupt; a native stand-in that only
// wrote the callback words left it unregistered and every later CD interrupt ended in the
// "intr timeout" at 0x8005CA18. See docs/issues/0159.

// The guest word libcd's CdReadyCallback reads and writes (CdInit's 8005DB6C stores the default
// ready callback 0x8005DC38 there through `lui $at,0x8007 ; sw $v1,-0x4c30($at)`).
constexpr std::uint32_t kCdReadyCallbackPointer = 0x8006B3D0u;

// The per-sector reader (0x80050504) ends a read on `$a0 & 0xFF == 2` and otherwise starts the next
// one, so it is delivered 2 once per stock CdRead (psxport issue 0143).
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
  // The HUD emitters hold edge widgets at their distance from the frame's edges; the projection
  // leaf carries the widening, and the moby walk and terrain drawer widen their horizontal culls.
  spyro3::hud_anchor::registerOverrides(game.core);
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

std::unique_ptr<spyro::Warp> Runtime::createWarp() const {
  return std::make_unique<Warp>();
}

} // namespace spyro3
