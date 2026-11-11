// screen_overlays.h — the cinematic letterbox all three images draw the same way (0x80018F30,
// 0x8004D604, 0x8001FF44), which only replayed draws call.
#pragma once

#include "native_dispatch.h"

#include <cstdint>

class Core;

namespace spyro::screen_overlays {

// The draw buffer's width, which the whole-screen overlays span.
inline constexpr std::uint32_t kScreenWidth = 0x200;

// The letterbox steps its bar height word toward 0 or its full height, then draws two opaque black
// POLY_F4 bars that tall, 512 wide, down from `top` and up from `bottom`.
struct LetterboxFacts {
  std::uint32_t entry = 0;
  const char *overrideName = "";
  std::uint32_t height = 0;
  std::uint32_t primitiveCursor = 0; // the bars are the next two packets it writes
  std::uint32_t top = 0;
  std::uint32_t bottom = 0;
  // The body steps the height one row per draw, so an in-between's edge lies between the row the
  // replay read and the row it drew, which no whole row can hold.
  bool rowPerDraw = false;
};

// Whether the guest body ran to its end; in a replay it places the bars' moving edge between the
// row read and the row drawn.
bool drawLetterbox(Core &core, const LetterboxFacts &facts);

// An override is a plain `void (*)(Core *)`, so each image's facts are a template argument.
template <const LetterboxFacts &facts> void letterboxOverride(Core *core) {
  (void)drawLetterbox(*core, facts);
}

// The letterbox's override `draw`, which runs drawLetterbox.
void installLetterbox(Core &core, const LetterboxFacts &facts, psx::cpu::NativeFunction draw);

} // namespace spyro::screen_overlays
