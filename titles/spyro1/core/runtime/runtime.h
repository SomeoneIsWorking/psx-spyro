#pragma once

#include "core/runtime.h"
#include "spyro1/core/debug/debug_menu.h"
#include "spyro1/core/input/input_phase.h"
#include "stock_libcd.h"
#include "stock_libgpu.h"
#include "stock_libgte.h"
#include "stock_pad_buffers.h"
#include "stock_vsync.h"

namespace spyro1 {

// Runtime owner for SCUS_942.28.
class Runtime final : public spyro::Runtime {
public:
  Runtime();

  void startProgram(Core &core) override;
  void registerOverrides(Game &game) override;
  std::unique_ptr<psx::game::FrameDriver> createFrameDriver(Game &game) override;
  void deliverPad(Core &core, const psx::input::PadDelivery &delivery) override;

  std::optional<psx::gpu::PacketPoolExtent> packetPool(Core &core) const override;
  // The boot logos are uploads at the native width; no widening applies to them.
  bool guestPictureIsNativeWidth(const Game &game) const override;
  bool sealedFrameIsCut(Core &core) const override;
  // Where this disc's own logo is; see titles/spyro1/core/frame/logo_facts.h.
  const spyro::TitleLogoFacts &logoFacts() const override;

  const psx::game::TitleDebugMenu &debugMenu() const override {
    return debugMenu_;
  }
  // The phase pad recordings are keyed on, so a press keeps its offset into its screen (issue
  // 0116).
  std::uint64_t inputPhase(Core &core) const override;

protected:
  std::unique_ptr<spyro::Warp> createWarp() const override;

private:
  psx::hle::StockVSync vsync_;
  psx::hle::StockLibgpu libgpu_;
  psx::hle::StockLibgte libgte_;
  psx::cd::StockLibcd libcd_;
  psx::input::StockPadBuffers pad_;
  InputPhase inputPhase_;
  DebugMenu debugMenu_;
};

} // namespace spyro1
