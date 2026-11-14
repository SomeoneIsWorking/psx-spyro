// draw_cuts.h — the guest calls that re-place the camera or Spyro, each a cut. The camera's is
// ResetCameraStateToTarget 0x80034358 (native_camera.cpp), which sets the springs to their target;
// SnapCameraToTarget 0x80034C84 only seats the eye on a spring-stepped target each tick.
#pragma once

class Core;

namespace spyro1 {

// ResetSpyroEntity 0x8004AC24, declaring a cut and running as the guest's.
void registerDrawCutOverrides(Core &core);

// A cut from inside a native body that re-places the camera.
void markDrawCut(Core &core);

} // namespace spyro1
