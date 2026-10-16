#pragma once

#include "core/runtime.h"
#include "stock_libcd.h"
#include "stock_libgpu.h"
#include "stock_libgte.h"
#include "stock_vsync.h"

#include <memory>

namespace spyro2 {

// Runtime owner for SCUS_944.25. Its executable runs directly; the shared owners it uses are the
// field owner, the archive transfer and the stock libraries.
class Runtime final : public spyro::Runtime {
public:
  Runtime();

  void startProgram(Core &core) override;
  void registerOverrides(Game &game) override;
  std::unique_ptr<psx::game::FrameDriver> createFrameDriver(Game &game) override;
  void stockCdReadLanded(Core &core, const psx::cd::StockReadLanding &landing) override;
  std::optional<psx::gpu::PacketPoolExtent> packetPool(Core &core) const override;

  bool interpolates() const override;
  bool sealedFrameIsCut(Core &core) const override;
  // Where this disc's own logo is; see titles/spyro2/core/logo_facts.h.
  const spyro::TitleLogoFacts &logoFacts() const override;

protected:
  std::unique_ptr<spyro::Warp> createWarp() const override;

private:
  psx::hle::StockVSync vsync_;
  psx::hle::StockLibgpu libgpu_;
  psx::cd::GuestInterruptLibcd libcd_;
  psx::hle::StockLibgte libgte_;
};

} // namespace spyro2
