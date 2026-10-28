// shaded_line.h — the shaded line func_8001844C(x0, y0, x1, y1) as a state producer: one LINE_G2
// whose endpoint greys follow D_800770C8.m_specularTime around the screen centre (the pause, flight
// and balloonist borders). Each call is keyed by its endpoints and saves its packet; an in-between
// draws the later packet with each endpoint's colour moved between the two saves.
#pragma once

#include "state_producer.h"

#include <cstdint>

class Core;

namespace spyro1::shaded_line {

class ShadedLineProducer final : public psx::present::StateProducer {
public:
  static constexpr std::uint32_t kEntry = 0x8001844Cu;

  void render(const psx::present::StateInterval &states,
              psx::present::PrimitiveSink &sink) const override;
  [[nodiscard]] Composition composition(const psx::present::StateInterval &states) const override;
};

void registerOverrides(Core &core);

} // namespace spyro1::shaded_line
