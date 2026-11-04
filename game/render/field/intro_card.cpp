#include "render/field/intro_card.h"

#include "core.h"
#include "emit_memory.h"
#include "game.h"
#include "gte_registers.h"
#include "native_dispatch.h"
#include "render/field/guest_moby_visibility.h"
#include "render/field/guest_packet.h"
#include "render/field/guest_word.h"
#include "render/field/moby_pose.h"
#include "render/field/moby_record.h"
#include "render/terrain/guest_depth_bins.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <vector>

namespace spyro::intro_card {
namespace {

using guest_word::asWord;
using moby_pose::Vertex;
namespace entry = guest_moby::entry;

// The render list's last entry ends this far below the scratch block, where the walk stops.
constexpr std::uint32_t kListBytes = guest_moby::kDrawerScratchBelow - guest_moby::kRenderListAboveScratch - 0x10;

struct Corner {
  psx::gte::ProjectedVertex vertex;
  std::uint32_t depth = 0;
};

class Extension {
public:
  Extension(Core &core,
            const Facts &facts,
            const guest_render_globals::Globals &globals,
            float share,
            int nativeWidth)
      : core_(core), facts_(facts), globals_(globals), share_(share), nativeWidth_(nativeWidth), memory_(core) {}

  void draw(std::uint32_t cardEntry) {
    std::uint32_t count = 0;
    for (const Row &row : facts_.rows) {
      count = std::max({count, row.left + 1u, row.right + 1u});
    }
    const std::vector<Vertex> pose = moby_pose::entryPose(core_, facts_.deltaTable, cardEntry, count);
    psx::cpu::Gte &gte = core_.gte();
    const GteRawState saved = gte.saveRaw();
    psx::gte::RotationWords rotation{};
    memory_.readWords(cardEntry + entry::kRotation, rotation);
    gte.loadRotation(rotation);
    gte.loadTranslation({core_.mem_r32(cardEntry + entry::kCentre),
                         core_.mem_r32(cardEntry + entry::kCentre + 4),
                         core_.mem_r32(cardEntry + entry::kCentre + 8)});
    std::int32_t widest = 0;
    for (const Row &row : facts_.rows) {
      widest = std::max(widest, screenX(project(pose[row.right])) - screenX(project(pose[row.left])));
    }
    // Full while the card spans the 4:3 view, none once it is no wider than that view on the canvas,
    // so a receding card shrinks back to its authored shape.
    const float spans = static_cast<float>(widest) / static_cast<float>(nativeWidth_);
    const float smallest = 1.0f / (1.0f + 2.0f * share_);
    const float share = share_ * std::clamp((spans - smallest) / (1.0f - smallest), 0.0f, 1.0f);
    std::vector<std::array<Corner, 4>> rows;
    if (share > 0.0f) {
      for (const Row &row : facts_.rows) {
        const Vertex &left = pose[row.left];
        const Vertex &right = pose[row.right];
        rows.push_back({project(left),
                        project(beyond(left, right, share)),
                        project(right),
                        project(beyond(right, left, share))});
      }
    }
    gte.restoreRaw(saved);
    // The drawer links the card's polygons from the bin before its entry's; the extension joins them.
    const std::uint32_t entryBin = core_.mem_r16(cardEntry + entry::kDepthBin);
    const std::uint32_t bin = entryBin == 0 ? 0 : entryBin - 1;
    for (std::size_t i = 0; i + 1 < rows.size(); ++i) {
      // Each side's quad: inner edge then outer, this row then the next.
      emit({rows[i][0], rows[i][1], rows[i + 1][0], rows[i + 1][1]}, bin);
      emit({rows[i][2], rows[i][3], rows[i + 1][2], rows[i + 1][3]}, bin);
    }
  }

private:
  [[nodiscard]] static std::int32_t screenX(const Corner &corner) {
    return guest_word::asSigned(guest_word::signedHalf(corner.vertex.word() & 0xFFFFu));
  }

  // `edge` continued away from `opposite` by `share` of the row's width.
  [[nodiscard]] static Vertex beyond(const Vertex &edge, const Vertex &opposite, float share) {
    const auto out = [&](std::int32_t from, std::int32_t to) {
      return from + static_cast<std::int32_t>(std::lround(static_cast<float>(from - to) * share));
    };
    return {.x = out(edge.x, opposite.x), .y = out(edge.y, opposite.y), .z = out(edge.z, opposite.z)};
  }

  [[nodiscard]] Corner project(const Vertex &at) {
    psx::cpu::Gte &gte = core_.gte();
    gte.writeData(psx::gte::kVxy0, (asWord(at.x) & 0xFFFFu) | (asWord(at.y) << 16));
    gte.writeData(psx::gte::kVz0, asWord(at.z));
    gte.execute(psx::gte::kRtps);
    return {.vertex = gte.projected(2), .depth = gte.readData(psx::gte::kSz3)};
  }

  void emit(const std::array<Corner, 4> &corners, std::uint32_t bin) {
    if (std::ranges::any_of(corners, [](const Corner &corner) { return corner.depth == 0; })) {
      return;
    }
    const std::uint32_t packet = core_.mem_r32(globals_.primitiveCursor);
    memory_.mem_w32(packet, guest_packet::kF4Tag);
    memory_.mem_w32(packet + 4, guest_packet::kF4Code);
    for (std::uint32_t i = 0; i < corners.size(); ++i) {
      memory_.storeVertex(packet + 8 + i * 4, corners[i].vertex);
    }
    core_.mem_w32(globals_.primitiveCursor, packet + guest_packet::kF4Bytes);
    (void)depth_bins::linkIntoBin(
        memory_, core_.mem_r32(globals_.orderingTable) + bin * depth_bins::kBinStride, packet, packet);
  }

  Core &core_;
  const Facts &facts_;
  const guest_render_globals::Globals &globals_;
  float share_;
  int nativeWidth_;
  psx::present::EmitMemory memory_;
};

} // namespace

void draw(Core &core, const Facts &facts, const guest_render_globals::Globals &globals) {
  const std::uint32_t list = core.gte().readData(psx::gte::kVxy1) + guest_moby::kRenderListAboveScratch;
  if (!psx::cpu::callOriginalOrPropagate(core, facts.drawer)) {
    return;
  }
  const psx::present::GuestProjectionPlan &plan = core.game->guestDisplay.plan();
  if (!plan.widescreen() || core.mem_r32(facts.gameState) != facts.titleGameState) {
    return;
  }
  const float share = (static_cast<float>(plan.projectionExtent.width) /
                           static_cast<float>(plan.nativeProjectionExtent.width) -
                       1.0f) /
                      2.0f;
  for (std::uint32_t at = list; at < list + kListBytes && core.mem_r32(at + entry::kClass) != 0;
       at += guest_moby::kEntryStride) {
    if (core.mem_r16(core.mem_r32(at + entry::kRecord) + moby_record::kMesh) == facts.mesh) {
      Extension(core, facts, globals, share, plan.nativeProjectionExtent.width).draw(at);
    }
  }
}

} // namespace spyro::intro_card
