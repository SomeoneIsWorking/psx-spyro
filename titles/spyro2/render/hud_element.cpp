#include "spyro2/render/hud_element.h"

#include "core.h"
#include "core/session.h"
#include "emit_memory.h"
#include "projected_vertex.h"
#include "r3000.h"
#include "render/overlay_pair.h"
#include "spyro2/render/render_facts.h"

#include <cstdint>

namespace spyro2::hud_element {
namespace {

// The element's width and height in pixels, one byte per size index.
constexpr std::uint32_t kWidths = 0x800634B8u;
constexpr std::uint32_t kHeights = 0x800634D0u;
constexpr std::uint32_t kSpriteTag = 0x04000000u; // 800520E4
constexpr std::uint32_t kQuadTag = 0x09000000u;   // 80052194
constexpr std::uint16_t kQuadPage = 0x38u;        // 80052224
constexpr std::uint8_t kGrey = 0x80u;
constexpr std::uint32_t kFrameBytes = 0x18u;

// FUN_8001B390(packet); every packet the sprite emitter draws is HUD.
void appendToOverlay(Core &core, const psx::present::EmitMemory &memory, std::uint32_t packet) {
  (void)spyro::overlay_pair::append(memory, kRenderGlobals.overlayPair, packet);
  spyro::session(core).layers.linked(core, packet);
}

// The image's CLUT halfword: palette column (+2 low bits) and row (+3).
std::uint16_t clutOf(const psx::present::EmitMemory &memory, std::uint32_t image) {
  return static_cast<std::uint16_t>((memory.mem_r8(image + 2) & 3u) +
                                    memory.mem_r8(image + 3) * 0x40u + 0x4020u);
}

std::uint32_t packed(std::uint32_t x, std::uint32_t y) {
  return (x & 0xFFFFu) | (y << 16);
}

} // namespace

void draw(Core &core, float rows) {
  const psx::present::EmitMemory memory(core);
  const std::uint32_t image = core.r[4];
  const std::uint32_t x = core.r[5] & 0xFFFFu;
  const std::uint32_t y = core.r[6] & 0xFFFFu;
  const std::uint32_t size = core.r[7];
  const std::uint32_t frame = core.r[psx::cpu::kSp] - kFrameBytes;
  memory.mem_w32(frame + 0x14, core.r[psx::cpu::kRa]);
  memory.mem_w32(frame + 0x10, core.r[16]);
  const std::uint32_t packet = memory.mem_r32(kRenderGlobals.primitiveCursor);
  const bool raw = (memory.mem_r8(image + 2) & 0x80u) != 0;
  const std::uint32_t width = memory.mem_r8(kWidths + size);
  const std::uint32_t height = memory.mem_r8(kHeights + size);
  const std::uint8_t u = memory.mem_r8(image);
  const std::uint8_t v = memory.mem_r8(image + 1);
  // A corner on the guest's halfwords, its place `rows` below them.
  const auto corner = [&](std::uint32_t at, std::uint32_t cornerX, std::uint32_t cornerY) {
    const auto placeX = static_cast<float>(static_cast<std::int16_t>(cornerX));
    const auto placeY = static_cast<float>(static_cast<std::int16_t>(cornerY)) + rows;
    memory.storeVertex(packet + at,
                       psx::gte::ProjectedVertex(packed(cornerX, cornerY), {placeX, placeY}));
  };
  memory.mem_w8(packet + 4, kGrey);
  memory.mem_w8(packet + 5, kGrey);
  memory.mem_w8(packet + 6, kGrey);
  std::uint32_t next = 0;
  if (static_cast<std::int32_t>(image) < 0) {
    memory.mem_w32(packet, kSpriteTag);
    memory.mem_w8(packet + 7, raw ? 0x66u : 0x64u);
    corner(8, x, y);
    memory.mem_w8(packet + 0xC, u);
    memory.mem_w8(packet + 0xD, v);
    memory.mem_w16(packet + 0xE, clutOf(memory, image));
    memory.mem_w16(packet + 0x10, static_cast<std::uint16_t>(width));
    memory.mem_w16(packet + 0x12, static_cast<std::uint16_t>(height));
    appendToOverlay(core, memory, packet);
    if (image - kDigitImages < kDigitGlyphs.size() * kDigitImageBytes) {
      spyro::session(core).layers.linkedGlyph(packet, kDigitFont);
    }
    next = packet + 0x14;
    core.r[2] = packet;
  } else {
    memory.mem_w32(packet, kQuadTag);
    memory.mem_w8(packet + 7, raw ? 0x2Eu : 0x2Cu);
    corner(8, x, y);
    corner(0x10, x + width, y);
    corner(0x18, x, y + height);
    corner(0x20, x + width, y + height);
    memory.mem_w16(packet + 0xE, clutOf(memory, image));
    memory.mem_w16(packet + 0x16, kQuadPage);
    // The quad samples its image mirrored: the left corners take the right column.
    memory.mem_w8(packet + 0xC, static_cast<std::uint8_t>(u + width - 1u));
    memory.mem_w8(packet + 0xD, v);
    memory.mem_w8(packet + 0x14, u);
    memory.mem_w8(packet + 0x15, v);
    memory.mem_w8(packet + 0x1C, static_cast<std::uint8_t>(u + width - 1u));
    memory.mem_w8(packet + 0x1D, static_cast<std::uint8_t>(v + height - 1u));
    memory.mem_w8(packet + 0x24, u);
    memory.mem_w8(packet + 0x25, static_cast<std::uint8_t>(v + height - 1u));
    appendToOverlay(core, memory, packet);
    next = packet + 0x28;
    core.r[2] = 0;
  }
  memory.mem_w32(kRenderGlobals.primitiveCursor, next);
  core.r[3] = next;
}

} // namespace spyro2::hud_element
