// A picker panel's logo is read from the GPU device's VRAM, where the guest's uploads land.
#include "core.h"
#include "core/title_logo.h"
#include "gpu_ports.h"
#include "test_game.h"
#include "testutil.h"

#include <array>
#include <cstdint>
#include <memory>

namespace {

constexpr std::uint32_t kPointerGlobal = 0x80010000u;
constexpr std::uint32_t kBlock = 0x80010100u;
constexpr std::uint32_t kRecord = 0x80010200u;
// A 15-bit direct page at the VRAM origin.
constexpr std::uint16_t kDirectPage = 2u << 7;
constexpr std::uint16_t kWhite = 0x7FFFu;

constexpr spyro::TitleLogoPart kParts[] = {{.record = kRecord}};
constexpr spyro::TitleLogoFacts kFacts{
    .valid = true,
    .gate = {.pointerGlobal = kPointerGlobal, .tickOffset = 0, .tickValue = 1},
    .parts = kParts,
};

void test_the_logo_is_read_from_the_device_vram() {
  auto game = std::make_unique<spyro::test::SessionGame>();
  Core &core = game->core;
  core.mem_w32(kPointerGlobal, kBlock);
  core.mem_w32(kBlock, 1);
  core.mem_w16(kRecord, kDirectPage);
  core.mem_w16(kRecord + 2, 0);
  core.mem_w8(kRecord + 4, 4);
  core.mem_w8(kRecord + 5, 2);
  std::array<std::uint16_t, 8> texels{};
  texels.fill(kWhite);
  psx::gpu::loadImage(core, {0, 0, 4, 2}, texels);

  const auto logo = spyro::extractPanelLogo(core, kFacts);
  CHECK(logo.has_value());
  if (!logo.has_value()) {
    return;
  }
  CHECK_EQ(logo->width, 4);
  CHECK_EQ(logo->height, 2);
  CHECK_EQ(static_cast<int>(logo->rgba[0]), 255);
  CHECK_EQ(static_cast<int>(logo->rgba[3]), 255);
}

void test_a_closed_gate_reads_nothing() {
  auto game = std::make_unique<spyro::test::SessionGame>();
  CHECK(!spyro::extractPanelLogo(game->core, kFacts).has_value());
}

} // namespace

int main() {
  RUN(the_logo_is_read_from_the_device_vram);
  RUN(a_closed_gate_reads_nothing);
  return psx::test::summary();
}
