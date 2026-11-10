// replay_angles.h — the sixteenths of a step each moby angle byte lies past, by record, while a
// replay draws an in-between: a byte cannot hold half a step, and the list builders and the sprite
// queue turn a record by its bytes refined with these (guest_moby_rotation::MobyAngles::flags).
// Empty outside a replay, so a real draw turns by its bytes alone.
#pragma once

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace spyro1::actors {

class ReplayAngles {
public:
  // `record`'s fractions in MobyAngles::flags form; records are noted in address order.
  void note(std::uint32_t record, std::uint32_t fractions) {
    noted_.emplace_back(record, fractions);
  }
  [[nodiscard]] std::uint32_t at(std::uint32_t record) const {
    const auto found = std::ranges::lower_bound(noted_, record, {}, &Noted::first);
    return found != noted_.end() && found->first == record ? found->second : 0u;
  }
  void clear() {
    noted_.clear();
  }

private:
  using Noted = std::pair<std::uint32_t, std::uint32_t>;
  std::vector<Noted> noted_;
};

} // namespace spyro1::actors
