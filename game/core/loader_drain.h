#pragma once

#include <cstdint>

namespace spyro {

// A stage loader returns after the read each stage issues and polls it on its next call, so a
// loading screen that calls it once per update shows its picture one frame per stage. Reads are
// due at once, so the screen's own call is followed in the same update by more calls while the
// last one moved `progress` forward and `more` holds; a call that moves nothing is the guest's own
// wait (a transfer, a presentation gate) and ends the update as it would have.
template <typename Call, typename Progress, typename More>
void drainLoader(Call call, Progress progress, More more) {
  std::int64_t before = progress();
  call();
  for (std::int64_t after = progress(); after > before && more(); after = progress()) {
    before = after;
    call();
  }
}

} // namespace spyro
