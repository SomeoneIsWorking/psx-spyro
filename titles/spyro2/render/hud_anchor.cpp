#include "spyro2/render/hud_anchor.h"

#include "core.h"
#include "native_dispatch.h"
#include "render/hud/ui_anchor.h"

namespace spyro2::hud_anchor {
namespace {

// FUN_8005251C(element, image, x, y): the counter's x is $a2.
void counterDrawer(Core *core) {
  if (const auto anchor = counterAnchor(core->r[4])) {
    spyro::ui_anchor::shiftArgument(*core, 6, {.name = "hud-counter", .index = core->r[4]}, *anchor);
  }
  (void)psx::cpu::callOriginalOrPropagate(*core, kCounterDrawer);
}

// FUN_800520CC(image, x, y, size): the emitted x is $a1.
void spriteEmitter(Core *core) {
  if (const auto anchor = emitterAnchor(core->r[31])) {
    spyro::ui_anchor::shiftArgument(*core, 5, {.name = "hud-meter", .index = core->r[31]}, *anchor);
  }
  (void)psx::cpu::callOriginalOrPropagate(*core, kSpriteEmitter);
}

} // namespace

void registerOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, kCounterDrawer, "spyro2-hud-counter", counterDrawer);
  psx::cpu::installNativeOverride(core, kSpriteEmitter, "spyro2-hud-sprite", spriteEmitter);
}

} // namespace spyro2::hud_anchor
