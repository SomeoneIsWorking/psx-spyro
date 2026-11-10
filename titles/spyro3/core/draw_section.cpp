#include "spyro3/core/draw_section.h"

#include "core.h"
#include "guest_redraw.h"

#include <array>
#include <cstdint>
#include <optional>
#include <utility>

namespace spyro3 {
namespace {

// State 2 runs the hook 0x80074380 names and the dispatcher with 0xC while the camera controller's
// sub-state 0x8006E048 is positive; otherwise it draws as state 0. State 0x12 runs the hook
// 0x80074488 names, then the dispatcher with 0x3D.
constexpr std::uint32_t kCameraSubState = 0x8006E048u;
constexpr std::uint32_t kStateHook = 0x80074380u;
constexpr std::uint32_t kHookFlags = 0xCu;
constexpr std::uint32_t kSceneHook = 0x80074488u;
constexpr std::uint32_t kSceneFlags = 0x3Du;
constexpr std::uint32_t kPlay = 0x8001D274u;
// The switch's other cases but state 5, the loading screen: 0x80059038 waits in VSync at 0x8005927C
// (UpdateLoadingImage), so it is never replayed. States 8 and 0x11 have no case.
constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 14> kHandlers{{
    {0u, kPlay},
    {0xEu, kPlay},
    {1u, 0x8001D424u},
    {3u, 0x8001DC5Cu},
    {4u, 0x8001DD1Cu},
    {6u, 0x8001E2A8u},
    {7u, 0x8001E2C8u},
    {9u, 0x8001E32Cu},
    {10u, 0x8001E374u},
    {0xBu, 0x80077CFCu}, // the title overlay's draw
    {0xCu, 0x8001E394u},
    {0xDu, 0x8001E3F8u},
    {0xFu, 0x8001DC3Cu},
    {0x10u, 0x8001E618u},
}};

// The second pass (after the device tail of 0x8001E638).
constexpr std::uint32_t kPauseState = 4u;
constexpr std::uint32_t kSceneState = 0xDu;
constexpr std::uint32_t kSparx = 0x8006C65Cu;      // Sparx's record, 0 while he is absent
constexpr std::uint32_t kSparxShown = 0x80070134u; // byte
constexpr std::uint32_t kSparxEnd = 0xA0u;         // the byte after his record, 0xFF ends the walk
constexpr std::uint32_t kSceneStep = 0x8006C4F8u;  // state 0xD draws the pass unless it is 2
constexpr std::uint32_t kMobyArray = 0x8006C550u;
constexpr std::uint32_t kChainTail = 0x8006C6B0u;
constexpr std::int32_t kPassScratch = 0xA00;
constexpr std::uint32_t kDrawerScratchBelow = 0x3000u;
constexpr std::uint32_t kClearScratch = 0x8004E7ACu; // (address, 0, words)
constexpr std::uint32_t kClearWords = 0xC00u;
constexpr std::array<std::uint32_t, 2> kSparxDrawers{0x80030478u, 0x80031124u};
constexpr std::array<std::uint32_t, 1> kSceneDrawers{0x8003CDA0u};

// State 3's handler 0x8001DC5C, in steps 0 and 4 of 0x8006FBC8 with the fade 0x8006C598 at 0xFF,
// first clears both buffers through 0x8001EBAC (DrawSync, VSync, ClearImage), a device wait.
constexpr std::uint32_t kLevelLoadState = 3u;
constexpr std::uint32_t kLevelLoadStep = 0x8006FBC8u;
constexpr std::uint32_t kFade = 0x8006C598u;
constexpr std::uint32_t kFadeBlack = 0xFFu;

bool clearsScreen(Core &core) {
  const std::uint32_t step = core.mem_r32(kLevelLoadStep);
  return (step == 0u || step == 4u) && core.mem_r32(kFade) == kFadeBlack;
}

std::optional<std::uint32_t> handlerOf(std::uint32_t state) {
  for (const auto &[handled, entry] : kHandlers) {
    if (handled == state) {
      return entry;
    }
  }
  return std::nullopt;
}

} // namespace

bool DrawSection::replaysState(Core &core, std::uint32_t state) const {
  if (state == kLevelLoadState && clearsScreen(core)) {
    return false;
  }
  return state == 2u || state == 0x12u || handlerOf(state).has_value();
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
  if (state == 0x12u) {
    return step(redraw, core.mem_r32(kSceneHook)) && dispatch(redraw, kSceneFlags);
  }
  const std::optional<std::uint32_t> entry = handlerOf(state);
  return entry && step(redraw, *entry);
}

bool DrawSection::secondPass(Core &core,
                             psx::present::GuestRedraw &redraw,
                             std::span<const std::uint32_t> drawers) const {
  const spyro::DrawSectionFacts &image = facts();
  const std::uint32_t tail = core.mem_r32(kChainTail);
  const std::uint32_t scratch = core.mem_r32(image.scratchEnd);
  const std::uint32_t other =
      core.mem_r32(image.currentBuffer) == image.buffer0 ? image.buffer1 : image.buffer0;
  const std::uint32_t passEnd = core.mem_r32(other + kBufferPool) + core.mem_r32(image.scratchSize);
  redraw.writeWord(image.scratchEnd, passEnd);
  const std::array<std::uint32_t, 3> clear{passEnd - kDrawerScratchBelow, 0u, kClearWords};
  if (!step(redraw, kClearScratch, clear)) {
    return false;
  }
  for (const std::uint32_t drawer : drawers) {
    if (!step(redraw, drawer)) {
      return false;
    }
  }
  redraw.writeWord(image.scratchEnd, scratch);
  const std::optional<std::uint32_t> head = flatten(core, redraw);
  if (!head) {
    return false;
  }
  redraw.writeWord(tail, (core.mem_r32(tail) & 0xFF000000u) | (*head & 0x00FFFFFFu));
  return true;
}

std::optional<std::uint32_t> DrawSection::flattenFrame(Core &core,
                                                       psx::present::GuestRedraw &redraw,
                                                       std::uint32_t state) const {
  const spyro::DrawSectionFacts &image = facts();
  const std::uint32_t sparx = core.mem_r32(kSparx);
  const bool pause = state == kPauseState && sparx != 0 && core.mem_r8(kSparxShown) != 0 &&
                     static_cast<std::int32_t>(core.mem_r32(image.scratchEnd) -
                                               core.mem_r32(image.primitiveCursor)) > kPassScratch;
  const bool scene = !pause && state == kSceneState && core.mem_r32(kSceneStep) != 2u;
  const std::optional<std::uint32_t> head = flatten(core, redraw);
  if (!head || (!pause && !scene)) {
    return head;
  }
  if (pause) {
    redraw.writeWord(kMobyArray, sparx);
    redraw.writeByte(sparx + kSparxEnd, 0xFFu);
  }
  const std::span<const std::uint32_t> drawers =
      pause ? std::span<const std::uint32_t>(kSparxDrawers)
            : std::span<const std::uint32_t>(kSceneDrawers);
  if (!secondPass(core, redraw, drawers)) {
    return std::nullopt;
  }
  return head;
}

} // namespace spyro3
