#include "core/table_slide.h"

#include "blend.h"
#include "core.h"
#include "guest_redraw.h"

namespace spyro {

std::uint32_t TableSlide::addressOf(const Entry &entry) {
  return entry.table + entry.index * (entry.width == Width::Byte ? 1u : 2u);
}

void TableSlide::sample(Core &core) {
  from_ = to_;
  const std::optional<Entry> entry = entryOf(core);
  if (!entry) {
    to_.reset();
    return;
  }
  const std::uint32_t address = addressOf(*entry);
  const std::int32_t value = entry->width == Width::Byte
                                 ? std::int32_t{core.mem_r8(address)}
                                 : std::int32_t{static_cast<std::int16_t>(core.mem_r16(address))};
  to_ = Read{.entry = *entry, .value = value};
}

void TableSlide::write(psx::present::GuestRedraw &redraw, float t) const {
  if (!from_ || !to_ || from_->entry.phase != to_->entry.phase ||
      from_->entry.table != to_->entry.table || to_->entry.index <= from_->entry.index) {
    return;
  }
  const std::uint32_t address = addressOf(to_->entry);
  const std::int32_t value = psx::present::lerpInt(from_->value, to_->value, t);
  if (to_->entry.width == Width::Byte) {
    redraw.writeByte(address, static_cast<std::uint8_t>(value));
    return;
  }
  redraw.writeHalf(address, static_cast<std::uint16_t>(value));
}

} // namespace spyro
