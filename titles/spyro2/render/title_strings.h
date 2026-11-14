// title_strings.h — the title screen's memory-card strings, which the boot overlay's image drawer
// FUN_80079878(x, y, image, colour) draws as whole-string sprites from a picture uploaded to VRAM
// (512, 256) (spyro-2 `spyro2.title-strings`), as psxport draws them from an outline font (plan
// 0207): the one-colour lines on the 4bpp pages (768, 256) and (832, 256). Each glyph is a run of
// ink columns of its line in the picture; the lines in grey ramps and the 8bpp art keep their
// bitmap.
#pragma once

#include "guest_code_module.h"
#include "host_input.h"
#include "outline_text.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

class Core;

namespace spyro2::title_strings {

// The boot overlay (WAD 0x23000, 37 sectors read to 0x8006D264), as its stock read lands.
inline constexpr std::string_view kBootOverlaySha256 =
    "a91b532f3f2db952e890883b5147e11b04356660f7f2c4bf985ef78d95f4471b";
inline constexpr std::uint32_t kImageDrawer = 0x80079878u;
// The drawer's DR_TPAGE, then the sprite it links (8007992C).
inline constexpr std::uint32_t kSpriteOffset = 0xCu;

// Images 0x3C-0x4A of the drawer's table (0x80063F2C + 6 * image): u across both pages, v.
struct StringLine {
  std::uint8_t image = 0;
  std::uint16_t u = 0;
  std::uint8_t v = 0;
};
inline constexpr std::uint8_t kFirstImage = 0x3C;
inline constexpr std::uint8_t kLastImage = 0x4A;
inline constexpr std::array<StringLine, kLastImage - kFirstImage + 1> kLines = {{
    {0x3C, 256, 120}, // Memory Card 1 Triangle to Return
    {0x3D, 256, 132}, // Memory Card 2 Triangle to Return
    {0x3E, 256, 144}, // Select File for New Game
    {0x3F, 256, 156}, // Select File to Load Game
    {0x40, 0, 216},   // Start Game
    {0x41, 0, 228},   // Cancel
    {0x42, 0, 240},   // Slot 1
    {0x43, 128, 216}, // Slot 2
    {0x44, 128, 228}, // Format
    {0x45, 128, 240}, // Retry
    {0x46, 256, 216}, // Create Block
    {0x47, 256, 228}, // Continue
    {0x48, 256, 240}, // New Game
    {0x49, 384, 216}, // Load Game
    {0x4A, 384, 228}, // Overwrite
}};
static_assert(std::ranges::all_of(kLines,
                                  [](const StringLine &line) {
                                    return line.image == kFirstImage + (&line - kLines.data());
                                  }),
              "the lines run image by image");

// One glyph: its image, the first ink column from the line's u, the run's width and what it shows
// (0 the Triangle button).
struct StringGlyph {
  std::uint8_t image = 0;
  std::uint8_t start = 0;
  std::uint8_t width = 0;
  char32_t codepoint = 0;
};
inline constexpr std::uint16_t kLineHeight = 12;
inline constexpr std::array<StringGlyph, 159> kGlyphs = {
    {{0x3C, 6, 11, U'M'},  {0x3C, 21, 7, U'e'},  {0x3C, 30, 10, U'm'},  {0x3C, 43, 6, U'o'},
     {0x3C, 52, 6, U'r'},  {0x3C, 60, 8, U'y'},  {0x3C, 75, 8, U'C'},   {0x3C, 85, 7, U'a'},
     {0x3C, 94, 6, U'r'},  {0x3C, 103, 7, U'd'}, {0x3C, 119, 6, U'1'},  {0x3C, 156, 15, 0},
     {0x3C, 179, 6, U't'}, {0x3C, 187, 6, U'o'}, {0x3C, 201, 8, U'R'},  {0x3C, 211, 7, U'e'},
     {0x3C, 220, 6, U't'}, {0x3C, 228, 6, U'u'}, {0x3C, 237, 6, U'r'},  {0x3C, 245, 6, U'n'},
     {0x3D, 6, 11, U'M'},  {0x3D, 21, 7, U'e'},  {0x3D, 30, 10, U'm'},  {0x3D, 43, 6, U'o'},
     {0x3D, 52, 6, U'r'},  {0x3D, 60, 8, U'y'},  {0x3D, 75, 8, U'C'},   {0x3D, 85, 7, U'a'},
     {0x3D, 94, 6, U'r'},  {0x3D, 103, 7, U'd'}, {0x3D, 119, 7, U'2'},  {0x3D, 156, 15, 0},
     {0x3D, 179, 6, U't'}, {0x3D, 187, 6, U'o'}, {0x3D, 201, 8, U'R'},  {0x3D, 211, 7, U'e'},
     {0x3D, 220, 6, U't'}, {0x3D, 228, 6, U'u'}, {0x3D, 237, 6, U'r'},  {0x3D, 245, 6, U'n'},
     {0x3E, 23, 9, U'S'},  {0x3E, 35, 7, U'e'},  {0x3E, 45, 2, U'l'},   {0x3E, 49, 7, U'e'},
     {0x3E, 58, 6, U'c'},  {0x3E, 67, 6, U't'},  {0x3E, 83, 7, U'F'},   {0x3E, 93, 2, U'i'},
     {0x3E, 98, 2, U'l'},  {0x3E, 101, 7, U'e'}, {0x3E, 118, 7, U'f'},  {0x3E, 127, 6, U'o'},
     {0x3E, 135, 6, U'r'}, {0x3E, 152, 9, U'N'}, {0x3E, 164, 7, U'e'},  {0x3E, 174, 10, U'w'},
     {0x3E, 193, 9, U'G'}, {0x3E, 204, 7, U'a'}, {0x3E, 213, 10, U'm'}, {0x3E, 226, 7, U'e'},
     {0x3F, 26, 9, U'S'},  {0x3F, 38, 7, U'e'},  {0x3F, 48, 2, U'l'},   {0x3F, 52, 7, U'e'},
     {0x3F, 61, 6, U'c'},  {0x3F, 70, 6, U't'},  {0x3F, 86, 7, U'F'},   {0x3F, 96, 2, U'i'},
     {0x3F, 101, 2, U'l'}, {0x3F, 104, 7, U'e'}, {0x3F, 121, 6, U't'},  {0x3F, 129, 6, U'o'},
     {0x3F, 146, 7, U'L'}, {0x3F, 154, 6, U'o'}, {0x3F, 163, 7, U'a'},  {0x3F, 172, 7, U'd'},
     {0x3F, 190, 9, U'G'}, {0x3F, 201, 7, U'a'}, {0x3F, 210, 10, U'm'}, {0x3F, 223, 7, U'e'},
     {0x40, 21, 9, U'S'},  {0x40, 32, 6, U't'},  {0x40, 39, 7, U'a'},   {0x40, 47, 6, U'r'},
     {0x40, 55, 6, U't'},  {0x40, 69, 9, U'G'},  {0x40, 79, 7, U'a'},   {0x40, 88, 10, U'm'},
     {0x40, 100, 7, U'e'}, {0x41, 41, 8, U'C'},  {0x41, 51, 7, U'a'},   {0x41, 59, 6, U'n'},
     {0x41, 67, 6, U'c'},  {0x41, 75, 7, U'e'},  {0x41, 85, 2, U'l'},   {0x42, 40, 9, U'S'},
     {0x42, 53, 2, U'l'},  {0x42, 56, 6, U'o'},  {0x42, 65, 6, U't'},   {0x42, 81, 6, U'1'},
     {0x43, 40, 9, U'S'},  {0x43, 53, 2, U'l'},  {0x43, 56, 6, U'o'},   {0x43, 65, 6, U't'},
     {0x43, 81, 7, U'2'},  {0x44, 37, 7, U'F'},  {0x44, 46, 6, U'o'},   {0x44, 55, 6, U'r'},
     {0x44, 63, 10, U'm'}, {0x44, 76, 7, U'a'},  {0x44, 85, 6, U't'},   {0x45, 43, 8, U'R'},
     {0x45, 53, 7, U'e'},  {0x45, 62, 6, U't'},  {0x45, 70, 6, U'r'},   {0x45, 78, 8, U'y'},
     {0x46, 13, 8, U'C'},  {0x46, 23, 6, U'r'},  {0x46, 31, 7, U'e'},   {0x46, 41, 7, U'a'},
     {0x46, 50, 6, U't'},  {0x46, 58, 7, U'e'},  {0x46, 76, 8, U'B'},   {0x46, 86, 2, U'l'},
     {0x46, 90, 6, U'o'},  {0x46, 99, 6, U'c'},  {0x46, 107, 7, U'k'},  {0x47, 31, 8, U'C'},
     {0x47, 41, 6, U'o'},  {0x47, 50, 6, U'n'},  {0x47, 59, 6, U't'},   {0x47, 68, 2, U'i'},
     {0x47, 72, 6, U'n'},  {0x47, 80, 6, U'u'},  {0x47, 89, 7, U'e'},   {0x48, 24, 9, U'N'},
     {0x48, 37, 7, U'e'},  {0x48, 46, 10, U'w'}, {0x48, 65, 9, U'G'},   {0x48, 76, 7, U'a'},
     {0x48, 86, 10, U'm'}, {0x48, 99, 7, U'e'},  {0x49, 22, 7, U'L'},   {0x49, 30, 6, U'o'},
     {0x49, 39, 7, U'a'},  {0x49, 48, 7, U'd'},  {0x49, 66, 9, U'G'},   {0x49, 77, 7, U'a'},
     {0x49, 86, 10, U'm'}, {0x49, 99, 7, U'e'},  {0x4A, 27, 11, U'O'},  {0x4A, 39, 7, U'v'},
     {0x4A, 47, 7, U'e'},  {0x4A, 55, 6, U'r'},  {0x4A, 63, 10, U'w'},  {0x4A, 75, 6, U'r'},
     {0x4A, 84, 2, U'i'},  {0x4A, 87, 6, U't'},  {0x4A, 94, 7, U'e'}}};

constexpr std::array<psx::present::OutlineGlyph, kGlyphs.size()> outlineGlyphs() {
  std::array<psx::present::OutlineGlyph, kGlyphs.size()> glyphs{};
  for (std::size_t i = 0; i < kGlyphs.size(); i++) {
    const StringGlyph &glyph = kGlyphs[i];
    const StringLine &line = kLines[glyph.image - kFirstImage];
    glyphs[i] = {.u = static_cast<std::uint16_t>(line.u + glyph.start),
                 .v = line.v,
                 .width = glyph.width,
                 .height = kLineHeight,
                 .codepoint = glyph.codepoint};
    if (glyph.codepoint == 0) {
      glyphs[i].face = psx::present::OutlineFace::Prompt;
      glyphs[i].button = psx::input::kButtonTriangle;
    }
  }
  return glyphs;
}
inline constexpr std::array<psx::present::OutlineGlyph, kGlyphs.size()> kOutlineGlyphs =
    outlineGlyphs();

inline constexpr psx::present::OutlineFont kFont{
    .page = {.x = 768, .y = 256, .depth = psx::gpu::TexDepth::Indexed4},
    .glyphs = kOutlineGlyphs,
    .textFace = "Fredoka.ttf",
    .weight = 500};

// Installs the image drawer's override on the boot overlay as it lands.
void landed(Core &core, const psx::code_module::Digest &content);

} // namespace spyro2::title_strings
