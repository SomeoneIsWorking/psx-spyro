// sprite_fonts.h — Spyro 3's fonts drawn a glyph a call through the icon emitter FUN_800289C8 from
// a SpriteData table (8 bytes an entry: u0, v0, CLUT, u1, v1 inclusive, DR_TPAGE), as psxport draws
// them from outline fonts (plan 0207): the Atlas book font (spyro-3 `spyro3.atlas-font`), whose
// string drawer 0x80076228 hands the emitter entries 0x42-0xA2 of *0x80070108, and the HUD digits
// the value emitter 0x800291B8 draws from *0x8006C788, whose entries move with the level.
#pragma once

#include "host_input.h"
#include "outline_text.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

class Core;

namespace spyro3::sprite_fonts {

using psx::present::OutlineFace;
using psx::present::OutlineGlyph;

inline constexpr std::uint32_t kAtlasSprites = 0x80070108u; // word: the Atlas's SpriteData table
inline constexpr std::uint32_t kSpriteBytes = 8u;
inline constexpr std::uint32_t kAtlasFirst = 0x42u;
inline constexpr std::uint32_t kAtlasLast = 0xA2u;

// Each font's place in Runtime::outlineFonts; the 2D font is 0.
inline constexpr std::uint8_t kAtlasFont = 1;
inline constexpr std::uint8_t kDigitFont = 2;

inline constexpr std::uint32_t kDigitSprites = 0x8006C788u; // word: the level's SpriteData table
inline constexpr std::uint32_t kFirstDigit = 0x800719CCu;   // halfword: the entry of digit 0
// Digits 0-9, then the slash the ratio drawer 0x800293C4 draws between two values.
inline constexpr std::size_t kDigitGlyphs = 11;
// The digits' gold bands lie over a shadow one texel right and below in these CLUT indices.
inline constexpr std::array<int, 3> kDigitShadowIndices{2, 7, 0xA};

// One SpriteData entry of a font: its index, DR_TPAGE and rectangle (all glyphs 12 wide, 4bpp, y
// 0).
struct SpriteRect {
  std::uint8_t index = 0;
  std::uint8_t tpage = 0;
  std::uint8_t u = 0;
  std::uint8_t v = 0;
  std::uint8_t height = 0;
};

// The Atlas's entries as the table holds them in every homeworld.
inline constexpr std::array<SpriteRect, kAtlasLast - kAtlasFirst + 1> kAtlasRects = {{
    {0x42, 0x09, 12, 24, 9},   {0x43, 0x09, 24, 24, 9},   {0x44, 0x09, 12, 33, 9},
    {0x45, 0x09, 24, 33, 9},   {0x46, 0x0C, 192, 39, 9},  {0x47, 0x0C, 204, 39, 9},
    {0x48, 0x0C, 216, 39, 9},  {0x49, 0x0C, 228, 39, 9},  {0x4A, 0x0C, 240, 39, 9},
    {0x4B, 0x0D, 96, 20, 12},  {0x4C, 0x0D, 108, 20, 9},  {0x4D, 0x09, 12, 42, 9},
    {0x4E, 0x09, 24, 42, 9},   {0x4F, 0x09, 12, 51, 9},   {0x50, 0x09, 24, 51, 9},
    {0x51, 0x09, 12, 60, 9},   {0x52, 0x09, 24, 60, 9},   {0x53, 0x09, 36, 60, 9},
    {0x54, 0x09, 48, 60, 9},   {0x55, 0x09, 60, 60, 9},   {0x56, 0x09, 12, 69, 9},
    {0x57, 0x09, 24, 69, 9},   {0x58, 0x09, 36, 69, 9},   {0x59, 0x09, 48, 69, 9},
    {0x5A, 0x09, 60, 69, 9},   {0x5B, 0x09, 72, 61, 9},   {0x5C, 0x09, 12, 78, 9},
    {0x5D, 0x09, 24, 78, 9},   {0x5E, 0x09, 36, 78, 9},   {0x5F, 0x09, 48, 78, 9},
    {0x60, 0x09, 60, 78, 9},   {0x61, 0x09, 72, 70, 9},   {0x62, 0x09, 12, 87, 9},
    {0x63, 0x09, 24, 87, 9},   {0x64, 0x09, 36, 87, 9},   {0x65, 0x09, 48, 87, 9},
    {0x66, 0x09, 60, 87, 9},   {0x67, 0x09, 72, 79, 9},   {0x68, 0x0A, 144, 65, 9},
    {0x69, 0x0A, 144, 74, 9},  {0x6A, 0x0A, 144, 83, 9},  {0x6B, 0x0A, 156, 65, 9},
    {0x6C, 0x0A, 156, 74, 9},  {0x6D, 0x0A, 156, 83, 9},  {0x6E, 0x0B, 144, 65, 9},
    {0x6F, 0x0B, 144, 74, 9},  {0x70, 0x0B, 144, 83, 9},  {0x71, 0x0B, 156, 65, 9},
    {0x72, 0x0B, 156, 74, 9},  {0x73, 0x0B, 156, 83, 9},  {0x74, 0x0C, 240, 81, 9},
    {0x75, 0x0C, 132, 40, 9},  {0x76, 0x09, 72, 88, 9},   {0x77, 0x0B, 144, 92, 9},
    {0x78, 0x0B, 156, 92, 9},  {0x79, 0x0C, 240, 90, 9},  {0x7A, 0x0B, 96, 44, 9},
    {0x7B, 0x0B, 108, 44, 9},  {0x7C, 0x0C, 240, 48, 9},  {0x7D, 0x0C, 132, 49, 9},
    {0x7E, 0x0C, 240, 57, 9},  {0x7F, 0x0B, 96, 53, 9},   {0x80, 0x0B, 108, 53, 9},
    {0x81, 0x0B, 120, 54, 9},  {0x82, 0x0B, 132, 54, 9},  {0x83, 0x0B, 144, 54, 9},
    {0x84, 0x0B, 156, 54, 9},  {0x85, 0x09, 12, 96, 9},   {0x86, 0x09, 24, 96, 9},
    {0x87, 0x09, 36, 96, 9},   {0x88, 0x09, 48, 96, 9},   {0x89, 0x09, 60, 96, 9},
    {0x8A, 0x0C, 96, 96, 9},   {0x8B, 0x0C, 108, 96, 9},  {0x8C, 0x0C, 120, 96, 9},
    {0x8D, 0x0C, 132, 96, 9},  {0x8E, 0x0C, 144, 96, 9},  {0x8F, 0x0C, 156, 96, 9},
    {0x90, 0x0C, 168, 96, 9},  {0x91, 0x0C, 180, 96, 9},  {0x92, 0x0C, 192, 96, 9},
    {0x93, 0x0C, 204, 96, 9},  {0x94, 0x0C, 216, 96, 9},  {0x95, 0x0C, 228, 96, 9},
    {0x96, 0x09, 12, 105, 9},  {0x97, 0x09, 24, 105, 9},  {0x98, 0x09, 36, 105, 9},
    {0x99, 0x09, 48, 105, 9},  {0x9A, 0x09, 60, 105, 9},  {0x9B, 0x09, 72, 97, 9},
    {0x9C, 0x0B, 144, 101, 9}, {0x9D, 0x0B, 156, 101, 9}, {0x9E, 0x0C, 240, 99, 9},
    {0x9F, 0x09, 12, 114, 9},  {0xA0, 0x09, 24, 114, 9},  {0xA1, 0x09, 36, 114, 9},
    {0xA2, 0x09, 48, 114, 9},
}};
static_assert(std::ranges::all_of(kAtlasRects,
                                  [](const SpriteRect &rect) {
                                    return rect.index == kAtlasFirst + (&rect - kAtlasRects.data());
                                  }),
              "the Atlas rects run entry by entry");

// The Atlas font's first page, DR_TPAGE 0x09; its glyphs run on across 0x0A-0x0D.
inline constexpr psx::gpu::TexPage kAtlasPage{
    .x = 576, .y = 0, .depth = psx::gpu::TexDepth::Indexed4};
inline constexpr int kAtlasFirstTpage = 0x09;
inline constexpr int kPageTexels = 256; // 4bpp texels across a page

// What entry `index` shows: a-z, A-Z, 0-9, ' : , ! . ? / %, the '@' escapes' ¡ ¿ ß Ç ç, the marks
// drawn over the letter before them (grave, acute, diaeresis, circumflex, tilde, each in variants
// placed for capitals and small letters), and the Triangle, Square, Circle and Cross icons.
constexpr OutlineGlyph atlasGlyph(const SpriteRect &rect) {
  OutlineGlyph glyph{
      .u = static_cast<std::uint16_t>(rect.u + ((rect.tpage - kAtlasFirstTpage) * kPageTexels)),
      .v = rect.v,
      .width = 12,
      .height = rect.height};
  constexpr std::array<char32_t, 18> kMarks = {U'`',
                                               U'`',
                                               U'`',
                                               U'`',
                                               U'´',
                                               U'´',
                                               U'´',
                                               U'´',
                                               U'¨',
                                               U'¨',
                                               U'¨',
                                               U'¨',
                                               U'ˆ',
                                               U'ˆ',
                                               U'ˆ',
                                               U'ˆ',
                                               U'˜',
                                               U'˜'};
  constexpr std::array<char32_t, 13> kSigns = {
      U'\'', U':', U',', U'!', U'.', U'?', U'/', U'%', U'¡', U'¿', U'ß', U'Ç', U'ç'};
  constexpr std::array<std::uint16_t, 4> kButtons = {psx::input::kButtonTriangle,
                                                     psx::input::kButtonSquare,
                                                     psx::input::kButtonCircle,
                                                     psx::input::kButtonCross};
  const int index = rect.index;
  if (index < 0x5C) {
    glyph.codepoint = U'a' + static_cast<char32_t>(index - 0x42);
  } else if (index < 0x76) {
    glyph.codepoint = U'A' + static_cast<char32_t>(index - 0x5C);
  } else if (index < 0x80) {
    glyph.codepoint = U'0' + static_cast<char32_t>(index - 0x76);
  } else if (index < 0x8D) {
    glyph.codepoint = kSigns.at(static_cast<std::size_t>(index - 0x80));
  } else if (index < 0x9F) {
    glyph.codepoint = kMarks.at(static_cast<std::size_t>(index - 0x8D));
  } else {
    // Each icon is the PlayStation button in several colours, in a CLUT of its own.
    glyph.face = OutlineFace::Prompt;
    glyph.button = kButtons.at(static_cast<std::size_t>(index - 0x9F));
    glyph.padArt = true;
  }
  return glyph;
}

constexpr std::array<OutlineGlyph, kAtlasRects.size()> atlasGlyphs() {
  std::array<OutlineGlyph, kAtlasRects.size()> glyphs{};
  for (std::size_t i = 0; i < glyphs.size(); i++) {
    glyphs[i] = atlasGlyph(kAtlasRects[i]);
  }
  return glyphs;
}

inline constexpr std::array<OutlineGlyph, kAtlasRects.size()> kAtlasGlyphs = atlasGlyphs();

inline constexpr psx::present::OutlineFont kAtlasFontFace{
    .page = kAtlasPage, .glyphs = kAtlasGlyphs, .textFace = "Fredoka.ttf", .weight = 600};

// After the icon emitter drew SpriteData entry `sprite` as its packets from `firstPacket` to the
// cursor, names them as that entry's font's glyphs, if it is one.
void emitted(Core &core, std::uint32_t sprite, std::uint32_t firstPacket);

// The fonts as the loaded level lays them out: the 2D font, the Atlas font and the HUD digits read
// from the level's table (none while no table is loaded).
class Fonts {
public:
  // Reads the digits' entries again and returns every font.
  std::span<const psx::present::OutlineFont> follow(Core &core);

private:
  std::array<OutlineGlyph, kDigitGlyphs> digits_{};
  std::array<psx::present::OutlineFont, 3> fonts_{};
};

} // namespace spyro3::sprite_fonts
