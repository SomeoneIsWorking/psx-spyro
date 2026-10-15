// A terrain field drawn through the title's shipping overrides over a one-sector synthetic level,
// then walked by DMA into the device's record: what the Record tests compare frames with. The
// chain walk and the compose check are every title's; `TerrainScene` is Spyro 2/3's level.
#pragma once

#include "core.h"
#include "core/session.h"
#include "frame_composer.h"
#include "gp0_primitive_decode.h"
#include "guest_call.h"
#include "image_identity.h"
#include "native_dispatch.h"
#include "record_frames.h"
#include "render/field/guest_render_globals.h"
#include "render/terrain/guest_camera_builder.h"
#include "render/terrain/guest_depth_bins.h"
#include "render/terrain/guest_terrain_facts.h"
#include "render/terrain/guest_terrain_frame.h"
#include "render/terrain/terrain_state_producer.h"
#include "test_game.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace spyro::test {

namespace terrain = spyro::guest_terrain;

// What a title's Record path is made of: its facts, its flatten, and the shipping install.
struct TerrainTitle {
  const terrain::Facts &facts;
  const guest_render_globals::Globals &globals;
  std::uint32_t flatten = 0;
  void (*registerRenderOverrides)(Core &core) = nullptr;
  // One image holding the drawer, the flatten and the visibility leaf, so the dispatcher resolves
  // them.
  std::uint32_t imageStart = 0;
  std::uint32_t imageEnd = 0;
};

inline constexpr std::uint32_t kTable = 0x80100000u;
inline constexpr std::uint32_t kPacketCursor = 0x80120000u;
inline constexpr std::uint32_t kScratchEnd = 0x80190000u;
inline constexpr std::uint32_t kSector = 0x80010000u;
inline constexpr std::uint32_t kSectorTable = 0x80010400u;
inline constexpr std::uint32_t kTextures = 0x80011000u;
// The packet a flattened chain ends at: the GPU's end-of-list tag.
inline constexpr std::uint32_t kChainEnd = 0x80130F00u;
inline constexpr std::uint32_t kEndOfList = 0x00FFFFFFu;

inline constexpr terrain::CameraState kRestCamera{{0, 0, 0}, {0x0u, 0x800u, 0x400u}};

// The GTE constants retail's display bootstrap leaves: the screen centre of a 512x240 window and H.
inline void setProjection(Core &core) {
  core.gte().writeControl(24, 256u << 16); // OFX
  core.gte().writeControl(25, 120u << 16); // OFY
  core.gte().writeControl(26, 341u);       // H
}

// The guest's visibility leaf, which marks the one sector of the synthetic level visible.
inline void visibilityLeaf(Core *core) {
  core->mem_w8(terrain::kScratchpad, 1);
  setProjection(*core);
  core->r[2] = 1;
}

struct Emitted {
  psx::present::OtSlot slot;
  psx::present::DrawPrimitive primitive;

  bool operator==(const Emitted &) const = default;
};

class Collect final : public psx::present::PrimitiveSink {
public:
  void emit(psx::present::OtSlot slot, const psx::present::DrawPrimitive &primitive) override {
    emitted.push_back({slot, primitive});
  }

  std::vector<Emitted> emitted;
};

// The packets the real draw linked, as the flatten's walk reaches them: the deepest bin first, each
// bin from the first packet linked on, with the bucket the title assigned each.
struct Linked {
  std::vector<Emitted> packets;
  std::vector<std::optional<psx::present::OtSlot>> assigned;
  unsigned undecoded = 0;
};

// One drawn field: the record the GPU made of it and the states its producers saved.
struct DrawnFrame {
  psx::present::FrameRecord shown;
  psx::present::FrameState state;
};

// A table of depth bins.
struct BinTable {
  std::uint32_t base = 0;
  std::uint32_t bins = 0;
};

// The guest flatten's chain over `bins`: each bin's packets from the first linked to the head,
// deepest bin first, the last head pointing at the end-of-list tag.
inline std::uint32_t linkChain(Core &core, const BinTable &bins) {
  const auto [table, count] = bins;
  namespace sink = depth_bins;
  const auto linkAfter = [&core](std::uint32_t packet, std::uint32_t next) {
    const std::uint32_t tag = core.mem_r32(packet);
    core.mem_w32(packet, (tag & 0xFF000000u) | psx::gpu::mainRamOffsetOf(next));
  };
  core.mem_w32(kChainEnd, kEndOfList);
  std::uint32_t head = kChainEnd;
  std::uint32_t tail = 0;
  for (std::int32_t bin = static_cast<std::int32_t>(count) - 1; bin >= 0; --bin) {
    const std::uint32_t pair = table + static_cast<std::uint32_t>(bin) * sink::kBinStride;
    const std::uint32_t first = core.mem_r32(pair + sink::kBinFirstOffset);
    if (first == 0) {
      continue;
    }
    if (tail == 0) {
      head = first;
    } else {
      linkAfter(tail, first);
    }
    tail = core.mem_r32(pair);
  }
  if (tail != 0) {
    linkAfter(tail, kChainEnd);
  }
  return head;
}

