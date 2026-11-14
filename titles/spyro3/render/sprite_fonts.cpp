#include "spyro3/render/sprite_fonts.h"

#include "core.h"
#include "core/session.h"
#include "spyro3/render/hud_slide.h"

namespace spyro3::sprite_fonts {
namespace {

constexpr std::uint32_t kPacketBytes = 0x28u; // one POLY_FT4 with its tag

} // namespace

void emitted(Core &core, std::uint32_t sprite, std::uint32_t firstPacket) {
  const std::uint32_t atlas = core.mem_r32(kAtlasSprites);
  if (sprite < atlas + (kAtlasFirst * kSpriteBytes) ||
      sprite > atlas + (kAtlasLast * kSpriteBytes) || (sprite - atlas) % kSpriteBytes != 0) {
    return;
  }
  const std::uint32_t end = hud_slide::primitiveCursor(core);
  for (std::uint32_t packet = firstPacket; packet < end; packet += kPacketBytes) {
    spyro::session(core).layers.linkedGlyph(packet, kAtlasFont);
  }
}

} // namespace spyro3::sprite_fonts
