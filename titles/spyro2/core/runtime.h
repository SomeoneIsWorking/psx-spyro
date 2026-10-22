#pragma once

#include "core/boot_prefix_runtime.h"
#include "spyro2/core/moby_teleport.h"
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
  std::optional<psx::gpu::PacketPoolExtent> packetPool(Core &core) const override;
  // `warp`, then `mobys` and `moby-teleport`.
  bool
  titleCommand(Core &core, const psx::game::TitleCommandLine &input, std::FILE *reply) override;

protected:
  std::unique_ptr<spyro::Warp> createWarp() const override;

private:
  psx::hle::StockVSync vsync_;
  psx::hle::StockLibgpu libgpu_;
  psx::cd::GuestInterruptLibcd libcd_;
  psx::hle::StockLibgte libgte_;
  MobyTeleport mobyTeleport_;
};

} // namespace spyro2
