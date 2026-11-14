#include "spyro2/render/title_strings.h"

#include "core.h"
#include "core/session.h"
#include "native_dispatch.h"
#include "spyro2/render/render_facts.h"

namespace spyro2::title_strings {
namespace {

// FUN_80079878(x, y, image, colour): the image is $a2.
void imageDrawer(Core *core) {
  const std::uint32_t image = core->r[6];
  const std::uint32_t packet = core->mem_r32(kRenderGlobals.primitiveCursor);
  if (!psx::cpu::callOriginalOrPropagate(*core, kImageDrawer)) {
    return;
  }
  if (image >= kFirstImage && image <= kLastImage) {
    spyro::session(*core).layers.linkedGlyph(packet + kSpriteOffset, kTitleStringFont);
  }
}

} // namespace

void landed(Core &core, const psx::code_module::Digest &content) {
  if (content.hex == kBootOverlaySha256) {
    (void)psx::cpu::tryInstallNativeOverride(
        core, kImageDrawer, "spyro2-title-strings", imageDrawer);
  }
}

} // namespace spyro2::title_strings
