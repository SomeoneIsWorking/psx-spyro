#include "render/screen_overlays.h"

#include "core.h"
#include "emit_memory.h"
#include "render/field/guest_packet.h"
#include "replay_frame.h"

#include <array>
#include <optional>

namespace spyro::screen_overlays {
namespace {

constexpr std::uint32_t kBarBytes = guest_packet::kF4Bytes;
// The bars' height `t` of the way from `before` to `after`.
float barHeight(std::int32_t before, std::int32_t after, float t) {
  return static_cast<float>(before) + static_cast<float>(after - before) * t;
}

// A corner on a bar's moving edge, `y` down the screen.
psx::gte::ProjectedVertex edge(std::uint32_t x, float y) {
  return psx::gte::ProjectedVertex::nearest({static_cast<float>(x), y});
}

// The two bar packets the body linked from `first`: each one's last two corners (+0x10, +0x14) are
// its moving edge, kept as the body wrote them and placed `height` rows in.
void placeEdges(Core &core, const LetterboxFacts &facts, std::uint32_t first, float height) {
  const psx::present::EmitMemory memory(core);
  const std::array<float, 2> rows{static_cast<float>(facts.top) + height,
                                  static_cast<float>(facts.bottom) - height};
  for (std::uint32_t bar = 0; bar < 2; ++bar) {
    for (std::uint32_t corner = 0; corner < 2; ++corner) {
      const std::uint32_t address = first + bar * kBarBytes + 0x10u + 4u * corner;
      const std::uint32_t word = memory.mem_r32(address);
      memory.storeVertex(address, {word, edge(word & 0xFFFFu, rows[bar]).place()});
    }
  }
}

} // namespace

bool drawLetterbox(Core &core, const LetterboxFacts &facts) {
  const std::uint32_t first = core.mem_r32(facts.primitiveCursor);
  const auto read = static_cast<std::int32_t>(core.mem_r32(facts.height));
  if (!psx::cpu::callOriginalOrPropagate(core, facts.entry)) {
    return false;
  }
  const auto drawn = static_cast<std::int32_t>(core.mem_r32(facts.height));
  if (drawn == 0) {
    return true;
  }
  const psx::present::ReplayFrame *replay = core.runtime.replayFrame(core);
  if (const std::optional<float> t = replay != nullptr ? replay->replaying() : std::nullopt;
      facts.rowPerDraw && t && read != drawn) {
    placeEdges(core, facts, first, barHeight(read, drawn, *t));
  }
  return true;
}

void installLetterbox(Core &core, const LetterboxFacts &facts, psx::cpu::NativeFunction draw) {
  psx::cpu::installNativeOverride(core, facts.entry, facts.overrideName, draw);
}

} // namespace spyro::screen_overlays
