#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

class Core;

namespace spyro {

// The debug menu's warp arrival. Armed between frames; a level start is applied at the first frame
// boundary at which the title screen is idle, by handing the title its own path into the level; a
// placement after it runs once the level is playing.
class Warp {
public:
  // Runs once `ready` holds, after any level start.
  struct Placement {
    std::function<bool(Core &)> ready;
    std::function<std::string(Core &)> apply;
  };

  Warp() = default;
  virtual ~Warp() = default;
  Warp(const Warp &) = delete;
  Warp &operator=(const Warp &) = delete;
  Warp(Warp &&) = delete;
  Warp &operator=(Warp &&) = delete;

  // Arms `level` from the title screen, then `placement`; a placement already ready skips the level
  // start. The returned text answers the client.
  std::string arm(Core &core, std::uint32_t level, Placement placement = {});
  // At a frame boundary: starts the armed level once the title screen is idle, then the placement.
  void applyArmed(Core &core, std::uint32_t frame);

protected:
  // The title screen is idle and takes a level start.
  [[nodiscard]] virtual bool titleScreenIdle(Core &core) const = 0;
  // Enters the title's own path into `level`.
  virtual void startLevel(Core &core, std::uint32_t level) const = 0;

private:
  std::optional<std::uint32_t> level_;
  Placement placement_;
};

} // namespace spyro
