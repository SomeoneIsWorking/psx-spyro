#pragma once

#include "game_runtime.h"

#include <cstdio>

class Core;

namespace spyro1::teleport {

// `gates`, `gate-teleport <portal> <node>`, `dragons`, `dragon-teleport <n>`, `mobys <class>` and
// `moby-teleport <class> <n>`: list a level's portals, crystal dragons or mobys of one class and
// place Spyro on one, for the guest's own touch or range check to act on.
bool command(Core &core, const psx::game::TitleCommandLine &input, std::FILE *reply);

} // namespace spyro1::teleport
