#include "core/session.h"

#include "core.h"
#include "game.h"

#include <cstdlib>
#include <lucent/log.h>

namespace spyro {
namespace {

Session &requireSession(const Core &core) {
  if (core.game == nullptr || core.game->sessionState == nullptr) {
    lucent::error("runtime", "FATAL: per-Core Spyro session missing");
    std::abort();
  }
  return static_cast<Session &>(*core.game->sessionState);
}

} // namespace

Session &session(Core &core) {
  return requireSession(core);
}

const Session &session(const Core &core) {
  return requireSession(core);
}

} // namespace spyro
