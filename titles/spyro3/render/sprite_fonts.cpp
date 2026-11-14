#include "spyro3/render/sprite_fonts.h"

#include "core.h"
#include "core/session.h"
#include "render/text_font.h"
#include "spyro3/render/hud_slide.h"
#include "spyro3/render/render_facts.h"

#include <algorithm>
#include <limits>
#include <optional>

namespace spyro3::sprite_fonts {
namespace {

constexpr std::uint32_t kPacketBytes = 0x28u; // one POLY_FT4 with its tag
constexpr std::uint32_t kRamFirst = 0x80000000u;
constexpr std::uint32_t kRamEnd = 0x80200000u;

// A SpriteData entry as the emitter reads it.
struct Sprite {
  int u0 = 0;
  int v0 = 0;
  int u1 = 0;
  int v1 = 0;
  psx::gpu::TexPage page;
};

Sprite spriteAt(Core &core, std::uint32_t entry) {
  return {.u0 = core.mem_r8(entry),
          .v0 = core.mem_r8(entry + 1),
          .u1 = core.mem_r8(entry + 4),
          .v1 = core.mem_r8(entry + 5),
          .page = psx::gpu::decodeTexPage(core.mem_r16(entry + 6))};
}

// The digit table's first entry, or none while no level's table is loaded.
std::optional<std::uint32_t> firstDigitEntry(Core &core) {
  const std::uint32_t table = core.mem_r32(kDigitSprites);
  const auto first = static_cast<std::int16_t>(core.mem_r16(kFirstDigit));
  const std::uint32_t entry =
      table + static_cast<std::uint32_t>(first * static_cast<int>(kSpriteBytes));
  if (table < kRamFirst || entry < kRamFirst || entry + (kDigitGlyphs * kSpriteBytes) > kRamEnd) {
    return std::nullopt;
  }
  return entry;
}

void name(Core &core, std::uint32_t firstPacket, std::uint8_t font) {
  const std::uint32_t end = hud_slide::primitiveCursor(core);
  for (std::uint32_t packet = firstPacket; packet < end; packet += kPacketBytes) {
    spyro::session(core).layers.linkedGlyph(packet, font);
  }
}

} // namespace

std::span<const psx::present::OutlineFont> Fonts::follow(Core &core) {
  std::array<Sprite, kDigitGlyphs> sprites{};
  std::size_t count = 0;
  if (const std::optional<std::uint32_t> first = firstDigitEntry(core)) {
    count = kDigitGlyphs;
    for (std::size_t i = 0; i < kDigitGlyphs; i++) {
      sprites.at(i) = spriteAt(core, *first + static_cast<std::uint32_t>(i * kSpriteBytes));
      // The font is one 4bpp page box; a table not yet filled has no font.
      if (sprites.at(i).page.depth != psx::gpu::TexDepth::Indexed4 ||
          sprites.at(i).u1 < sprites.at(i).u0 || sprites.at(i).v1 < sprites.at(i).v0) {
        count = 0;
      }
    }
  }
  // The pages' top-left is the font's page; each glyph's u and v run on from it across VRAM.
  psx::gpu::TexPage page{.x = std::numeric_limits<int>::max(),
                         .y = std::numeric_limits<int>::max(),
                         .depth = psx::gpu::TexDepth::Indexed4};
  for (std::size_t i = 0; i < count; i++) {
    page.x = std::min(page.x, sprites.at(i).page.x);
    page.y = std::min(page.y, sprites.at(i).page.y);
  }
  constexpr int kTexelsPerHalfword = 4;
  for (std::size_t i = 0; i < count; i++) {
    const Sprite &sprite = sprites.at(i);
    digits_.at(i) = {.u = static_cast<std::uint16_t>(
                         ((sprite.page.x - page.x) * kTexelsPerHalfword) + sprite.u0),
                     .v = static_cast<std::uint16_t>(sprite.page.y - page.y + sprite.v0),
                     .width = static_cast<std::uint16_t>(sprite.u1 - sprite.u0 + 1),
                     .height = static_cast<std::uint16_t>(sprite.v1 - sprite.v0 + 1),
                     .codepoint = i < 10 ? U'0' + static_cast<char32_t>(i) : U'/'};
  }
  fonts_ = {kTextFont,
            kAtlasFontFace,
            spyro::hudDigitFont(count != 0 ? page : psx::gpu::TexPage{},
                                std::span<const OutlineGlyph>(digits_.data(), count),
                                kDigitShadowIndices)};
  return fonts_;
}

void emitted(Core &core, std::uint32_t sprite, std::uint32_t firstPacket) {
  const std::uint32_t atlas = core.mem_r32(kAtlasSprites);
  if (sprite >= atlas + (kAtlasFirst * kSpriteBytes) &&
      sprite <= atlas + (kAtlasLast * kSpriteBytes) && (sprite - atlas) % kSpriteBytes == 0) {
    name(core, firstPacket, kAtlasFont);
    return;
  }
  if (const std::optional<std::uint32_t> digits = firstDigitEntry(core);
      digits && sprite >= *digits && sprite < *digits + (kDigitGlyphs * kSpriteBytes)) {
    name(core, firstPacket, kDigitFont);
  }
}

} // namespace spyro3::sprite_fonts
