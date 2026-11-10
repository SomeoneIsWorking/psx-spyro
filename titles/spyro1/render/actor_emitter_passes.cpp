#include "spyro1/render/actor_emitter_passes.h"

#include "core.h"
#include "native_dispatch.h"
#include "render/field/guest_render_globals.h"
#include "render/field/guest_view_slope.h"
#include "spyro1/render/actor_emitter.h"
#include "spyro1/render/actor_emitter_layout.h"
#include "spyro1/render/actor_facts.h"
#include "spyro1/render/lit_actor_emitter.h"
#include "spyro1/render/terrain_facts.h"

#include <optional>

namespace spyro1::actors {
namespace {

// The pass's emitter, held in place.
class PassEmitter {
public:
  PassEmitter(const EmitterPass &pass, Core &core, const psx::present::EmitMemory &memory) {
    const auto bounds = spyro::presentedBounds(core, spyro1::terrain::kSharedFacts);
    if (pass.lit) {
      emitter_ = &lit_.emplace(core, memory, bounds);
    } else {
      emitter_ = &plain_.emplace(core, memory, bounds);
    }
  }
  PassEmitter(const PassEmitter &) = delete;
  PassEmitter &operator=(const PassEmitter &) = delete;

  ActorEmitter *operator->() const {
    return emitter_;
  }

private:
  std::optional<ActorEmitter> plain_;
  std::optional<LitActorEmitter> lit_;
  ActorEmitter *emitter_ = nullptr;
};

void emitList(Core &guest, const EmitterPass &pass) {
  spyro::guest_render_globals::spillBorrowedRegisters(guest, terrain::kRenderGlobals);
  const psx::present::EmitMemory memory(guest);
  const PassEmitter emitter(pass, guest, memory);
  emitter->begin(guest.r[3]);
  for (std::uint32_t entry = kDrawList; memory.mem_r32(entry + entry::kFlags) != 0;
       entry += entry::kBytes) {
    (void)emitter->draw(entry);
  }
  emitter->end();
}

template <const EmitterPass &Pass> void emitterOverride(Core *core) {
  emitList(*core, Pass);
}

} // namespace

void registerEmitterOverrides(Core &core) {
  psx::cpu::installNativeOverride(
      core, kRegularEmitter.entry, kRegularEmitter.overrideName, emitterOverride<kRegularEmitter>);
  psx::cpu::installNativeOverride(
      core, kLitEmitter.entry, kLitEmitter.overrideName, emitterOverride<kLitEmitter>);
  core.depthTest.mark(kRegularEmitter.entry, psx::present::DepthOrder::Object);
  core.depthTest.mark(kLitEmitter.entry, psx::present::DepthOrder::Object);
}

} // namespace spyro1::actors
