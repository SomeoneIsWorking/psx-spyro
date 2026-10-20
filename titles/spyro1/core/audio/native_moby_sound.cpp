#include "spyro1/core/audio/native_moby_sound.h"

#include "core/native_execution.h"
#include "guest_call.h"
#include "spyro1/core/guest_globals.h"

#include <cstdint>

namespace spyro1::native {
namespace {

using spyro1::guest::kModels;

constexpr std::uint32_t kMobyModelClass = 0x36u;
constexpr std::uint32_t kSoundChannel = 0x54u;
constexpr std::uint32_t kSoundReferenceByte = 4u;
constexpr std::uint32_t kPositionalSound = 8u;
constexpr std::uint32_t kStopEverySound = 1u;
// Stop the sounds a moby owns (native_sound_position owns that entry).
constexpr std::uint32_t kStopMobySounds = 0x800562A4u; // `jal` at 0x8003856C
// Start one sound, 3D (flag 8) or 2D, on a named channel.
constexpr std::uint32_t kPlaySound = 0x80055A78u; // `jal` at 0x8003859C

// The `lbu 4(v0)` one level above the model's sound table: the class is a SIGNED halfword, so a
// negative one indexes downwards, and the entry itself is taken from the model's table plus the
// sound index. Both the channel-present and channel-absent paths re-run these four instructions,
// the second one after the stop call, so the class is read again rather than carried over.
struct MobySound {
  std::uint32_t moby;
  std::uint32_t index;
};

std::uint32_t mobySoundReference(Core *c, MobySound sound) {
  const auto modelClass = static_cast<std::uint32_t>(c->mem_r16s(sound.moby + kMobyModelClass));
  const std::uint32_t soundTable = c->mem_r32(kModels + (modelClass << 2));
  return c->mem_r8(soundTable + sound.index + kSoundReferenceByte);
}

// ── 0x8003851C — play one of a moby model's sounds positionally, on a caller-named channel.
//     With a channel nothing else happens; without one the moby's own sounds are stopped FIRST and
//     the play goes to its own channel at +0x54. Both paths end in the same `jal`, so v0 and v1 are
//     whatever it leaves — the body writes neither register itself, and the "with a channel" path
//     never ran the stop call whose v1 would otherwise still be there.
void playMobySound(Core *c) {
  const std::uint32_t moby = c->r[4];
  const std::uint32_t soundIndex = c->r[5];
  const std::uint32_t channel = c->r[6];
  if (channel != 0u) {
    psx::cpu::callGuestNow(*c,
                           "play_moby_sound",
                           kPlaySound,
                           mobySoundReference(c, {.moby = moby, .index = soundIndex}),
                           moby,
                           kPositionalSound,
                           channel);
    return;
  }
  psx::cpu::callGuestNow(*c, "play_moby_sound", kStopMobySounds, moby, kStopEverySound);
  psx::cpu::callGuestNow(*c,
                         "play_moby_sound",
                         kPlaySound,
                         mobySoundReference(c, {.moby = moby, .index = soundIndex}),
                         moby,
                         kPositionalSound,
                         moby + kSoundChannel);
}

} // namespace

void registerMobySoundOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, 0x8003851Cu, "play_moby_sound", playMobySound);
}

} // namespace spyro1::native
