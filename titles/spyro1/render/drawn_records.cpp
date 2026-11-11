#include "spyro1/render/drawn_records.h"

#include "core.h"
#include "game.h"
#include "render/field/record_blend.h"
#include "replay_frame.h"

#include <algorithm>
#include <span>

namespace spyro1::actors {
namespace {

namespace moby_record = spyro::moby_record;

moby_record::Bytes read(Core &core, std::uint32_t record) {
  moby_record::Bytes bytes{};
  core.readBytes(record, std::as_writable_bytes(std::span(bytes)));
  return bytes;
}

} // namespace

std::optional<std::uint32_t>
DrawnRecords::queued(Core &core, std::uint32_t record, std::uint32_t slot) {
  const std::optional<float> t = core.runtime.replayFrame(core)->replaying();
  if (!t) {
    if (core.game->timing.logicFrame != frame_) {
      frame_ = core.game->timing.logicFrame;
      drawn_.clear();
    }
    drawn_.emplace_back(slot, read(core, record));
    return std::nullopt;
  }
  const auto found = std::ranges::find(drawn_, slot, &Saved::first);
  if (found == drawn_.end()) {
    return std::nullopt;
  }
  const moby_record::Bytes to = read(core, record);
  if (!moby_record::sameObject(found->second, to)) {
    return std::nullopt;
  }
  const moby_record::Between shown = moby_record::between(found->second, to, *t);
  for (const std::uint32_t at :
       {moby_record::kX, moby_record::kY, moby_record::kZ, moby_record::kAngles}) {
    core.mem_w32(record + at, spyro::record_blend::load<std::uint32_t>(shown.bytes, at));
  }
  return shown.angleFractions;
}

} // namespace spyro1::actors
