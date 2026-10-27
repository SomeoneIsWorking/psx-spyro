#include "core/debug/moby_teleport.h"

#include "core.h"
#include "render/field/guest_moby_visibility.h"
#include "render/field/moby_record.h"

#include <lucent/log.h>

namespace spyro {

std::vector<MobyTeleport::LevelMoby> MobyTeleport::ofClass(Core &core,
                                                           std::uint32_t mobyClass) const {
  std::vector<LevelMoby> mobys;
  const std::uint32_t first = core.mem_r32(levelMobysWord());
  if (first == 0) {
    return mobys;
  }
  for (std::uint32_t moby = first; core.isGuestStorage(moby, moby_record::kBytes);
       moby += moby_record::kBytes) {
    if (static_cast<std::int8_t>(core.mem_r8(moby + guest_moby::record::kFlag)) ==
        guest_moby::record::kEnd) {
      break;
    }
    if (core.mem_r16(moby + moby_record::kMesh) == mobyClass) {
      mobys.push_back(
          {.address = moby,
           .position = {static_cast<std::int32_t>(core.mem_r32(moby + moby_record::kX)),
                        static_cast<std::int32_t>(core.mem_r32(moby + moby_record::kY)),
                        static_cast<std::int32_t>(core.mem_r32(moby + moby_record::kZ))}});
    }
  }
  return mobys;
}

std::string MobyTeleport::list(Core &core, std::uint32_t mobyClass) const {
  const std::vector<LevelMoby> mobys = ofClass(core, mobyClass);
  std::string reply = lucent::format("class={} mobys={}", mobyClass, mobys.size());
  for (std::size_t i = 0; i < mobys.size(); ++i) {
    reply += lucent::format("\nmoby={} address={:#010x} state={} position=({}, {}, {})",
                            i,
                            mobys[i].address,
                            core.mem_r8(mobys[i].address + guest_moby::record::kFlag),
                            mobys[i].position.x,
                            mobys[i].position.y,
                            mobys[i].position.z);
  }
  return reply;
}

std::string MobyTeleport::teleport(Core &core, std::uint32_t mobyClass, std::uint32_t index) const {
  const std::vector<LevelMoby> mobys = ofClass(core, mobyClass);
  if (index >= mobys.size()) {
    return lucent::format("refused: the level has {} mobys of class {}", mobys.size(), mobyClass);
  }
  const LevelMoby &target = mobys[index];
  placeSpyro(core, target.position);
  lucent::info("spyro-teleport",
               "moby-teleport class={} index={} moby={:#010x} position=({}, {}, {})",
               mobyClass,
               index,
               target.address,
               target.position.x,
               target.position.y,
               target.position.z);
  return lucent::format("ok: Spyro on moby {:#010x}", target.address);
}

} // namespace spyro
