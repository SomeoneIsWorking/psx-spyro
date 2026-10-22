// actor_list.h — SCUS_942.28's actor list builders: BuildActorDrawList 0x8001F158 and
// BuildSecondaryActorDrawList 0x800208FC. Each walks a zero-ended list of moby records, culls each
// against the camera's distance box and view frustum, stages drop shadows, and writes a 0x38-byte
// draw entry (edge class, frames, view-space centre, composed rotation) for the emitter after it.
#pragma once

#include "render/field/guest_working_memory.h"
#include "spyro1/render/actor_facts.h"

#include <cstdint>

class Core;

namespace spyro1::actors {

// The two builders differ only in their input list, where the shadow list starts, whether the
// draw list is bounded and whether the entry carries the record's +0x4C word.
struct ListPass {
  std::uint32_t entry = 0;
  const char *overrideName = nullptr;
  std::uint32_t input = 0;
  bool continuesShadowList = false;
  bool bounded = false;
  bool copiesSecondaryWord = false;
};

inline constexpr ListPass kPrimaryListPass{.entry = 0x8001F158u,
                                           .overrideName = "spyro1-actor-list",
                                           .input = kActorList,
                                           .continuesShadowList = false,
                                           .bounded = true,
                                           .copiesSecondaryWord = false};

inline constexpr ListPass kSecondaryListPass{.entry = 0x800208FCu,
                                             .overrideName = "spyro1-secondary-actor-list",
                                             .input = kSecondaryList,
                                             .continuesShadowList = true,
                                             .bounded = false,
                                             .copiesSecondaryWord = true};

// Runs one builder over the guest's own memory.
void buildList(Core &core, const ListPass &pass);

// One record through a builder's culls over `memory`, its entry written at kDrawList; true when it
// was given one. An in-between's list, its angles refined by `angleFractions`
// (spyro::moby_record::Between).
bool listRecord(Core &core,
                const psx::present::EmitMemory &memory,
                const ListPass &pass,
                std::uint32_t record,
                std::uint32_t angleFractions);

void registerListOverrides(Core &core);

} // namespace spyro1::actors
