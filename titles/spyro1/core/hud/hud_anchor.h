// hud_anchor.h — Spyro 1's HUD records in the sprite queue and the anchor class of each,
// which RasterizeSpritePrimQueue 0x80022A2C draws them with (titles/spyro1/render/sprite_queue.h).
//
// EnqueueLoadingScreenSprites 0x80019300 queues the twelve icon records at 0x80077FEC, placed from
// their rest positions at 0x8006E68C:
//
//   0..4   gem digits and icon    x 46..174    left edge
//   5..7   dragon digits and icon x 230..292   centred
//   8..10  life digits and icon   x 394..464   right edge
//   11     key                    x 430        right edge
//
// While the gem counter shows the level total (display state 4) it also queues the "%d/%d" glyph
// records BuildTextSpriteChain 0x80017FE4 writes at x 0x5A, right after icon 4: left edge. Every
// other screen-space record (menus, titles) is centred.
//
// The same routine draws two rows of 2D quads through EmitTexturedQuad 0x8001919C, from rects at
// 0x8007840C: the life orbs ringing the life icon (800195C4, x from icon 10's), right edge; and the
// egg row, up to twelve along the bottom from x 36 (8001965C), left edge.
#pragma once

#include "render/hud/ui_anchor.h"

#include <cstdint>
#include <optional>

class Core;

namespace spyro1::hud_anchor {

inline constexpr std::uint32_t kIconRecords = 0x80077FECu;
inline constexpr std::uint32_t kIconCount = 12;
inline constexpr std::uint32_t kRecordBytes = 0x58u;
inline constexpr std::uint32_t kGemIcon = 4;

constexpr std::optional<std::uint32_t> iconIndex(std::uint32_t record) {
  const std::uint32_t offset = record - kIconRecords;
  if (offset >= kIconCount * kRecordBytes || offset % kRecordBytes != 0) {
    return std::nullopt;
  }
  return offset / kRecordBytes;
}

constexpr spyro::ui_anchor::Anchor iconAnchor(std::uint32_t index) {
  if (index <= kGemIcon) {
    return spyro::ui_anchor::Anchor::LeftEdge;
  }
  if (index <= 7) {
    return spyro::ui_anchor::Anchor::Centred;
  }
  return spyro::ui_anchor::Anchor::RightEdge;
}

// The sprite queue's screen-space records in queue order, each named by its class.
class QueueAnchors {
public:
  [[nodiscard]] spyro::ui_anchor::Anchor next(std::uint32_t record) {
    if (const auto index = iconIndex(record)) {
      afterGemIcon_ = *index == kGemIcon;
      return iconAnchor(*index);
    }
    return afterGemIcon_ ? spyro::ui_anchor::Anchor::LeftEdge : spyro::ui_anchor::Anchor::Centred;
  }

private:
  bool afterGemIcon_ = false;
};

inline constexpr std::uint32_t kTexturedQuad = 0x8001919Cu;
// The return addresses of the two rows' calls (jal at 800195C4 and 8001965C).
inline constexpr std::uint32_t kOrbQuadReturn = 0x800195CCu;
inline constexpr std::uint32_t kEggQuadReturn = 0x80019664u;

constexpr std::optional<spyro::ui_anchor::Anchor> quadAnchor(std::uint32_t returnAddress) {
  if (returnAddress == kOrbQuadReturn) {
    return spyro::ui_anchor::Anchor::RightEdge;
  }
  if (returnAddress == kEggQuadReturn) {
    return spyro::ui_anchor::Anchor::LeftEdge;
  }
  return std::nullopt;
}

// The columns an element of `anchor` moves from its authored x on the presented frame.
[[nodiscard]] std::int32_t columns(const Core &core,
                                   const spyro::ui_anchor::Element &element,
                                   spyro::ui_anchor::Anchor anchor);

// EmitTexturedQuad's override: the original, then a HUD row's quad moved by its class.
void registerOverrides(Core &core);

} // namespace spyro1::hud_anchor
