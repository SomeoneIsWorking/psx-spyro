#pragma once

#include "core/press_latch.h"

#include <cstdint>

class Core;

namespace spyro {

// The loading picture of the level load (game state 5 in both images): sub-state 0 counts its
// frames, draws the picture at count * 16 brightness until count 8 and full after it, and leaves
// at its own count; the next sub-state shows that last drawn frame through the load.
struct LoadingPictureFacts {
  std::uint32_t gameState = 0; // 0: no loading picture
  std::uint32_t loadingState = 0;
  std::uint32_t subState = 0;
  std::uint32_t counter = 0;
  // The latest count a press writes: the update still draws two full-brightness frames, one into
  // each display buffer, before it leaves. A press writes it or one below, keeping the parity.
  std::uint32_t landingCount = 0;
  // Spyro 2 finishes a pending autosave when its update reaches this count, if the kind's flag
  // byte is set; a press lands one before it instead. 0: none.
  std::uint32_t autosaveCount = 0;
  std::uint32_t autosaveFlags = 0;
  std::uint32_t kind = 0;
};

// A Start/Cross press on the loading picture brings its count to the end, so the guest leaves
// through its own exit at normal speed.
class LoadingPictureSkip {
public:
  explicit LoadingPictureSkip(const LoadingPictureFacts &facts) : facts_(facts) {}

  // Before each frame update.
  void observe(Core &core, bool pressed);

private:
  [[nodiscard]] std::uint32_t landing(Core &core, std::uint32_t count) const;

  LoadingPictureFacts facts_;
  PressLatch press_;
};

} // namespace spyro
