#include "core/debug/moby_teleport.h"

#include "core.h"
#include "render/field/guest_moby_visibility.h"
#include "render/field/moby_record.h"

#include <cstring>
#include <lucent/log.h>
#include <optional>

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

void MobyTeleport::list(Core &core, std::uint32_t mobyClass, std::FILE *reply) const {
  const std::vector<LevelMoby> mobys = ofClass(core, mobyClass);
  std::fprintf(reply, "class=%u mobys=%zu\n", mobyClass, mobys.size());
  for (std::size_t i = 0; i < mobys.size(); ++i) {
    std::fprintf(reply,
                 "moby=%zu address=0x%08x state=%u position=(%d, %d, %d)\n",
                 i,
                 mobys[i].address,
                 core.mem_r8(mobys[i].address + guest_moby::record::kFlag),
                 mobys[i].position.x,
                 mobys[i].position.y,
                 mobys[i].position.z);
  }
}

bool MobyTeleport::teleport(Core &core, std::uint32_t mobyClass, std::uint32_t index) const {
  if (!inActiveField(core)) {
    lucent::info("spyro-teleport", "moby-teleport refused: gameplay is not in an active field");
    return false;
  }
  const std::vector<LevelMoby> mobys = ofClass(core, mobyClass);
  if (index >= mobys.size()) {
    lucent::info("spyro-teleport",
                 "moby-teleport refused: the level has {} mobys of class {}",
                 mobys.size(),
                 mobyClass);
    return false;
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
  return true;
}

bool MobyTeleport::command(Core &core,
                           const psx::game::TitleCommandLine &input,
                           std::FILE *reply) const {
  if (std::strcmp(input.command, "mobys") == 0) {
    const std::optional<std::uint32_t> mobyClass = input.args.number(1);
    if (!mobyClass) {
      std::fprintf(reply, "usage: mobys <class>\n");
      return true;
    }
    list(core, *mobyClass, reply);
    return true;
  }
  if (std::strcmp(input.command, "moby-teleport") == 0) {
    const std::optional<std::uint32_t> mobyClass = input.args.number(1);
    const std::optional<std::uint32_t> index = input.args.number(2);
    if (!mobyClass || !index) {
      std::fprintf(reply, "usage: moby-teleport <class> <index>\n");
      return true;
    }
    std::fprintf(reply,
                 "moby-teleport %u %u: %s\n",
                 *mobyClass,
                 *index,
                 teleport(core, *mobyClass, *index) ? "ok" : "refused");
    return true;
  }
  return false;
}

} // namespace spyro
