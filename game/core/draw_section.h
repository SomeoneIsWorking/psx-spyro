// draw_section.h — the Spyro 2/3 per-field draw body less its device tail, as a replay section.
#pragma once

#include "replay_frame.h"

#include <cstdint>
#include <optional>
#include <span>

class Core;

namespace spyro {

struct DrawSectionFacts {
  std::uint32_t gate = 0; // the body returns at once while this word is set
  // The current-buffer word and the two 0x74-byte draw buffers it alternates between; each holds
  // its packet pool's start at +0x70.
  std::uint32_t currentBuffer = 0;
  std::uint32_t buffer0 = 0;
  std::uint32_t buffer1 = 0;
  std::uint32_t primitiveCursor = 0;
  std::uint32_t binMark = 0; // set to [orderingTable] + 0x1000
  std::uint32_t orderingTable = 0;
  std::uint32_t scratchEnd = 0; // set to the cursor plus [scratchSize]
  std::uint32_t scratchSize = 0;
  std::uint32_t cameraBuilder = 0; // (angles, view rotation, projection rotation)
  std::uint32_t cameraAngles = 0;
  std::uint32_t viewRotation = 0;
  std::uint32_t projectionRotation = 0;
  std::uint32_t positionCopy = 0; // (draw copy, camera position)
  std::uint32_t drawCameraPosition = 0;
  std::uint32_t cameraPosition = 0;
  std::uint32_t gameState = 0; // the switch operand
  std::uint32_t dispatcher = 0;
  std::uint32_t flatten = 0;
};

class DrawSection : public psx::present::ReplaySection {
public:
  inline static constexpr std::uint32_t kBufferBytes = 0x74u;
  inline static constexpr std::uint32_t kBufferPool = 0x70u;

  explicit DrawSection(const DrawSectionFacts &facts) : facts_(facts) {}

  // Whether this frame's body can be replayed: its gate is clear and its state's handler reaches
  // no device.
  [[nodiscard]] bool replays(Core &core) const;
  [[nodiscard]] std::uint32_t gameState(Core &core) const;

  [[nodiscard]] std::optional<std::uint32_t> draw(Core &core,
                                                  psx::present::GuestRedraw &redraw) const final;
  // The frame's other chain is PutDrawEnv's packet inside the two draw buffers.
  [[nodiscard]] bool linked(std::uint32_t head) const final;

protected:
  // Whether `state` has a handler that stays off the devices; any other state is never replayed.
  [[nodiscard]] virtual bool replaysState(std::uint32_t state) const = 0;
  // The state's handler, as the body's switch calls it; false when a step was refused.
  [[nodiscard]] virtual bool
  drawState(Core &core, psx::present::GuestRedraw &redraw, std::uint32_t state) const = 0;
  // The flatten and whatever the image links after it; the chain's head.
  [[nodiscard]] virtual std::optional<std::uint32_t>
  flattenFrame(Core &core, psx::present::GuestRedraw &redraw, std::uint32_t state) const;

  // `entry` called as the guest calls it, its override in force.
  static bool step(psx::present::GuestRedraw &redraw,
                   std::uint32_t entry,
                   std::span<const std::uint32_t> args = {});
  // The dispatcher with its flags.
  [[nodiscard]] bool dispatch(psx::present::GuestRedraw &redraw, std::uint32_t flags) const;
  // The flatten over every bin; its head.
  [[nodiscard]] std::optional<std::uint32_t> flatten(Core &core,
                                                     psx::present::GuestRedraw &redraw) const;

  [[nodiscard]] const DrawSectionFacts &facts() const {
    return facts_;
  }

private:
  const DrawSectionFacts &facts_;
};

} // namespace spyro
