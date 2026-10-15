// booted_title.h — a Spyro title's Game admitted through the shipping path with its overrides
// registered.
#pragma once

#include "core.h"
#include "core/title_runtime_registry.h"
#include "game.h"
#include "psx_exe_image.h"
#include "typed_config.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

namespace spyro::test {

// A PS-X EXE with the catalog entry's header fields and zeroed text.
inline std::vector<std::uint8_t> syntheticExecutable(const psx::host::TitleIdentity &identity) {
  std::vector<std::uint8_t> bytes(psx::cpu::kPsxExeHeaderBytes + identity.textSize);
  const auto word = [&bytes](std::size_t offset, std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index) {
      bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8u));
    }
  };
  const char magic[] = "PS-X EXE";
  for (std::size_t index = 0; index < 8; ++index) {
    bytes[index] = static_cast<std::uint8_t>(magic[index]);
  }
  word(0x10, identity.entry);
  word(0x14, identity.globalPointer);
  word(0x18, identity.textAddress);
  word(0x1C, identity.textSize);
  word(0x30, identity.stackAddress);
  word(0x34, identity.stackOffset);
  return bytes;
}

class BootedTitle {
public:
  // `registered` false leaves every guest address to guest code, for a fixture that plants its own.
  explicit BootedTitle(Title title, bool registered = true) : registered_(registered) {
    for (std::size_t index = 0; index < catalog_.titles().size(); ++index) {
      if (catalog_.title(index) == title) {
        index_ = index;
      }
    }
    admit();
  }

  // A new Game for the same runtime, as the picker starts a title again.
  void restart() {
    game_.reset();
    admit();
  }

  Runtime &runtime() const {
    return catalog_.runtime(index_);
  }
  Game &game() {
    return *game_;
  }
  Core &core() {
    return game_->core;
  }

private:
  void admit() {
    game_ = std::make_unique<Game>(runtime(), psx::config::Config{});
    if (!runtime().admitExecutable(game_->core, syntheticExecutable(catalog_.titles()[index_]))) {
      throw std::runtime_error("the title refused its synthetic executable");
    }
    if (registered_) {
      runtime().registerOverrides(*game_);
    }
  }

  Catalog catalog_;
  bool registered_ = true;
  std::size_t index_ = 0;
  std::unique_ptr<Game> game_;
};

} // namespace spyro::test
