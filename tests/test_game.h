// test_game.h — a Game or Core that owns the runtime it is built around.
#pragma once

#include "core.h"
#include "core/runtime.h"
#include "core/session.h"
#include "game.h"
#include "stub_game_runtime.h"
#include "typed_config.h"

namespace spyro::test {

// Runtime::createSession with no frame driver, overrides or presentation of a title.
class SessionRuntime final : public Runtime {
public:
  SessionRuntime() : Runtime(Title::Spyro1) {}

  void registerOverrides(Game &game) override {
    (void)game;
  }
};

// Built before the Game or Core that keeps a reference to it.
template <class Runtime> class RuntimeSlot {
protected:
  Runtime ownedRuntime_;
};

template <class Runtime> class OwnedGame : private RuntimeSlot<Runtime>, public Game {
public:
  explicit OwnedGame(const psx::config::Config &config = {}) : Game(this->ownedRuntime_, config) {}
};

template <class Runtime> class OwnedCore : private RuntimeSlot<Runtime>, public Core {
public:
  OwnedCore() : Core(this->ownedRuntime_) {}
};

// A Game with no title, and one with the shared Spyro session.
using StubGame = OwnedGame<psx::test::StubGameRuntime>;
using SessionGame = OwnedGame<SessionRuntime>;
using StubCore = OwnedCore<psx::test::StubGameRuntime>;

} // namespace spyro::test