// The chain DrawOTag walks into the device, and the states its producers saved.
inline DrawnFrame submitField(Core &core, std::uint32_t head) {
  psx::present::FrameRecord shown = submitChain(core, head);
  psx::present::FrameState state = core.frameStates.collect(shown);
  return {std::move(shown), std::move(state)};
}

class TerrainScene {
public:
  explicit TerrainScene(const TerrainTitle &title) : title_(title) {
    game_ = std::make_unique<SessionGame>();
    core().gte().bind();
    core().imageCatalog().activate("synthetic terrain", {title.imageStart, title.imageEnd}, 2);
    core().mem_w32(title.flatten, 0x03E00008u); // jr ra, the flatten's original body
    title.registerRenderOverrides(core());
    psx::cpu::installNativeOverride(
        core(), title.facts.sectorVisibility, "synthetic-visibility", visibilityLeaf);
    plantTables();
    plantLevel();
    plantFrame();
    setCamera(kRestCamera);
  }

  Core &core() {
    return game_->core;
  }

  // The guest's camera words for `camera`, as its builder writes them.
  void setCamera(const terrain::CameraState &camera) {
    const auto &globals = title_.globals;
    const guest_camera::Matrices matrices =
        guest_camera::EngineBuilder(
            core(), {.sine = globals.cameraSineTable, .cosine = globals.cameraCosineTable})
            .build({camera.angles[0], camera.angles[1], camera.angles[2]});
    for (std::uint32_t i = 0; i < 5; ++i) {
      core().mem_w32(globals.cameraRotation + 4 * i, matrices.projection[i]);
      core().mem_w32(title_.facts.classify.viewRotation + 4 * i, matrices.view[i]);
    }
    core().mem_w32(globals.cameraAngles,
                   (static_cast<std::uint32_t>(camera.angles[0]) & 0xFFFFu) |
                       (static_cast<std::uint32_t>(camera.angles[1]) << 16));
    core().mem_w32(globals.cameraAngles + 4,
                   static_cast<std::uint32_t>(camera.angles[2]) & 0xFFFFu);
    for (std::uint32_t i = 0; i < 3; ++i) {
      core().mem_w32(globals.cameraPosition + 4 * i, camera.position[i]);
    }
  }

  // The real draw, reached as the guest's one caller reaches it: $a0 the scratch block's end, $a1
  // 0.
  void drawTerrain(std::uint32_t a0 = kScratchEnd) {
    setProjection(core());
    psx::cpu::callGuestNow(core(), "test", title_.facts.entry, a0, 0u);
  }

  void flatten() {
    psx::cpu::callGuestNow(core(), "test", title_.flatten);
  }

  // The flatten reached by a guest `jal` at `callSite`, so the override sees that site's return
  // address: `move t0, ra` before the site, `jal flatten ; nop ; jr t0 ; nop` from it.
  void flattenFrom(std::uint32_t callSite) {
    constexpr std::uint32_t kMoveT0Ra = 0x03E04021u;
    constexpr std::uint32_t kJal = 0x0C000000u;
    constexpr std::uint32_t kJrT0 = 0x01000008u;
    core().mem_w32(callSite - 4, kMoveT0Ra);
    core().mem_w32(callSite, kJal | ((title_.flatten >> 2) & 0x03FFFFFFu));
    core().mem_w32(callSite + 4, 0);
    core().mem_w32(callSite + 8, kJrT0);
    core().mem_w32(callSite + 12, 0);
    psx::cpu::callGuestNow(core(), "test", callSite - 4);
  }

  // One field from an empty table and pool: the draw, the flatten's bucket assignment, the chain
  // the guest flatten links, and DrawOTag's walk of it into the device.
  // The guest's caller passes a $a0 that alternates with the double-buffered packet pool, so each
  // `field` has its own.
  DrawnFrame drawFrame(const terrain::CameraState &camera, std::uint32_t field) {
    plantFrame();
    clearTable();
    core().frameStates.clear();
    setCamera(camera);
    drawTerrain(kScratchEnd + 0x100u * field);
    flatten();
    return submitField(core(), linkChain(core(), {.base = kTable, .bins = depth_bins::kBins}));
  }

