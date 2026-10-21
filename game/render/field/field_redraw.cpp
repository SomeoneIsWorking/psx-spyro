#include "render/field/field_redraw.h"

#include "blend.h"
#include "core/session.h"
#include "game.h"
#include "guest_redraw.h"
#include "lucent/log.h"
#include "native_dispatch.h"
#include "ordering_table.h"
#include "packet_chains.h"
#include "render/field/guest_moby_visibility.h"
#include "render/field/keyframe_blend.h"
#include "render/field/record_blend.h"
#include "render/terrain/guest_depth_bins.h"

#include <cstdlib>
#include <cstring>
#include <span>
#include <type_traits>

namespace spyro::field_redraw {
namespace {

namespace record = guest_moby::record;
using psx::present::kInBetweenAt;

// A saved field: this header, then the redraw's primitives.
struct Header {
  std::uint32_t serial = 0;
  std::uint32_t redrawn = 0; // 1 when the primitives are the in-between from `lerpedFrom`
  std::uint32_t lerpedFrom = 0;
};

struct SavedPrimitive {
  psx::present::OtSlot slot;
  psx::present::DrawPrimitive primitive;
};
static_assert(std::is_trivially_copyable_v<Header> && std::is_trivially_copyable_v<SavedPrimitive>);

Header headerOf(std::span<const std::byte> state) {
  if (state.size() < sizeof(Header) ||
      (state.size() - sizeof(Header)) % sizeof(SavedPrimitive) != 0) {
    lucent::error("redraw", "a saved field of {} bytes is not one", state.size());
    std::abort();
  }
  Header header;
  std::memcpy(&header, state.data(), sizeof(Header));
  return header;
}

// Appends each primitive to a saved field.
class StateSink final : public psx::present::PrimitiveSink {
public:
  explicit StateSink(std::vector<std::byte> &state) : state_(state) {}
  void emit(psx::present::OtSlot slot, const psx::present::DrawPrimitive &primitive) override {
    const SavedPrimitive saved{slot, primitive};
    const auto *bytes = reinterpret_cast<const std::byte *>(&saved);
    state_.insert(state_.end(), bytes, bytes + sizeof(saved));
  }

private:
  std::vector<std::byte> &state_;
};

void readInputs(Core &core, const Image &image, Inputs &inputs) {
  inputs.serial = 0;
  inputs.camera = guest_terrain::readCamera(core, image.terrain.globals());
  inputs.mobyArray = core.mem_r32(image.mobyArrayWord);
  inputs.mobys.clear();
  for (std::uint32_t rec = inputs.mobyArray; core.isGuestStorage(rec, moby_record::kBytes) &&
                                             core.mem_r8s(rec + record::kFlag) != record::kEnd;
       rec += moby_record::kBytes) {
    core.readBytes(rec, std::as_writable_bytes(std::span(inputs.mobys.emplace_back())));
  }
  const Facts &facts = image.field;
  Inputs::Spyro &spyro = inputs.spyro;
  for (std::uint32_t i = 0; i < 3; ++i) {
    spyro.position[i] = core.mem_r32(facts.spyroPosition + 4 * i);
  }
  spyro.angles = core.mem_r32(facts.spyroAngles);
  spyro.headAngles = core.mem_r32(facts.spyroHeadAngles);
  for (std::uint32_t i = 0; i < 2; ++i) {
    spyro.keyframes[i] = core.mem_r32(facts.spyroKeyframes + 4 * i);
  }
  spyro.keyProgress = core.mem_r32(facts.spyroKeyProgress);
  effect_pools::read(core, facts.effects, inputs.effects);
}

// Three angle bytes at `t` the short way round; the fourth byte is `to`'s.
std::uint32_t anglesBetween(std::uint32_t from, std::uint32_t to, float t) {
  std::uint32_t shown = to & 0xFF000000u;
  for (std::uint32_t shift = 0; shift < 24; shift += 8) {
    shown |=
        std::uint32_t{record_blend::angleBetween(
            static_cast<std::uint8_t>(from >> shift), static_cast<std::uint8_t>(to >> shift), t)}
        << shift;
  }
  return shown;
}

// A record between two fields: its pose, and with both refining their angles by the flags'
// fractions, at that precision.
moby_record::Bytes
recordBetween(const moby_record::Bytes &from, const moby_record::Bytes &to, float t) {
  moby_record::Bytes shown = moby_record::between(from, to, t);
  const auto poseOf = [](const moby_record::Bytes &bytes) {
    return keyframe_blend::Pose{.keyframes =
                                    record_blend::load<std::uint32_t>(bytes, record::kKeyframes),
                                .progress = bytes[record::kKeyProgress]};
  };
  const keyframe_blend::Pose pose = keyframe_blend::between(poseOf(from), poseOf(to), t);
  record_blend::store(shown, record::kKeyframes, pose.keyframes);
  shown[record::kKeyProgress] = pose.progress;
  const auto flagsOf = [](const moby_record::Bytes &bytes) {
    return record_blend::load<std::uint32_t>(bytes, record::kFlags);
  };
  if ((flagsOf(from) & flagsOf(to) & record::kInterpolatedAngles) == 0) {
    return shown;
  }
  std::uint32_t flags = flagsOf(to);
  for (std::uint32_t axis = 0; axis < 3; ++axis) {
    const std::uint32_t shift = axis * 8;
    const auto refined = [&](const moby_record::Bytes &bytes) {
      return (std::uint32_t{bytes[moby_record::kAngles + axis]} << 4) |
             ((flagsOf(bytes) >> shift) & 0xFu);
    };
    const std::uint32_t angle = psx::present::lerpTurn(refined(from), refined(to), 12, t);
    shown[moby_record::kAngles + axis] = static_cast<std::uint8_t>(angle >> 4);
    flags = (flags & ~(0xFu << shift)) | ((angle & 0xFu) << shift);
  }
  record_blend::store(shown, record::kFlags, flags);
  return shown;
}

void writeRotation(psx::present::GuestRedraw &run,
                   std::uint32_t address,
                   const psx::gte::RotationWords &words) {
  for (std::uint32_t i = 0; i < words.size(); ++i) {
    run.writeWord(address + 4 * i, words[i]);
  }
}

// The camera, the mobys, Spyro and the effects of the in-between from `from` to `to`.
void writeInBetween(Core &core,
                    psx::present::GuestRedraw &run,
                    const Image &image,
                    const Inputs &from,
                    const Inputs &to) {
  const guest_render_globals::Globals &globals = image.terrain.globals();
  const guest_terrain::InBetweenCamera camera =
      guest_terrain::cameraBetween(core, image.terrain, from.camera, to.camera, kInBetweenAt);
  writeRotation(run, globals.cameraRotation, camera.rotation);
  writeRotation(run, image.terrain.facts().classify.viewRotation, camera.viewRotation);
  for (std::uint32_t i = 0; i < 3; ++i) {
    run.writeWord(globals.cameraPosition + 4 * i, camera.position[i]);
    run.writeWord(image.field.drawCameraPosition + 4 * i, camera.position[i]);
  }
  const auto half = [](std::int16_t angle) {
    return std::uint32_t{static_cast<std::uint16_t>(angle)};
  };
  run.writeWord(globals.cameraAngles, half(camera.angles[0]) | (half(camera.angles[1]) << 16));
  run.writeWord(globals.cameraAngles + 4,
                (core.mem_r32(globals.cameraAngles + 4) & 0xFFFF0000u) | half(camera.angles[2]));

  if (from.mobyArray == to.mobyArray) {
    const std::size_t paired = std::min(from.mobys.size(), to.mobys.size());
    for (std::size_t i = 0; i < paired; ++i) {
      if (!moby_record::sameObject(from.mobys[i], to.mobys[i])) {
        continue;
      }
      const moby_record::Bytes shown = recordBetween(from.mobys[i], to.mobys[i], kInBetweenAt);
      const std::uint32_t rec = to.mobyArray + static_cast<std::uint32_t>(i) * moby_record::kBytes;
      for (const std::uint32_t at : {moby_record::kX,
                                     moby_record::kY,
                                     moby_record::kZ,
                                     record::kKeyframes,
                                     record::kKeyProgress,
                                     moby_record::kAngles,
                                     record::kFlags}) {
        run.writeWord(rec + at, record_blend::load<std::uint32_t>(shown, at));
      }
    }
  }

  const Facts &facts = image.field;
  const Inputs::Spyro &a = from.spyro;
  const Inputs::Spyro &b = to.spyro;
  for (std::uint32_t i = 0; i < 3; ++i) {
    run.writeWord(facts.spyroPosition + 4 * i,
                  psx::present::lerpWord(a.position[i], b.position[i], kInBetweenAt));
  }
  run.writeWord(facts.spyroAngles, anglesBetween(a.angles, b.angles, kInBetweenAt));
  run.writeWord(facts.spyroHeadAngles, anglesBetween(a.headAngles, b.headAngles, kInBetweenAt));
  std::uint32_t progress = b.keyProgress;
  for (std::uint32_t i = 0; i < 2; ++i) {
    const std::uint32_t shift = 8 * i;
    const auto poseOf = [shift, i](const Inputs::Spyro &spyro) {
      return keyframe_blend::Pose{.keyframes = spyro.keyframes[i],
                                  .progress =
                                      static_cast<std::uint8_t>(spyro.keyProgress >> shift)};
    };
    const keyframe_blend::Pose pose = keyframe_blend::between(poseOf(a), poseOf(b), kInBetweenAt);
    run.writeWord(facts.spyroKeyframes + 4 * i, pose.keyframes);
    progress = (progress & ~(0xFFu << shift)) | (std::uint32_t{pose.progress} << shift);
  }
  run.writeWord(facts.spyroKeyProgress, progress);

  effect_pools::writeBetween(run, facts.effects, from.effects, to.effects, kInBetweenAt);
}

// The dispatcher run at the in-between with `flags` less the terrain's, the packets it linked
// appended to `state` in the flatten's order: the bins deepest first, then the overlay pair. False
// when the run or its packets cannot stand for the in-between.
bool redraw(Core &core,
            const Image &image,
            const Inputs &from,
            const Inputs &to,
            std::vector<std::byte> &state) {
  psx::present::GuestRedraw run(core);
  writeInBetween(core, run, image, from, to);
  const guest_render_globals::Globals &globals = image.terrain.globals();
  const std::uint32_t flags = core.r[psx::cpu::kA0] & ~image.field.terrainFlag;
  if (!run.run(image.field.entry, std::span(&flags, 1))) {
    return false;
  }
  const psx::present::EmitMemory memory(core);
  psx::present::PacketChainWalker walker(memory,
                                         psx::present::PacketBound::count(psx::gpu::kOtNodeLimit));
  psx::present::DrawModeTracker drawMode;
  StateSink sink(state);
  const auto emitPair = [&](std::uint32_t pair, psx::present::OtSlot slot) {
    const std::uint32_t first = core.mem_r32(pair + depth_bins::kBinFirstOffset);
    if (first == 0) {
      return true;
    }
    // A pair's chain ends at its head, whose link the flatten has yet to write.
    const std::uint32_t head = core.mem_r32(pair);
    run.writeWord(head, core.mem_r32(head) | psx::gpu::kOtChainEnd);
    if (walker.tryEmitChain(first, slot, drawMode, sink)) {
      return true;
    }
    lucent::warn("redraw",
                 "the redraw of 0x{:08X} linked a packet that is not a primitive into slot {}:{}",
                 image.field.entry,
                 slot.table,
                 slot.index);
    return false;
  };
  for (const depth_bins::Bins &bins :
       {depth_bins::publishedBins(core, globals), depth_bins::overlayBins(core, globals)}) {
    for (std::uint32_t bin = bins.linked; bin-- > 0;) {
      if (!emitPair(bins.base + bin * depth_bins::kBinStride,
                    psx::present::OtSlot{bins.table, bin})) {
        return false;
      }
    }
  }
  return true;
}

} // namespace

void redrawField(Core &core, const Image &image) {
  History &history = session(core).fieldRedraw;
  readInputs(core, image, history.now);
  history.state.resize(sizeof(Header));
  history.redrawn = core.game->mods.fps60 != 0 && history.last.serial != 0 &&
                    redraw(core, image, history.last, history.now, history.state);
  if (!history.redrawn) {
    history.state.resize(sizeof(Header));
  }
  history.packetsEnd = 0;
  history.lerpedFrom = history.last.serial;
  std::swap(history.last, history.now);
  history.pending = true;
  (void)psx::cpu::callOriginalOrPropagate(core, image.field.entry);
}

void endField(Core &core, const Image &image) {
  if (psx::cpu::callOriginalOrPropagate(core, image.field.lastDrawer)) {
    session(core).fieldRedraw.packetsEnd = core.mem_r32(image.terrain.globals().primitiveCursor);
  }
}

depth_bins::Claim fieldClaim(Core &core) {
  const History &history = session(core).fieldRedraw;
  return depth_bins::Claim{.below = history.pending ? history.packetsEnd : 0};
}

void saveField(Core &core) {
  History &history = session(core).fieldRedraw;
  if (!history.pending) {
    return;
  }
  const psx::present::RecordKey &scope = core.emission.current();
  const Header header{.serial = scope.serial,
                      .redrawn = history.redrawn ? 1u : 0u,
                      .lerpedFrom = history.lerpedFrom};
  std::memcpy(history.state.data(), &header, sizeof(Header));
  core.frameStates.save(scope, std::span<const std::byte>(history.state));
  history.last.serial = scope.serial;
  history.pending = false;
}

void FieldRedrawProducer::render(const psx::present::StateInterval &states,
                                 psx::present::PrimitiveSink &sink) const {
  (void)headerOf(states.to);
  for (std::size_t at = sizeof(Header); at < states.to.size(); at += sizeof(SavedPrimitive)) {
    SavedPrimitive saved;
    std::memcpy(&saved, states.to.data() + at, sizeof(saved));
    sink.emit(saved.slot, saved.primitive);
  }
}

psx::present::StateProducer::Composition
FieldRedrawProducer::composition(const psx::present::StateInterval &states) const {
  if (states.t >= 1.0f) {
    return Composition::Recorded;
  }
  const Header from = headerOf(states.from);
  const Header to = headerOf(states.to);
  return to.redrawn != 0 && to.lerpedFrom == from.serial && states.t == kInBetweenAt
             ? Composition::Render
             : Composition::Refused;
}

} // namespace spyro::field_redraw
