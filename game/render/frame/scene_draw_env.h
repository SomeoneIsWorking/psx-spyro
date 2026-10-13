// The DRAWENV state a stage's guest render arm writes before its display tail programs the env.
#pragma once
#include <cstdint>
class Core;

namespace spyro::render {

// Every arm of 0x8001ED5C writes its background colour before the tail's PutDrawEnv clears with it,
// so this runs before frameBegin programs the env.
void prepareSceneDrawEnv(Core *core, std::uint32_t stage);

} // namespace spyro::render
