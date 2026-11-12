#include "render/field/guest_moby_drawer.h"

#include "core.h"
#include "core/boot_prefix_frame_driver.h"
#include "game.h"
#include "gte_registers.h"
#include "guest_redraw.h"
#include "native_dispatch.h"
#include "r3000.h"
#include "render/field/guest_view_slope.h"
#include "render/field/guest_working_memory.h"
#include "render/terrain/guest_depth_bins.h"

#include <lucent/log.h>

#include <cstdlib>

namespace spyro::guest_moby_drawer {
namespace {

// The object pass clears the drawer's depth buckets before the walk (SCUS_944.25 8004C53C, a
// 0xC00-byte memset from the scratch block's start).
constexpr std::uint32_t kBucketBytes = 0xC00;

// Which of the drawer's calls a margin pass follows.
enum class Pass : std::uint8_t {
  // The field's object pass: the walk's shadows are drawn after the drawer.
  Field,
  // Spyro 3's chained Sparx pass, which draws no shadows.
  Chained,
};

class MarginPass {
public:
  MarginPass(Core &core, const Image &image, Pass pass)
      : core_(core), image_(image), pass_(pass),
        driver_(core.game->frameDriverAs<BootPrefixFrameDriver>()),
        pool_(driver_.section().idlePool(core)) {}

  // Whether the drawer's scratch block is the one below the idle pool's end, where the chained
  // pass moves it (SCUS_944.67 8001EA14).
  [[nodiscard]] static bool inIdleScratch(Core &core, const Image &image) {
    const DrawSection::PacketPool pool =
        core.game->frameDriverAs<BootPrefixFrameDriver>().section().idlePool(core);
    return core.mem_r32(image.globals.scratchBaseWord) == pool.start + pool.bytes;
  }

  void draw() {
    // The packets go to the idle pool below its own scratch block, so the guest's cursor and the
    // room it tests (the Sparx pass's 0xA00 bytes, SCUS_944.67 8001E638) stay retail's. The field
    // pass starts the draw's margin packets; the chained pass places its own after them.
    MarginPackets &packets = driver_.marginPackets();
    const std::uint32_t start = pass_ == Pass::Field ? pool_.start : packets.next(pool_.start);
    packets.drew(pool_.start, drawFrom(start));
  }

private:
  // The pass's packets from `start`; returns where they end (`start` when the run kept nothing).
  std::uint32_t drawFrom(std::uint32_t start) {
    const guest_render_globals::Globals &globals = image_.globals;
    const std::uint32_t scratch =
        core_.mem_r32(globals.scratchBaseWord) - guest_moby::kDrawerScratchBelow;
    const std::uint32_t limit = pool_.start + pool_.bytes - guest_moby::kDrawerScratchBelow;
    const std::uint32_t table = core_.mem_r32(globals.orderingTable);
    psx::present::GuestRedraw redraw(core_);
    redraw.keep(start, limit - start);
    redraw.keep(globals.orderingTableMark, 4);
    redraw.keep(table, depth_bins::kBins * depth_bins::kBinStride);
    // The drawer links a bin's new packets after the packet it last linked.
    for (std::uint32_t bin = 0; bin < depth_bins::kBins; ++bin) {
      if (const std::uint32_t last = core_.mem_r32(table + bin * depth_bins::kBinStride);
          last != 0) {
        redraw.keep(last, guest_memory::kTagLinkBytes);
      }
    }
    core_.mem_w32(globals.primitiveCursor, start);
    for (std::uint32_t word = scratch; word < scratch + kBucketBytes; word += 4) {
      core_.mem_w32(word, 0);
    }
    const MarginSectors &sectors = driver_.marginSectors();
    guest_moby::walk(core_,
                     image_.walk,
                     globals,
                     {.margin = &sectors, .level = core_.mem_r32(image_.sectorTable)});
    const std::uint32_t listed = listedEntries(scratch + guest_moby::kRenderListAboveScratch);
    if (!redraw.run(image_.drawer.entry, {}) ||
        (pass_ == Pass::Field && !redraw.dispatch(image_.shadowDrawer, {}))) {
      redraw.keepNothing();
      return start;
    }
    const std::uint32_t end = core_.mem_r32(globals.primitiveCursor);
    if (end > limit) {
      lucent::error("mobymargin",
                    "the margin mobys' packets end at 0x{:08X}, past the idle pool's 0x{:08X}",
                    end,
                    limit);
      std::abort();
    }
    lucent::debug("mobymargin",
                  "{} margin pass: {} moby(s) listed, packets 0x{:08X}..0x{:08X} of the idle pool, "
                  "0x{:X} bytes left",
                  pass_ == Pass::Field ? "field" : "chained",
                  listed,
                  start,
                  end,
                  limit - end);
    return end;
  }

  [[nodiscard]] std::uint32_t listedEntries(std::uint32_t list) {
    std::uint32_t count = 0;
    for (std::uint32_t at = list; core_.mem_r32(at + guest_moby::entry::kClass) != 0;
         at += guest_moby::kEntryStride) {
      ++count;
    }
    return count;
  }

  Core &core_;
  const Image &image_;
  Pass pass_;
  BootPrefixFrameDriver &driver_;
  DrawSection::PacketPool pool_;
};

} // namespace

void draw(Core &core, const Image &image) {
  const std::uint32_t list =
      core.gte().readData(psx::gte::kVxy1) + guest_moby::kRenderListAboveScratch;
  const std::uint32_t caller = core.r[psx::cpu::kRa];
  if (!psx::cpu::callOriginalOrPropagate(core, image.drawer.entry)) {
    return;
  }
  if (presentedSlope(core) != guest_moby_frustum::kRetailSlope) {
    if (caller == image.drawer.fieldPassReturn) {
      MarginPass(core, image, Pass::Field).draw();
    } else if (image.drawer.chainedPass && MarginPass::inIdleScratch(core, image)) {
      MarginPass(core, image, Pass::Chained).draw();
    }
  }
  intro_card::extend(core, image.drawer.introCard, image.globals, list);
}

} // namespace spyro::guest_moby_drawer
