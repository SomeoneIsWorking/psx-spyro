// draw_section.h — GamestateDraw 0x8001ED5C's GS_Playing section as a replay: the buffer flip it
// opens with, CameraUpdateMatrices and its drawers in order, and the bin flatten whose chain
// DrawOTag submits.
#pragma once

#include "motion_set.h"
#include "replay_frame.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace spyro1 {

class DrawSection final : public psx::present::ReplaySection {
public:
  [[nodiscard]] std::optional<std::uint32_t> draw(Core &core,
                                                  psx::present::GuestRedraw &redraw) const override;
  [[nodiscard]] bool linked(std::uint32_t head) const override;
};

// Spyro's body, head and tail poses (g_Spyro +0x18..+0x27): each part's animation pair and progress
// step one unit, blended as keyframe_blend does for the guest's 16-step weight.
class SpyroPoses final : public psx::present::MotionRule {
public:
  void blend(const Samples &samples, float t, std::span<std::uint32_t> out) const override;
};

// What the section reads that moves between logic frames.
class DrawMotion {
public:
  [[nodiscard]] std::span<const psx::present::MotionTrack> tracks() const {
    return tracks_;
  }

private:
  // g_Camera: m_Position, then m_Rotation's three 12-bit halfwords.
  static constexpr std::array<psx::present::MotionField, 5> kCamera{{
      {0x28u, psx::present::Motion::Linear},
      {0x2Cu, psx::present::Motion::Linear},
      {0x30u, psx::present::Motion::Linear},
      {0x4Cu, psx::present::Motion::Angle12x2},
      {0x50u, psx::present::Motion::Angle12Low},
  }};
  // g_Spyro: m_Position, then the body, head and tail rotations, three 8-bit angles and a pad byte
  // each.
  static constexpr std::array<psx::present::MotionField, 6> kSpyro{{
      {0x00u, psx::present::Motion::Linear},
      {0x04u, psx::present::Motion::Linear},
      {0x08u, psx::present::Motion::Linear},
      {0x0Cu, psx::present::Motion::Angle8x3},
      {0x10u, psx::present::Motion::Angle8x3},
      {0x14u, psx::present::Motion::Angle8x3},
  }};
  static constexpr std::array<psx::present::MotionField, 4> kSpyroPoses{{
      {0x18u, psx::present::Motion::Step},
      {0x1Cu, psx::present::Motion::Step},
      {0x20u, psx::present::Motion::Step},
      {0x24u, psx::present::Motion::Step},
  }};
  SpyroPoses poses_;
  std::array<psx::present::MotionTrack, 3> tracks_{{
      {"camera", 0x80076DD0u, kCamera},
      {"spyro", 0x80078A58u, kSpyro},
      {"spyro poses", 0x80078A58u, kSpyroPoses, &poses_},
  }};
};

} // namespace spyro1
