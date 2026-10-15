// Spyro 1's fullscreen tint and cinematic letterbox through their shipping overrides, linked into
// the overlay pair, walked into the record and composed: t = 1 is the frame drawn, t = 0.5 the
// frame drawn halfway. The guest bodies are synthetic stand-ins that write the retail packets.
#include "frame_composer.h"
#include "guest_call.h"
#include "image_identity.h"
#include "mips_asm.h"
#include "record_frames.h"
#include "screen_overlays.h"
#include "spyro1_ordering_table.h"
#include "spyro_session.h"
#include "test_game.h"
#include "testutil.h"

#include <lucent/log.h>

#include <cstdint>
#include <exception>
#include <memory>
#include <utility>
#include <vector>

namespace {

using spyro::test::primitives;
using spyro::test::sameFrame;
using spyro1::CinematicLetterbox;
using spyro1::FullscreenTint;
using spyro1::OrderingTable;
using spyro1::ScreenOverlays;
namespace mips = psx::test::mips;

constexpr std::uint32_t kBins = 0x80100000u;
constexpr std::uint32_t kBinCount = 0x800u;
constexpr std::uint32_t kOverlayPair = 0x80104000u;
constexpr std::uint32_t kPool = 0x80120000u;
constexpr std::uint32_t kFlatQuad = 0x28000000u;
constexpr std::uint32_t kFlatQuadSemiTransparent = 0x2A000000u;

struct Store {
  std::uint32_t address;
  std::uint32_t value;
};

std::uint32_t tag(std::uint32_t words, std::uint32_t next) {
  return (words << 24) | (next & 0x00FFFFFFu);
}

std::uint32_t position(std::uint32_t x, std::uint32_t y) {
  return ((y & 0xFFFFu) << 16) | (x & 0xFFFFu);
}

struct DrawnFrame {
  psx::present::FrameRecord shown;
  psx::present::FrameState state;
};

class OverlayScene {
public:
  OverlayScene() {
    game_ = std::make_unique<spyro::test::SessionGame>();
    core().imageCatalog().activate("synthetic overlays", {0x10000u, 0x78000u}, 3);
    core().mem_w32(OrderingTable::kFlatten, mips::jrRa());
    core().mem_w32(OrderingTable::kFlatten + 4, mips::nop());
    OrderingTable::registerOverrides(core());
    ScreenOverlays::registerOverrides(core());
  }

  Core &core() {
    return game_->core;
  }

  const psx::present::StateProducers &producers() {
    return spyro::session(core()).stateProducers;
  }

  // An empty table and overlay pair, then `draw`, the flatten and DrawOTag's walk of the pair.
  template <class Draw> DrawnFrame frame(Draw draw) {
    for (std::uint32_t at = 0; at < kBinCount * 8; at += 4) {
      core().mem_w32(kBins + at, 0);
    }
    core().mem_w32(kOverlayPair, 0);
    core().mem_w32(kOverlayPair + 4, 0);
    core().mem_w32(OrderingTable::kBinTablePointer, kBins);
    core().mem_w32(OrderingTable::kOverlayPairPointer, kOverlayPair);
    core().mem_w32(ScreenOverlays::kPacketCursor, kPool);
    core().frameStates.clear();
    draw();
    const std::uint32_t head = core().mem_r32(kOverlayPair + 4);
    psx::cpu::callGuestNow(core(), "test", OrderingTable::kFlatten, kBinCount);
    psx::present::FrameRecord shown = spyro::test::submitChain(core(), head);
    psx::present::FrameState state = core().frameStates.collect(shown);
    return {std::move(shown), std::move(state)};
  }

  // The tint's packets as 0x800190D4 writes and links them: a DR_MODE, then the POLY_F4.
  void tint(std::uint32_t blend, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    const std::uint32_t poly = kPool + 0xC;
    writeBody(FullscreenTint::kEntry,
              {{kPool, tag(2, poly)},
               {kPool + 4, 0xE1000400u | (blend << 5)},
               {kPool + 8, 0},
               {poly, tag(5, 0)},
               {poly + 4,
                kFlatQuadSemiTransparent | (std::uint32_t{b} << 16) | (std::uint32_t{g} << 8) | r},
               {poly + 8, position(0, 8)},
               {poly + 12, position(0x200, 8)},
               {poly + 16, position(0, 0xE8)},
               {poly + 20, position(0x200, 0xE8)},
               {kOverlayPair, poly},
               {kOverlayPair + 4, kPool},
               {ScreenOverlays::kPacketCursor, kPool + 0x24}});
    psx::cpu::callGuestNow(core(), "test", FullscreenTint::kEntry, blend, r, g, b);
  }

  // The letterbox's two bars at `height`, as 0x80018F30 leaves them.
  void letterbox(std::uint32_t height) {
    const std::uint32_t bottom = kPool + 0x18;
    writeBody(CinematicLetterbox::kEntry,
              {{CinematicLetterbox::kBarHeight, height},
               {kPool, tag(5, bottom)},
               {kPool + 4, kFlatQuad},
               {kPool + 8, position(0, 0)},
               {kPool + 12, position(0x200, 0)},
               {kPool + 16, position(0, height)},
               {kPool + 20, position(0x200, height)},
               {bottom, tag(5, 0)},
               {bottom + 4, kFlatQuad},
               {bottom + 8, position(0, 0xF0 - height)},
               {bottom + 12, position(0x200, 0xF0 - height)},
               {bottom + 16, position(0, 0xF0)},
               {bottom + 20, position(0x200, 0xF0)},
               {kOverlayPair, bottom},
               {kOverlayPair + 4, kPool},
               {ScreenOverlays::kPacketCursor, kPool + 0x30}});
    psx::cpu::callGuestNow(core(), "test", CinematicLetterbox::kEntry);
  }

private:
  // A guest body at `entry` making `stores`; a call reaches it through the override.
  void writeBody(std::uint32_t entry, const std::vector<Store> &stores) {
    std::uint32_t at = entry;
    const auto emit = [this, &at](std::uint32_t word) {
      core().mem_w32(at, word);
      at += 4;
    };
    for (const Store &store : stores) {
      emit(mips::lui(mips::kT0, store.value >> 16));
      emit(mips::ori(mips::kT0, mips::kT0, store.value & 0xFFFFu));
      emit(mips::lui(mips::kT1, store.address >> 16));
      emit(mips::ori(mips::kT1, mips::kT1, store.address & 0xFFFFu));
      emit(mips::sw(mips::kT0, mips::kT1, 0));
    }
    emit(mips::jrRa());
    emit(mips::nop());
  }

