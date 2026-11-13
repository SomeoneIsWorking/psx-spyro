// text_font.h — Spyro 2 and 3's 2D font (spyro-2 `spyro2.text-font`, spyro-3 `spyro3.text-font`) as
// psxport draws it from outline fonts (plan 0207).
//
// Both text layouts (SCUS_944.25 0x8004A374/0x8004A59C, SCUS_944.67 0x8002E748/0x8002E970) draw
// 12x9 cells of the 4bpp page at (512, 256), DR_TPAGE 0xE1000618, 32 cells a row; g_TextGlyphCells
// names each character's cell. A cell shows the character whose entry names it: '[' is a bullet,
// the accent cells (0x00-0x16, reached by the '@' escapes of g_TextEscapes: ! ? S C A G U I T) a
// mark the layout draws over the letter before it, and '<' '>' '{' '}' the Cross, Square, Circle
// and Triangle cells the layouts draw through the icon CLUT. Spyro 3 shares Spyro 2's cells and
// adds '+' (cell 130, empty in Spyro 2's page). Glyphs shade down the cell, a CLUT index a band
// of rows, over a shadow one texel right and below in indices of their own (no texel of which has
// an empty texel up-left of it); the button cells are one index with no shadow.
#pragma once

#include "host_input.h"
#include "outline_text.h"

#include <algorithm>
#include <array>
#include <span>

namespace spyro {

namespace text_font {

using psx::present::OutlineCell;
using psx::present::OutlineFace;

constexpr OutlineCell letter(std::uint16_t cell, char32_t codepoint) {
  return {.cell = cell, .codepoint = codepoint};
}

constexpr OutlineCell button(std::uint16_t cell, std::uint16_t psxButton) {
  return {.cell = cell, .face = OutlineFace::Prompt, .button = psxButton};
}

// A run of consecutive cells holding consecutive codepoints.
template <std::size_t N>
constexpr void run(std::array<OutlineCell, N> &cells,
                   std::size_t &at,
                   std::uint16_t cell,
                   char32_t first,
                   char32_t last) {
  for (char32_t codepoint = first; codepoint <= last; codepoint++) {
    cells[at++] = letter(static_cast<std::uint16_t>(cell + (codepoint - first)), codepoint);
  }
}

// Letters, digits and the other cells.
inline constexpr std::size_t kCells = 26 + 26 + 10 + 25;

constexpr std::array<OutlineCell, kCells> cells() {
  std::array<OutlineCell, kCells> cells{};
  std::size_t at = 0;
  run(cells, at, 0, U'A', U'U');
  run(cells, at, 32, U'V', U'Z');
  run(cells, at, 37, U'a', U'p');
  run(cells, at, 64, U'q', U'z');
  run(cells, at, 74, U'0', U'9');
  for (const OutlineCell cell : {letter(84, U'!'),
                                 letter(96, U'\''),
                                 letter(97, U','),
                                 letter(98, U'-'),
                                 letter(99, U'.'),
                                 letter(100, U'?'),
                                 letter(101, U'/'),
                                 letter(102, U':'),
                                 letter(103, U'•'),
                                 letter(104, U'%'),
                                 button(105, psx::input::kButtonCross),
                                 button(106, psx::input::kButtonSquare),
                                 button(107, psx::input::kButtonCircle),
                                 button(108, psx::input::kButtonTriangle),
                                 {.cell = 109, .codepoint = U'★', .face = OutlineFace::Prompt},
                                 letter(110, U'¡'),
                                 letter(111, U'¿'),
                                 letter(112, U'ß'),
                                 letter(113, U'¸'),
                                 letter(114, U'´'),
                                 letter(115, U'`'),
                                 letter(116, U'¨'),
                                 letter(128, U'ˆ'),
                                 letter(129, U'˜'),
                                 letter(130, U'+')}) {
    cells[at++] = cell;
  }
  return cells;
}

inline constexpr std::array<OutlineCell, kCells> kCellTable = cells();
static_assert(std::ranges::all_of(kCellTable,
                                  [](const OutlineCell &cell) {
                                    return cell.codepoint != 0 || cell.button != 0;
                                  }),
              "kCells counts more cells than the table names");

} // namespace text_font

// The font of a page whose glyph shadow is drawn in CLUT indices `shadowIndices`.
constexpr psx::present::OutlineFont textFont(std::span<const int> shadowIndices) {
  return {.page = {.x = 512, .y = 256, .depth = psx::gpu::TexDepth::Indexed4},
          .cellWidth = 12,
          .cellHeight = 9,
          .columns = 32,
          .cells = text_font::kCellTable,
          .textFace = "Fredoka.ttf",
          .weight = 700,
          .shadowIndices = shadowIndices,
          .shadowX = 1,
          .shadowY = 1};
}

} // namespace spyro
