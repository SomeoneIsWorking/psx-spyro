#include "core/debug/warp.h"

#include <charconv>
#include <lucent/log.h>
#include <string_view>

namespace spyro {
namespace {

constexpr std::string_view kSpace = " \t";

// The words after the command name.
std::string_view argumentsOf(std::string_view line) {
  const std::size_t name = line.find_first_not_of(kSpace);
  if (name == std::string_view::npos) {
    return {};
  }
  const std::size_t gap = line.find_first_of(kSpace, name);
  if (gap == std::string_view::npos) {
    return {};
  }
  const std::size_t first = line.find_first_not_of(kSpace, gap);
  if (first == std::string_view::npos) {
    return {};
  }
  const std::size_t last = line.find_last_not_of(kSpace);
  return line.substr(first, last - first + 1);
}

std::optional<std::uint32_t> decimal(std::string_view word) {
  if (word.empty()) {
    return std::nullopt;
  }
  std::uint32_t value = 0;
  const char *first = &word.front();
  const char *end = first + word.size();
  const auto [stop, error] = std::from_chars(first, end, value);
  if (error != std::errc{} || stop != end) {
    return std::nullopt;
  }
  return value;
}

} // namespace

std::string Warp::arm(const char *line) {
  const std::optional<std::uint32_t> level =
      decimal(argumentsOf(line != nullptr ? std::string_view(line) : std::string_view()));
  if (!level) {
    return "usage: warp <level-id>";
  }
  if (!levelExists(*level)) {
    return lucent::format("refused: level {} is not a level on this disc", *level);
  }
  level_ = level;
  return lucent::format("ok: warp armed for level {}; it starts from the title screen", *level);
}

void Warp::applyArmed(Core &core, std::uint32_t frame) {
  if (!level_ || !titleScreenIdle(core)) {
    return;
  }
  const std::uint32_t level = *level_;
  level_.reset();
  startLevel(core, level);
  lucent::info("warp", "level {} started from the title screen at f{}", level, frame);
}

} // namespace spyro
