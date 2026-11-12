#include "core/debug/portal_entry.h"

#include "core.h"

namespace spyro {
namespace {

constexpr std::uint8_t kPortalTrigger = 6u;
constexpr std::uint32_t kPortalRecordSize = 0x44u;
constexpr std::uint32_t kPortalDestination = 0x42u;

} // namespace

std::optional<std::uint32_t> portalInto(Core &core, const PortalFacts &facts, std::uint32_t level) {
  const std::uint32_t table = core.mem_r32(facts.triggerTable);
  const std::uint32_t portals = core.mem_r32(facts.portalTable);
  if (table == 0u || portals == 0u) {
    return std::nullopt;
  }
  const std::uint32_t records = core.mem_r32(table);
  for (std::uint32_t slot = table; slot < records; slot += 4u) {
    const std::uint32_t record = core.mem_r32(slot);
    if (core.mem_r8(record) != kPortalTrigger) {
      continue;
    }
    const std::uint32_t index = core.mem_r32(record + 4u);
    if (core.mem_r8(portals + index * kPortalRecordSize + kPortalDestination) == level) {
      return index;
    }
  }
  return std::nullopt;
}

} // namespace spyro
