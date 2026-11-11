// native_state.h — the per-Core state Spyro 1's natives own (spyro::Session::titleState).
#pragma once

#include "core/session.h"
#include "render/terrain/guest_camera_builder.h"
#include "spyro1/core/effect/particle_lives.h"
#include "spyro1/render/drawn_records.h"
#include "spyro1/render/replay_angles.h"

#include <optional>

class Core;

namespace spyro1 {

class NativeState final : public spyro::TitleState {
public:
  actors::ReplayAngles replayAngles;
  actors::DrawnRecords drawnRecords;
  ParticleLives particleLives;
  // m_Rotation's blended angles, with their fractions, while a replay draws.
  std::optional<spyro::guest_camera::Angles> replayCamera;
};

[[nodiscard]] inline NativeState &nativeState(Core &core) {
  return static_cast<NativeState &>(*spyro::session(core).titleState);
}

} // namespace spyro1
