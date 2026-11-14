// text_font.h — Spyro 2 and 3's 2D font (spyro-2 `spyro2.text-font`, spyro-3 `spyro3.text-font`)
// and HUD digits as psxport draws them from outline fonts (plan 0207).
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

using psx::present::OutlineFace;
using psx::present::OutlineGlyph;

// The page's 12x9 cells, 32 a row from its top-left.
inline constexpr psx::present::OutlineGrid kGrid{.cellWidth = 12, .cellHeight = 9, .columns = 32};

constexpr OutlineGlyph letter(int cell, char32_t codepoint) {
  return kGrid.glyph(cell, codepoint);
}

constexpr OutlineGlyph button(int cell, std::uint16_t psxButton) {
  return kGrid.button(cell, psxButton);
}

// A run of consecutive cells holding consecutive codepoints.
template <std::size_t N>
constexpr void
run(std::array<OutlineGlyph, N> &glyphs, std::size_t &at, int cell, char32_t first, char32_t last) {
  for (char32_t codepoint = first; codepoint <= last; codepoint++) {
    glyphs[at++] = letter(cell + static_cast<int>(codepoint - first), codepoint);
  }
}

// Letters, digits and the other cells.
inline constexpr std::size_t kGlyphs = 26 + 26 + 10 + 25;

constexpr std::array<OutlineGlyph, kGlyphs> glyphs() {
  std::array<OutlineGlyph, kGlyphs> glyphs{};
  std::size_t at = 0;
  run(glyphs, at, 0, U'A', U'U');
  run(glyphs, at, 32, U'V', U'Z');
  run(glyphs, at, 37, U'a', U'p');
  run(glyphs, at, 64, U'q', U'z');
  run(glyphs, at, 74, U'0', U'9');
  for (const OutlineGlyph glyph : {letter(84, U'!'),
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
                                   kGrid.glyph(109, U'★', OutlineFace::Prompt),
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
    glyphs[at++] = glyph;
  }
  return glyphs;
}

inline constexpr std::array<OutlineGlyph, kGlyphs> kGlyphTable = glyphs();
static_assert(std::ranges::all_of(kGlyphTable,
                                  [](const OutlineGlyph &glyph) {
                                    return glyph.codepoint != 0 || glyph.button != 0;
                                  }),
              "kGlyphs counts more glyphs than the table names");

} // namespace text_font

// The 2D font's page.
inline constexpr psx::gpu::TexPage kTextPage{
    .x = 512, .y = 256, .depth = psx::gpu::TexDepth::Indexed4};

// The font of a page whose glyph shadow is drawn in CLUT indices `shadowIndices`.
constexpr psx::present::OutlineFont textFont(std::span<const int> shadowIndices) {
  return {.page = kTextPage,
          .glyphs = text_font::kGlyphTable,
          .textFace = "Fredoka.ttf",
          .weight = 700,
          .shadowIndices = shadowIndices,
          .shadowX = 1,
          .shadowY = 1};
}

// The HUD counters' 24x16 digits, gold bands down each glyph over a shadow one texel right and
// below in CLUT indices `shadowIndices`, their 4bpp `page` holding `glyphs`.
constexpr psx::present::OutlineFont hudDigitFont(psx::gpu::TexPage page,
                                                 std::span<const psx::present::OutlineGlyph> glyphs,
                                                 std::span<const int> shadowIndices) {
  return {.page = page,
          .glyphs = glyphs,
          .textFace = "Fredoka.ttf",
          .weight = 500,
          .shadowIndices = shadowIndices,
          .shadowX = 1,
          .shadowY = 1};
}

} // namespace spyro
