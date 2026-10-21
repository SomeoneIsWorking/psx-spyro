#pragma once

#include "core/boot_prefix_frame_driver.h"
#include "core/runtime.h"
#include "crt0_boot.h"

#include <memory>

namespace spyro {

// A title whose retail executable is its boot: crt0, then the shared boot-prefix driver.
class BootPrefixRuntime : public Runtime {
public:
  void startProgram(Core &core) final;
  std::unique_ptr<psx::game::FrameDriver> createFrameDriver(Game &game) final;
  // The loader streams its code modules through the stock read and then calls into them.
  void stockCdReadLanded(Core &core, const psx::cd::StockReadLanding &landing) final;
  bool sealedFrameIsCut(Core &core) const final;
  psx::state::NativeStatePort *nativeState(Core &core) const final;

protected:
  BootPrefixRuntime(const psx::game::Crt0Image &crt0, const BootPrefixFacts &facts);

private:
  psx::game::Crt0Image crt0_;
  const BootPrefixFacts &facts_;
};

} // namespace spyro
