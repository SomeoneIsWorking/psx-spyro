#pragma once

#include <cstdint>
#include <optional>
#include <string>

class Core;

namespace spyro {

// The control-channel `warp <level>` request. Armed between frames; applied at the first frame
// boundary at which the title screen is idle, by handing the title its own path into a level with
// the requested level id.
class Warp {
public:
  Warp() = default;
  virtual ~Warp() = default;
  Warp(const Warp &) = delete;
  Warp &operator=(const Warp &) = delete;
  Warp(Warp &&) = delete;
  Warp &operator=(Warp &&) = delete;

  // Parses one `warp <level>` line and arms it; the returned text answers the client.
  std::string arm(const char *line);
  // At a frame boundary: starts the armed level once the title screen is idle.
  void applyArmed(Core &core, std::uint32_t frame);

  [[nodiscard]] std::optional<std::uint32_t> armedLevel() const {
    return level_;
  }

protected:
  // The guest's level id names a level on this disc.
  [[nodiscard]] virtual bool levelExists(std::uint32_t level) const = 0;
  // The title screen is idle and takes a level start.
  [[nodiscard]] virtual bool titleScreenIdle(Core &core) const = 0;
  // Enters the title's own path into `level`.
  virtual void startLevel(Core &core, std::uint32_t level) const = 0;

private:
  std::optional<std::uint32_t> level_;
};

} // namespace spyro
