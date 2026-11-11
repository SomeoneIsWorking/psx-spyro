#include "spyro1/render/drawn_records.h"

#include "core.h"
#include "native_dispatch.h"
#include "render/field/record_blend.h"
#include "replay_frame.h"
#include "spyro1/core/guest_globals.h"
#include "spyro1/core/native_state.h"

#include <algorithm>
#include <array>
#include <span>

namespace spyro1::actors {
namespace {

namespace moby_record = spyro::moby_record;

// func_800181AC (text), func_80017FE4 (digits) and func_80018534 (cursor).
constexpr std::array<std::uint32_t, 3> kBuilders{0x800181ACu, 0x80017FE4u, 0x80018534u};

moby_record::Bytes read(Core &core, std::uint32_t record) {
  moby_record::Bytes bytes{};
  core.readBytes(record, std::as_writable_bytes(std::span(bytes)));
  return bytes;
}

template <std::uint32_t entry> void builder(Core *core) {
  DrawnRecords &records = nativeState(*core).drawnRecords;
  const DrawnRecords::Builder outer = records.enterBuilder(core->r[31]);
  (void)psx::cpu::callOriginalOrPropagate(*core, entry);
  records.leaveBuilder(outer);
}

} // namespace

void DrawnRecords::beginDraw(Draw draw) {
  built_.clear();
  calls_.clear();
  call_ = {};
  if (draw == Draw::Real) {
    drawn_.clear();
  }
}

DrawnRecords::Builder DrawnRecords::enterBuilder(std::uint32_t caller) {
  const Builder outer{call_};
  if (call_.caller == 0) {
    const auto found =
        std::ranges::find(calls_, caller, &std::pair<std::uint32_t, std::uint32_t>::first);
    std::uint32_t turn = 0;
    if (found == calls_.end()) {
      calls_.emplace_back(caller, 1u);
    } else {
      turn = found->second++;
    }
    call_ = {.caller = caller, .turn = turn, .built = 0};
  }
  return outer;
}

void DrawnRecords::leaveBuilder(Builder outer) {
  call_ = outer.call;
}

void DrawnRecords::filled(Core &core, const Fill &fill) {
  if (fill.bytes != moby_record::kBytes || fill.address != core.mem_r32(guest::kHudMobys)) {
    return;
  }
  if (call_.caller != 0) {
    built_.emplace_back(
        fill.address,
        Name{
            .site = fill.site, .caller = call_.caller, .turn = call_.turn, .index = call_.built++});
    return;
  }
  // A draw's own build: its loop's index is its turn at the site.
  const auto turn =
      static_cast<std::uint32_t>(std::ranges::count_if(built_, [&](const auto &entry) {
        return entry.second.site == fill.site && entry.second.caller == 0;
      }));
  built_.emplace_back(fill.address, Name{.site = fill.site, .turn = turn});
}

std::optional<DrawnRecords::Name> DrawnRecords::nameOf(std::uint32_t record) const {
  const auto found = std::ranges::find_if(built_.rbegin(), built_.rend(), [&](const auto &entry) {
    return entry.first == record;
  });
  if (found == built_.rend()) {
    return std::nullopt;
  }
  return found->second;
}

std::optional<std::uint32_t> DrawnRecords::queued(Core &core, std::uint32_t record) {
  const std::optional<Name> name = nameOf(record);
  if (!name) {
    return std::nullopt;
  }
  const std::optional<float> t = core.runtime.replayFrame(core)->replaying();
  if (!t) {
    drawn_.emplace_back(*name, read(core, record));
    return std::nullopt;
  }
  const auto found = std::ranges::find(drawn_, *name, &decltype(drawn_)::value_type::first);
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

void registerBuildSites(Core &core) {
  psx::cpu::installNativeOverride(core, kBuilders[0], "spyro1-text-builder", builder<kBuilders[0]>);
  psx::cpu::installNativeOverride(
      core, kBuilders[1], "spyro1-digit-builder", builder<kBuilders[1]>);
  psx::cpu::installNativeOverride(
      core, kBuilders[2], "spyro1-cursor-builder", builder<kBuilders[2]>);
}

} // namespace spyro1::actors
