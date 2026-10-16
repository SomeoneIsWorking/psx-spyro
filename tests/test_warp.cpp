#include "booted_title.h"
#include "core.h"
#include "core/session.h"
#include "testutil.h"

#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <stdexcept>
#include <string>

namespace {

// A broken fixture, not a failed check: runCase reports it and fails the case.
void require(bool condition, const char *what) {
  if (!condition) {
    throw std::runtime_error(what);
  }
}

constexpr std::uint32_t kJrRa = 0x03E00008u;
constexpr std::uint32_t kNop = 0u;
// lui $at, 0x8007
constexpr std::uint32_t kLuiAt8007 = 0x3C018007u;
// A word written through `sw $rt, offset($at)` with $at = 0x80070000.
constexpr std::uint32_t storeAt(std::uint32_t rt, std::uint32_t address) {
  return 0xAC200000u | (rt << 16) | ((address - 0x80070000u) & 0xFFFFu);
}
constexpr std::uint32_t kV0 = 2u;
constexpr std::uint32_t kA0 = 4u;
constexpr std::uint32_t kA1 = 5u;
// addiu $v0, $zero, value
constexpr std::uint32_t loadV0(std::uint32_t value) {
  return 0x24020000u | value;
}

void plant(Core &core, std::uint32_t entry, std::initializer_list<std::uint32_t> body) {
  std::uint32_t address = entry;
  for (const std::uint32_t word : body) {
    core.mem_w32(address, word);
    address += 4u;
  }
  core.mem_w32(address, kJrRa);
  core.mem_w32(address + 4u, kNop);
}

std::string command(spyro::test::BootedTitle &title, const char *line) {
  std::FILE *reply = std::tmpfile();
  require(reply != nullptr, "reply stream");
  const bool handled =
      title.runtime().titleCommand(title.core(), {.command = "warp", .line = line}, reply);
  require(handled, "warp is the title's command");
  require(std::fseek(reply, 0, SEEK_SET) == 0, "reply stream rewinds");
  std::string text;
  for (int c = std::fgetc(reply); c != EOF; c = std::fgetc(reply)) {
    text.push_back(static_cast<char>(c));
  }
  std::fclose(reply);
  return text;
}

bool startsWith(const std::string &text, const char *prefix) {
  return text.rfind(prefix, 0) == 0;
}

void apply(spyro::test::BootedTitle &title) {
  spyro::session(title.core()).warp->applyArmed(title.core(), 1u);
}

void test_parsing() {
  spyro::test::BootedTitle title(spyro::Title::Spyro2, false);
  CHECK_MSG(startsWith(command(title, "warp"), "usage:"), "no level is a usage reply");
  CHECK_MSG(startsWith(command(title, "warp 1x"), "usage:"), "a malformed level is a usage reply");
  CHECK_MSG(startsWith(command(title, "warp 14"), "refused:"), "Spyro 2 has no level 14");
  CHECK_MSG(!spyro::session(title.core()).warp->armedLevel(), "a refusal arms nothing");
  CHECK_MSG(startsWith(command(title, "warp  21 "), "ok:"), "Hurricos arms");
  CHECK_MSG(spyro::session(title.core()).warp->armedLevel() == 21u, "the armed level");
}

// Spyro 1: the reset runs, then the level and the TSD_Level handoff, only on the idle title.
void test_spyro1() {
  constexpr std::uint32_t kGamestate = 0x800757D8u;
  constexpr std::uint32_t kLevelId = 0x8007596Cu;
  constexpr std::uint32_t kTitle = 0x80078D78u;
  spyro::test::BootedTitle title(spyro::Title::Spyro1, false);
  Core &core = title.core();
  plant(core, 0x8001277Cu, {kLuiAt8007, loadV0(10u), storeAt(kV0, kLevelId)});
  CHECK_MSG(startsWith(command(title, "warp 65"), "refused:"), "Spyro 1 has no level 65");
  CHECK_MSG(startsWith(command(title, "warp 16"), "refused:"), "Spyro 1 worlds have six levels");
  CHECK_MSG(startsWith(command(title, "warp 33"), "ok:"), "Wizard Peak arms");

  core.mem_w32(kGamestate, 13u);
  core.mem_w32(kTitle + 0x10u, 2u);
  apply(title);
  CHECK_MSG(core.mem_r32(kTitle) == 0u && core.mem_r32(kLevelId) == 0u,
            "nothing starts before the title waits for Start");

  core.mem_w32(kTitle + 0x10u, 3u);
  apply(title);
  CHECK_MSG(core.mem_r32(kLevelId) == 33u, "the level follows the reset's level 10");
  CHECK_MSG(core.mem_r32(kTitle) == 3u && core.mem_r32(kTitle + 0x04u) == 0u &&
                core.mem_r32(kTitle + 0x1Cu) == 1u,
            "TSM_Demo, TSS_Setup, TSD_Level");
  CHECK_MSG(!spyro::session(core).warp->armedLevel(), "an applied warp is spent");
}

// Spyro 2: title state 5 from its first tick, the start index keeping the level id.
void test_spyro2() {
  constexpr std::uint32_t kGameState = 0x800681C8u;
  constexpr std::uint32_t kTitleState = 0x80067E0Cu;
  constexpr std::uint32_t kTitleTick = 0x80067E00u;
  constexpr std::uint32_t kStartIndex = 0x80067E14u;
  constexpr std::uint32_t kLevelId = 0x80066F90u;
  spyro::test::BootedTitle title(spyro::Title::Spyro2, false);
  Core &core = title.core();
  CHECK_MSG(startsWith(command(title, "warp 61"), "ok:"), "Robotica Farms arms");

  core.mem_w32(kGameState, 11u);
  core.mem_w32(kTitleState, 6u);
  core.mem_w32(kTitleTick, 9u);
  core.mem_w32(kStartIndex, 3u);
  apply(title);
  CHECK_MSG(core.mem_r32(kTitleState) == 6u && core.mem_r32(kLevelId) == 0u,
            "the demo's title state starts nothing");

  core.mem_w32(kTitleState, 2u);
  apply(title);
  CHECK_MSG(core.mem_r32(kTitleState) == 5u && core.mem_r32(kTitleTick) == 0u,
            "title state 5 from its first tick");
  CHECK_MSG(core.mem_r32(kStartIndex) == 0u && core.mem_r32(kLevelId) == 61u,
            "start index 0 keeps the level id");
}

// Spyro 3: title state 19 from its first step, with the level id state 20 passes on.
void test_spyro3() {
  constexpr std::uint32_t kGameState = 0x8006E344u;
  constexpr std::uint32_t kTitleState = 0x8007F12Cu;
  constexpr std::uint32_t kTitleStep = 0x8007F12Eu;
  constexpr std::uint32_t kLevelId = 0x8006C5BCu;
  spyro::test::BootedTitle title(spyro::Title::Spyro3, false);
  Core &core = title.core();
  CHECK_MSG(startsWith(command(title, "warp 19"), "refused:"), "Spyro 3's first world has nine");
  CHECK_MSG(startsWith(command(title, "warp 49"), "ok:"), "Super Bonus Round arms");

  core.mem_w32(kGameState, 0u);
  core.mem_w32(kTitleState, 0x01010101u);
  apply(title);
  CHECK_MSG(core.mem_r32(kLevelId) == 0u, "gameplay starts nothing");
  CHECK_MSG(spyro::session(core).warp->armedLevel() == 49u, "the request waits for the title");

  core.mem_w32(kGameState, 11u);
  apply(title);
  CHECK_MSG(core.mem_r8(kTitleState) == 19u && core.mem_r8(kTitleStep) == 0u,
            "title state 19 from its first step");
  CHECK_MSG(core.mem_r32(kLevelId) == 49u, "the level id state 20 passes on");
}

} // namespace

int main() {
  RUN(parsing);
  RUN(spyro1);
  RUN(spyro2);
  RUN(spyro3);
  return psx::test::summary();
}
