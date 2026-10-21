#include "spyro1/render/paired_actor_producer.h"

#include "core.h"
#include "guest_memory_map.h"
#include "render/field/guest_render_globals.h"
#include "render/field/keyframe_blend.h"
#include "render/field/record_blend.h"
#include "render/state_producer_override.h"
#include "spyro1/render/actor_between.h"
#include "spyro1/render/paired_actor.h"
#include "spyro1/render/paired_actor_layout.h"
#include "spyro1/render/terrain_image.h"

#include <memory>
#include <vector>

namespace spyro1::actors {
namespace {

namespace memory = spyro::guest_memory;
using terrain::kRenderGlobals;

// g_anSpyroHornStrikeAnchorPos + 0xB8: the five rotation words the routine may write there.
constexpr std::uint32_t kHornRotation = kHornAnchor + 0xB8u;
constexpr std::uint32_t kHornRotationBytes = 0x14u;

void drawSpyro(Core *core) {
  Core &guest = *core;
  spyro::guest_render_globals::spillBorrowedRegisters(guest, kRenderGlobals);
  const std::array<GuestSpan, 6> working =
      binnedWorkingRanges(guest,
                          GuestSpan{kVertexScreens, psx::kScratchpadBytes},
                          GuestSpan{kWorkBlock, kWorkBlockBytes},
                          GuestSpan{kHornRotation, kHornRotationBytes});
  psx::present::LevelReads reads;
  const psx::present::EmitMemory memory(guest, reads, working);
  PairedActorState state{.camera = spyro::guest_terrain::readCamera(guest, kRenderGlobals)};
  guest.readBytes(kSpyroRecord, std::as_writable_bytes(std::span(state.record)));
  const auto scope = guest.emission.instance(kSpyroRecord);
  PairedActor actor(guest, memory);
  actor.draw();
  actor.end();
  memory::saveDraw(guest, state, std::span<const psx::present::LevelWord>(reads.words()));
}

} // namespace

std::array<std::uint8_t, spyro_record::kBytes>
spyroBetween(const std::array<std::uint8_t, spyro_record::kBytes> &from,
             const std::array<std::uint8_t, spyro_record::kBytes> &to,
             float t) {
  std::array<std::uint8_t, spyro_record::kBytes> shown = to;
  for (std::uint32_t axis = 0; axis < 3; ++axis) {
    spyro::record_blend::wordBetween(shown, from, to, spyro_record::kPosition + 4 * axis, t);
  }
  // Bytes 0..2 of each part's angle word; byte 3's sign says whether the part turns at all.
  for (std::uint32_t part = 0; part < 3; ++part) {
    for (std::uint32_t axis = 0; axis < 3; ++axis) {
      const std::uint32_t at = spyro_record::kAngles + 4 * part + axis;
      shown[at] = spyro::record_blend::angleBetween(from[at], to[at], t);
    }
  }
  // Each part's animation and frame pairs as one pose: slot A, slot B, frame A, frame B.
  for (std::uint32_t part = 0; part < paired::kParts; ++part) {
    const std::uint32_t animations = spyro_record::kAnimations + 2 * part;
    const std::uint32_t frames = spyro_record::kFrames + 2 * part;
    const auto poseOf = [animations, frames, part](
                            const std::array<std::uint8_t, spyro_record::kBytes> &bytes) {
      return spyro::keyframe_blend::Pose{.keyframes = std::uint32_t{bytes[animations]} |
                                                      (std::uint32_t{bytes[animations + 1]} << 8) |
                                                      (std::uint32_t{bytes[frames]} << 16) |
                                                      (std::uint32_t{bytes[frames + 1]} << 24),
                                         .progress = bytes[spyro_record::kBlends + part]};
    };
    const spyro::keyframe_blend::Pose pose =
        spyro::keyframe_blend::between(poseOf(from), poseOf(to), t, spyro_record::kPartProgress);
    shown[animations] = static_cast<std::uint8_t>(pose.keyframes);
    shown[animations + 1] = static_cast<std::uint8_t>(pose.keyframes >> 8);
    shown[frames] = static_cast<std::uint8_t>(pose.keyframes >> 16);
    shown[frames + 1] = static_cast<std::uint8_t>(pose.keyframes >> 24);
    shown[spyro_record::kBlends + part] = pose.progress;
  }
  return shown;
}

void PairedActorProducer::render(const psx::present::StateInterval &states,
                                 psx::present::PrimitiveSink &sink) const {
  const auto before = memory::unpackDraw<PairedActorState>(states.from, "Spyro draw");
  const auto after = memory::unpackDraw<PairedActorState>(states.to, "Spyro draw");
  InBetween between(core_, after.reads);
  between.mapWorkspace();
  between.mapCamera(spyro::guest_terrain::cameraBetween(
      core_, terrain::kTerrainImage, before.header.camera, after.header.camera, states.t));
  between.mapTable();
  between.provide(kSpyroRecord, spyroBetween(before.header.record, after.header.record, states.t));
  between.host().zero(kHornRotation, kHornRotationBytes);
  PairedActor actor(core_, between.memory());
  if (actor.draw()) {
    between.emitBins(sink);
  }
}

void registerPairedActorOverrides(Core &core) {
  spyro::installProducerOverride(core,
                                 PairedActorProducer::kEntry,
                                 "spyro1-paired-actor",
                                 drawSpyro,
                                 std::make_unique<PairedActorProducer>(core));
}

} // namespace spyro1::actors
