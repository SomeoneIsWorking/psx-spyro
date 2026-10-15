#include "render/terrain/terrain_rebuild.h"

#include "gte_registers.h"
#include "lucent/log.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace spyro::guest_terrain {
namespace {

namespace gte = psx::gte;

// The end of the 2 MB main-RAM window a guest packet address can name.

// One field's terrain allocates at most 55,672 bytes of packets (measured on SCUS_944.25).
constexpr std::uint32_t kMinArenaBytes = 0x10000u;

std::int32_t signedHalf(std::uint32_t word) {
  const std::uint32_t half = word & 0xFFFFu;
  return half >= 0x8000u ? static_cast<std::int32_t>(half) - 0x10000
                         : static_cast<std::int32_t>(half);
}

// One camera word at the two guest frames.
struct WordPair {
  std::uint32_t from = 0;
  std::uint32_t to = 0;
};

std::uint32_t lerpWord(const WordPair &words, double f) {
  const auto a = static_cast<std::int64_t>(static_cast<std::int32_t>(words.from));
  const auto b = static_cast<std::int64_t>(static_cast<std::int32_t>(words.to));
  const auto v = a + static_cast<std::int64_t>(std::llround(static_cast<double>(b - a) * f));
  return static_cast<std::uint32_t>(static_cast<std::uint64_t>(v));
}

} // namespace

CameraState readCamera(Core &core, const guest_render_globals::Globals &globals) {
  // The first two angles share a word, low half then high half; the third is the next word's low
  // half.
  CameraState state;
  state.angles[0] = signedHalf(core.mem_r32(globals.cameraAngles));
  state.angles[1] = signedHalf(core.mem_r32(globals.cameraAngles) >> 16);
  state.angles[2] = signedHalf(core.mem_r32(globals.cameraAngles + 4));
  for (std::uint32_t i = 0; i < 3; ++i) {
    state.position[i] = core.mem_r32(globals.cameraPosition + 4 * i);
  }
  return state;
}

InBetweenCamera cameraBetween(Core &core,
                              const TerrainImage &image,
                              const CameraState &from,
                              const CameraState &to,
                              double f) {
  const double at = std::clamp(f, 0.0, 1.0);
  const guest_camera::Angles angles =
      guest_camera::interpolate({from.angles[0], from.angles[1], from.angles[2]},
                                {to.angles[0], to.angles[1], to.angles[2]},
                                at);
  const guest_camera::Matrices matrices = image.camera(core, angles);
  InBetweenCamera camera{};
  for (std::uint32_t i = 0; i < gte::kRotationWords; ++i) {
    camera.rotation[i] = matrices.projection[i];
    camera.viewRotation[i] = matrices.view[i];
  }
  for (std::uint32_t i = 0; i < 3; ++i) {
    camera.position[i] = lerpWord({from.position[i], to.position[i]}, at);
  }
  return camera;
}

std::vector<std::uint8_t> unionVisibility(std::span<const std::uint8_t> from,
                                          std::span<const std::uint8_t> to) {
  std::vector<std::uint8_t> out(to.size(), 0);
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i] = static_cast<std::uint8_t>(to[i] != 0 || (i < from.size() && from[i] != 0));
  }
  return out;
}

Rebuild::Rebuild(Core &core,
                 const TerrainImage &image,
                 const InBetweenCamera &camera,
                 const FieldInputs &field)
    : core_(core), memory_(core), camera_(camera) {
  const FieldWindows windows = image.windows(core);
  const guest_memory::PacketArena arena = windows.inBetweenArena;
  if (arena.bytes < kMinArenaBytes) {
    lucent::error("terrrebuild",
                  "refusing a terrain in-between: its packet arena runs from 0x{:08X} for {} "
                  "bytes, and one field's terrain needs more than {}",
                  arena.base,
                  arena.bytes,
                  kMinArenaBytes);
    std::abort();
  }

  // Every range starts empty: seeding from the guest would hand the traversal the last real
  // field's lists and bins.
  for (const guest_memory::GuestRange &range : windows.owned) {
    memory_.mapEmpty(range.base, range.bytes);
  }
  memory_.mapEmpty(arena.base, arena.bytes);
  memory_.overlay(field.readWords);

  // The traversal issues the guest's own GTE instructions; the guest reads them back afterwards.
  const GteRegs &gte = core.gte().registers();
  std::copy(std::begin(gte.REG), std::end(gte.REG), gteRegistersBefore_.begin());
  gteFlagsBefore_ = gte.FLAGS;

  for (std::size_t i = 0; i < field.visibleSectors.size(); ++i) {
    if (field.visibleSectors[i] != 0) {
      memory_.w8(kScratchpad + static_cast<std::uint32_t>(i), field.visibleSectors[i]);
      ++visible_;
    }
  }
  drawer_.emplace(core,
                  image,
                  memory_,
                  FrameMode::InBetween,
                  InBetweenWindow{.camera = &camera_,
                                  .argument = field.argument,
                                  .arenaBase = arena.base,
                                  .packetBudget = field.packetBudget});
  const TerrainFrame &ran = drawer_->run();
  tableBase_ = windows.tableBase;
  arenaBase_ = arena.base;
  arenaBytes_ = arena.bytes;
  linkedBins_ = &ran.linkedBins();
}

Rebuild::~Rebuild() {
  GteRegs &gte = core_.gte().registers();
  std::copy(gteRegistersBefore_.begin(), gteRegistersBefore_.end(), std::begin(gte.REG));
  gte.FLAGS = gteFlagsBefore_;
}

} // namespace spyro::guest_terrain
