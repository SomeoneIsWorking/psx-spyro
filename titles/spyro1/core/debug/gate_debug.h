#pragma once

#include "game_runtime.h"

#include <cstdint>
#include <cstdio>

class Core;

namespace spyro1::gate_debug {

struct Position {
  int32_t x = 0;
  int32_t y = 0;
  int32_t z = 0;
};

struct GateInfo {
  bool valid = false;
  uint32_t portalAddress = 0;
  int32_t pathMoby = -1;
  int32_t levelId = -1;
  Position firstPoint;
  uint32_t nodeCount = 0;
  Position nodes[16]{};
};

bool inspectGate(Core &core, uint32_t index, GateInfo &out);
bool teleportToGate(Core &core, uint32_t index, uint32_t node);
// `gates` and `gate-teleport <portal> <node>`; replies go to `reply`.
bool command(Core &core, const psx::game::TitleCommandLine &input, std::FILE *reply);

} // namespace spyro1::gate_debug
