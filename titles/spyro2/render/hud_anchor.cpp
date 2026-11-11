#include "spyro2/render/hud_anchor.h"

#include "core.h"
#include "core/boot_prefix_frame_driver.h"
#include "game.h"
#include "native_dispatch.h"
#include "render/hud/ui_anchor.h"
#include "spyro2/render/hud_element.h"

namespace spyro2::hud_anchor {
namespace {

// FUN_8005251C(element, image, x, y): the counter's x is $a2.
void counterDrawer(Core *core) {
  if (const auto anchor = counterAnchor(core->r[4])) {
    spyro::ui_anchor::shiftArgument(
        *core, 6, {.name = "hud-counter", .index = core->r[4]}, *anchor);
  }
  (void)psx::cpu::callOriginalOrPropagate(*core, kCounterDrawer);
}

// FUN_800520CC(image, x, y, size): the emitted x is $a1; its y lies where the caller's slide put
// it.
void spriteEmitter(Core *core) {
  if (const auto anchor = emitterAnchor(core->r[31])) {
    spyro::ui_anchor::shiftArgument(*core, 5, {.name = "hud-meter", .index = core->r[31]}, *anchor);
  }
  spyro::HudSlide &slide =
      core->game->frameDriverAs<spyro::BootPrefixFrameDriver>().motion().hudSlide();
  hud_element::draw(*core, slide.takeRemainder(*core));
}

} // namespace

void registerOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, kCounterDrawer, "spyro2-hud-counter", counterDrawer);
  psx::cpu::installNativeOverride(core, kSpriteEmitter, "spyro2-hud-sprite", spriteEmitter);
}

} // namespace spyro2::hud_anchor
