#include "spyro1/render/hud_anchor.h"

#include "core.h"
#include "native_dispatch.h"
#include "spyro1/render/terrain_facts.h"

#include <array>

namespace spyro1::hud_anchor {
namespace {

// A POLY_FT4's four x halfwords.
constexpr std::array<std::uint32_t, 4> kQuadX = {0x08, 0x10, 0x18, 0x20};

void texturedQuad(Core *core) {
  const std::uint32_t packet = core->mem_r32(terrain::kRenderGlobals.primitiveCursor);
  const std::uint32_t returnAddress = core->r[31];
  if (!psx::cpu::callOriginalOrPropagate(*core, kTexturedQuad)) {
    return;
  }
  const std::optional<spyro::ui_anchor::Anchor> anchor = quadAnchor(returnAddress);
  if (!anchor) {
    return;
  }
  const std::int32_t shift =
      spyro::ui_anchor::correction(*core, {.name = "spyro1-hud-quad", .index = returnAddress}, *anchor);
  if (shift == 0) {
    return;
  }
  for (const std::uint32_t at : kQuadX) {
    const auto x = static_cast<std::int16_t>(core->mem_r16(packet + at));
    core->mem_w16(packet + at, static_cast<std::uint16_t>(x + shift));
  }
}

} // namespace

void registerOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, kTexturedQuad, "spyro1-hud-quad", texturedQuad);
}

} // namespace spyro1::hud_anchor
