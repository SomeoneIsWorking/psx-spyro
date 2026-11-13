#pragma once

#include "core/boot_prefix_runtime.h"
#include "spyro2/core/debug_menu.h"
#include "spyro2/core/draw_section.h"
#include "stock_libcd.h"
#include "stock_libgpu.h"
#include "stock_libgte.h"
#include "stock_vsync.h"

#include <memory>

namespace spyro2 {

// Runtime owner for SCUS_944.25. Its executable runs directly; the shared owners it uses are the
// field owner, the archive transfer and the stock libraries.
class Runtime final : public spyro::BootPrefixRuntime {
public:
  Runtime();

  void registerOverrides(Game &game) override;
  const spyro::FollowCameraFacts &followCamera() const override;
  std::optional<psx::gpu::PacketPoolExtent> packetPool(Core &core) const override;
  const spyro::TitleLogoFacts &logoFacts() const override;
  // The 2D font (render/text_font.h).
  const psx::present::OutlineFont *outlineFont() const override;
  const psx::game::TitleDebugMenu &debugMenu() const override {
    return debugMenu_;
  }

protected:
  const spyro::DrawSection &drawSection() const override {
    return section_;
  }
  const spyro::DrawMotionFacts &drawMotion() const override;
  std::unique_ptr<spyro::Warp> createWarp() const override;

private:
  DrawSection section_;
  psx::hle::StockVSync vsync_;
  psx::hle::StockLibgpu libgpu_;
  psx::cd::GuestInterruptLibcd libcd_;
  psx::hle::StockLibgte libgte_;
  DebugMenu debugMenu_;
};

} // namespace spyro2
