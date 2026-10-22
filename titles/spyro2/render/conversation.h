// conversation.h — the conversation's overlay as state producers: the box, the name tab, and the
// underline and text drawn over them.
//
// The conversation's draw FUN_80013BC4 places the box and tab at one slide column taken from a table by
// the box's timer, then draws the field: the box FUN_8004D3FC(slide + 0x18, slide + 0x1E8, 0x92, 0xD4)
// (a draw mode, three FUN_8004D2D8 rects, four FUN_800520CC corner sprites), then the name tab's
// three rects at 80013D40, 80013D54 and 80013D68 under the draw mode FUN_8004D104(0x18). Once the
// slide is 0 it draws the name's underline FUN_8004D1BC (80013DB0) and the name FUN_8004A374
// (80013DC8); the lines, choices and prompts are its other FUN_8004A374 calls.
//
// The box and each tab rect are keyed where they are drawn and save their left column with their
// packets; an in-between draws them moved by the slide between two saves, rounded to whole columns so
// the rects and corners stay joined. The underline and each text, keyed by its string, save their
// packets; an in-between draws the earlier field's, and only where both fields drew them, so the name
// waits for the box to rest and a letter, line or prompt changes at the field that changes it.
#pragma once

#include "state_producer.h"

#include <cstddef>
#include <cstdint>
#include <span>

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

// The underline FUN_8004D1BC and the text FUN_8004A374, each installed under its own entry.
class ConversationText final : public psx::present::StateProducer {
public:
  static constexpr std::uint32_t kUnderlineEntry = 0x8004D1BCu;
  static constexpr std::uint32_t kTextEntry = 0x8004A374u;

  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;
  [[nodiscard]] Composition composition(const psx::present::StateInterval &states) const override;
  // A field that did not draw it.
  [[nodiscard]] bool rests(std::span<const std::byte> drawn,
                           std::span<std::byte> into) const override;
};

void installConversation(Core &core);

} // namespace spyro2
