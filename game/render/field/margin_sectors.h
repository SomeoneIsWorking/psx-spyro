// margin_sectors.h — the terrain sectors a widened view lists that retail's 4:3 view does not see,
// from the last drawn field's classification pass, so the moby drawer's margin pass can list the
// mobys standing in them.
//
// The objects draw before the terrain (SCUS_944.25 8004C534 before 8004C4FC), so a field's margin
// pass reads the set the previous field's classification published. An in-between's classification
// runs inside a rolled-back run and publishes nothing, so its set never reaches a real field.
#pragma once

#include "core.h"

#include <bitset>
#include <cstdint>

namespace spyro {

class MarginSectors {
public:
  // One bit per sector index, which is the moby group byte (+0x52) that names it.
  using Set = std::bitset<256>;

  // The classification of a drawn field over the sector table `level`.
  void publish(const Core &core, std::uint32_t level, const Set &listed) {
    if (core.writeRollback.armed()) {
      return;
    }
    level_ = level;
    listed_ = listed;
  }
  // Whether group `group` is a margin-only sector of the level whose sector table is `level`.
  [[nodiscard]] bool lists(std::uint32_t level, std::uint32_t group) const {
    return level == level_ && group < listed_.size() && listed_.test(group);
  }
  // A restored machine has no previous field.
  void clear() {
    level_ = 0;
    listed_.reset();
  }

private:
  std::uint32_t level_ = 0;
  Set listed_;
};

} // namespace spyro
