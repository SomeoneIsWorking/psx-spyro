#pragma once

#include "game_runtime.h"

#include <cstdio>

class Core;

namespace spyro1::teleport {

// `gates`, `gate-teleport <portal> <node>`, `dragons` and `dragon-teleport <n>`: list a level's
// portals or crystal dragons and place Spyro on one, for the guest's own touch to act on.
bool command(Core &core, const psx::game::TitleCommandLine &input, std::FILE *reply);

} // namespace spyro1::teleport
