// overlay_pair.h — the leaf all three images link a screen-space packet with (SCUS_942.28
// 0x800168DC, SCUS_944.25 0x8001B390, SCUS_944.67 0x8004E758), the same fourteen instructions over
// each image's pair word:
//
//     v1 = [pair] ; at = [v1] ; beq at,zero,EMPTY ; sw a0,0(v1)
//     sh a0,0(at) ; srl a0,a0,16 ; jr ra ; sb a0,2(at)      EMPTY: jr ra ; sw a0,4(v1)
//
// [pair] points at a (head, first) pair: an empty list records the packet as its first, a non-empty
// one links it after the head. Each packet is crop widescreen UI unless its call is one of the
// image's whole-screen draws.
//
// The two text layouts (SCUS_944.25 0x8004A374 and 0x8004A59C, SCUS_944.67 0x8002E748 and
// 0x8002E970, sprites and quads) write their glyphs from the primitive cursor on and link the chain
// into the pair themselves, inline; it is all UI, and its glyphs are the 2D font's
// (render/text_font.h).
#pragma once

#include "emit_memory.h"
#include "native_dispatch.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

class Core;

namespace spyro::overlay_pair {

struct TextLayout {
  std::uint32_t entry = 0;
  const char *overrideName = "";
};

struct Facts {
  std::uint32_t entry = 0;
  const char *overrideName = "";
  std::uint32_t pairWord = 0;
  // The returns of the calls that draw the whole screen, which crop with the picture.
  std::span<const std::uint32_t> wholeScreenReturns;
  std::uint32_t primitiveCursor = 0;
  std::array<TextLayout, 2> textLayouts;
};

struct Appended {
  std::uint32_t pair = 0;
  std::uint32_t head = 0; // the head before, 0 when the list was empty
};

// The leaf's links, for a native emitter.
Appended
append(const psx::present::EmitMemory &memory, std::uint32_t pairWord, std::uint32_t packet);
// The leaf at a call: links $a0, leaves v1, at and a0 as the guest does, and names the packet's
// crop layer.
void link(Core &core, const Facts &facts);
// A text layout at a call: runs it and names the chain it linked UI and font glyphs.
void layText(Core &core, const Facts &facts, std::uint32_t entry);

template <const Facts &facts> void linkOverride(Core *core) {
  link(*core, facts);
}

template <const Facts &facts, std::size_t index> void textOverride(Core *core) {
  layText(*core, facts, facts.textLayouts[index].entry);
}

void install(Core &core, const Facts &facts, psx::cpu::NativeFunction link);
void installText(Core &core, const TextLayout &layout, psx::cpu::NativeFunction override);

// The link and the image's text layouts.
template <const Facts &facts> void installAll(Core &core) {
  install(core, facts, linkOverride<facts>);
  installText(core, facts.textLayouts[0], textOverride<facts, 0>);
  installText(core, facts.textLayouts[1], textOverride<facts, 1>);
}

} // namespace spyro::overlay_pair
