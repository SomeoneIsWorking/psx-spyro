#include "spyro_session.h"

#include "core.h"
#include "game.h"

#include <cstdlib>
#include <lucent/log.h>

namespace spyro {
namespace {

SpyroSession &requireSession(const Core &core) {
  if (core.game == nullptr || core.game->sessionState == nullptr) {
    lucent::error("runtime", "FATAL: per-Core Spyro session missing");
    std::abort();
  }
  return static_cast<SpyroSession &>(*core.game->sessionState);
}

} // namespace

SpyroSession &session(Core &core) {
  return requireSession(core);
}

const SpyroSession &session(const Core &core) {
  return requireSession(core);
}

} // namespace spyro
