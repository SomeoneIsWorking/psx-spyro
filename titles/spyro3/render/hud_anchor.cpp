#include "spyro3/render/hud_anchor.h"

#include "core.h"
#include "core/session.h"
#include "native_dispatch.h"
#include "render/hud/ui_anchor.h"

#include <cstdint>

namespace spyro3::hud_anchor {
namespace {

DrawState &drawState(Core &core) {
  return static_cast<DrawState &>(*spyro::session(core).titleState);
}

// The emitter's x ($a1) by the class of the widget being drawn, if it has one.
void shiftEmitterArgument(Core &core) {
  const std::uint32_t element = drawState(core).current();
  if (const auto anchor = counterAnchor(element)) {
    spyro::ui_anchor::shiftArgument(core, 5, {.name = "hud-counter", .index = element}, *anchor);
  }
}

// A widget drawer publishes its element for the emitters it calls and clears it after.
template <std::uint32_t Drawer> void widgetDrawer(Core *core) {
  drawState(*core).begin(core->r[4]);
  (void)psx::cpu::callOriginalOrPropagate(*core, Drawer);
  drawState(*core).end();
}

// The value emitter's digit glyphs come through here too and are not the widget's icon.
void iconEmitter(Core *core) {
  if (!drawState(*core).insideValue()) {
    shiftEmitterArgument(*core);
  }
  (void)psx::cpu::callOriginalOrPropagate(*core, kIconEmitter);
}

// The glyph xs it hands the icon emitter already carry this correction.
void valueEmitter(Core *core) {
  shiftEmitterArgument(*core);
  drawState(*core).beginValue();
  (void)psx::cpu::callOriginalOrPropagate(*core, kValueEmitter);
  drawState(*core).endValue();
}

} // namespace

void registerOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, kCounterDrawer, "spyro3-hud-counter", widgetDrawer<kCounterDrawer>);
  psx::cpu::installNativeOverride(core, kLivesDrawer, "spyro3-hud-lives", widgetDrawer<kLivesDrawer>);
  psx::cpu::installNativeOverride(core, kIconEmitter, "spyro3-hud-icon", iconEmitter);
  psx::cpu::installNativeOverride(core, kValueEmitter, "spyro3-hud-value", valueEmitter);
}

} // namespace spyro3::hud_anchor