#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

class Core;

namespace spyro {

// The debug menu's warp arrival. Armed between frames; the level is entered at the first frame
// boundary at which the title screen is idle or, where the title has one, gameplay takes a level
// change, each through the game's own path into the level. A change the game routes elsewhere
// (its own story scene, a homeworld reached through one of its levels) is followed by the next
// change from where gameplay resumes, until the level is reached; then the placement runs.
class Warp {
public:
  // Runs once `ready` holds, after the level is reached.
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

  // Arms `level`, then `placement`; a placement already ready skips the level entry. The returned
  // text answers the client.
  std::string arm(Core &core, std::uint32_t level, Placement placement = {});
  // At a frame boundary: enters the armed level once the title screen or gameplay takes it, then
  // the placement.
  void applyArmed(Core &core, std::uint32_t frame);

protected:
  // The title screen is idle and takes a level start.
  [[nodiscard]] virtual bool titleScreenIdle(Core &core) const = 0;
  // Enters the title's own path into `level`.
  virtual void startLevel(Core &core, std::uint32_t level) const = 0;
  // Gameplay is running and takes a level change now.
  [[nodiscard]] virtual bool levelChangeReady(Core &core) const = 0;
  // Enters the game's own level transition from the level being played toward `level`.
  virtual void changeLevel(Core &core, std::uint32_t level) const = 0;
  // Gameplay is in `level`. True for a title whose start and change always land where asked.
  [[nodiscard]] virtual bool playing(Core & /*core*/, std::uint32_t /*level*/) const {
    return true;
  }

private:
  // Changes in one warp before the route counts as lost: the longest route is a story scene, a
  // level of the homeworld, then its exit.
  static constexpr std::uint32_t kMaxChanges = 4;

  // Starts or changes toward the armed level, or ends the route once gameplay is in it.
  void enterArmed(Core &core, std::uint32_t frame);

  // The level still to enter; empty once reached.
  std::optional<std::uint32_t> level_;
  std::uint32_t target_ = 0;
  // A start or change was entered, and gameplay has been left since, so its return is a landing.
  bool issued_ = false;
  bool left_ = false;
  std::uint32_t changes_ = 0;
  Placement placement_;
};

} // namespace spyro
