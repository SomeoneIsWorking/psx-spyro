// spyro1_actor_emitter_producer.h — the moby emitters 0x8001F798 and 0x80020F34 as state
// producers: the override draws every draw-list entry under its record's emission scope and saves
// the record and camera it was drawn from; the render runs the record through its list builder and
// emitter again over host memory at t between two saves.
#pragma once

#include "spyro1_actor_between.h"
#include "spyro1_actor_list.h"
#include "state_producer.h"
#include "terrain_rebuild.h"

#include <cstdint>
#include <type_traits>

class Core;

namespace spyro1::actors {

// One saved entry: the camera it was drawn under and its record's bytes. The guest words its draw
// read follow it.
struct EmittedRecordState {
  spyro::guest_terrain::CameraState camera;
  std::uint32_t record = 0;
  MobyBytes bytes{};
};
static_assert(std::is_trivially_copyable_v<EmittedRecordState>);

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

class ActorEmitterProducer final : public psx::present::StateProducer {
public:
  ActorEmitterProducer(Core &core, const EmitterPass &pass) : core_(core), pass_(pass) {}

  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;

private:
  Core &core_;
  const EmitterPass &pass_;
};

// The overrides and their renders.
void registerEmitterOverrides(Core &core);

} // namespace spyro1::actors
