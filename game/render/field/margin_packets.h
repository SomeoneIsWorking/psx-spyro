// margin_packets.h — where a draw's margin moby packets end in the idle draw buffer's pool, so a
// later pass of the same draw (Spyro 3's chained Sparx pass, SCUS_944.67 8001E638) places its own
// after them.
#pragma once

#include <cstdint>

namespace spyro {

class MarginPackets {
public:
  // The margin packets of the pool starting at `pool` end at `end`.
  void drew(std::uint32_t pool, std::uint32_t end) {
    pool_ = pool;
    end_ = end;
  }
  // Where the next margin packets in the pool starting at `pool` begin.
  [[nodiscard]] std::uint32_t next(std::uint32_t pool) const {
    return pool == pool_ ? end_ : pool;
  }
  // A restored machine has drawn nothing.
  void clear() {
    pool_ = 0;
    end_ = 0;
  }

private:
  std::uint32_t pool_ = 0;
  std::uint32_t end_ = 0;
};

} // namespace spyro
