// sky_wave.h — the sky objects' wave as a motion source. The sky-object drawer stores, per object,
// (sine[(tick * step + object * objectStep) & 0xFF] >> 4) + base into both halves of one word just
// before it calls the consumer that hands that word to the GTE; a tick moves the sine `step`
// entries, so an in-between reads the guest's own table `t` of the way between the two ticks'
// entries, through the consumer's native override.
#pragma once

#include "motion_set.h"

#include <cstdint>
#include <optional>

class Core;

namespace spyro {

struct SkyWaveFacts {
  std::uint32_t consumer = 0;   // called by the drawer with its frame unchanged
  std::uint32_t objectSlot = 0; // the drawer's object index, at this offset from $sp
  std::uint32_t tick = 0;       // the word the drawer multiplies by `step`
  std::uint32_t sine = 0;       // 256 s16 entries
  std::uint32_t wave = 0;       // the word both halves of which take the value
  std::uint32_t step = 0;
  std::uint32_t objectStep = 0;
  std::int32_t base = 0;
};

// One object's draw inside a replay: the drawer's object index and the replay's fraction.
struct SkyWaveDraw {
  std::uint32_t object = 0;
  float t = 0.0F;
};

class SkyWave final : public psx::present::MotionSource {
public:
  explicit SkyWave(const SkyWaveFacts *facts) : facts_(facts) {}

  [[nodiscard]] const char *name() const override {
    return "sky wave";
  }
  void sample(Core &core) override;
  psx::present::MotionCount write(psx::present::GuestRedraw &redraw, float t) const override;

  // The wave halfword of the draw's object `t` of the way from the earlier tick to the later; none
  // outside a pair of samples.
  [[nodiscard]] std::optional<std::uint16_t> between(Core &core, SkyWaveDraw draw) const;
  [[nodiscard]] const SkyWaveFacts *facts() const {
    return facts_;
  }

private:
  const SkyWaveFacts *facts_;
  std::optional<std::uint32_t> from_;
  std::optional<std::uint32_t> to_;
};

// Installs the consumer's override: in a replay it rewrites the wave word at the replay's fraction
// before the consumer runs.
void installSkyWave(Core &core, const SkyWaveFacts &facts);

} // namespace spyro
