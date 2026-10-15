#include "spyro1_actor_list.h"

#include "core.h"
#include "gte_registers.h"
#include "guest_globals.h"
#include "guest_moby_frustum.h"
#include "guest_moby_rotation.h"
#include "guest_render_globals.h"
#include "guest_view_slope.h"
#include "guest_word.h"
#include "native_dispatch.h"

#include <algorithm>
#include <array>

namespace spyro1::actors {
namespace {

namespace gte = psx::gte;
namespace frustum = spyro::guest_moby_frustum;
namespace rotation = spyro::guest_moby_rotation;
using terrain::kRenderGlobals;

using spyro::guest_word::asSigned;

// The pointer list ends a draw list before this, so the bounded builder stops one entry short.
constexpr std::uint32_t kDrawListLimit = kSecondaryList;
// Closer than this a staged actor casts its drop shadow (8001F34C).
constexpr std::int32_t kShadowDepth = 0x1200;

// 8001F2B4..8001F2D8 and 8001F328..8001F348: the horizontal pair is the engine's; the vertical
// lateral margin is r - r/32 - r/64 where Spyro 2 uses r/2 + r/16.
constexpr frustum::SphereMargins margins(std::uint32_t radius) {
  return frustum::SphereMargins{
      .horizontalLateral = static_cast<std::int32_t>((radius >> 1) + (radius >> 2) + (radius >> 5)),
      .horizontalDepth = static_cast<std::int32_t>((radius >> 1) + (radius >> 4) + (radius >> 5)),
      .verticalLateral = static_cast<std::int32_t>(radius - (radius >> 5) - (radius >> 6)),
      .verticalDepth = static_cast<std::int32_t>((radius >> 2) + (radius >> 4)),
  };
}

class ListBuilder {
public:
  ListBuilder(Core &core, spyro::guest_memory::WorkingMemory &memory, const ListPass &pass)
      : core_(core), memory_(memory), pass_(pass) {}

  void run();
  // The camera, then `record` alone; true when it was given an entry.
  bool runOne(std::uint32_t record);

private:
  void loadCamera();
  void visit(std::uint32_t record);
  [[nodiscard]] std::uint32_t frame(std::uint32_t mesh, std::uint32_t index) const {
    return memory_.r32(mesh + mesh::kFrames + index * 4);
  }