  std::unique_ptr<spyro::test::SessionGame> game_;
};

struct Composed {
  bool asDrawn = false;
  bool between = false;
  bool midwayDiffers = false;
};

// Three frames drawn in order; `midway` is drawn at the state halfway between the others.
struct ThreeFrames {
  DrawnFrame from;
  DrawnFrame midway;
  DrawnFrame to;
};

Composed compose(OverlayScene &scene, const ThreeFrames &frames) {
  Composed result;
  const auto &[from, midway, to] = frames;
  const auto atOne =
      psx::present::composeFrame(to.shown, &from.state, to.state, 1.0f, scene.producers());
  result.asDrawn =
      !primitives(to.shown).empty() && atOne && sameFrame(primitives(*atOne), primitives(to.shown));
  const auto atHalf =
      psx::present::composeFrame(to.shown, &from.state, to.state, 0.5f, scene.producers());
  result.between = atHalf && sameFrame(primitives(*atHalf), primitives(midway.shown));
  result.midwayDiffers = !sameFrame(primitives(midway.shown), primitives(to.shown));
  return result;
}

void test_the_flatten_assigns_bins_and_the_overlay_pair() {
  OverlayScene scene;
  Core &core = scene.core();
  constexpr std::uint32_t kFirst = kPool;
  constexpr std::uint32_t kHead = kPool + 0x20;
  constexpr std::uint32_t kPast = kPool + 0x40;
  constexpr std::uint32_t kOverlay = kPool + 0x60;
  core.mem_w32(OrderingTable::kBinTablePointer, kBins);
  core.mem_w32(OrderingTable::kOverlayPairPointer, kOverlayPair);
  core.mem_w32(kBins + 5 * 8, kHead);
  core.mem_w32(kBins + 5 * 8 + 4, kFirst);
  core.mem_w32(kFirst, tag(5, kHead));
  core.mem_w32(kHead, tag(5, kPast)); // linked on by the flatten, past the bin
  core.mem_w32(kPast, tag(5, 0));
  core.mem_w32(kOverlayPair, kOverlay);
  core.mem_w32(kOverlayPair + 4, kOverlay);
  core.mem_w32(kOverlay, tag(5, 0));
  OrderingTable::assignSlots(core, kBinCount);
  const psx::present::OtSlot bin{0, 5};
  const psx::present::OtSlot overlay{OrderingTable::kOverlayTable, 0};
  CHECK(core.otTables.assigned(kFirst) == bin);
  CHECK(core.otTables.assigned(kHead) == bin);
  CHECK(!core.otTables.assigned(kPast));
  CHECK(core.otTables.assigned(kOverlay) == overlay);
}

void test_the_tint_composes_its_colour_between_two_frames() {
  OverlayScene scene;
  const ThreeFrames frames{scene.frame([&scene] {
                             scene.tint(1, 0x20, 0x10, 0x00);
                           }),
                           scene.frame([&scene] {
                             scene.tint(1, 0x40, 0x30, 0x20);
                           }),
                           scene.frame([&scene] {
                             scene.tint(1, 0x60, 0x50, 0x40);
                           })};
  const Composed composed = compose(scene, frames);
  CHECK(composed.asDrawn);
  CHECK(composed.between);
  CHECK(composed.midwayDiffers);
}

void test_a_tint_that_changes_blend_composes_as_drawn() {
  OverlayScene scene;
  const DrawnFrame from = scene.frame([&scene] {
    scene.tint(2, 0x20, 0x20, 0x20);
  });
  const DrawnFrame to = scene.frame([&scene] {
    scene.tint(1, 0x60, 0x60, 0x60);
  });
  const auto atHalf =
      psx::present::composeFrame(to.shown, &from.state, to.state, 0.5f, scene.producers());
  CHECK(atHalf && sameFrame(primitives(*atHalf), primitives(to.shown)));
}

void test_the_letterbox_composes_its_bars_between_two_frames() {
  OverlayScene scene;
  const ThreeFrames frames{scene.frame([&scene] {
                             scene.letterbox(4);
                           }),
                           scene.frame([&scene] {
                             scene.letterbox(8);
                           }),
                           scene.frame([&scene] {
                             scene.letterbox(12);
                           })};
  CHECK_EQ(primitives(frames.to.shown).size(), 2u);
  const Composed composed = compose(scene, frames);
  CHECK(composed.asDrawn);
  CHECK(composed.between);
  CHECK(composed.midwayDiffers);
}

} // namespace

int main() {
  try {
    RUN(the_flatten_assigns_bins_and_the_overlay_pair);
    RUN(the_tint_composes_its_colour_between_two_frames);
    RUN(a_tint_that_changes_blend_composes_as_drawn);
    RUN(the_letterbox_composes_its_bars_between_two_frames);
    return pt_summary();
  } catch (const std::exception &error) {
    lucent::error("test_spyro1_screen_overlays", "FAIL: {}", error.what());
    return 1;
  }
}
