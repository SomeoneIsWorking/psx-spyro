// texture_scroll.h — water and lava scroll as a motion source. The update's 0x8002A6FC bakes each
// scroll node's position into its texture records, so an in-between rebakes the later tick's
// records from the position `t` of the way (the short way round its 7 bits) from the earlier
// tick's.
#pragma once

#include "motion_set.h"
#include "spyro1/core/moby/scroll_bake.h"

#include <array>
#include <cstdint>
#include <vector>

namespace spyro1 {

class TextureScroll final : public psx::present::MotionSource {
public:
  void sample(Core &core) override;
  void write(psx::present::GuestRedraw &redraw, float t) const override;

private:
  using NarrowWords = std::array<std::uint32_t, scroll::kNarrowRecordSize / 4>;
  using WideWords = std::array<std::uint32_t, scroll::kWideRecordSize / 4>;
  // One node, its position and the words of the two records it bakes.
  struct Node {
    std::uint32_t address = 0;
    std::uint32_t index = 0;
    std::uint32_t position = 0;
    std::uint32_t narrow = 0;
    std::uint32_t wide = 0;
    NarrowWords narrowWords{};
    WideWords wideWords{};
  };

  std::vector<Node> from_;
  std::vector<Node> to_;
};

} // namespace spyro1
