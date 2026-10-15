#include "core.h"
#include "core/gate_debug.h"
#include "core/guest_globals.h"
#include "test_game.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace {

void require(bool condition, const char *what) {
  if (!condition) {
    std::cerr << "gate_debug: " << what << '\n';
    std::exit(1);
  }
}

void seedGate(Core &core) {
  core.mem_w32(spyro::guest::kGamestate, 0u);
  core.mem_w32(spyro::guest::kLoadStage, 0xffffffffu);
  core.mem_w32(0x8007596cu, 10u);
  core.mem_w32(0x800758b4u, 14u);
  core.mem_w32(spyro::guest::kPortalCount, 1u);
  core.mem_w32(spyro::guest::kPortals, 0x80010000u);
  core.mem_w32(0x80010018u, 40u);
  core.mem_w32(0x8001001cu, 14u);
  core.mem_w32(0x80010020u, 100u);
  core.mem_w32(0x80010024u, 200u);
  core.mem_w32(0x80010028u, 300u);
  core.mem_w32(spyro::guest::kLevelMobys, 0x80020000u);
  core.mem_w32(0x80020000u + 40u * 0x58u, 0x80030000u);
  core.mem_w32(0x80030000u, 0x80031000u);
  core.mem_w8(0x80031000u, 2u);
  core.mem_w32(0x80031008u, 1000u);
  core.mem_w32(0x8003100cu, 1100u);
  core.mem_w32(0x80031010u, 1200u);
  core.mem_w32(0x80031018u, static_cast<uint32_t>(-1000));
  core.mem_w32(0x8003101cu, static_cast<uint32_t>(-1100));
  core.mem_w32(0x80031020u, static_cast<uint32_t>(-1200));
}

void testInspectionAndTeleport() {
  auto core = std::make_unique<spyro::test::StubCore>();
  seedGate(*core);
  core->mem_w32(spyro::guest::kSpyro, 1u);
  core->mem_w32(0x80078a5cu, 2u);
  core->mem_w32(0x80078a60u, 3u);
  spyro::gate_debug::GateInfo gate;
  require(spyro::gate_debug::inspectGate(*core, 0u, gate), "valid gate inspection");
  require(gate.levelId == 14 && gate.pathMoby == 40 && gate.nodeCount == 2u, "gate metadata");
  require(gate.firstPoint.x == 100 && gate.firstPoint.y == 200 && gate.firstPoint.z == 300,
          "portal first-point decode");
  require(gate.nodes[1].x == -1000 && gate.nodes[1].z == -1200, "path node decode");
  require(spyro::gate_debug::teleportToGate(*core, 0u, 1u), "teleport accepted in field");
  require(static_cast<int32_t>(core->mem_r32(spyro::guest::kSpyro)) == -1000, "position x");
  require(static_cast<int32_t>(core->mem_r32(0x80078a5cu)) == -1100, "position y");
  require(static_cast<int32_t>(core->mem_r32(0x80078a60u)) == -1200, "position z");
  require(static_cast<int32_t>(core->mem_r32(0x80078ae4u)) == -1000, "previous position x");
}

void testTeleportRefusesOutsideGameplay() {
  auto core = std::make_unique<spyro::test::StubCore>();
  seedGate(*core);
  core->mem_w32(spyro::guest::kGamestate, 1u);
  require(!spyro::gate_debug::teleportToGate(*core, 0u, 0u), "non-gameplay refusal");
}

// The reply stream's whole text after `command` ran with `line`.
std::string runCommand(Core &core, const char *name, const char *line, bool &handled) {
  std::FILE *reply = std::tmpfile();
  require(reply != nullptr, "reply stream");
  handled = spyro::gate_debug::command(core, {.command = name, .line = line}, reply);
  require(std::fseek(reply, 0, SEEK_SET) == 0, "reply stream rewinds");
  std::string text;
  for (int c = std::fgetc(reply); c != EOF; c = std::fgetc(reply)) {
    text.push_back(static_cast<char>(c));
  }
  std::fclose(reply);
  return text;
}

void testCommandsReplyOnTheStream() {
  auto core = std::make_unique<spyro::test::StubCore>();
  seedGate(*core);
  bool handled = false;
  const std::string gates = runCommand(*core, "gates", "gates", handled);
  require(handled, "gates is the title's command");
  require(gates.find("portals=1") != std::string::npos, "gates reports the portal count");
  require(gates.find("gate=0 targetLevel=14 pathMoby=40") != std::string::npos,
          "gates lists the portal");
  const std::string usage = runCommand(*core, "gate-teleport", "gate-teleport", handled);
  require(handled && usage.starts_with("usage: gate-teleport"),
          "a teleport without arguments replies its usage");
  const std::string other = runCommand(*core, "warp", "warp 3", handled);
  require(!handled && other.empty(), "another command is not the title's and writes nothing");
}

} // namespace

int main() {
  testInspectionAndTeleport();
  testTeleportRefusesOutsideGameplay();
  testCommandsReplyOnTheStream();
  std::cout << "gate_debug: PASS (portal/path decode, guarded teleport, command replies)\n";
  return 0;
}
