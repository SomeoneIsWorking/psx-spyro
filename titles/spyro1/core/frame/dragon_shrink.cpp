#include "spyro1/core/frame/dragon_shrink.h"

#include "blend.h"
#include "core.h"
#include "render/terrain/guest_camera_builder.h"
#include "spyro1/core/guest_globals.h"

#include <cmath>

namespace spyro1 {
namespace {

constexpr std::uint32_t kShrinkState = 5u;
// g_SonyImage.u.m_Draw: unk_0x1600, set when the setup found a moby to draw, then the two scale
// vectors at unk_0x161C and unk_0x1622.
constexpr std::uint32_t kDrawsMoby = 0x800712F4u;
constexpr std::uint32_t kScale = 0x80071310u;
constexpr std::uint32_t kTurnScale = 0x80071316u;
// D_8006F3C0: 24 halfword factors, one a tick.
constexpr std::uint32_t kShrinkTable = 0x8006F3C0u;
constexpr float kTurnFromTick = 8.0F;
constexpr float kTurnPerTick = 42.0F;
constexpr auto kFineUnit = static_cast<float>(1u << spyro::guest_camera::kFineBits);

std::int32_t factorAt(Core &core, std::uint32_t ticks) {
  return static_cast<std::int16_t>(core.mem_r16(kShrinkTable + 2u * ticks));
}

// FIXED_MUL on each halfword of a vector: the signed product's low word shifted down 12.
void scaleVector(Core &core,
                 psx::present::GuestRedraw &redraw,
                 std::uint32_t at,
                 std::int32_t factor) {
  for (std::uint32_t half = 0; half < 3u; ++half) {
    const std::int32_t value = static_cast<std::int16_t>(core.mem_r16(at + 2u * half));
    redraw.writeHalf(at + 2u * half, static_cast<std::uint16_t>((value * factor) >> 12));
  }
}

} // namespace

std::optional<std::uint32_t> DragonShrink::ticksOf(Core &core) {
  if (core.mem_r32(guest::kGamestate) != guest::kGamestateDragon ||
      core.mem_r32(guest::kDragonCutsceneState) != kShrinkState) {
    return std::nullopt;
  }
  return core.mem_r32(guest::kDragonCutsceneTicks);
}

void DragonShrink::sample(Core &core) {
  from_ = to_;
  to_ = ticksOf(core);
}

psx::present::MotionCount DragonShrink::write(psx::present::GuestRedraw &, float) const {
  if (!to_) {
    return {};
  }
  return {.drawn = 1, .unpaired = from_ ? 0u : 1u};
}

void DragonShrink::scale(Core &core,
                         psx::present::GuestRedraw &redraw,
                         std::optional<float> t) const {
  if (core.mem_r32(kDrawsMoby) == 0) {
    return;
  }
  const std::uint32_t ticks = core.mem_r32(guest::kDragonCutsceneTicks);
  const bool paired = t && from_ && to_ && *to_ == ticks;
  const std::uint32_t before = paired ? *from_ : ticks;
  const float at = paired ? *t : 1.0F;
  scaleVector(core,
              redraw,
              kScale,
              psx::present::lerpInt(factorAt(core, before), factorAt(core, ticks), at));
  const float shown =
      static_cast<float>(before) + (static_cast<float>(ticks) - static_cast<float>(before)) * at;
  if (shown <= kTurnFromTick) {
    return;
  }
  const spyro::guest_camera::AngleTable cosine(core, guest::kCosTable);
  const auto angle =
      static_cast<std::uint32_t>(std::lround((shown - kTurnFromTick) * kTurnPerTick * kFineUnit));
  scaleVector(core, redraw, kTurnScale, cosine.atFine(angle));
}

} // namespace spyro1
