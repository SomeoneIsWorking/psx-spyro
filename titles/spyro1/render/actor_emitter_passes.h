// actor_emitter_passes.h — the moby emitters 0x8001F798 and 0x80020F34 over their builders' lists.
#pragma once

#include "spyro1/render/actor_list.h"

#include <cstdint>

class Core;

namespace spyro1::actors {

// An emitter and the list builder whose entries it draws.
struct EmitterPass {
  std::uint32_t entry = 0;
  const char *overrideName = nullptr;
  const ListPass *list = nullptr;
  bool lit = false; // 0x80020F34 lights each face
};

inline constexpr EmitterPass kRegularEmitter{.entry = 0x8001F798u,
                                             .overrideName = "spyro1-actor-emitter",
                                             .list = &kPrimaryListPass,
                                             .lit = false};
inline constexpr EmitterPass kLitEmitter{.entry = 0x80020F34u,
                                         .overrideName = "spyro1-lit-actor-emitter",
                                         .list = &kSecondaryListPass,
                                         .lit = true};

// The two emitters' overrides.
void registerEmitterOverrides(Core &core);

} // namespace spyro1::actors
