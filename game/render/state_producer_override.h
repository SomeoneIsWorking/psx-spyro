// state_producer_override.h — a native draw override and the state producer that renders its saved
// objects again for in-betweens, installed together.
#pragma once

#include "core.h"
#include "core/session.h"
#include "emission_scope.h"
#include "native_dispatch.h"
#include "state_producer.h"

#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

namespace spyro {

// `draw` at `entry` saves one state per object of the producer `arg` names; `producer` renders them.
inline void installProducerOverride(Core &core,
                                    std::uint32_t entry,
                                    std::string_view name,
                                    psx::cpu::NativeFunction draw,
                                    std::unique_ptr<psx::present::StateProducer> producer,
                                    psx::present::Arg arg = psx::present::Arg::A0) {
  session(core).stateProducers.install(entry, std::move(producer));
  psx::cpu::installNativeOverride(core, entry, name, draw, psx::present::Producer{arg});
}

} // namespace spyro