  Core &core_;
  spyro::guest_memory::WorkingMemory &memory_;
  const ListPass &pass_;
  frustum::HorizontalSlope slope_;
  rotation::RotationWords camera_{};
  std::uint32_t cameraPosition_[3] = {};
  std::uint32_t out_ = kDrawList;
  std::uint32_t shadows_ = kShadowList;
  std::uint32_t v1_ = 0;
};

void ListBuilder::loadCamera() {
  slope_ = spyro::presentedSlope(core_);
  if (pass_.continuesShadowList) {
    shadows_ = memory_.r32(kShadowListEnd);
  }
  for (std::uint32_t i = 0; i < 3; ++i) {
    cameraPosition_[i] = memory_.r32(kRenderGlobals.cameraPosition + i * 4);
  }
  for (std::uint32_t i = 0; i < gte::kRotationWords; ++i) {
    camera_[i] = memory_.r32(kRenderGlobals.cameraRotation + i * 4);
  }
}

bool ListBuilder::runOne(std::uint32_t record) {
  loadCamera();
  visit(record);
  return out_ != kDrawList;
}

void ListBuilder::run() {
  spyro::guest_render_globals::spillBorrowedRegisters(core_, kRenderGlobals);
  loadCamera();
  v1_ = core_.r[3];
  std::uint32_t input = pass_.input;
  for (;;) {
    const std::uint32_t record = memory_.r32(input);
    input += 4;
    if (record == 0) {
      break;
    }
    const std::uint32_t written = out_;
    visit(record);
    // 8001F720: the bounded list stops once the next entry would reach the secondary list.
    if (pass_.bounded && out_ != written && asSigned(kDrawListLimit - (out_ + entry::kBytes)) < 0) {
      break;
    }
  }
  memory_.w32(out_, 0);
  memory_.w32(kShadowListEnd, shadows_);
  core_.r[2] = kShadowListHolder;
  core_.r[3] = v1_;
}

// 8001F1E8..8001F740 (800208FC: 80020994..80020EDC).
void ListBuilder::visit(std::uint32_t record) {
  const std::uint32_t cull = memory_.r16(record + moby::kCullDistance);
  const std::uint32_t worldX = memory_.r32(record + moby::kX);
  memory_.w8(record + moby::kDrawn, 0);
  const std::int32_t reach = asSigned(((cull & 0xFFu) << 8) + ((cull & 0x100u) << 1));
  const std::int32_t dx = asSigned(worldX - cameraPosition_[0]) >> 2;
  if (dx + reach <= 0 || dx - reach >= 0) {
    return;
  }
  const std::uint32_t worldY = memory_.r32(record + moby::kY);
  const std::int32_t dy = asSigned(cameraPosition_[1] - worldY) >> 2;
  v1_ = memory_.r32(record + moby::kZ);
  if (dy + reach <= 0) {
    return;
  }
  v1_ = cameraPosition_[2] - v1_;
  if (dy - reach >= 0) {
    return;
  }
  const std::int32_t dz = asSigned(v1_) >> 2;
  v1_ = static_cast<std::uint32_t>(dz);
  if (dz + reach <= 0 || dz - reach >= 0) {
    return;
  }

  const std::uint32_t mesh = memory_.r32(spyro::guest::kModelSoundTables +
                                         std::uint32_t{memory_.r16(record + moby::kMesh)} * 4);
  const std::uint32_t frames = memory_.r32(record + moby::kFrames);
  const std::uint32_t frame0 = frame(mesh, frames & 0xFFu);
  for (std::uint32_t i = 0; i < gte::kRotationWords; ++i) {
    core_.gte().writeControl(gte::kRotation + i, camera_[i]);
  }
  const std::uint32_t radius = std::uint32_t{memory_.r8(frame0 + mesh::kRadius)} << 4;
  core_.gte().writeData(gte::kIr3, static_cast<std::uint32_t>(dx));
  core_.gte().writeData(gte::kIr1, static_cast<std::uint32_t>(dy));
  core_.gte().writeData(gte::kIr2, static_cast<std::uint32_t>(dz));
  core_.gte().execute(gte::kMvmvaRtIr);
  const frustum::SphereMargins m = margins(radius);
  const frustum::ViewPoint view{.x = asSigned(core_.gte().readData(gte::kMac1)),
                                .y = asSigned(core_.gte().readData(gte::kMac2)),
                                .z = asSigned(core_.gte().readData(gte::kMac3))};
  v1_ = static_cast<std::uint32_t>(view.z);
  if (view.z - reach >= 0 || frustum::behindEye(view, radius) ||
      frustum::outsideHorizontal(view, m, slope_)) {
    return;
  }
  // 8001F344: a staged shadow is kept even when the vertical test then rejects the actor.
  if (asSigned(memory_.r32(record + moby::kShadowFlag)) < 0 && view.z - kShadowDepth < 0) {
    const std::uint32_t shadowFrame = memory_.r8(record + moby::kShadowFrame);
    memory_.w32(shadows_, record);
    memory_.w32(shadows_ + 4, memory_.r8(frame0 + shadowFrame * 8 + 6 + mesh::kVertexSteps));
    shadows_ += 8;
  }
  if (frustum::outsideVertical(view, m)) {
    return;
  }
  std::uint32_t flags = frustum::insideFrustum(view, m, slope_) ? kInsideView : kStraddlesView;
  // Level code reads +0x51, so a margin actor gets its entry but keeps retail's mark.
  const bool retailSees = !frustum::outsideHorizontal(view, m, frustum::kRetailSlope);

  const std::uint32_t angles = memory_.r32(record + moby::kRotation);
  const std::uint32_t blend = memory_.r8(record + moby::kBlend);
  const std::uint32_t tint = memory_.r8(record + moby::kTint);
  const std::uint32_t steps = frame0 + mesh::kVertexSteps + ((frames >> 16) & 0xFFu) * 8;
  std::uint32_t depthShift = memory_.r8(frame0 + mesh::kDepthShift);
  std::uint32_t blendSteps = 0;
  if (blend != 0) {
    flags += blend << 8;
    const std::uint32_t frame1 = frame(mesh, (frames >> 8) & 0xFFu);
    blendSteps = frame1 + mesh::kVertexSteps + (frames >> 24) * 8;
    depthShift = std::max<std::uint32_t>(depthShift, memory_.r8(frame1 + mesh::kDepthShift));
  }
  flags += (angles >> 24) << 16;
  flags += depthShift << 24;
  flags += tint;
  const std::uint32_t depthBias =
      ((std::uint32_t{memory_.r8(record + moby::kDepthBias)} & 0x3Fu) << 8) -
      static_cast<std::uint32_t>(view.z);

  if (retailSees) {
    memory_.w8(record + moby::kDrawn, 1);
  }
  memory_.w32(out_ + entry::kFlags, flags);
  memory_.w32(out_ + entry::kFrame, frame0);
  memory_.w32(out_ + entry::kSteps, steps);
  memory_.w32(out_ + entry::kBlendSteps, blendSteps);
  memory_.w32(out_ + entry::kView, static_cast<std::uint32_t>(view.x));
  memory_.w32(out_ + entry::kView + 4, static_cast<std::uint32_t>(view.y));
  memory_.w32(out_ + entry::kView + 8, static_cast<std::uint32_t>(view.z));
  memory_.w16(out_ + entry::kDepthBias, static_cast<std::uint16_t>(depthBias));
  if (pass_.copiesSecondaryWord) {
    memory_.w32(out_ + entry::kSecondaryWord, memory_.r32(record + moby::kSecondaryWord));
  }
  memory_.w32(out_ + entry::kRecord, record);

  rotation::RotationWords composed = camera_;
  if (const auto last = rotation::composeTabled(core_,
                                                kRenderGlobals.cameraSineTable,
                                                {.angles = angles},
                                                composed,
                                                rotation::Byte0Result::Stored)) {
    v1_ = *last;
  }
  for (std::uint32_t i = 0; i < 4; ++i) {
    memory_.w32(out_ + entry::kRotation + i * 4, composed[i]);
  }
  memory_.w16(out_ + entry::kRotation33, static_cast<std::uint16_t>(composed[4]));
  out_ += entry::kBytes;
}

// The real field's lists keep no level words: every address is working memory.
constexpr std::array<spyro::guest_memory::GuestRange, 1> kWholeSpace = {{{0, 0xFFFFFFFFu}}};

} // namespace

void buildList(Core &core, const ListPass &pass) {
  spyro::guest_memory::GuestMemory memory(core, kWholeSpace);
  ListBuilder(core, memory, pass).run();
}

bool listRecord(Core &core,
                spyro::guest_memory::WorkingMemory &memory,
                const ListPass &pass,
                std::uint32_t record) {
  return ListBuilder(core, memory, pass).runOne(record);
}

namespace {

template <const ListPass &Pass> void listOverride(Core *core) {
  buildList(*core, Pass);
}

} // namespace

void registerListOverrides(Core &core) {
  psx::cpu::installNativeOverride(
      core, kPrimaryListPass.entry, kPrimaryListPass.overrideName, listOverride<kPrimaryListPass>);
  psx::cpu::installNativeOverride(core,
                                  kSecondaryListPass.entry,
                                  kSecondaryListPass.overrideName,
                                  listOverride<kSecondaryListPass>);
}

} // namespace spyro1::actors
