#include "spyro1/core/audio/spu_library.h"

#include "core/native_execution.h"

#include <cstdint>

namespace spyro1::native {
namespace {


constexpr std::uint32_t kReturnRoundedPitch = 0xFFFFFFFEu;
constexpr std::uint32_t kReturnShiftedPitch = 0xFFFFFFFFu;

// 0x8005C588 — libspu SpuSetVoicePitch: give one voice a pitch, or hand the caller the number the
// SPU register needs. The pitch is first rounded UP to a whole multiple of the format's step and
// then divided down by the format's shift, and the divided value is what a voice slot records — so
// the step and the shift are the format's own resolution, and the guest's own reader at 0x8005C62C
// shifts the stored halfword back up to recover the pitch.
//
// The four format words are written once by SpuInit (0x8005BCE0..0x8005BD04) and already hold
// 2/3/8/7 in the shipped image's data, so the rounding below is live and the step is never zero.
// A zero step would make the retail body take `break 7` after its `divu`; the guard here keeps that
// divide unreachable rather than trapping, which is the same state the guest never reaches.
//
// The voice argument is captured into $a2 by the `move a2, a0` that is the DELAY SLOT of the
// rounding guard's own branch, so it survives $a0 being reloaded with the step, and it is that
// captured word both exit tests read. Three delay slots then leave v0 on three different exits, and
// the arms are not interchangeable:
//   voice == -2  returns the ROUNDED pitch, unmasked, and stores nothing;
//   voice == -1  returns the SHIFTED pitch masked to 16 bits, and stores nothing — the caller at
//                0x8005CB44 puts that straight into the register bit-field at 0x8007356C;
//   any other voice stores the SHIFTED pitch at table[voice] and returns the ROUNDED pitch.
// v1 holds that store address on the storing arm and the caller's own word on the other two, so it
// must stay unwritten on them.
void setVoicePitch(Core *c) {
  const std::uint32_t voice = c->r[4];
  std::uint32_t pitch = c->r[5];

  if (c->mem_r32(libspu::kSpuPitchRoundingGuard) != 0u) {
    const std::uint32_t step = c->mem_r32(libspu::kSpuPitchRoundingStep);
    if (step != 0u && (pitch % step) != 0u) {
      pitch = (pitch + step) & ~c->mem_r32(libspu::kSpuPitchRoundingMask);
    }
  }

  const std::uint32_t shifted = pitch >> (c->mem_r32(libspu::kSpuFieldShift) & 31u);
  if (voice == kReturnRoundedPitch) {
    c->r[2] = pitch;
    return;
  }
  if (voice == kReturnShiftedPitch) {
    c->r[2] = shifted & 0xFFFFu;
    return;
  }
  const std::uint32_t slot = (voice << 1) + c->mem_r32(libspu::kSpuRegisterBase);
  c->mem_w16(slot, static_cast<std::uint16_t>(shifted));
  c->r[2] = pitch;
  c->r[3] = slot;
}

} // namespace

void registerSpuVoicePitchOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, 0x8005C588u, "spu_set_voice_pitch", setVoicePitch);
}

} // namespace spyro1::native
