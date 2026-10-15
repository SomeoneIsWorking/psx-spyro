#include "spyro1_hud_anchor.h"

#include "core.h"
#include "execution_control.h"
#include "native_dispatch.h"
#include "native_execution.h"
#include "spyro1_terrain_facts.h"

#include <array>

namespace spyro1::hud_anchor {
namespace {

// A POLY_FT4's four x halfwords.
constexpr std::array<std::uint32_t, 4> kQuadX = {0x08, 0x10, 0x18, 0x20};

void texturedQuad(Core *core) {
  const std::uint32_t packet = core->mem_r32(terrain::kRenderGlobals.primitiveCursor);
  const std::uint32_t returnAddress = core->r[31];
  if (!psx::cpu::completeOrPropagate(
          *core,
          psx::cpu::callOriginal(
              *core, kTexturedQuad, psx::cpu::ExecutionBudget::currentTurn(*core)))) {
    return;
  }
  const std::optional<spyro::ui_anchor::Anchor> anchor = quadAnchor(returnAddress);
  if (!anchor) {
    return;
  }
  const std::int32_t shift =
      columns(*core, {.name = "spyro1-hud-quad", .index = returnAddress}, *anchor);
  if (shift == 0) {
    return;
  }
  for (const std::uint32_t at : kQuadX) {
    const auto x = static_cast<std::int16_t>(core->mem_r16(packet + at));
    core->mem_w16(packet + at, static_cast<std::uint16_t>(x + shift));
  }
}

} // namespace

std::int32_t columns(const Core &core,
                     const spyro::ui_anchor::Element &element,
                     spyro::ui_anchor::Anchor anchor) {
  const spyro::ui_anchor::Frame frame = spyro::ui_anchor::widescreenFrame(
      core, static_cast<std::uint32_t>(terrain::kSharedFacts.nativeWidth));
  return spyro::ui_anchor::correctionAndReport(element, anchor, frame);
}

void registerOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, kTexturedQuad, "spyro1-hud-quad", texturedQuad);
}

} // namespace spyro1::hud_anchor
