#pragma once

#include <cstdint>
#include <optional>

class Core;

namespace spyro {

// S2/S3 level triggers and portals: a pointer list of trigger records followed by the records it
// points at; a type-6 trigger enters the portal its next word indexes, whose byte 0x42 is where
// it leads from a homeworld.
struct PortalFacts {
  std::uint32_t triggerTable = 0;
  std::uint32_t portalTable = 0;
};

// The index of the current level's portal into `level`, as its trigger names it.
[[nodiscard]] std::optional<std::uint32_t>
portalInto(Core &core, const PortalFacts &facts, std::uint32_t level);

} // namespace spyro