  [[nodiscard]] Linked linkedPackets() {
    namespace sink = depth_bins;
    Linked linked;
    const std::uint32_t table = core().mem_r32(title_.globals.orderingTable);
    for (std::int32_t bin = static_cast<std::int32_t>(sink::kBins) - 1; bin >= 0; --bin) {
      std::uint32_t packet = core().mem_r32(
          table + static_cast<std::uint32_t>(bin) * sink::kBinStride + sink::kBinFirstOffset);
      while (packet != 0) {
        const std::uint32_t tag = core().mem_r32(packet);
        std::vector<std::uint32_t> words(tag >> 24);
        for (std::uint32_t i = 0; i < words.size(); ++i) {
          words[i] = core().mem_r32(packet + 4 + 4 * i);
        }
        const auto primitive = psx::gpu::decodePacketPrimitive(words);
        if (primitive) {
          linked.packets.push_back(
              {psx::present::OtSlot{depth_bins::kTable, static_cast<std::uint32_t>(bin)},
               *primitive});
          linked.assigned.push_back(core().otTables.assigned(packet));
        } else {
          ++linked.undecoded;
        }
        packet = (tag & 0xFFFFFFu) == 0 ? 0 : psx::gpu::guestAddressOf(tag & 0xFFFFFFu);
      }
    }
    return linked;
  }

  [[nodiscard]] std::uint32_t firstLinkedPacket() {
    namespace sink = depth_bins;
    const std::uint32_t table = core().mem_r32(title_.globals.orderingTable);
    for (std::int32_t bin = static_cast<std::int32_t>(sink::kBins) - 1; bin >= 0; --bin) {
      const std::uint32_t packet = core().mem_r32(
          table + static_cast<std::uint32_t>(bin) * sink::kBinStride + sink::kBinFirstOffset);
      if (packet != 0) {
        return packet;
      }
    }
    return 0;
  }

  // The state the drawer saved, collected by a record holding one of its packets.
  [[nodiscard]] std::optional<std::vector<std::byte>>
  savedState(const psx::present::DrawPrimitive &packet, std::uint32_t address) {
    psx::present::FrameRecord record(1, true);
    psx::present::DrawPrimitive keyed = packet;
    keyed.key = core().emission.keyFor(address);
    record.append(keyed);
    const psx::present::FrameState states = core().frameStates.collect(record);
    const auto found = states.find({title_.facts.entry, 0});
    if (!found) {
      return std::nullopt;
    }
    return std::vector<std::byte>(found->begin(), found->end());
  }

  [[nodiscard]] const psx::present::StateProducers &producers() {
    return session(core()).stateProducers;
  }

  // Moves a level vertex the field read, as a guest update between fields would.
  void rewriteLevel() {
    core().mem_w32(kSector + 0x1C + 4 * 2, vertexWord(0x40, 0x240, 0x120));
  }

private:
  void plantTables() {
    const auto &globals = title_.globals;
    for (std::uint32_t i = 0; i < 257; ++i) {
      const double turn = 2.0 * 3.14159265358979323846 * static_cast<double>(i % 256) / 256.0;
      core().mem_w32(globals.cameraSineTable + 2 * i,
                     static_cast<std::uint32_t>(std::lround(std::sin(turn) * 4096.0)) & 0xFFFFu);
      core().mem_w32(globals.cameraCosineTable + 2 * i,
                     static_cast<std::uint32_t>(std::lround(std::cos(turn) * 4096.0)) & 0xFFFFu);
    }
  }

  static std::uint32_t vertexWord(std::uint32_t z, std::uint32_t x, std::uint32_t y) {
    return ((z / 4) << 21) | ((x / 4) << 10) | (y / 8);
  }

  static std::uint32_t
  packedIndices(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
    return (a << 24) | (b << 16) | (c << 8) | d;
  }

