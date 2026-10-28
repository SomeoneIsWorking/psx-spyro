// shaded_line.h — the shaded line func_8001844C(x0, y0, x1, y1) as a state producer: one LINE_G2
// whose endpoint greys follow D_800770C8.m_specularTime around the screen centre (the pause, flight,
// balloonist and inventory borders). Each call is keyed by its call site, its turn at that site in the
// logic frame and the inventory page, and saves its packet; an in-between draws the line with its
// endpoints and their colours moved between the two saves.
#pragma once

#include "core/session.h"
#include "state_producer.h"

#include <cstdint>
#include <utility>
#include <vector>

class Core;

namespace spyro1::shaded_line {

class ShadedLineProducer final : public psx::present::StateProducer {
public:
  static constexpr std::uint32_t kEntry = 0x8001844Cu;

  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;
  [[nodiscard]] Composition composition(const psx::present::StateInterval &states) const override;
};

// How many times each call site has drawn a line in the current logic frame.
class CallSites final : public spyro::TitleState {
public:
  // This call's turn at `site` in logic frame `frame`, from 0.
  std::uint32_t turn(std::uint32_t frame, std::uint32_t site);

private:
  std::uint32_t frame_ = 0;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> calls_; // site, calls so far
};

void registerOverrides(Core &core);

} // namespace spyro1::shaded_line
