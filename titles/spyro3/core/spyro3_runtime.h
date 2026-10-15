#pragma once

#include "spyro_runtime.h"
#include "stock_libcd.h"
#include "stock_libgte.h"
#include "stock_vsync.h"

namespace spyro3 {

// Runtime owner for SCUS_944.67. Its executable runs directly; the shared owners it uses are the
// field owner, the archive transfer and the stock libraries.
class Spyro3Runtime final : public spyro::SpyroRuntime {
public:
  Spyro3Runtime();

  void startProgram(Core &core) override;
  void registerOverrides(Game &game) override;
  std::unique_ptr<psx::game::FrameDriver> createFrameDriver(Game &game) override;
  void stockCdReadLanded(Core &core, const psx::cd::StockReadLanding &landing) override;

  bool interpolates() const override;
  RenderCapabilities renderCapabilities() const override;
  bool guestVramIsPicture(const Game &game) const override;
  bool sealedFrameIsCut(Core &core) const override;
  // Where this disc's own logo is, once measured; see spyro3_logo_facts.h.
  const spyro::TitleLogoFacts &logoFacts() const override;

private:
  psx::hle::StockVSync vsync_;
  psx::cd::GuestInterruptLibcd libcd_;
  psx::hle::StockLibgte libgte_;
};

} // namespace spyro3
