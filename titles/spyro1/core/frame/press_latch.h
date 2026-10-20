#pragma once

namespace spyro1 {

// Holds one per-field Start/Cross edge until a consumer that can act on it takes it.
class PressLatch {
public:
  void observe(bool pressedThisField) {
    held_ = held_ || pressedThisField;
  }

  // True exactly once for each held press.
  bool take() {
    const bool wasHeld = held_;
    held_ = false;
    return wasHeld;
  }

  bool held() const {
    return held_;
  }

  void clear() {
    held_ = false;
  }

private:
  bool held_ = false;
};

} // namespace spyro1
