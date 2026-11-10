// native_state.h — the per-Core state Spyro 1's natives own (spyro::Session::titleState).
#pragma once

#include "core/session.h"
#include "spyro1/render/replay_angles.h"
#include "spyro1/render/shaded_line.h"

class Core;

namespace spyro1 {

class NativeState final : public spyro::TitleState {
public:
  shaded_line::CallSites callSites;
  actors::ReplayAngles replayAngles;
};

[[nodiscard]] inline NativeState &nativeState(Core &core) {
  return static_cast<NativeState &>(*spyro::session(core).titleState);
}

} // namespace spyro1
