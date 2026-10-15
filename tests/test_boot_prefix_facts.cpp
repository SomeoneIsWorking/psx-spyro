// Each title's measured boot-prefix facts, and the stock library leaves its runtime binds.
//
// The shared driver is tested over synthetic facts in `test_boot_prefix_frame_driver.cpp`. What
// that cannot see is a wrong guest address in a title's own facts, which shows up only in a run
// with a disc attached: a driver that entered the wrong boot prefix, or a leaf bound at the wrong
// address.
//
// The literal addresses below are deliberately a SECOND copy of what
// `titles/spyro2/core/boot_facts.h` and `titles/spyro3/core/boot_facts.h` declare, each quoted from
// that title's executable: a test that read them back from the facts would pass for any value. The
// runtimes are asked for their driver, so a title whose runtime composed the other title's facts
// fails here too.

#include "booted_title.h"
#include "core/boot_prefix_frame_driver.h"
#include "platform_hle.h"
#include "stock_vsync.h"
#include "testutil.h"

#include <lucent/log.h>

#include <cstdint>
#include <exception>
#include <string_view>

namespace {

struct ExpectedTitle {
  const char *name;
  std::uint32_t bootPrefix;
  std::uint32_t frameUpdate;
  std::uint32_t frameDraw;
  std::uint32_t vsync;
  // The guest vblank word retail VSync(-1) returns (libetc's callback increments it); never the
  // HBlank root counter 0x1F801110, which wraps and stalls the title's frame limiter (issue 0160).
  std::uint32_t vsyncQueryCounter;
  // libcd CdInit: retail runs it so libcd registers its interrupt handler (issue 0159).
  std::uint32_t cdInit;
  // libcd CdControlB, bound to the synchronous owner when the title's body would wait out libcd's
  // deadline (issue 0161); zero when the title does not bind it.
  std::uint32_t boundCdControlB;
  // The step bound the title declares. Spyro 2 keeps the shared default; Spyro 3's boot spends 669
  // steps once its CD reads take the drive's time, so it states its own measured bound.
  std::uint64_t bootStepLimit;
};

constexpr ExpectedTitle kSpyro2{"Spyro 2",
                                0x80011E9Cu,
                                0x8001B140u,
                                0x800156FCu,
                                0x80058EDCu,
                                0x80066618u,
                                0x800582B8u,
                                0x80058994u,
                                spyro::kDefaultBootStepLimit};
constexpr ExpectedTitle kSpyro3{"Spyro 3",
                                0x8002AB38u,
                                0x80055400u,
                                0x8001E638u,
                                0x8005956Cu,
                                0x8006B480u,
                                0x8005DB1Cu,
                                0u,
                                1024u};

void checkDriverFacts(spyro::test::BootedTitle &booted, const ExpectedTitle &expected) {
  const auto *driver =
      dynamic_cast<const spyro::BootPrefixFrameDriver *>(booted.game().frameDriver.get());
  CHECK(driver != nullptr);
  if (driver == nullptr) {
    return;
  }
  const spyro::BootPrefixFacts &facts = driver->facts();
  CHECK(std::string_view(facts.titleName) == expected.name);
  CHECK(std::string_view(facts.field.titleName) == expected.name);
  CHECK_EQ(facts.bootPrefix, expected.bootPrefix);
  CHECK_EQ(facts.frameUpdate, expected.frameUpdate);
  CHECK_EQ(facts.frameDraw, expected.frameDraw);
  // Neither title overrides the field bound. Spyro 3 states its own step bound beside its
  // measurement, and that bound must still be tighter than "never", or the stall detector it feeds
  // would be disabled.
  CHECK_EQ(facts.bootStepFieldLimit, spyro::kDefaultBootStepFieldLimit);
  CHECK_EQ(facts.bootStepLimit, expected.bootStepLimit);
  CHECK(facts.bootStepLimit <= 4096u);
  CHECK(!driver->bootComplete());
  CHECK(!driver->bootStalled());
}

// The leaves registerOverrides bound: the VSync this title's boot waits on, CdControlB where the
// title binds it, and CdInit left to the guest.
void checkBindings(spyro::test::BootedTitle &booted, const ExpectedTitle &expected) {
  const PlatformHle &table = booted.game().platform_hle;
  CHECK(table.vsync() != nullptr);
  if (table.vsync() == nullptr) {
    return;
  }
  CHECK_EQ(table.vsync()->entry(), expected.vsync);
  CHECK_EQ(table.vsync()->queryCounter(), expected.vsyncQueryCounter);
  CHECK(table.lookup(expected.cdInit) == nullptr);
  if (expected.boundCdControlB != 0u) {
    CHECK(table.lookup(expected.boundCdControlB) != nullptr);
  }
}

// Spyro 2 reports its parity pool from the two guest words holding its live bounds; Spyro 3 has
// none.
void test_only_spyro2_reports_a_packet_pool() {
  spyro::test::BootedTitle spyro2(spyro::Title::Spyro2);
  Core &core = spyro2.core();
  core.mem_w32(0x80069998u, 0x80080000u);
  core.mem_w32(0x8006B2A4u, 0x80090000u);
  const auto pool = spyro2.runtime().packetPool(core);
  CHECK(pool.has_value());
  if (pool.has_value()) {
    CHECK_EQ(pool->count, 1u);
    CHECK(pool->contains(0x80080000u));
    CHECK(!pool->contains(0x80090000u));
  }
  spyro::test::BootedTitle spyro3(spyro::Title::Spyro3);
  CHECK(!spyro3.runtime().packetPool(spyro3.core()).has_value());
}

void test_spyro2_facts_and_bindings_match_its_executable() {
  spyro::test::BootedTitle booted(spyro::Title::Spyro2);
  checkDriverFacts(booted, kSpyro2);
  checkBindings(booted, kSpyro2);
}

void test_spyro3_facts_and_bindings_match_its_executable() {
  spyro::test::BootedTitle booted(spyro::Title::Spyro3);
  checkDriverFacts(booted, kSpyro3);
  checkBindings(booted, kSpyro3);
}

} // namespace

int main() {
  try {
    RUN(spyro2_facts_and_bindings_match_its_executable);
    RUN(spyro3_facts_and_bindings_match_its_executable);
    RUN(only_spyro2_reports_a_packet_pool);
    return psx::test::summary();
  } catch (const std::exception &error) {
    lucent::error("test_boot_prefix_facts", "FAIL: {}", error.what());
    return 1;
  }
}
