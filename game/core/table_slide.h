// table_slide.h — a slide the draw places at one entry of a table that each tick steps through, as
// a motion source: an in-between writes the entry the later tick reads `t` of the way from the one
// the earlier tick read.
#pragma once

#include "motion_set.h"

#include <cstdint>
#include <optional>

namespace spyro {

class TableSlide : public psx::present::MotionSource {
public:
  void sample(Core &core) final;
  void write(psx::present::GuestRedraw &redraw, float t) const final;

protected:
  enum class Width : std::uint8_t { Byte, SignedHalf };
  // The entry a tick's draw reads. Two ticks pair while the phase and table hold and the index
  // advanced.
  struct Entry {
    std::uint32_t phase = 0;
    std::uint32_t table = 0;
    std::uint32_t index = 0;
    Width width = Width::Byte;
  };
  // None while the draw reads no entry.
  [[nodiscard]] virtual std::optional<Entry> entryOf(Core &core) const = 0;

private:
  struct Read {
    Entry entry;
    std::int32_t value = 0;
  };
  [[nodiscard]] static std::uint32_t addressOf(const Entry &entry);

  std::optional<Read> from_;
  std::optional<Read> to_;
};

} // namespace spyro
