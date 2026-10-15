// startProgram replays each title's crt0 from its own measured boot group.
//
// The group below is a second copy of what each runtime declares, quoted from that title's
// executable; the crt0 is assembled from it, so Crt0Boot's audit refuses a runtime whose group
// disagrees, and the applied registers and heap globals are checked against it.
#include "booted_title.h"
#include "crt0_fixture.h"
#include "testutil.h"

#include <lucent/log.h>

#include <concepts>
#include <cstdint>
#include <exception>

namespace {

static_assert(std::derived_from<spyro3::Spyro3Runtime, spyro::SpyroRuntime>);

constexpr std::uint32_t kStackTopWord = 0x00200000u;
constexpr std::uint32_t kReserveWord = 0x00008000u;

constexpr Crt0Template kSpyro2{.entry = 0x8005478Cu,
                               .bssLo = 0x80066ED8u,
                               .bssHi = 0x8006D264u,
                               .stackTopBase = 0x80066D3Cu,
                               .stackTopBase2 = 0x80066D38u,
                               .heapBase = 0x8006D264u,
                               .gp = 0x80066D38u,
                               .libcInit = 0x8005ABD8u,
                               .bias = -8,
                               .heapSizePtr = 0x8006509Cu,
                               .heapBasePtr = 0x80065098u};

constexpr Crt0Template kSpyro3{.entry = 0x80059444u,
                               .bssLo = 0x8006C4F4u,
                               .bssHi = 0x800742D0u,
                               .stackTopBase = 0x8006C3E4u,
                               .stackTopBase2 = 0x8006C3E0u,
                               .heapBase = 0x800742D0u,
                               .gp = 0x8006C3B0u,
                               .libcInit = 0x8005F63Cu,
                               .bias = -8,
                               .heapSizePtr = 0x80069F04u,
                               .heapBasePtr = 0x80069F00u};

void test_the_fixture_encoder_matches_measured_bytes() {
  for (const Crt0PinRow &pin : crt0_fixture_pins()) {
    CHECK_EQ(pin.got, pin.want);
  }
}

void checkStartProgram(spyro::SpyroTitle title, const Crt0Template &crt0) {
  spyro::test::BootedTitle booted(title);
  Core &core = booted.core();
  crt0_fixture_emit(crt0, [&core](std::uint32_t address, std::uint32_t word) {
    core.mem_w32(address, word);
  });
  core.mem_w32(crt0.stackTopBase, kStackTopWord);
  core.mem_w32(crt0.stackTopBase2, kReserveWord);
  core.mem_w32(crt0.bssLo, 0xDEADBEEFu);

  booted.runtime().startProgram(core);

  const std::uint32_t stackTop = kStackTopWord - 8u;
  const std::uint32_t heapSize = (stackTop - kReserveWord) - (crt0.heapBase & 0x1FFFFFFFu);
  CHECK_EQ(core.r[28], crt0.gp);
  CHECK_EQ(core.r[29], stackTop | 0x80000000u);
  CHECK_EQ(core.mem_r32(crt0.bssLo), 0u);
  CHECK_EQ(core.mem_r32(crt0.heapSizePtr), heapSize);
  CHECK_EQ(core.mem_r32(crt0.heapBasePtr), crt0.heapBase | 0x80000000u);
}

void test_spyro2_start_program_applies_its_crt0() {
  checkStartProgram(spyro::SpyroTitle::Spyro2, kSpyro2);
}

void test_spyro3_start_program_applies_its_crt0() {
  checkStartProgram(spyro::SpyroTitle::Spyro3, kSpyro3);
}

} // namespace

int main() {
  try {
    RUN(the_fixture_encoder_matches_measured_bytes);
    RUN(spyro2_start_program_applies_its_crt0);
    RUN(spyro3_start_program_applies_its_crt0);
    return pt_summary();
  } catch (const std::exception &error) {
    lucent::error("test_start_program", "FAIL: {}", error.what());
    return 1;
  }
}
