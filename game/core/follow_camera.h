// follow_camera.h — S2/S3 follow camera with the right stick (plan 0252). Both images poll the L/R
// shoulders in camera modes 0, 1 and 6 (S2 0x8001F020, S3 0x80013918) into the azimuth override at
// camera+0x150, and step the camera on the error between the target row and the current one,
// computed by S2 0x8001DB58 / S3 0x8001241C for the camera's own error step and for a level's. The
// stick's x writes that override as a shoulder does; its y raises the target row's elevation for
// that computation only, so the game's own target is never changed.
#pragma once

#include <cstdint>

class Core;

namespace spyro {

struct FollowCameraFacts {
  std::uint32_t shoulderPoll; // (speed): clears the override, then +speed right / -speed left
  std::uint32_t errorCompute; // row 3 = row 1 - row 2, the error the camera steps on
  std::uint32_t camera;       // g_Camera
  std::uint32_t padHeld;      // the held word the poll reads
  std::uint32_t turnButtons;  // the shoulder bits the poll turns on
  std::uint32_t refusedHeld;  // held bits that refuse the poll; 0 for none
  std::uint32_t spyroBusy;    // g_Spyro + 0x240: nonzero refuses the poll
  std::uint32_t gamestate;    // the gamestate word, or 0 when the poll does not read one
  std::uint32_t refusedGamestate;
};

void registerFollowCamera(Core &core, const FollowCameraFacts &facts);

} // namespace spyro
