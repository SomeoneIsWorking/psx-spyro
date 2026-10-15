#pragma once

#include "spyro1_input_phase.h"
#include "spyro_runtime.h"
#include "stock_libcd.h"
#include "stock_libgpu.h"
#include "stock_libgte.h"
#include "stock_pad_buffers.h"
#include "stock_vsync.h"

namespace spyro1 {

// Runtime owner for SCUS_942.28.
class Spyro1Runtime final : public spyro::SpyroRuntime {
public:
  Spyro1Runtime();

  void startProgram(Core &core) override;
  void registerOverrides(Game &game) override;
  std::unique_ptr<psx::game::FrameDriver> createFrameDriver(Game &game) override;
  void deliverPad(Core &core, const psx::input::PadDelivery &delivery) override;

  bool interpolates() const override;
  RenderCapabilities renderCapabilities() const override;
  bool guestVramIsPicture(const Game &game) const override;
  // The boot logos are uploads at the native width; no widening applies to them.
  bool guestPictureIsNativeWidth(const Game &game) const override;
  bool sealedFrameIsCut(Core &core) const override;
  // Where this disc's own logo is; see spyro1_logo_facts.h.
  const spyro::TitleLogoFacts &logoFacts() const override;

  // `gates` and `gate-teleport`, the route to a level portal.
  bool
  titleCommand(Core &core, const psx::game::TitleCommandLine &input, std::FILE *reply) override;
  // The phase pad recordings are keyed on, so a press keeps its offset into its screen (issue
  // 0116).
  std::uint64_t inputPhase(Core &core) const override;

private:
  psx::hle::StockVSync vsync_;
  psx::hle::StockLibgpu libgpu_;
  psx::hle::StockLibgte libgte_;
  psx::cd::StockLibcd libcd_;
  psx::input::StockPadBuffers pad_;
  InputPhase inputPhase_;
};

} // namespace spyro1
