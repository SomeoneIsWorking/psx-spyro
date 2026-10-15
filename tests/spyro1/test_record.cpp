// Spyro 1 on the Record path: the render path it selects, and GamestateDraw run through its own
// display tail, each VSync(0) a field and the frame presented once after the return.
#include "booted_title.h"
#include "core.h"
#include "core/field_owner.h"
#include "core/frame_cut.h"
#include "core/guest_globals.h"
#include "game.h"
#include "spyro1/core/frame/frame_draw.h"
#include "spyro1/core/frame/frame_driver.h"
#include "spyro1/core/runtime/field_scheduler.h"
#include "spyro1/core/runtime/runtime.h"
#include "testutil.h"

#include <lucent/log.h>

#include <cstdint>
#include <exception>
#include <initializer_list>

namespace {

constexpr std::uint32_t kFrameDraw = 0x8001ED5Cu;
constexpr std::uint32_t kVSync = 0x8005DBC4u;
// A bss word the synthetic draw stores after its waits, standing in for DrawOTag.
constexpr std::uint32_t kAfterWaits = 0x80077000u;

constexpr std::uint32_t jal(std::uint32_t target) {
  return 0x0C000000u | ((target >> 2u) & 0x03FFFFFFu);
}

// Two VSync(0) waits, then the store, as GamestateDraw's tail waits before its DrawOTag.
void writeSyntheticDraw(Core &core) {
  const std::initializer_list<std::uint32_t> words = {
      0x27BDFFF8u, // addiu sp, sp, -8
      0xAFBF0004u, // sw    ra, 4(sp)
      jal(kVSync),
      0x24040000u, // addiu a0, zero, 0
      jal(kVSync),
      0x24040000u, // addiu a0, zero, 0
      0x3C088007u, // lui   t0, 0x8007
      0x24090055u, // addiu t1, zero, 0x55
      0xAD097000u, // sw    t1, 0x7000(t0)
      0x8FBF0004u, // lw    ra, 4(sp)
      0x27BD0008u, // addiu sp, sp, 8
      0x03E00008u, // jr    ra
      0x00000000u, // nop
  };
  std::uint32_t address = kFrameDraw;
  for (const std::uint32_t word : words) {
    core.mem_w32(address, word);
    address += 4u;
  }
}

void test_spyro1_interpolates() {
  const spyro1::Runtime spyro1;
  CHECK(spyro1.interpolates());
  CHECK(spyro1.guestWidescreenProjection() != nullptr);
}

// Every guest call the native boot and one main-loop step make, answered by `jr ra`; the loader
// stage call advances the stage to 10 as the retail loader does.
void plantBootStubs(Core &core) {
  constexpr std::uint32_t kJrRa = 0x03E00008u;
  for (const std::uint32_t entry :
       {0x8005B988u, 0x800122A8u, 0x80012460u, 0x800123C8u, 0x8005595Cu, 0x80012480u, 0x8001256Cu,
        0x80062350u, 0x80062618u, 0x80062638u, 0x8005F6C8u, 0x80016914u, 0x80017F24u, 0x8005FA28u,
        0x8005F764u, 0x80060030u, 0x8001250Cu, 0x80016500u, 0x80016958u, 0x8005F8F8u, 0x8005B7D8u,
        0x8002D338u, 0x8002D170u, 0x8003385Cu, kFrameDraw}) {
    core.mem_w32(entry, kJrRa);
    core.mem_w32(entry + 4u, 0);
  }
  const std::initializer_list<std::uint32_t> advanceLoadStage = {
      0x3C088007u, // lui   t0, 0x8007
      0x2409000Au, // addiu t1, zero, 10
      0x03E00008u, // jr    ra
      0xAD095864u, // sw    t1, 0x5864(t0)
  };
  std::uint32_t address = 0x80014564u;
  for (const std::uint32_t word : advanceLoadStage) {
    core.mem_w32(address, word);
    address += 4u;
  }
}

// The boot logos are native-width uploads; the main loop's frames are widened.
void test_the_boot_picture_is_native_width_until_the_main_loop() {
  spyro::test::BootedTitle booted(spyro::Title::Spyro1, false);
  Game &game = booted.game();
  Core &core = booted.core();
  game.disablePresentation();
  CHECK(booted.runtime().guestPictureIsNativeWidth(game));
  plantBootStubs(core);
  core.r[29] = 0x801FFF00u;
  game.frameDriver = booted.runtime().createFrameDriver(game);
  spyro1::FrameDriver &driver = spyro1::frameDriver(core);
  driver.initialize(core);
  CHECK(booted.runtime().guestPictureIsNativeWidth(game));
  constexpr std::uint32_t kBootStepBound = 1000;
  std::uint32_t step = 0;
  while (!driver.bootComplete() && step < kBootStepBound) {
    driver.stepFrame(core, step++);
  }
  CHECK(driver.bootComplete());
  CHECK(!booted.runtime().guestPictureIsNativeWidth(game));
}

// A session's first record is a cut; after that a cut is a new game state or level.
void test_a_cut_is_a_new_game_state_or_level() {
  spyro::test::BootedTitle booted(spyro::Title::Spyro1);
  Core &core = booted.core();
  CHECK(booted.runtime().sealedFrameIsCut(core));
  spyro::FrameCut cut(spyro1::kFrameCutFacts);
  const auto drawThenTail = [&] {
    cut.onFrameDrawn(core);
    cut.onFrameTail(core);
    return cut.isCut();
  };
  core.mem_w32(spyro::guest::kGamestate, 0);
  core.mem_w32(spyro::guest::kLevelId, 10);
  CHECK(drawThenTail());
  CHECK(!drawThenTail());
  core.mem_w32(spyro::guest::kLevelId, 11);
  CHECK(drawThenTail());
  CHECK(!drawThenTail());
  core.mem_w32(spyro::guest::kGamestate, 5);
  CHECK(drawThenTail());
}

void test_the_draws_field_waits_are_fields_and_the_frame_is_presented_once() {
  spyro::test::BootedTitle booted(spyro::Title::Spyro1);
  Core &core = booted.core();
  booted.game().disablePresentation();
  spyro1::FieldScheduler fields(booted.game());
  fields.publish();
  writeSyntheticDraw(core);
  core.r[29] = 0x801FFF00u;

  spyro1::FrameDraw draw;
  draw.run(core, fields);
  CHECK_EQ(core.mem_r32(kAfterWaits), 0x55u);
  CHECK_EQ(spyro::fieldOwner(core).fields(), 2u);
  CHECK_EQ(spyro::fieldOwner(core).presents(), 0u);
  CHECK_EQ(booted.game().presentation.fence(), 0u);

  fields.presentFrame();
  CHECK_EQ(spyro::fieldOwner(core).fields(), 2u);
  CHECK_EQ(booted.game().presentation.fence(), 1u);
}

} // namespace

int main() {
  try {
    RUN(spyro1_interpolates);
    RUN(the_boot_picture_is_native_width_until_the_main_loop);
    RUN(a_cut_is_a_new_game_state_or_level);
    RUN(the_draws_field_waits_are_fields_and_the_frame_is_presented_once);
    return psx::test::summary();
  } catch (const std::exception &error) {
    lucent::error("test_spyro1_record", "FAIL: {}", error.what());
    return 1;
  }
}
