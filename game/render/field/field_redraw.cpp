#include "render/field/field_redraw.h"

#include "blend.h"
#include "core/field_owner.h"
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
#include "render/terrain/guest_terrain_passes.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

namespace spyro::field_redraw {
namespace {

namespace record = guest_moby::record;

// A saved field: this header, then for each in-between of a frame of `fields` fields, a primitive
// count and that redraw's primitives.
struct Header {
  std::uint32_t serial = 0;
  std::uint32_t redrawn = 0; // 1 when the redraws are the in-betweens from `lerpedFrom`
  std::uint32_t lerpedFrom = 0;
  std::uint32_t fields = 0;
};

struct SavedPrimitive {
  psx::present::OtSlot slot;
  psx::present::DrawPrimitive primitive;
};
static_assert(std::is_trivially_copyable_v<Header> && std::is_trivially_copyable_v<SavedPrimitive>);

[[noreturn]] void notASavedField(std::span<const std::byte> state) {
  lucent::error("redraw", "a saved field of {} bytes is not one", state.size());
  std::abort();
}

Header headerOf(std::span<const std::byte> state) {
  if (state.size() < sizeof(Header)) {
    notASavedField(state);
  }
  Header header;
  std::memcpy(&header, state.data(), sizeof(Header));
  return header;
}

// The primitives the redraw saved for `t`; none when it made no redraw for `t`.
std::optional<std::span<const std::byte>> redrawAt(std::span<const std::byte> state, float t) {
  const Header header = headerOf(state);
  std::size_t at = sizeof(Header);
  for (std::uint32_t field = 1; header.redrawn != 0 && field < header.fields; ++field) {
    std::uint32_t count = 0;
    if (state.size() < at + sizeof(count)) {
      notASavedField(state);
    }
    std::memcpy(&count, state.data() + at, sizeof(count));
    at += sizeof(count);
    const std::size_t bytes = std::size_t{count} * sizeof(SavedPrimitive);
    if (state.size() < at + bytes) {
      notASavedField(state);
    }
    if (psx::present::inBetweenAt(static_cast<int>(field), static_cast<int>(header.fields)) == t) {
      return state.subspan(at, bytes);
    }
    at += bytes;
  }
  return std::nullopt;
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
  inputs.mobyArray = core.mem_r32(image.moby.firstRecordWord);
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
  moby_record::Bytes shown = moby_record::between(from, to, t).bytes;
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

// The camera, the mobys, Spyro and the effects `t` of the way from `from` to `to`.
void writeInBetween(Core &core,
                    psx::present::GuestRedraw &run,
                    const Image &image,
                    const Inputs &from,
                    const Inputs &to,
                    float t) {
  const guest_render_globals::Globals &globals = image.terrain.globals();
  const guest_terrain::InBetweenCamera camera =
      guest_terrain::cameraBetween(core, image.terrain, from.camera, to.camera, t);
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
  // The sky links one bin past the mark the terrain raises, and the terrain is not redrawn here, so
  // the redraw runs past every bin the far pass can reach; saveField moves the sky to its real bin.
  run.writeWord(globals.orderingTableMark,
                std::max(core.mem_r32(globals.orderingTableMark),
                         guest_terrain::deepestFarMark(core, image.terrain.facts(), globals)));

  if (from.mobyArray == to.mobyArray) {
    const std::size_t paired = std::min(from.mobys.size(), to.mobys.size());
    for (std::size_t i = 0; i < paired; ++i) {
      if (!moby_record::sameObject(from.mobys[i], to.mobys[i])) {
        continue;
      }
      const moby_record::Bytes shown = recordBetween(from.mobys[i], to.mobys[i], t);
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
                  psx::present::lerpWord(a.position[i], b.position[i], t));
  }
  run.writeWord(facts.spyroAngles, anglesBetween(a.angles, b.angles, t));
  run.writeWord(facts.spyroHeadAngles, anglesBetween(a.headAngles, b.headAngles, t));
  std::uint32_t progress = b.keyProgress;
  for (std::uint32_t i = 0; i < 2; ++i) {
    const std::uint32_t shift = 8 * i;
    const auto poseOf = [shift, i](const Inputs::Spyro &spyro) {
      return keyframe_blend::Pose{.keyframes = spyro.keyframes[i],
                                  .progress =
                                      static_cast<std::uint8_t>(spyro.keyProgress >> shift)};
    };
    const keyframe_blend::Pose pose = keyframe_blend::between(poseOf(a), poseOf(b), t);
    run.writeWord(facts.spyroKeyframes + 4 * i, pose.keyframes);
    progress = (progress & ~(0xFFu << shift)) | (std::uint32_t{pose.progress} << shift);
  }
  run.writeWord(facts.spyroKeyProgress, progress);

  effect_pools::writeBetween(run, facts.effects, from.effects, to.effects, t);
}

// The deepest bin the flatten walks: where the mark ended, the sky's when it drew.
std::uint32_t topBin(Core &core, const guest_render_globals::Globals &globals) {
  const std::uint32_t linked = depth_bins::publishedBins(core, globals).linked;
  return linked == 0 ? 0 : linked - 1;
}

// 0x80029170 links the sky one bin past the mark and advances the mark onto it; the redraw's sky
// goes to the deeper of the two real fields' sky bins, behind the far terrain of both.
void placeSky(History &history, std::uint32_t skyBin) {
  const Header header = headerOf(history.state);
  std::size_t at = sizeof(Header);
  for (std::uint32_t field = 1; header.redrawn != 0 && field < header.fields; ++field) {
    std::uint32_t count = 0;
    std::memcpy(&count, history.state.data() + at, sizeof(count));
    at += sizeof(count);
    for (std::uint32_t i = 0; i < count; ++i, at += sizeof(SavedPrimitive)) {
      psx::present::OtSlot slot;
      std::memcpy(&slot, history.state.data() + at + offsetof(SavedPrimitive, slot), sizeof(slot));
      if (slot.table == depth_bins::kTable && slot.index == history.redrawSkyBin) {
        slot.index = skyBin;
        std::memcpy(
            history.state.data() + at + offsetof(SavedPrimitive, slot), &slot, sizeof(slot));
      }
    }
  }
}

// Before a redraw runs, passes the chain of the (head, first) pair at `pair` over the packets a
// producer keyed: logic linked them before the dispatcher and their producer's render draws them.
void passOverKeyed(Core &core, psx::present::GuestRedraw &run, std::uint32_t pair) {
  const std::uint32_t firstLink = pair + depth_bins::kBinFirstOffset;
  const std::uint32_t head = core.mem_r32(pair);
  std::uint32_t kept = 0;
  for (std::uint32_t packet = core.mem_r32(firstLink); packet != 0;) {
    const psx::gpu::OtNode node(psx::gpu::mainRamOffsetOf(packet),
                                psx::gpu::OtHeaderWord{core.mem_r32(packet)});
    const std::uint32_t next =
        packet == head || node.endsChain() ? 0 : psx::gpu::guestAddressOf(node.nextRamOffset());
    if (!core.emission.keyFor(packet)) {
      kept = packet;
    } else if (next == 0) {
      run.writeWord(kept == 0 ? firstLink : pair, kept);
    } else if (kept == 0) {
      run.writeWord(firstLink, next);
    } else {
      run.writeWord(kept,
                    (core.mem_r32(kept) & ~psx::gpu::kOtNextAddressMask) |
                        (next & psx::gpu::kOtNextAddressMask));
    }
    packet = next;
  }
}

// What a redraw runs: `entry` with `arguments`, after the native moby walk for a second pass, whose
// packets are read from the bins as its own table and not from the overlay pair.
struct RedrawCall {
  std::uint32_t entry = 0;
  std::span<const std::uint32_t> arguments;
  bool secondPass = false;
};

// `call` run at `t`, the packets it linked appended to `state` in the flatten's order: the bins
// deepest first, then the overlay pair. False when the run or its packets cannot stand for the
// in-between.
bool redraw(Core &core,
            const Image &image,
            const RedrawCall &call,
            const Inputs &from,
            const Inputs &to,
            float t,
            History &history,
            std::vector<std::byte> &state) {
  const std::uint32_t entry = call.entry;
  psx::present::GuestRedraw run(core);
  writeInBetween(core, run, image, from, to, t);
  const guest_render_globals::Globals &globals = image.terrain.globals();
  bool &redrawing = session(core).fieldRedraw.redrawing;
  redrawing = true;
  for (const depth_bins::Bins &bins :
       {depth_bins::publishedBins(core, globals), depth_bins::overlayBins(core, globals)}) {
    for (std::uint32_t bin = 0; bin < bins.linked; ++bin) {
      passOverKeyed(core, run, bins.base + bin * depth_bins::kBinStride);
    }
  }
  if (call.secondPass) {
    guest_moby::walk(core, image.moby, globals);
  }
  const bool ran = run.run(entry, call.arguments);
  redrawing = false;
  if (!ran) {
    return false;
  }
  history.redrawSkyBin = topBin(core, globals);
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
                 entry,
                 slot.table,
                 slot.index);
    return false;
  };
  depth_bins::Bins published = depth_bins::publishedBins(core, globals);
  depth_bins::Bins overlay = depth_bins::overlayBins(core, globals);
  if (call.secondPass) {
    published.table = depth_bins::kSecondPassTable;
    overlay.linked = 0;
  }
  for (const depth_bins::Bins &bins : {published, overlay}) {
    for (std::uint32_t bin = bins.linked; bin-- > 0;) {
      if (!emitPair(bins.base + bin * depth_bins::kBinStride,
                    psx::present::OtSlot{bins.table, bin})) {
        return false;
      }
    }
  }
  return true;
}

// The redraws of `call` for each in-between of the frame being drawn, waiting for its flatten.
void redrawFrame(Core &core, const Image &image, const RedrawCall &call, History &history) {
  readInputs(core, image, history.now);
  history.state.resize(sizeof(Header));
  // One redraw per in-between of the frame being drawn; a frame that then outlives its draw's wait
  // refuses them.
  history.fields = static_cast<std::uint32_t>(session(core).fieldOwner->drawnFrameFields());
  history.redrawn = core.game->mods.fps60 != 0 && history.last.serial != 0;
  for (std::uint32_t field = 1; history.redrawn && field < history.fields; ++field) {
    const std::size_t count = history.state.size();
    history.state.resize(count + sizeof(std::uint32_t));
    history.redrawn =
        redraw(core,
               image,
               call,
               history.last,
               history.now,
               psx::present::inBetweenAt(static_cast<int>(field), static_cast<int>(history.fields)),
               history,
               history.state);
    const auto primitives = static_cast<std::uint32_t>(
        (history.state.size() - count - sizeof(std::uint32_t)) / sizeof(SavedPrimitive));
    std::memcpy(history.state.data() + count, &primitives, sizeof(primitives));
  }
  if (!history.redrawn) {
    history.state.resize(sizeof(Header));
  }
  history.packetsEnd = 0;
  history.lerpedFrom = history.last.serial;
  std::swap(history.last, history.now);
  history.pending = true;
}

// The waiting redraws saved under the open producer scope.
void saveRedraws(Core &core, History &history) {
  const psx::present::RecordKey &scope = core.emission.current();
  const Header header{.serial = scope.serial,
                      .redrawn = history.redrawn ? 1u : 0u,
                      .lerpedFrom = history.lerpedFrom,
                      .fields = history.fields};
  std::memcpy(history.state.data(), &header, sizeof(Header));
  core.frameStates.save(scope, std::span<const std::byte>(history.state));
  history.last.serial = scope.serial;
  history.pending = false;
  history.screen = false;
}

} // namespace

