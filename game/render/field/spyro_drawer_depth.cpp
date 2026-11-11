#include "render/field/spyro_drawer_depth.h"

#include "core.h"
#include "gte.h"
#include "native_dispatch.h"

#include <algorithm>

namespace spyro::spyro_drawer_depth {
namespace {

constexpr std::uint32_t kDepthBias = 0x1E;
constexpr std::uint32_t kReflectionShift = 0x2C;

} // namespace

void draw(Core &core, const Facts &facts) {
  const bool slotOrder =
      core.mem_r32(facts.spyro + kReflectionShift) != 0 ||
      std::ranges::find(facts.unbinnedReturns, core.r[31]) != facts.unbinnedReturns.end();
  const auto bias = static_cast<std::int8_t>(core.mem_r8(facts.spyro + kDepthBias));
  const psx::cpu::DepthUnit unit(
      core.gte(),
      slotOrder ? 0.0f : guest_render_globals::kWorldDepthScale,
      slotOrder ? 0.0f : static_cast<float>(bias) * guest_render_globals::kDepthBinViewUnits);
  (void)psx::cpu::callOriginalOrPropagate(core, facts.entry);
}

void markDepth(Core &core, const Facts &facts) {
  core.depthTest.mark(facts.entry, psx::present::DepthOrder::Polygon, kBucketViewUnits);
}

} // namespace spyro::spyro_drawer_depth
