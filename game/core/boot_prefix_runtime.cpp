#include "core/boot_prefix_runtime.h"

#include "core.h"
#include "game.h"
#include "guest_code_module.h"

namespace spyro {

BootPrefixRuntime::BootPrefixRuntime(const psx::game::Crt0Image &crt0, const BootPrefixFacts &facts)
    : crt0_(crt0), facts_(facts) {}

void BootPrefixRuntime::startProgram(Core &core) {
  psx::game::Crt0Boot{crt0_}.run(core);
  core.game->frameDriverAs<BootPrefixFrameDriver>().initialize();
}

std::unique_ptr<psx::game::FrameDriver> BootPrefixRuntime::createFrameDriver(Game &game) {
  return std::make_unique<BootPrefixFrameDriver>(game, facts_);
}

void BootPrefixRuntime::stockCdReadLanded(Core &core, const psx::cd::StockReadLanding &landing) {
  psx::code_module::publishStockReadLanding(core, landing);
}

bool BootPrefixRuntime::sealedFrameIsCut(Core &core) const {
  // A session's first record, sealed before its driver exists, is a cut.
  return core.game->frameDriver == nullptr || core.game->frameDriverAs<BootPrefixFrameDriver>().cut().isCut();
}

psx::state::NativeStatePort *BootPrefixRuntime::nativeState(Core &core) const {
  return &core.game->frameDriverAs<BootPrefixFrameDriver>();
}

} // namespace spyro