void redrawField(Core &core, const Image &image) {
  History &history = session(core).fieldRedraw;
  if (history.redrawing) {
    // The terrain has its own producer, so a screen draw's redraw draws the field without it.
    core.r[psx::cpu::kA0] &= ~image.field.terrainFlag;
  } else if (!history.screen) {
    const std::uint32_t flags = core.r[psx::cpu::kA0] & ~image.field.terrainFlag;
    redrawFrame(core,
                image,
                RedrawCall{.entry = image.field.entry, .arguments = std::span(&flags, 1)},
                history);
  }
  (void)psx::cpu::callOriginalOrPropagate(core, image.field.entry);
}

void redrawScreen(Core &core, const Image &image, std::uint32_t entry) {
  const std::array<std::uint32_t, 4> arguments{
      core.r[psx::cpu::kA0], core.r[psx::cpu::kA1], core.r[psx::cpu::kA2], core.r[psx::cpu::kA3]};
  History &history = session(core).fieldRedraw;
  redrawFrame(core, image, RedrawCall{.entry = entry, .arguments = arguments}, history);
  history.screen = true;
  (void)psx::cpu::callOriginalOrPropagate(core, entry);
}

void endField(Core &core, const guest_render_globals::Globals &globals) {
  session(core).fieldRedraw.packetsEnd = core.mem_r32(globals.primitiveCursor);
}

