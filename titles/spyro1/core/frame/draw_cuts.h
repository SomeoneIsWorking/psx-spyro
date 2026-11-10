// draw_cuts.h — the guest calls that re-place the camera or Spyro during GS_Playing, each a cut.
#pragma once

class Core;

namespace spyro1 {

// CameraForceToDestination 0x80034C84 and the Spyro reset 0x8004AC24, each declaring a cut and
// running as the guest's.
void registerDrawCutOverrides(Core &core);

// A cut from inside a native body that re-places the camera.
void markDrawCut(Core &core);

} // namespace spyro1
