// hud_anchor.h — Spyro 3's HUD widgets anchored by the element their drawer was called with, applied to
// the x argument of the icon (FUN_800289C8) and value (FUN_800291B8) emitters; the value emitter's own
// icon calls (ra 0x80029378) are not shifted again.
#pragma once

#include "core/session.h"
#include "render/hud/ui_anchor.h"

#include <cstdint>
#include <optional>

class Core;

namespace spyro3::hud_anchor {

// The two widget drawers FUN_800285A4 registers, and the two emitters both of them draw through.
inline constexpr std::uint32_t kCounterDrawer = 0x80029904u;
inline constexpr std::uint32_t kLivesDrawer = 0x80029BB0u;
inline constexpr std::uint32_t kIconEmitter = 0x800289C8u;
inline constexpr std::uint32_t kValueEmitter = 0x800291B8u;

// The widget elements FUN_8002803C registers, and what each one is anchored to.
inline constexpr std::uint32_t kGemCounter = 0x80067248u;
inline constexpr std::uint32_t kLivesCounter = 0x8006729Cu;
inline constexpr std::uint32_t kEggCounter = 0x800672F0u;

// The class of a HUD widget; nullopt for the 3D passes' calls to the same emitters.
constexpr std::optional<spyro::ui_anchor::Anchor> counterAnchor(std::uint32_t element) {
  switch (element) {
  case kGemCounter:
    return spyro::ui_anchor::Anchor::LeftEdge;
  case kLivesCounter:
    return spyro::ui_anchor::Anchor::Centred;
  case kEggCounter:
    return spyro::ui_anchor::Anchor::RightEdge;
  default:
    return std::nullopt;
  }
}

// The widget whose drawer is running, carried down to the emitters it calls; zero is none.
class DrawState final : public spyro::TitleState {
public:
  void begin(std::uint32_t widgetElement) {
    element_ = widgetElement;
  }

  void end() {
    element_ = 0;
    inValue_ = false;
  }

  [[nodiscard]] std::uint32_t current() const {
    return element_;
  }

  void beginValue() {
    inValue_ = true;
  }

  void endValue() {
    inValue_ = false;
  }

  // Between the value emitter's entry and return, where its digit glyphs reach the icon emitter.
  [[nodiscard]] bool insideValue() const {
    return inValue_;
  }

private:
  std::uint32_t element_ = 0;
  bool inValue_ = false;
};

// Install the drawer and emitter overrides for this Core's resident SCUS_944.67 image.
void registerOverrides(Core &core);

} // namespace spyro3::hud_anchor