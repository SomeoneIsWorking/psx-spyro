#pragma once

#include "core/boot_prefix_runtime.h"
#include "stock_libcd.h"
#include "stock_libgte.h"
#include "stock_vsync.h"

namespace spyro3 {

// Runtime owner for SCUS_944.67. Its executable runs directly; the shared owners it uses are the
// field owner, the archive transfer and the stock libraries.
class Runtime final : public spyro::BootPrefixRuntime {
public:
  Runtime();

  void registerOverrides(Game &game) override;

protected:
  void codeLanded(Core &core,
                  const psx::cd::StockReadLanding &landing,
                  const psx::code_module::Digest &content) override;
  std::unique_ptr<spyro::Warp> createWarp() const override;
  std::unique_ptr<spyro::TitleState> createTitleState() const override;

private:
  psx::hle::StockVSync vsync_;
  psx::cd::GuestInterruptLibcd libcd_;
  psx::hle::StockLibgte libgte_;
};

} // namespace spyro3
