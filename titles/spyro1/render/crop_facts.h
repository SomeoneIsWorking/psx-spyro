// crop_facts.h — the packets of a Spyro 1 frame that crop widescreen shows as UI
// (render/crop_layers.h).
//
// The UI is what SCUS_942.28 lays out in screen space: the sprite queue's screen-space records (the
// moby cull word's screen-space bit, titles/spyro1/render/sprite_queue.h) and the packets
// AddPrimToOT 0x800168DC appends to the overlay pair. Among those, the whole-screen draws belong to
// the picture and crop with it; by return address:
//
//   8001A738  the pause backdrop's four grey strips (func_8001A40C)
//   8001A784  the line under them
//   80019120  FUN_800190D4's tint: its draw mode
//   80019170  and its 512x224 quad
//   8001904C  the cinematic letterbox's top bar (80018F30)
//   800190A8  and its bottom bar
//
// The rescue star (0x80058864) appends to the pair natively and is drawn in the world.
#pragma once

#include <array>
#include <cstdint>

namespace spyro1 {

inline constexpr std::array<std::uint32_t, 6> kWholeScreenLinks = {
    0x8001A738u, 0x8001A784u, 0x80019120u, 0x80019170u, 0x8001904Cu, 0x800190A8u};

} // namespace spyro1
