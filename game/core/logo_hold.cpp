#include "core/logo_hold.h"

#include "core.h"

#include <algorithm>
#include <lucent/log.h>

namespace spyro {

void LogoHoldSkip::observe(Core &core, psx::cpu::ResumableGuestCall &call, bool pressed) {
  press_.observe(pressed);
  const std::uint32_t resumePc = call.resumePc();
  const auto hold = std::find_if(facts_.holds.begin(),
                                 facts_.holds.end(),
                                 [resumePc](const LogoHold &h) { return h.waitReturn == resumePc; });
  if (hold == facts_.holds.end() || !press_.take()) {
    return;
  }
  const std::uint32_t start = core.mem_r32(facts_.vsyncCounter) - hold->fields;
  if (facts_.start.word != 0u) {
    core.mem_w32(facts_.start.word, start);
  } else {
    call.setSuspendedRegister(facts_.start.reg, start);
  }
  lucent::info("boot-native", "Start/Cross ends the logo hold at 0x{:08X}", resumePc);
}

} // namespace spyro