  // One detail-pass sector 0xC00 deep: six vertices, four colours, a quad and a triangle.
  void plantLevel() {
    const auto &facts = title_.facts;
    core().mem_w32(facts.classify.sectorCount, 1);
    core().mem_w32(facts.classify.sectorTable, kSectorTable);
    core().mem_w32(kSectorTable, kSector);
    core().mem_w32(facts.detail.textureTable, kTextures);
    core().mem_w32(facts.far.farDepth, 0x4650u);
    core().mem_w32(kSector + 0x00, (0x300u << 16) | 0x80u); // centre x, y in camera units / 16
    core().mem_w32(kSector + 0x04, (0x40u << 16) | 0x80u);  // centre z, radius, no flags
    core().mem_w32(kSector + 0x08, 0xC00u << 14);
    core().mem_w32(kSector + 0x0C, 0x1800u); // the polygon flag bytes, inside the scratch block
    core().mem_w32(kSector + 0x14, (2u << 16) | (4u << 8) | 6u);
    core().mem_w32(kSector + 0x18, 0xFFFF0000u); // animation marks already claimed
    const std::uint32_t vertices[6] = {vertexWord(0x00, 0x080, 0x040),
                                       vertexWord(0x00, 0x200, 0x040),
                                       vertexWord(0x40, 0x200, 0x100),
                                       vertexWord(0x40, 0x080, 0x100),
                                       vertexWord(0x80, 0x300, 0x0C0),
                                       vertexWord(0x80, 0x300, 0x180)};
    std::uint32_t at = kSector + 0x1C;
    for (const std::uint32_t word : vertices) {
      core().mem_w32(at, word);
      at += 4;
    }
    for (std::uint32_t i = 0; i < 4; ++i) { // near colours, then the far set
      core().mem_w32(at + 4 * i, 0x00A08060u + i * 0x000A0A0Au);
      core().mem_w32(at + 16 + 4 * i, 0x00402010u + i * 0x00050505u);
    }
    at += 32;
    core().mem_w32(at + 0x0, packedIndices(0, 1, 2, 3));
    core().mem_w32(at + 0x4, packedIndices(0, 1, 2, 3));
    core().mem_w32(at + 0xC, 0x800u);
    core().mem_w32(at + 0x10, packedIndices(1, 4, 2, 2));
    core().mem_w32(at + 0x14, packedIndices(0, 1, 2, 2));
    core().mem_w32(at + 0x1C, 0x801u);
    // Each record's two word pairs: the shaded texture, then the full-bright one.
    for (const std::uint32_t pair : {0u, 8u}) {
      core().mem_w32(kTextures + pair + 0, 0x7A200810u);
      core().mem_w32(kTextures + pair + 4, 0x00092010u);
      core().mem_w32(kTextures + 48 + pair, 0x7A200C0Cu);
      core().mem_w32(kTextures + 52 + pair, 0x80090C0Cu); // sign bit: semi-transparent
    }
  }

  void plantFrame() {
    const auto &globals = title_.globals;
    core().mem_w32(globals.orderingTable, kTable);
    core().mem_w32(globals.orderingTableMark, kTable + terrain::kOrderingTableBytes);
    core().mem_w32(globals.primitiveCursor, kPacketCursor);
    core().mem_w32(globals.scratchBaseWord, kScratchEnd);
  }

  void clearTable() {
    for (std::uint32_t at = 0; at < depth_bins::kTableBytes; at += 4) {
      core().mem_w32(kTable + at, 0);
    }
  }

  TerrainTitle title_;
  std::unique_ptr<SessionGame> game_;
};

// Three fields of one level: `from`, `to` a turned and moved camera later, and `midway` drawn at
// the camera halfway between.
struct ComposeCheck {
  bool drawn = false;         // every field linked packets
  bool asDrawn = false;       // t = 1 composes `to` as drawn
  bool between = false;       // t = 0.5 composes `midway`
  bool midwayMoved = false;   // `midway` is not `to`
  bool keepsItsLevel = false; // t = 1 is still `to` after the guest moves a vertex it read
};

// `to` composed from `from` at t = 1, and at t = 0.5 against `midway`.
inline ComposeCheck composeBetween(const psx::present::StateProducers &producers,
                                   const DrawnFrame &from,
                                   const DrawnFrame &midway,
                                   const DrawnFrame &to) {
  ComposeCheck check;
  check.drawn = !primitives(from.shown).empty() && !primitives(midway.shown).empty() &&
                !primitives(to.shown).empty();
  const auto atOne = psx::present::composeFrame(to.shown, &from.state, to.state, 1.0f, producers);
  check.asDrawn = atOne && sameFrame(primitives(*atOne), primitives(to.shown));
  const auto atHalf = psx::present::composeFrame(to.shown, &from.state, to.state, 0.5f, producers);
  check.between = atHalf && sameFrame(primitives(*atHalf), primitives(midway.shown));
  check.midwayMoved = !sameFrame(primitives(midway.shown), primitives(to.shown));
  return check;
}

template <typename Scene> ComposeCheck composeAcrossTwoFields(Scene &scene) {
  terrain::CameraState after = kRestCamera;
  after.angles[1] = 0x20;
  after.position[0] += 0x40;
  terrain::CameraState midway = kRestCamera;
  midway.angles[1] = 0x10;
  midway.position[0] += 0x20;
  const DrawnFrame from = scene.drawFrame(kRestCamera, 0);
  const DrawnFrame half = scene.drawFrame(midway, 1);
  const DrawnFrame to = scene.drawFrame(after, 2);

  ComposeCheck check = composeBetween(scene.producers(), from, half, to);
  // The next field's update rewrites level words before `to` is presented.
  scene.rewriteLevel();
  const auto rewritten =
      psx::present::composeFrame(to.shown, &from.state, to.state, 1.0f, scene.producers());
  check.keepsItsLevel = rewritten && sameFrame(primitives(*rewritten), primitives(to.shown));
  return check;
}

} // namespace spyro::test
