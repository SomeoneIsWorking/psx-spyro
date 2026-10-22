// conversation_box.h — the conversation's box and name tab as state producers.
//
// The conversation's draw FUN_80013BC4 places both at one slide column taken from a table by the
// box's timer, then draws the field: the box FUN_8004D3FC(slide + 0x18, slide + 0x1E8, 0x92, 0xD4)
// (a draw mode, three FUN_8004D2D8 rects, four FUN_800520CC corner sprites), then the name tab's
// three rects at 80013D40, 80013D54 and 80013D68 under the draw mode FUN_8004D104(0x18). The line
// and the name follow only once the slide is 0.
//
// Each is keyed where it is drawn (the box by its drawer, each tab rect by its call site) and saves
// its left column with the packets it wrote; an in-between draws them moved by the slide between
// two saves, rounded to whole columns so the rects and corners stay joined.
#pragma once

#include "state_producer.h"

#include <cstdint>

class Core;

namespace spyro2 {

class ConversationBox final : public psx::present::StateProducer {
public:
  static constexpr std::uint32_t kEntry = 0x8004D3FCu;

  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;
  [[nodiscard]] Composition composition(const psx::present::StateInterval &states) const override;
};

class NameTabRect final : public psx::present::StateProducer {
public:
  static constexpr std::uint32_t kEntry = 0x8004D2D8u;

  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;
  [[nodiscard]] Composition composition(const psx::present::StateInterval &states) const override;
};

void installConversationBox(Core &core);

} // namespace spyro2
