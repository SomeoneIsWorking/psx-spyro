// The phase a Spyro 1 pad recording is keyed on: g_Gamestate packed with g_LevelId (docs/issues/0116).
#pragma once

#include "game_runtime.h"
#include "input/input_phase.h"

#include <cstdint>

class Core;

namespace spyro1 {

class InputPhase {
public:
  std::uint64_t of(Core &core) const;

  // 16 bits each keeps the key 32 bits wide, so it never equals psx::input::kUnkeyedPhase.
  static constexpr std::uint64_t pack(std::uint32_t gamestate, std::uint32_t level) {
    return (static_cast<std::uint64_t>(gamestate & 0xFFFFu) << 16) | (level & 0xFFFFu);
  }
};

} // namespace spyro1
