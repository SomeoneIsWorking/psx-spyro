#include "render/overlay_pair.h"

#include "core.h"
#include "core/session.h"
#include "render/field/guest_working_memory.h"

#include <algorithm>

namespace spyro::overlay_pair {

Appended
append(const psx::present::EmitMemory &memory, std::uint32_t pairWord, std::uint32_t packet) {
  const std::uint32_t pair = memory.mem_r32(pairWord);
  const std::uint32_t head = memory.mem_r32(pair);
  memory.mem_w32(pair, packet);
  if (head == 0) {
    memory.mem_w32(pair + 4, packet);
  } else {
    guest_memory::linkAfter(memory, head, packet);
  }
  return {.pair = pair, .head = head};
}

void link(Core &core, const Facts &facts) {
  const std::uint32_t packet = core.r[4];
  const Appended appended = append(psx::present::EmitMemory(core), facts.pairWord, packet);
  core.r[3] = appended.pair;
  core.r[1] = appended.head;
  if (appended.head != 0) {
    core.r[4] = packet >> 16;
  }
  if (std::ranges::find(facts.wholeScreenReturns, core.r[31]) == facts.wholeScreenReturns.end()) {
    session(core).cropLayers.linked(core, packet);
  }
}

void layText(Core &core, const Facts &facts, std::uint32_t entry) {
  const std::uint32_t first = core.mem_r32(facts.primitiveCursor);
  const std::uint32_t pair = core.mem_r32(facts.pairWord);
  const std::uint32_t headBefore = core.mem_r32(pair);
  (void)psx::cpu::callOriginalOrPropagate(core, entry);
  // A layout with nothing to draw links nothing.
  if (const std::uint32_t head = core.mem_r32(pair); head != headBefore) {
    session(core).cropLayers.linkedChain(core, psx::present::EmitMemory(core), first, head);
  }
}

void install(Core &core, const Facts &facts, psx::cpu::NativeFunction link) {
  psx::cpu::installNativeOverride(core, facts.entry, facts.overrideName, link);
}

void installText(Core &core, const TextLayout &layout, psx::cpu::NativeFunction override) {
  if (layout.entry != 0) {
    psx::cpu::installNativeOverride(core, layout.entry, layout.overrideName, override);
  }
}

} // namespace spyro::overlay_pair
