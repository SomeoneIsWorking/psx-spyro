// draw_cuts.h — the guest's own re-placements during GS_Playing: a call that puts the camera or
// Spyro somewhere new starts the replay's samples over, so the interval it lands in is shown as
// drawn.
#pragma once

class Core;

namespace spyro1 {

// CameraForceToDestination 0x80034C84 (the camera's position set to its destination when the
// spherical deltas jump, 0x80035FB4, or on a level transition, 0x80037A20) and the Spyro reset
// 0x8004AC24 (level start, respawn, fairy and balloonist exits), each marking a cut and running as
// the guest's.
void registerDrawCutOverrides(Core &core);

// A cut from inside a native body that re-places the camera.
void markDrawCut(Core &core);

} // namespace spyro1