depth_bins::Claim fieldClaim(Core &core, const guest_render_globals::Globals &globals) {
  const History &history = session(core).fieldRedraw;
  if (!history.pending) {
    return depth_bins::Claim{};
  }
  return depth_bins::Claim{.below = history.screen ? core.mem_r32(globals.primitiveCursor)
                                                   : history.packetsEnd};
}

void saveField(Core &core, const guest_render_globals::Globals &globals) {
  History &history = session(core).fieldRedraw;
  const std::uint32_t skyBin = topBin(core, globals);
  const std::uint32_t lastSkyBin = std::exchange(history.lastSkyBin, skyBin);
  if (!history.pending) {
    return;
  }
  placeSky(history, std::max(lastSkyBin, skyBin));
  saveRedraws(core, history);
}

void redrawSecondPass(Core &core, const Image &image) {
  redrawFrame(core,
              image,
              RedrawCall{.entry = image.field.secondPass.drawer, .secondPass = true},
              session(core).secondPass);
}

void ownSecondPass(Core &core, const Image &image) {
  const guest_render_globals::Globals &globals = image.terrain.globals();
  History &history = session(core).secondPass;
  const auto object = core.emission.instance(core.r[psx::cpu::kRa]);
  depth_bins::Bins bins = depth_bins::publishedBins(core, globals);
  bins.table = depth_bins::kSecondPassTable;
  depth_bins::assignBins(
      core,
      bins,
      depth_bins::Claim{.below = history.pending ? core.mem_r32(globals.primitiveCursor) : 0});
  if (history.pending) {
    saveRedraws(core, history);
  }
}

void FieldRedrawProducer::render(const psx::present::StateInterval &states,
                                 psx::present::PrimitiveSink &sink) const {
  const std::optional<std::span<const std::byte>> primitives = redrawAt(states.to, states.t);
  if (!primitives) {
    lucent::error("redraw", "no redraw was saved for t={}", states.t);
    std::abort();
  }
  for (std::size_t at = 0; at < primitives->size(); at += sizeof(SavedPrimitive)) {
    SavedPrimitive saved;
    std::memcpy(&saved, primitives->data() + at, sizeof(saved));
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
  return to.redrawn != 0 && to.lerpedFrom == from.serial && redrawAt(states.to, states.t)
             ? Composition::Render
             : Composition::Refused;
}

} // namespace spyro::field_redraw
