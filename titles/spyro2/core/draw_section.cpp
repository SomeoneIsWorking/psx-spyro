#include "spyro2/core/draw_section.h"

#include "core.h"

#include <array>
#include <cstdint>
#include <optional>
#include <utility>

namespace spyro2 {
namespace {

// State 2 runs the hook 0x80066F88 names and the dispatcher with 0xC while the camera controller's
// sub-state 0x80067ED4 is positive; otherwise it draws as state 0.
constexpr std::uint32_t kCameraSubState = 0x80067ED4u;
constexpr std::uint32_t kStateHook = 0x80066F88u;
constexpr std::uint32_t kHookFlags = 0xCu;
constexpr std::uint32_t kPlay = 0x80013A14u;
// The switch's other cases but state 5, the loading screen: 0x800151F0 polls 0x800577A0 until its
// storybook pages load, so it is never replayed.
constexpr std::uint32_t kLoadingState = 5u;
constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 11> kHandlers{{
    {0u, kPlay},
    {1u, 0x80013BC4u},
    {3u, 0x80014504u},
    {4u, 0x800145ACu},
    {6u, 0x800153F8u},
    {7u, 0x80015430u},
    {8u, 0x80015494u},
    {9u, 0x800154F8u},
    {10u, 0x80015540u},
    {11u, 0x80015560u},
    {12u, 0x80015580u},
}};

std::optional<std::uint32_t> handlerOf(std::uint32_t state) {
  for (const auto &[handled, entry] : kHandlers) {
    if (handled == state) {
      return entry;
    }
  }
  return std::nullopt;
}

} // namespace

std::optional<std::string_view> DrawSection::declinesState(Core &, std::uint32_t state) const {
  if (state == 2u || handlerOf(state).has_value()) {
    return std::nullopt;
  }
  return state == kLoadingState ? "state 5 polls 0x800577A0 for its pages"
                                : "the switch has no replayed case";
}

bool DrawSection::drawState(Core &core,
                            psx::present::GuestRedraw &redraw,
                            std::uint32_t state) const {
  if (state == 2u) {
    if (static_cast<std::int32_t>(core.mem_r32(kCameraSubState)) > 0) {
      return step(redraw, core.mem_r32(kStateHook)) && dispatch(redraw, kHookFlags);
    }
    return step(redraw, kPlay);
  }
  const std::optional<std::uint32_t> entry = handlerOf(state);
  return entry && step(redraw, *entry);
}

} // namespace spyro2
